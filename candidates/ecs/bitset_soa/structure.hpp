#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "gds/types.hpp"

namespace gds::candidates {

// soa with membership held a second time as bit planes.
//
// soa's query and integrate test every slot's liveness and mask byte, so a
// query over a component few entities hold still visits every slot. Here one
// plane per component and one for liveness hold a bit per slot, packed into
// 64-bit words. Iteration ANDs one word from each plane it needs, skips a zero
// word, and visits the set bits, so its work is O(slots/64 + matches) rather
// than O(slots). Point access never reads the planes.
//
// soa's code is reproduced here rather than included by path. Every member of
// Soa is private, and query and integrate need its column arrays by slot index,
// which its public interface does not expose: a wrapper would have to keep a
// handle per slot and resolve it on every match. The index space, the handle
// layout, the free list, the seven parallel arrays and get, set, mask and
// alive are soa's, unchanged; what differs is where the planes are written or
// read.
class BitsetSoa {
 public:
  static const char* name() { return "bitset_soa"; }

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
      // The first slot of each run of 64 brings a zero word to every plane, so
      // the bits past the last slot are always zero and a scan can stop at the
      // last word without knowing the slot count.
      if ((index & 63u) == 0) {
        live_bits_.push_back(0);
        for (auto& plane : comp_bits_) plane.push_back(0);
      }
    }
    mask_[index] = mask;
    alive_[index] = 1;
    // All five bits are written, set or cleared, so a recycled slot's bits do
    // not depend on what its previous occupant left.
    put_bit(live_bits_, index, true);
    for (int c = 0; c < kComponentCount; ++c) put_bit(comp_bits_[c], index, (mask >> c) & 1u);
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
    put_bit(live_bits_, i, false);
    for (auto& plane : comp_bits_) put_bit(plane, i, false);
    ++generation_[i];
    free_list_.push_back(i);
    --live_;
  }

  bool alive(Entity e) const { return resolve(e) != kInvalid; }

  void add(Entity e, ComponentId c, const ComponentValue& v) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return;
    mask_[i] |= mask_of(c);
    put_bit(comp_bits_[static_cast<int>(c)], i, true);
    store(i, c, v);
  }

  void remove(Entity e, ComponentId c) {
    const std::uint32_t i = resolve(e);
    if (i == kInvalid) return;
    mask_[i] &= static_cast<ComponentMask>(~mask_of(c));
    put_bit(comp_bits_[static_cast<int>(c)], i, false);
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

  // The liveness word is ANDed in even though destroy clears a dead slot's
  // component bits, which already keeps it out of any non-empty mask: for the
  // empty mask, which matches every live entity, it is the only plane. Once a
  // slot is found, the gather and the digest are soa's.
  std::uint64_t query(ComponentMask required, std::uint64_t salt) const {
    // Bits 4 to 7 of a mask name no component and have no plane, but soa and
    // the oracle keep them in an entity's mask and match on them. A query that
    // names one takes soa's per-slot test, so the answer is soa's for every
    // mask. No workload can build such a mask; the planes serve every one that
    // a workload can.
    if (required & static_cast<ComponentMask>(~kAllComponents)) {
      return query_by_slot(required, salt);
    }
    const std::uint64_t* planes[kComponentCount];
    int np = 0;
    for (int c = 0; c < kComponentCount; ++c) {
      if (required & (1u << c)) planes[np++] = comp_bits_[c].data();
    }
    const std::uint64_t* live = live_bits_.data();
    const std::size_t words = live_bits_.size();
    std::uint64_t acc = 0;
    ComponentValue v[kComponentCount];
    for (std::size_t w = 0; w < words; ++w) {
      std::uint64_t bits = live[w];
      for (int k = 0; k < np; ++k) bits &= planes[k][w];
      while (bits != 0) {
        const std::size_t i = (w << 6) | static_cast<std::size_t>(std::countr_zero(bits));
        bits &= bits - 1;
        if (required & kPosition) v[0].position = position_[i];
        if (required & kVelocity) v[1].velocity = velocity_[i];
        if (required & kHealth) v[2].health = health_[i];
        if (required & kTag) v[3].tag = tag_[i];
        acc += digest_entity(required, v, salt);
      }
    }
    return acc;
  }

  // The same walk over liveness, Position and Velocity; the update is soa's.
  void integrate(float dt) {
    const std::uint64_t* live = live_bits_.data();
    const std::uint64_t* pos = comp_bits_[0].data();
    const std::uint64_t* vel = comp_bits_[1].data();
    const std::size_t words = live_bits_.size();
    for (std::size_t w = 0; w < words; ++w) {
      std::uint64_t bits = live[w] & pos[w] & vel[w];
      while (bits != 0) {
        const std::size_t i = (w << 6) | static_cast<std::size_t>(std::countr_zero(bits));
        bits &= bits - 1;
        Position& p = position_[i];
        const Velocity& v = velocity_[i];
        p.x += v.x * dt;
        p.y += v.y * dt;
        p.z += v.z * dt;
      }
    }
  }

  void sync() {}

  std::size_t entity_count() const { return live_; }

  std::size_t reported_bytes() const {
    std::size_t plane_words = live_bits_.capacity();
    for (const auto& plane : comp_bits_) plane_words += plane.capacity();
    return generation_.capacity() * sizeof(std::uint32_t) + mask_.capacity() +
           alive_.capacity() + position_.capacity() * sizeof(Position) +
           velocity_.capacity() * sizeof(Velocity) + health_.capacity() * sizeof(Health) +
           tag_.capacity() * sizeof(Tag) + free_list_.capacity() * sizeof(std::uint32_t) +
           plane_words * sizeof(std::uint64_t);
  }

 private:
  static constexpr std::uint32_t kInvalid = ~0u;

  static Entity make_handle(std::uint32_t index, std::uint32_t generation) {
    return Entity{(static_cast<std::uint64_t>(generation) << 32) | index};
  }

  // soa's query, unchanged, for masks the planes do not cover.
  std::uint64_t query_by_slot(ComponentMask required, std::uint64_t salt) const {
    std::uint64_t acc = 0;
    const std::size_t n = mask_.size();
    ComponentValue v[kComponentCount];
    for (std::size_t i = 0; i < n; ++i) {
      if (!alive_[i] || (mask_[i] & required) != required) continue;
      if (required & kPosition) v[0].position = position_[i];
      if (required & kVelocity) v[1].velocity = velocity_[i];
      if (required & kHealth) v[2].health = health_[i];
      if (required & kTag) v[3].tag = tag_[i];
      acc += digest_entity(required, v, salt);
    }
    return acc;
  }

  static void put_bit(std::vector<std::uint64_t>& plane, std::uint32_t i, bool on) {
    const std::uint64_t bit = std::uint64_t(1) << (i & 63u);
    std::uint64_t& word = plane[i >> 6];
    word = on ? (word | bit) : (word & ~bit);
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

  // Bit (i % 64) of word (i / 64) of a plane describes slot i. comp_bits_ is
  // indexed by ComponentId, so plane c mirrors bit c of mask_ for the four
  // component bits; bits 4 to 7 of mask_ have no plane, which query handles.
  std::vector<std::uint64_t> live_bits_;
  std::vector<std::uint64_t> comp_bits_[kComponentCount];
};

}  // namespace gds::candidates
