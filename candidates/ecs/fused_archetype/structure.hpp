#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "gds/types.hpp"

namespace gds::candidates {

// archetype with integrate deferred into the next query and applied in the
// walk that answers it.
//
// In every workload that integrates, a frame is point operations, integrate,
// one to three queries and sync. archetype walks the Position and Velocity
// columns once for integrate and again for every query that names them. Here
// integrate(dt) records the step and nothing else. The next query resolves it
// group by group: a group holding both of the pair that the query matches is
// stepped and digested block by block, so the digest reads each block from L1
// right after the step wrote it; a group holding the pair that the query does
// not match is only stepped; a matching group without the pair is only
// digested. The columns are then brought in from beyond L1 once per frame
// instead of twice.
//
// Every position still receives exactly one p += v*dt per integrate call, with
// the dt of that call, component by component in x, y, z order, before anything
// observes it. That is the parent's arithmetic in the parent's order per
// entity, so every observation is bit-identical to applying the step eagerly.
//
// The parent's code is reproduced rather than included by path. Every member of
// Archetype is private, and the fused query has to write a group's Position
// column and read all of its columns by row, which neither a derived class nor a
// wrapper around the public interface can reach.
//
// The reproduction keeps the parent's text wherever the mechanism does not need
// to change it, because the predictions compare frame time against archetype
// and a change in code shape moves frame time by as much as the thresholds. The
// registry, the handle layout, the group table, append, swap-remove, the move
// between groups and the query with no step pending are the parent's. Apart
// from the class name and the constants kPair and kFuseRows, what differs is
// this and nothing else:
// - integrate records the step. The walk that applies a step on its own is the
//   parent's integrate loop, moved into resolve_pending.
// - A query with a step pending is answered by query_and_step, the one new
//   walk.
// - create, add, remove, get, set and sync each test the pending flag and may
//   call resolve_pending; the rest of each body is the parent's.
// - The group vector and the pending state are mutable, because query and get
//   are const in the contract and may have to apply the step.
// - reported_bytes also counts the group vector, which the parent leaves out.
// query_and_step and resolve_pending are kept out of line. Inlined, the new walk
// would be compiled into the same function as the parent's query loop, and the
// step pass into every point operation, and the code around them would no
// longer be compiled as the parent's is.
class FusedArchetype {
 public:
  static const char* name() { return "fused_archetype"; }

  FusedArchetype() {
    for (int i = 0; i < kTableSize; ++i) table_[i] = kInvalid;
  }

  // Resolves when the new entity holds both of the pair: it is appended to a
  // group the pending step covers, and would otherwise receive a step recorded
  // before it existed. Any other mask lands in a group the step never touches.
  Entity create(ComponentMask mask, const ComponentValue* values) {
    if (pending_ && (mask & kPair) == kPair) resolve_pending();
    std::uint32_t index;
    if (!free_list_.empty()) {
      index = free_list_.back();
      free_list_.pop_back();
    } else {
      index = static_cast<std::uint32_t>(entries_.size());
      entries_.push_back(Entry{1, 0, kInvalid, 0});
    }
    Entry& en = entries_[index];
    en.alive = 1;
    const std::uint32_t g = group_for(mask);
    en.group = g;
    en.row = append_row(g, index, mask, values);
    ++live_;
    return Entity{(static_cast<std::uint64_t>(en.generation) << 32) | index};
  }

  // No resolve. The swap-remove moves the last row's Position and Velocity
  // together into the hole, so the moved entity still meets the step with its
  // own two values, and the destroyed entity's step can no longer be observed.
  void destroy(Entity e) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return;
    remove_row(entries_[i].group, entries_[i].row);
    entries_[i].alive = 0;
    entries_[i].group = kInvalid;
    ++entries_[i].generation;
    free_list_.push_back(i);
    --live_;
  }

  // No resolve: the step changes no liveness.
  bool alive(Entity e) const { return resolve(e) != kInvalid; }

