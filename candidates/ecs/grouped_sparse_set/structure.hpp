#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "gds/types.hpp"

namespace gds::candidates {

// sparse_set with one owning group over {Position, Velocity}.
//
// Every entity holding both components sits in the first G positions of both
// Position's and Velocity's dense arrays, at the same position in each. The
// group is therefore two aligned dense prefixes: integrate walks [0, G) of both
// value arrays in lockstep with no sparse lookup, and a query over a mask that
// contains both can be driven from the group the same way. Entities holding
// only one of the pair sit at position G or later in that component's array;
// join_group and leave_group both rely on position G holding a non-member.
//
// The parent's code is reproduced here rather than included by path. Every
// member of SparseSet is private and it has no extension point, and the group
// has to change what its insert and erase do to the dense order, which neither
// a derived class nor a wrapper around its public interface can reach. The
// registry (generation, mask, liveness, free list), the handle layout, get,
// set, mask and the swap-erase are the parent's, unchanged; what differs is
// marked where it happens.
class GroupedSparseSet {
 public:
  static const char* name() { return "grouped_sparse_set"; }

  Entity create(ComponentMask mask, const ComponentValue* values) {
    std::uint32_t index;
    if (!free_list_.empty()) {
      index = free_list_.back();
      free_list_.pop_back();
    } else {
      index = static_cast<std::uint32_t>(generation_.size());
      generation_.push_back(1);
      mask_.push_back(0);
      alive_.push_back(0);
      for (auto& s : sets_) s.sparse.push_back(kInvalid);
    }
    mask_[index] = mask;
    alive_[index] = 1;
    for (int c = 0; c < kComponentCount; ++c) {
      if (mask & (1u << c)) insert(c, index, values[c]);
    }
    if ((mask & kPair) == kPair) join_group(index);
    ++live_;
    return Entity{(static_cast<std::uint64_t>(generation_[index]) << 32) | index};
  }

  void destroy(Entity e) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return;
    // Leaving first puts the entity at position G of both arrays, outside the
    // prefix, so the swap-erases below never move a group member.
    if ((mask_[i] & kPair) == kPair) leave_group(i);
    for (int c = 0; c < kComponentCount; ++c) {
      if (mask_[i] & (1u << c)) erase(c, i);
    }
    mask_[i] = 0;
    alive_[i] = 0;
    ++generation_[i];
    free_list_.push_back(i);
    --live_;
  }

  bool alive(Entity e) const { return resolve(e) != kInvalid; }

