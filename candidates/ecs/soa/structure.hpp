#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gds/types.hpp"

namespace gds::candidates {

// Same index space as `aos`, but each component lives in its own array.
// A query over Position+Velocity streams three arrays (mask, position,
// velocity) and never loads Health or Tag, so the bytes moved per matching
// entity fall while the number of streams rises.
class Soa {
 public:
  static const char* name() { return "soa"; }

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
      position_.push_back(Position{});
      velocity_.push_back(Velocity{});
      health_.push_back(Health{});
      tag_.push_back(Tag{});
    }
    mask_[index] = mask;
    alive_[index] = 1;
    if (mask & kPosition) position_[index] = values[0].position;
    if (mask & kVelocity) velocity_[index] = values[1].velocity;
    if (mask & kHealth) health_[index] = values[2].health;
    if (mask & kTag) tag_[index] = values[3].tag;
    ++live_;
    return make_handle(index, generation_[index]);
  }

  void destroy(Entity e) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return;
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
    mask_[i] |= mask_of(c);
    store(i, c, v);
  }

  void remove(Entity e, ComponentId c) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return;
    mask_[i] &= static_cast<ComponentMask>(~mask_of(c));
  }

  bool get(Entity e, ComponentId c, ComponentValue& out) const {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid || !(mask_[i] & mask_of(c))) return false;
    load(i, c, out);
    return true;
  }

  bool set(Entity e, ComponentId c, const ComponentValue& v) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid || !(mask_[i] & mask_of(c))) return false;
    store(i, c, v);
    return true;
  }

  ComponentMask mask(Entity e) const {
    const std::uint32_t i = resolve(e);
    return i == kInvalid ? ComponentMask(0) : mask_[i];
  }

  std::uint64_t query(ComponentMask required) const {
    std::uint64_t acc = 0;
    const std::size_t n = mask_.size();
    ComponentValue v[kComponentCount];
    for (std::size_t i = 0; i < n; ++i) {
      if (!alive_[i] || (mask_[i] & required) != required) continue;
      if (required & kPosition) v[0].position = position_[i];
      if (required & kVelocity) v[1].velocity = velocity_[i];
      if (required & kHealth) v[2].health = health_[i];
      if (required & kTag) v[3].tag = tag_[i];
      acc += digest_entity(required, v);
    }
    return acc;
  }

  void integrate(float dt) {
    const ComponentMask need = kPosition | kVelocity;
    const std::size_t n = mask_.size();
    for (std::size_t i = 0; i < n; ++i) {
      if (!alive_[i] || (mask_[i] & need) != need) continue;
      Position& p = position_[i];
      const Velocity& v = velocity_[i];
      p.x += v.x * dt;
      p.y += v.y * dt;
      p.z += v.z * dt;
    }
  }

  void sync() {}

  std::size_t entity_count() const { return live_; }

  std::size_t reported_bytes() const {
    return generation_.capacity() * sizeof(std::uint32_t) + mask_.capacity() +
           alive_.capacity() + position_.capacity() * sizeof(Position) +
           velocity_.capacity() * sizeof(Velocity) + health_.capacity() * sizeof(Health) +
           tag_.capacity() * sizeof(Tag) + free_list_.capacity() * sizeof(std::uint32_t);
  }

 private:
  static constexpr std::uint32_t kInvalid = ~0u;

  static Entity make_handle(std::uint32_t index, std::uint32_t generation) {
    return Entity{(static_cast<std::uint64_t>(generation) << 32) | index};
  }

  std::uint32_t resolve(Entity e) const {
    const std::uint32_t i = static_cast<std::uint32_t>(e.bits & 0xFFFFFFFFu);
    if (i >= generation_.size()) return kInvalid;
    if (generation_[i] != static_cast<std::uint32_t>(e.bits >> 32)) return kInvalid;
    if (!alive_[i]) return kInvalid;
    return i;
  }

  void store(std::uint32_t i, ComponentId c, const ComponentValue& v) {
    switch (c) {
      case ComponentId::Position: position_[i] = v.position; break;
      case ComponentId::Velocity: velocity_[i] = v.velocity; break;
      case ComponentId::Health: health_[i] = v.health; break;
      case ComponentId::Tag: tag_[i] = v.tag; break;
    }
  }

  void load(std::uint32_t i, ComponentId c, ComponentValue& out) const {
    switch (c) {
      case ComponentId::Position: out.position = position_[i]; break;
      case ComponentId::Velocity: out.velocity = velocity_[i]; break;
      case ComponentId::Health: out.health = health_[i]; break;
      case ComponentId::Tag: out.tag = tag_[i]; break;
    }
  }

  std::vector<std::uint32_t> generation_;
  std::vector<ComponentMask> mask_;
  std::vector<std::uint8_t> alive_;
  std::vector<Position> position_;
  std::vector<Velocity> velocity_;
  std::vector<Health> health_;
  std::vector<Tag> tag_;
  std::vector<std::uint32_t> free_list_;
  std::size_t live_ = 0;
};

}  // namespace gds::candidates