  // Resolves when the component is Position or Velocity and the entity holds
  // both after the add. Overwriting Position would have the step added to the
  // new value; overwriting Velocity would have the step computed from the new
  // velocity; gaining the second of the pair moves the entity into a group the
  // step covers although it did not hold both when integrate was called. An
  // entity that does not hold both after the add did not hold both before, and
  // the step never touches it. Adding Health or Tag changes neither value, and
  // the move it causes carries Position and Velocity together into a group that
  // is stepped exactly when the old one was.
  void add(Entity e, ComponentId c, const ComponentValue& v) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return;
    Entry& en = entries_[i];
    if (pending_ && (mask_of(c) & kPair) &&
        ((groups_[en.group].mask | mask_of(c)) & kPair) == kPair) {
      resolve_pending();
    }
    if (groups_[en.group].mask & mask_of(c)) {
      write(en.group, en.row, c, v);
      return;
    }
    ComponentValue vals[kComponentCount];
    const ComponentMask old_mask = groups_[en.group].mask;
    read_all(en.group, en.row, old_mask, vals);
    vals[static_cast<int>(c)] = v;
    move_entity(i, static_cast<ComponentMask>(old_mask | mask_of(c)), vals);
  }

  // Resolves only when Velocity is removed from an entity that holds Position:
  // the position stays and must receive the step computed from the velocity
  // being discarded. Removing Position from an entity holding both discards the
  // one value the step would change, and no later operation can read it back,
  // since an add writes a fresh value. Removing Health or Tag keeps the pair
  // together, as in add.
  void remove(Entity e, ComponentId c) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return;
    Entry& en = entries_[i];
    const ComponentMask old_mask = groups_[en.group].mask;
    if (!(old_mask & mask_of(c))) return;
    if (pending_ && c == ComponentId::Velocity && (old_mask & kPosition)) resolve_pending();
    ComponentValue vals[kComponentCount];
    read_all(en.group, en.row, old_mask, vals);
    move_entity(i, static_cast<ComponentMask>(old_mask & ~mask_of(c)), vals);
  }

  // Resolves only for the Position of an entity holding both, the one value
  // the step changes.
  bool get(Entity e, ComponentId c, ComponentValue& out) const {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return false;
    const Entry& en = entries_[i];
    if (!(groups_[en.group].mask & mask_of(c))) return false;
    if (pending_ && c == ComponentId::Position && (groups_[en.group].mask & kPair) == kPair) {
      resolve_pending();
    }
    read(en.group, en.row, c, out);
    return true;
  }

  // Resolves when the component is Position or Velocity and the entity holds
  // both, for the reasons an overwriting add does.
  bool set(Entity e, ComponentId c, const ComponentValue& v) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return false;
    const Entry& en = entries_[i];
    if (!(groups_[en.group].mask & mask_of(c))) return false;
    if (pending_ && (mask_of(c) & kPair) && (groups_[en.group].mask & kPair) == kPair) {
      resolve_pending();
    }
    write(en.group, en.row, c, v);
    return true;
  }

  // No resolve: the step changes no mask.
  ComponentMask mask(Entity e) const {
    const std::uint32_t i = resolve(e);
    return i == kInvalid ? ComponentMask(0) : groups_[entries_[i].group].mask;
  }

  // With no step pending, the parent's query. With one pending, query_and_step
  // answers and resolves it in one walk and clears the flag, so a second query
  // in the same frame comes here and takes the parent's loop.
  std::uint64_t query(ComponentMask required, std::uint64_t salt) const {
    if (pending_) return query_and_step(required, salt);
    std::uint64_t acc = 0;
    ComponentValue v[kComponentCount];
    for (const Group& g : groups_) {
      if ((g.mask & required) != required) continue;
      const std::size_t n = g.entity.size();
      for (std::size_t r = 0; r < n; ++r) {
        if (required & kPosition) v[0].position = g.position[r];
        if (required & kVelocity) v[1].velocity = g.velocity[r];
        if (required & kHealth) v[2].health = g.health[r];
        if (required & kTag) v[3].tag = g.tag[r];
        acc += digest_entity(required, v, salt);
      }
    }
    return acc;
  }

  // A second step cannot be folded into the first: (p + v*a) + v*b is not
  // p + v*(a + b) in float arithmetic. So a pending step is applied before the
  // next is recorded, and at most one is ever pending.
  void integrate(float dt) {
    if (pending_) resolve_pending();
    pending_ = true;
    pending_dt_ = dt;
  }

  // A frame whose queries name nothing, or a frame with no query at all, still
  // has its step applied here, so no step outlives the frame it belongs to.
  void sync() {
    if (pending_) resolve_pending();
  }

  // No resolve: the step creates and destroys nothing.
  std::size_t entity_count() const { return live_; }

  // The parent's count plus the group vector itself, which is a heap
  // allocation the structure owns and the parent leaves out. The pending step
  // allocates nothing.
  std::size_t reported_bytes() const {
    std::size_t b = entries_.capacity() * sizeof(Entry) +
                    free_list_.capacity() * sizeof(std::uint32_t) + sizeof(table_) +
                    groups_.capacity() * sizeof(Group);
    for (const Group& g : groups_) {
      b += g.entity.capacity() * sizeof(std::uint32_t) +
           g.position.capacity() * sizeof(Position) + g.velocity.capacity() * sizeof(Velocity) +
           g.health.capacity() * sizeof(Health) + g.tag.capacity() * sizeof(Tag);
    }
    return b;
  }

 private:
  static constexpr std::uint32_t kInvalid = ~0u;
  static constexpr int kTableSize = 1 << kComponentCount;
  static constexpr ComponentMask kPair = kPosition | kVelocity;

  // Rows per block of the fused walk. A block of all four columns is at most
  // 36 bytes a row, 9 KB at 256 rows: under a fifth of a 48 KB L1d and under a
  // third of a 32 KB one, so the block the step writes is still in L1 when the
  // digest reads it, beside the stack and the prefetcher's lines for the next
  // block. Per block the overhead is two loop entries, tens of cycles, against
  // 256 digests of tens of cycles each. 256 rows of each column is a whole
  // number of 64-byte lines (48, 48, 32 and 16), so a block boundary falls at
  // the same offset within a line in every block.
  static constexpr std::size_t kFuseRows = 256;

  struct Entry {
    std::uint32_t generation;
    std::uint32_t alive;
    std::uint32_t group;
    std::uint32_t row;
  };

  struct Group {
    ComponentMask mask = 0;
    std::vector<std::uint32_t> entity;
    std::vector<Position> position;
    std::vector<Velocity> velocity;
    std::vector<Health> health;
    std::vector<Tag> tag;
  };

  // The one new walk. Every group holding both of the pair is stepped whether
  // or not the query matches it, so the step is resolved when this returns.
  // A group that is both stepped and matched is walked in blocks of kFuseRows,
  // each stepped and then digested, so the digest's loads hit lines the step
  // has just written; a group that is only one of the two is a single block.
  // Per block the step is the parent's integrate loop and the digest is the
  // parent's query loop, rather than one loop doing both per row: that keeps
  // the step vectorised, so the measurement charges the fusion with the second
  // walk it removes and not also with the loss of a vectorised step.
  [[gnu::noinline]] std::uint64_t query_and_step(ComponentMask required,
                                                 std::uint64_t salt) const {
    const float dt = pending_dt_;
    pending_ = false;
    std::uint64_t acc = 0;
    ComponentValue v[kComponentCount];
    for (Group& g : groups_) {
      const bool stepped = (g.mask & kPair) == kPair;
      const bool matched = (g.mask & required) == required;
      if (!stepped && !matched) continue;
      const std::size_t n = g.entity.size();
      const std::size_t block = stepped && matched ? kFuseRows : n;
      for (std::size_t b = 0; b < n; b += block) {
        const std::size_t end = std::min(n, b + block);
        if (stepped) {
          // Scoped so that the restrict pointers cover the step only; the
          // digest below reads the same column through the vector.
          Position* __restrict p = g.position.data();
          const Velocity* __restrict w = g.velocity.data();
          for (std::size_t r = b; r < end; ++r) {
            p[r].x += w[r].x * dt;
            p[r].y += w[r].y * dt;
            p[r].z += w[r].z * dt;
          }
        }
        if (matched) {
          for (std::size_t r = b; r < end; ++r) {
            if (required & kPosition) v[0].position = g.position[r];
            if (required & kVelocity) v[1].velocity = g.velocity[r];
            if (required & kHealth) v[2].health = g.health[r];
            if (required & kTag) v[3].tag = g.tag[r];
            acc += digest_entity(required, v, salt);
          }
        }
      }
    }
    return acc;
  }

  // The parent's integrate loop: the plain pass every operation other than
  // query uses to catch up. It is const because get and query are const in the
  // contract; applying the step changes no answer the structure gives, only how
  // far its columns lag behind those answers, which is what the mutable
  // members below are for.
  [[gnu::noinline]] void resolve_pending() const {
    const float dt = pending_dt_;
    pending_ = false;
    const ComponentMask need = kPosition | kVelocity;
    for (Group& g : groups_) {
      if ((g.mask & need) != need) continue;
      const std::size_t n = g.position.size();
      Position* __restrict p = g.position.data();
      const Velocity* __restrict v = g.velocity.data();
      for (std::size_t r = 0; r < n; ++r) {
        p[r].x += v[r].x * dt;
        p[r].y += v[r].y * dt;
        p[r].z += v[r].z * dt;
      }
    }
  }

  std::uint32_t resolve(Entity e) const {
    const std::uint32_t i = static_cast<std::uint32_t>(e.bits & 0xFFFFFFFFu);
    if (i >= entries_.size()) return kInvalid;
    const Entry& en = entries_[i];
    if (en.generation != static_cast<std::uint32_t>(e.bits >> 32) || !en.alive) return kInvalid;
    return i;
  }

  std::uint32_t group_for(ComponentMask mask) {
    if (table_[mask] != kInvalid) return table_[mask];
    const std::uint32_t id = static_cast<std::uint32_t>(groups_.size());
    groups_.push_back(Group{});
    groups_.back().mask = mask;
    table_[mask] = id;
    return id;
  }

  std::uint32_t append_row(std::uint32_t g, std::uint32_t entity, ComponentMask mask,
                           const ComponentValue* values) {
    Group& gr = groups_[g];
    const std::uint32_t row = static_cast<std::uint32_t>(gr.entity.size());
    gr.entity.push_back(entity);
    if (mask & kPosition) gr.position.push_back(values[0].position);
    if (mask & kVelocity) gr.velocity.push_back(values[1].velocity);
    if (mask & kHealth) gr.health.push_back(values[2].health);
    if (mask & kTag) gr.tag.push_back(values[3].tag);
    return row;
  }

  void remove_row(std::uint32_t g, std::uint32_t row) {
    Group& gr = groups_[g];
    const std::uint32_t moved = gr.entity.back();
    gr.entity[row] = moved;
    gr.entity.pop_back();
    if (gr.mask & kPosition) { gr.position[row] = gr.position.back(); gr.position.pop_back(); }
    if (gr.mask & kVelocity) { gr.velocity[row] = gr.velocity.back(); gr.velocity.pop_back(); }
    if (gr.mask & kHealth) { gr.health[row] = gr.health.back(); gr.health.pop_back(); }
    if (gr.mask & kTag) { gr.tag[row] = gr.tag.back(); gr.tag.pop_back(); }
    if (row < gr.entity.size()) entries_[moved].row = row;
  }

  void read(std::uint32_t g, std::uint32_t row, ComponentId c, ComponentValue& out) const {
    const Group& gr = groups_[g];
    switch (c) {
      case ComponentId::Position: out.position = gr.position[row]; break;
      case ComponentId::Velocity: out.velocity = gr.velocity[row]; break;
      case ComponentId::Health: out.health = gr.health[row]; break;
      case ComponentId::Tag: out.tag = gr.tag[row]; break;
    }
  }

  void write(std::uint32_t g, std::uint32_t row, ComponentId c, const ComponentValue& v) {
    Group& gr = groups_[g];
    switch (c) {
      case ComponentId::Position: gr.position[row] = v.position; break;
      case ComponentId::Velocity: gr.velocity[row] = v.velocity; break;
      case ComponentId::Health: gr.health[row] = v.health; break;
      case ComponentId::Tag: gr.tag[row] = v.tag; break;
    }
  }

  void read_all(std::uint32_t g, std::uint32_t row, ComponentMask mask,
                ComponentValue* out) const {
    const Group& gr = groups_[g];
    if (mask & kPosition) out[0].position = gr.position[row];
    if (mask & kVelocity) out[1].velocity = gr.velocity[row];
    if (mask & kHealth) out[2].health = gr.health[row];
    if (mask & kTag) out[3].tag = gr.tag[row];
  }

  void move_entity(std::uint32_t i, ComponentMask new_mask, const ComponentValue* vals) {
    Entry& en = entries_[i];
    const std::uint32_t old_group = en.group;
    const std::uint32_t old_row = en.row;
    const std::uint32_t new_group = group_for(new_mask);
    const std::uint32_t new_row = append_row(new_group, i, new_mask, vals);
    remove_row(old_group, old_row);
    en.group = new_group;
    en.row = new_row;
  }

  std::vector<Entry> entries_;
  std::vector<std::uint32_t> free_list_;
  // Mutable because query and get are const in the contract and may have to
  // apply the pending step to the Position columns before they answer.
  mutable std::vector<Group> groups_;
  std::uint32_t table_[kTableSize];
  std::size_t live_ = 0;
  mutable bool pending_ = false;
  mutable float pending_dt_ = 0.0f;
};

}  // namespace gds::candidates
