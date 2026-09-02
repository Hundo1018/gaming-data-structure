#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "gds/types.hpp"

namespace gds::candidates {

// One sparse set per component: a dense, packed array of values plus a sparse
// index from entity index to dense position. Component values are contiguous
// with no holes, so a query pays only for entities that actually carry the
// component; adding or removing a component touches one component's arrays and
// never relocates the entity.
class SparseSet {
 public:
  static const char* name() { return "sparse_set"; }

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
    ++live_;
    return Entity{(static_cast<std::uint64_t>(generation_[index]) << 32) | index};
  }

  void destroy(Entity e) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return;
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
    if (mask_[i] & mask_of(c)) {
      sets_[ci].values[sets_[ci].sparse[i]] = v;
      return;
    }
    mask_[i] |= mask_of(c);
    insert(ci, i, v);
  }

  void remove(Entity e, ComponentId c) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid || !(mask_[i] & mask_of(c))) return;
    mask_[i] &= static_cast<ComponentMask>(~mask_of(c));
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

  // Iterates the smallest participating set and probes the others. The choice
  // of driving set is what makes the cost proportional to the rarest component
  // rather than to the entity count.
  std::uint64_t query(ComponentMask required) const {
    if (required == 0) return 0;
    int driver = -1;
    std::size_t best = ~std::size_t(0);
    for (int c = 0; c < kComponentCount; ++c) {
      if (!(required & (1u << c))) continue;
      if (sets_[c].dense.size() < best) {
        best = sets_[c].dense.size();
        driver = c;
      }
    }
    std::uint64_t acc = 0;
    ComponentValue v[kComponentCount];
    const Set& d = sets_[driver];
    for (std::size_t k = 0; k < d.dense.size(); ++k) {
      const std::uint32_t i = d.dense[k];
      if ((mask_[i] & required) != required) continue;
      for (int c = 0; c < kComponentCount; ++c) {
        if (required & (1u << c)) v[c] = sets_[c].values[sets_[c].sparse[i]];
      }
      acc += digest_entity(required, v);
    }
    return acc;
  }

  void integrate(float dt) {
    const int pi = static_cast<int>(ComponentId::Position);
    const int vi = static_cast<int>(ComponentId::Velocity);
    const Set& vs = sets_[vi];
    Set& ps = sets_[pi];
    const ComponentMask need = kPosition | kVelocity;
    for (std::size_t k = 0; k < vs.dense.size(); ++k) {
      const std::uint32_t i = vs.dense[k];
      if ((mask_[i] & need) != need) continue;
      Position& p = ps.values[ps.sparse[i]].position;
      const Velocity& v = vs.values[k].velocity;
      p.x += v.x * dt;
      p.y += v.y * dt;
      p.z += v.z * dt;
    }
  }

  void sync() {}

  std::size_t entity_count() const { return live_; }

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

  struct Set {
    std::vector<std::uint32_t> sparse;  // entity index -> dense position
    std::vector<std::uint32_t> dense;   // dense position -> entity index
    std::vector<ComponentValue> values; // parallel to dense
  };

  std::uint32_t resolve(Entity e) const {
    const std::uint32_t i = static_cast<std::uint32_t>(e.bits & 0xFFFFFFFFu);
    if (i >= generation_.size()) return kInvalid;
    if (generation_[i] != static_cast<std::uint32_t>(e.bits >> 32)) return kInvalid;
    if (!alive_[i]) return kInvalid;
    return i;
  }

  void insert(int c, std::uint32_t i, const ComponentValue& v) {
    Set& s = sets_[c];
    s.sparse[i] = static_cast<std::uint32_t>(s.dense.size());
    s.dense.push_back(i);
    s.values.push_back(v);
  }

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

  std::vector<std::uint32_t> generation_;
  std::vector<ComponentMask> mask_;
  std::vector<std::uint8_t> alive_;
  std::vector<std::uint32_t> free_list_;
  Set sets_[kComponentCount];
  std::size_t live_ = 0;
};

}  // namespace gds::candidates