  void add(Entity e, ComponentId c, const ComponentValue& v) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return;
    const int ci = static_cast<int>(c);
    const ComponentMask bit = mask_of(c);
    if (mask_[i] & bit) {
      sets_[ci].values[sets_[ci].sparse[i]] = v;
      return;
    }
    mask_[i] |= bit;
    insert(ci, i, v);
    if ((bit & kPair) && (mask_[i] & kPair) == kPair) join_group(i);
  }

  void remove(Entity e, ComponentId c) {
    const std::uint32_t i = resolve(e);
    const ComponentMask bit = mask_of(c);
    if (i == kInvalid || !(mask_[i] & bit)) return;
    if ((bit & kPair) && (mask_[i] & kPair) == kPair) leave_group(i);
    mask_[i] &= static_cast<ComponentMask>(~bit);
    erase(static_cast<int>(c), i);
  }

  bool get(Entity e, ComponentId c, ComponentValue& out) const {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid || !(mask_[i] & mask_of(c))) return false;
    out = sets_[static_cast<int>(c)].values[sets_[static_cast<int>(c)].sparse[i]];
    return true;
  }

  bool set(Entity e, ComponentId c, const ComponentValue& v) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid || !(mask_[i] & mask_of(c))) return false;
    sets_[static_cast<int>(c)].values[sets_[static_cast<int>(c)].sparse[i]] = v;
    return true;
  }

  ComponentMask mask(Entity e) const {
    const std::uint32_t i = resolve(e);
    return i == kInvalid ? ComponentMask(0) : mask_[i];
  }

  // Drives from the smallest of the group, when the mask names both of its
  // components, and each required component's set. Ties go to the group,
  // because it is the only driver from which two components are read without a
  // lookup; since G never exceeds either set of the pair, a mask containing
  // both is never driven from Position's or Velocity's set. Only the required
  // components the driver does not already supply are probed.
  std::uint64_t query(ComponentMask required, std::uint64_t salt) const {
    if (required == 0) return 0;
    const bool from_group = (required & kPair) == kPair;
    int driver = -1;
    std::size_t best = from_group ? group_size_ : ~std::size_t(0);
    for (int c = 0; c < kComponentCount; ++c) {
      if (!(required & (1u << c))) continue;
      if (sets_[c].dense.size() < best) {
        best = sets_[c].dense.size();
        driver = c;
      }
    }
    if (driver >= 0) return query_set(driver, required, salt);
    return from_group ? query_group(required, salt) : 0;
  }

  // The aligned walk: position k of Position's values and position k of
  // Velocity's values belong to the same entity for every k below G, and every
  // entity holding both is below G, so no mask test is needed either.
  void integrate(float dt) {
    ComponentValue* __restrict p = sets_[kPos].values.data();
    const ComponentValue* __restrict v = sets_[kVel].values.data();
    const std::size_t n = group_size_;
    for (std::size_t k = 0; k < n; ++k) {
      p[k].position.x += v[k].velocity.x * dt;
      p[k].position.y += v[k].velocity.y * dt;
      p[k].position.z += v[k].velocity.z * dt;
    }
  }

  void sync() {}

  std::size_t entity_count() const { return live_; }

  // The group adds no allocation: it is an ordering of the parent's arrays and
  // one counter.
  std::size_t reported_bytes() const {
    std::size_t b = generation_.capacity() * sizeof(std::uint32_t) + mask_.capacity() +
                    alive_.capacity() + free_list_.capacity() * sizeof(std::uint32_t);
    for (const Set& s : sets_) {
      b += s.sparse.capacity() * sizeof(std::uint32_t) +
           s.dense.capacity() * sizeof(std::uint32_t) +
           s.values.capacity() * sizeof(ComponentValue);
    }
    return b;
  }

 private:
  static constexpr std::uint32_t kInvalid = ~0u;
  static constexpr int kPos = static_cast<int>(ComponentId::Position);
  static constexpr int kVel = static_cast<int>(ComponentId::Velocity);
  static constexpr ComponentMask kPair = kPosition | kVelocity;

  struct Set {
    std::vector<std::uint32_t> sparse;  // entity index -> dense position
    std::vector<std::uint32_t> dense;   // dense position -> entity index
    std::vector<ComponentValue> values; // parallel to dense
  };

  std::uint64_t query_group(ComponentMask required, std::uint64_t salt) const {
    const ComponentMask rest = static_cast<ComponentMask>(required & ~kPair);
    const Set& ps = sets_[kPos];
    const Set& vs = sets_[kVel];
    std::uint64_t acc = 0;
    ComponentValue v[kComponentCount];
    for (std::size_t k = 0; k < group_size_; ++k) {
      if (rest) {
        const std::uint32_t i = ps.dense[k];
        if ((mask_[i] & rest) != rest) continue;
        for (int c = 0; c < kComponentCount; ++c) {
          if (rest & (1u << c)) v[c] = sets_[c].values[sets_[c].sparse[i]];
        }
      }
      v[kPos] = ps.values[k];
      v[kVel] = vs.values[k];
      acc += digest_entity(required, v, salt);
    }
    return acc;
  }

  // Driven from one component's set. When both of the pair are still to be
  // probed, a qualifying entity is a group member, so its Velocity sits at the
  // same position as its Position and one sparse lookup serves both.
  //
  // The mask test and the driver's read below differ from the parent for
  // reasons that do not depend on the group: the test covers only the
  // components still to be probed and is skipped when there are none, and the
  // driver's value is read at k instead of back through its own sparse array.
  // Restoring the parent's form of those two lines leaves a group-only variant,
  // which is the comparison that attributes a query-side gain to the group.
  std::uint64_t query_set(int d, ComponentMask required, std::uint64_t salt) const {
    const ComponentMask rest = static_cast<ComponentMask>(required & ~(1u << d));
    const bool pair_rest = (rest & kPair) == kPair;
    const ComponentMask lookups = pair_rest ? static_cast<ComponentMask>(rest & ~kPair) : rest;
    const Set& ds = sets_[d];
    const Set& ps = sets_[kPos];
    const Set& vs = sets_[kVel];
    std::uint64_t acc = 0;
    ComponentValue v[kComponentCount];
    for (std::size_t k = 0; k < ds.dense.size(); ++k) {
      const std::uint32_t i = ds.dense[k];
      if (rest && (mask_[i] & rest) != rest) continue;
      v[d] = ds.values[k];
      if (pair_rest) {
        const std::uint32_t pos = ps.sparse[i];
        v[kPos] = ps.values[pos];
        v[kVel] = vs.values[pos];
      }
      for (int c = 0; c < kComponentCount; ++c) {
        if (lookups & (1u << c)) v[c] = sets_[c].values[sets_[c].sparse[i]];
      }
      acc += digest_entity(required, v, salt);
    }
    return acc;
  }

  std::uint32_t resolve(Entity e) const {
    const std::uint32_t i = static_cast<std::uint32_t>(e.bits & 0xFFFFFFFFu);
    if (i >= generation_.size()) return kInvalid;
    if (generation_[i] != static_cast<std::uint32_t>(e.bits >> 32)) return kInvalid;
    if (!alive_[i]) return kInvalid;
    return i;
  }

  // Appends past the group, as the parent does. A new member of the pair is
  // brought into the prefix afterwards by join_group.
  void insert(int c, std::uint32_t i, const ComponentValue& v) {
    Set& s = sets_[c];
    s.sparse[i] = static_cast<std::uint32_t>(s.dense.size());
    s.dense.push_back(i);
    s.values.push_back(v);
  }

  // The parent's swap-erase. For Position and Velocity, callers guarantee the
  // entity is not a group member here, so its position and the last position
  // are both at or past G and the element moved into the hole is not a member
  // either.
  void erase(int c, std::uint32_t i) {
    Set& s = sets_[c];
    const std::uint32_t pos = s.sparse[i];
    const std::uint32_t last = s.dense.back();
    s.dense[pos] = last;
    s.values[pos] = s.values.back();
    s.sparse[last] = pos;
    s.dense.pop_back();
    s.values.pop_back();
    s.sparse[i] = kInvalid;
  }

  static void swap_positions(Set& s, std::uint32_t a, std::uint32_t b) {
    if (a == b) return;
    const std::uint32_t ea = s.dense[a];
    const std::uint32_t eb = s.dense[b];
    s.dense[a] = eb;
    s.dense[b] = ea;
    std::swap(s.values[a], s.values[b]);
    s.sparse[eb] = a;
    s.sparse[ea] = b;
  }

  // The entity has just come to hold both components and sits at or past G in
  // each array; so does whatever is at position G, which is not a member. One
  // swap per array puts the entity at G in both, and the prefix grows over it.
  void join_group(std::uint32_t i) {
    swap_positions(sets_[kPos], sets_[kPos].sparse[i], group_size_);
    swap_positions(sets_[kVel], sets_[kVel].sparse[i], group_size_);
    ++group_size_;
  }

  // The reverse: the last member of the prefix is swapped into the leaving
  // entity's position in both arrays, which keeps the two aligned, and the
  // prefix shrinks off the leaving entity, which is left at G in both.
  void leave_group(std::uint32_t i) {
    --group_size_;
    const std::uint32_t pos = sets_[kPos].sparse[i];
    swap_positions(sets_[kPos], pos, group_size_);
    swap_positions(sets_[kVel], pos, group_size_);
  }

  std::vector<std::uint32_t> generation_;
  std::vector<ComponentMask> mask_;
  std::vector<std::uint8_t> alive_;
  std::vector<std::uint32_t> free_list_;
  Set sets_[kComponentCount];
  std::uint32_t group_size_ = 0;
  std::size_t live_ = 0;
};

}  // namespace gds::candidates
