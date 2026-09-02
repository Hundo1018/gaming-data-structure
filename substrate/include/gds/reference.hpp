// The oracle.
//
// Slow, obvious, and written so that its correctness is easy to see by reading
// it. Every candidate is checked against this, never against another candidate.
#pragma once

#include <cstddef>
#include <cstring>
#include <unordered_map>

#include "gds/types.hpp"

namespace gds {

class ReferenceStructure {
 public:
  static const char* name() { return "reference"; }

  Entity create(ComponentMask mask, const ComponentValue* values) {
    const std::uint64_t id = next_id_++;
    Record r{};
    r.mask = mask;
    for (int i = 0; i < kComponentCount; ++i) {
      if (mask & (1u << i)) r.values[i] = values[i];
    }
    records_.emplace(id, r);
    return Entity{id};
  }

  void destroy(Entity e) { records_.erase(e.bits); }

  bool alive(Entity e) const { return records_.find(e.bits) != records_.end(); }

  void add(Entity e, ComponentId c, const ComponentValue& v) {
    auto it = records_.find(e.bits);
    if (it == records_.end()) return;
    it->second.mask |= mask_of(c);
    it->second.values[static_cast<int>(c)] = v;
  }

  void remove(Entity e, ComponentId c) {
    auto it = records_.find(e.bits);
    if (it == records_.end()) return;
    it->second.mask &= static_cast<ComponentMask>(~mask_of(c));
  }

  bool get(Entity e, ComponentId c, ComponentValue& out) const {
    auto it = records_.find(e.bits);
    if (it == records_.end()) return false;
    if (!(it->second.mask & mask_of(c))) return false;
    out = it->second.values[static_cast<int>(c)];
    return true;
  }

  bool set(Entity e, ComponentId c, const ComponentValue& v) {
    auto it = records_.find(e.bits);
    if (it == records_.end()) return false;
    if (!(it->second.mask & mask_of(c))) return false;
    it->second.values[static_cast<int>(c)] = v;
    return true;
  }

  ComponentMask mask(Entity e) const {
    auto it = records_.find(e.bits);
    return it == records_.end() ? ComponentMask(0) : it->second.mask;
  }

  std::uint64_t query(ComponentMask required) const {
    std::uint64_t acc = 0;
    for (const auto& kv : records_) {
      if ((kv.second.mask & required) == required) {
        acc += digest_entity(required, kv.second.values);
      }
    }
    return acc;
  }

  void integrate(float dt) {
    const ComponentMask need = kPosition | kVelocity;
    for (auto& kv : records_) {
      if ((kv.second.mask & need) != need) continue;
      Position& p = kv.second.values[static_cast<int>(ComponentId::Position)].position;
      const Velocity& v = kv.second.values[static_cast<int>(ComponentId::Velocity)].velocity;
      p.x += v.x * dt;
      p.y += v.y * dt;
      p.z += v.z * dt;
    }
  }

  void sync() {}

  std::size_t entity_count() const { return records_.size(); }

  std::size_t reported_bytes() const {
    return records_.size() * (sizeof(Record) + sizeof(std::uint64_t) + 2 * sizeof(void*));
  }

 private:
  struct Record {
    ComponentMask mask;
    ComponentValue values[kComponentCount];
  };
  std::unordered_map<std::uint64_t, Record> records_;
  std::uint64_t next_id_ = 1;
};

}  // namespace gds
