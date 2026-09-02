#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gds/types.hpp"

namespace gds::candidates {

// Entities are grouped by their exact component set. Within a group every
// component is a densely packed typed column with no holes and no per-entity
// mask test, so a query is a straight walk over the groups that match.
// The cost lands on structural change: add or remove a component and the whole
// entity is copied into a different group.
//
// Caveat recorded for later comparison: with four component types the group
// lookup is a 16-entry direct table. A production archetype store hashes an
// unbounded component set, so this implementation understates lookup cost.
class Archetype {
 public:
  static const char* name() { return "archetype"; }

  Archetype() {
    for (int i = 0; i < kTableSize; ++i) table_[i] = kInvalid;
  }

  Entity create(ComponentMask mask, const ComponentValue* values) {
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

  bool alive(Entity e) const { return resolve(e) != kInvalid; }

  void add(Entity e, ComponentId c, const ComponentValue& v) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return;
    Entry& en = entries_[i];
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

  void remove(Entity e, ComponentId c) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return;
    Entry& en = entries_[i];
    const ComponentMask old_mask = groups_[en.group].mask;
    if (!(old_mask & mask_of(c))) return;
    ComponentValue vals[kComponentCount];
    read_all(en.group, en.row, old_mask, vals);
    move_entity(i, static_cast<ComponentMask>(old_mask & ~mask_of(c)), vals);
  }

  bool get(Entity e, ComponentId c, ComponentValue& out) const {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return false;
    const Entry& en = entries_[i];
    if (!(groups_[en.group].mask & mask_of(c))) return false;
    read(en.group, en.row, c, out);
    return true;
  }

  bool set(Entity e, ComponentId c, const ComponentValue& v) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return false;
    const Entry& en = entries_[i];
    if (!(groups_[en.group].mask & mask_of(c))) return false;
    write(en.group, en.row, c, v);
    return true;
  }

  ComponentMask mask(Entity e) const {
    const std::uint32_t i = resolve(e);
    return i == kInvalid ? ComponentMask(0) : groups_[entries_[i].group].mask;
  }

  std::uint64_t query(ComponentMask required) const {
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
        acc += digest_entity(required, v);
      }
    }
    return acc;
  }

  void integrate(float dt) {
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

  void sync() {}

  std::size_t entity_count() const { return live_; }

  std::size_t reported_bytes() const {
    std::size_t b = entries_.capacity() * sizeof(Entry) +
                    free_list_.capacity() * sizeof(std::uint32_t) + sizeof(table_);
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
  std::vector<Group> groups_;
  std::uint32_t table_[kTableSize];
  std::size_t live_ = 0;
};

}  // namespace gds::candidates
