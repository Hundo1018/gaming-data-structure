#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gds/types.hpp"

namespace gds::candidates {

// One record per slot, every component inline, whether present or not.
// Storage is a flat vector indexed by the handle's index field; a generation
// counter in the handle invalidates stale references. Nothing is ever moved,
// so a handle stays valid for the whole life of the entity.
class Aos {
 public:
  static const char* name() { return "aos"; }

  Entity create(ComponentMask mask, const ComponentValue* values) {
    std::uint32_t index;
    if (!free_list_.empty()) {
      index = free_list_.back();
      free_list_.pop_back();
    } else {
      index = static_cast<std::uint32_t>(records_.size());
      records_.push_back(Record{});
      records_.back().generation = 1;
    }
    Record& r = records_[index];
    r.alive = 1;
    r.mask = mask;
    for (int i = 0; i < kComponentCount; ++i) {
      if (mask & (1u << i)) r.values[i] = values[i];
    }
    ++live_;
    return make_handle(index, r.generation);
  }

  void destroy(Entity e) {
    Record* r = resolve(e);
    if (!r) return;
    r->mask = 0;
    r->alive = 0;
    ++r->generation;  // every outstanding handle to this slot is now stale
    free_list_.push_back(index_of(e));
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
      if (r.alive && (r.mask & required) == required) {
        acc += digest_entity(required, r.values);
      }
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
  // An entity with no components is still a live entity, so liveness is a
  // separate bit and never inferred from the mask.
  struct Record {
    std::uint32_t generation;
    ComponentMask mask;
    std::uint8_t alive;
    ComponentValue values[kComponentCount];
  };

  static Entity make_handle(std::uint32_t index, std::uint32_t generation) {
    return Entity{(static_cast<std::uint64_t>(generation) << 32) | index};
  }
  static std::uint32_t index_of(Entity e) { return static_cast<std::uint32_t>(e.bits & 0xFFFFFFFFu); }
  static std::uint32_t generation_of(Entity e) { return static_cast<std::uint32_t>(e.bits >> 32); }

  Record* resolve(Entity e) {
    const std::uint32_t i = index_of(e);
    if (i >= records_.size()) return nullptr;
    Record& r = records_[i];
    if (r.generation != generation_of(e) || !r.alive) return nullptr;
    return &r;
  }
  const Record* resolve(Entity e) const {
    return const_cast<Aos*>(this)->resolve(e);
  }

  std::vector<Record> records_;
  std::vector<std::uint32_t> free_list_;
  std::size_t live_ = 0;
};

}  // namespace gds::candidates
