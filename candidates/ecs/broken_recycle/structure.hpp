#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gds/types.hpp"

namespace gds::candidates {

// NEGATIVE CONTROL. Not a research candidate.
//
// Identical to `aos` except that the handle is a bare slot index with no
// generation counter, so a recycled slot silently answers for a destroyed
// entity's handle. It exists to demonstrate that the correctness gate rejects
// a structure that is fast for the wrong reason: it is expected to pass every
// workload without stale accesses and to fail h03_stale_handles.
class BrokenRecycle {
 public:
  static const char* name() { return "broken_recycle"; }

  Entity create(ComponentMask mask, const ComponentValue* values) {
    std::uint32_t index;
    if (!free_list_.empty()) {
      index = free_list_.back();
      free_list_.pop_back();
    } else {
      index = static_cast<std::uint32_t>(records_.size());
      records_.push_back(Record{});
    }
    Record& r = records_[index];
    r.alive = 1;
    r.mask = mask;
    for (int i = 0; i < kComponentCount; ++i) {
      if (mask & (1u << i)) r.values[i] = values[i];
    }
    ++live_;
    return Entity{index};
  }

  void destroy(Entity e) {
    Record* r = resolve(e);
    if (!r) return;
    r->mask = 0;
    r->alive = 0;
    free_list_.push_back(static_cast<std::uint32_t>(e.bits));
    --live_;
  }

  bool alive(Entity e) const { return resolve(e) != nullptr; }

  void add(Entity e, ComponentId c, const ComponentValue& v) {
    Record* r = resolve(e);
    if (!r) return;
    r->mask |= mask_of(c);
    r->values[static_cast<int>(c)] = v;
  }

  void remove(Entity e, ComponentId c) {
    Record* r = resolve(e);
    if (!r) return;
    r->mask &= static_cast<ComponentMask>(~mask_of(c));
  }

  bool get(Entity e, ComponentId c, ComponentValue& out) const {
    const Record* r = resolve(e);
    if (!r || !(r->mask & mask_of(c))) return false;
    out = r->values[static_cast<int>(c)];
    return true;
  }

  bool set(Entity e, ComponentId c, const ComponentValue& v) {
    Record* r = resolve(e);
    if (!r || !(r->mask & mask_of(c))) return false;
    r->values[static_cast<int>(c)] = v;
    return true;
  }

  ComponentMask mask(Entity e) const {
    const Record* r = resolve(e);
    return r ? r->mask : ComponentMask(0);
  }

  std::uint64_t query(ComponentMask required) const {
    std::uint64_t acc = 0;
    for (const Record& r : records_) {
      if (r.alive && (r.mask & required) == required) acc += digest_entity(required, r.values);
    }
    return acc;
  }

  void integrate(float dt) {
    const ComponentMask need = kPosition | kVelocity;
    for (Record& r : records_) {
      if (!r.alive || (r.mask & need) != need) continue;
      Position& p = r.values[static_cast<int>(ComponentId::Position)].position;
      const Velocity& v = r.values[static_cast<int>(ComponentId::Velocity)].velocity;
      p.x += v.x * dt;
      p.y += v.y * dt;
      p.z += v.z * dt;
    }
  }

  void sync() {}
  std::size_t entity_count() const { return live_; }
  std::size_t reported_bytes() const {
    return records_.capacity() * sizeof(Record) + free_list_.capacity() * sizeof(std::uint32_t);
  }

 private:
  struct Record {
    ComponentMask mask = 0;
    std::uint8_t alive = 0;
    ComponentValue values[kComponentCount];
  };

  Record* resolve(Entity e) {
    const std::uint32_t i = static_cast<std::uint32_t>(e.bits);
    if (i >= records_.size() || !records_[i].alive) return nullptr;
    return &records_[i];
  }
  const Record* resolve(Entity e) const { return const_cast<BrokenRecycle*>(this)->resolve(e); }

  std::vector<Record> records_;
  std::vector<std::uint32_t> free_list_;
  std::size_t live_ = 0;
};

}  // namespace gds::candidates
