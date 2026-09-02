// Core value types shared by every candidate in the ECS track.
//
// These types describe *what* a structure must store and answer, never *how*.
// A candidate is free to keep components in any layout, to reorder entities, to
// defer work, or to reconstruct information on demand.
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

namespace gds {

enum class ComponentId : std::uint8_t {
  Position = 0,
  Velocity = 1,
  Health = 2,
  Tag = 3,
};

inline constexpr int kComponentCount = 4;

using ComponentMask = std::uint8_t;

inline constexpr ComponentMask mask_of(ComponentId c) {
  return static_cast<ComponentMask>(1u << static_cast<std::uint8_t>(c));
}

inline constexpr ComponentMask kPosition = 1u << 0;
inline constexpr ComponentMask kVelocity = 1u << 1;
inline constexpr ComponentMask kHealth = 1u << 2;
inline constexpr ComponentMask kTag = 1u << 3;
inline constexpr ComponentMask kAllComponents = kPosition | kVelocity | kHealth | kTag;

struct Position {
  float x, y, z;
};
struct Velocity {
  float x, y, z;
};
struct Health {
  std::int32_t hp, max_hp;
};
struct Tag {
  std::uint32_t bits;
};

// 12 bytes. Only the words belonging to the addressed component are meaningful.
union ComponentValue {
  std::uint32_t raw[3];
  Position position;
  Velocity velocity;
  Health health;
  Tag tag;
};

inline constexpr int component_words(ComponentId c) {
  switch (c) {
    case ComponentId::Position:
    case ComponentId::Velocity:
      return 3;
    case ComponentId::Health:
      return 2;
    case ComponentId::Tag:
      return 1;
  }
  return 3;
}

inline bool value_equal(ComponentId c, const ComponentValue& a, const ComponentValue& b) {
  const int n = component_words(c);
  for (int i = 0; i < n; ++i) {
    if (a.raw[i] != b.raw[i]) return false;
  }
  return true;
}

// Opaque handle. The candidate chooses the bit layout; the harness only stores
// and returns handles. A handle of a destroyed entity must never be reported
// alive again, even if the candidate recycles storage.
struct Entity {
  std::uint64_t bits;
};

inline constexpr Entity kNullEntity{~std::uint64_t(0)};

inline bool operator==(Entity a, Entity b) { return a.bits == b.bits; }

// ---------------------------------------------------------------------------
// Observation digest.
//
// Query results must be independent of iteration order so that candidates are
// free to reorder entities. Every candidate uses these exact functions, so the
// digest is comparable across representations.
// ---------------------------------------------------------------------------

inline std::uint64_t splitmix64(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ull;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
  return x ^ (x >> 31);
}

inline std::uint64_t mix_word(std::uint64_t acc, std::uint32_t w) {
  return splitmix64(acc ^ (static_cast<std::uint64_t>(w) + 0x9E3779B97F4A7C15ull + (acc << 6) +
                           (acc >> 2)));
}

inline std::uint64_t digest_position(const Position& p) {
  std::uint64_t a = 0x1000193ull;
  a = mix_word(a, std::bit_cast<std::uint32_t>(p.x));
  a = mix_word(a, std::bit_cast<std::uint32_t>(p.y));
  a = mix_word(a, std::bit_cast<std::uint32_t>(p.z));
  return a;
}

inline std::uint64_t digest_velocity(const Velocity& v) {
  std::uint64_t a = 0x1000195ull;
  a = mix_word(a, std::bit_cast<std::uint32_t>(v.x));
  a = mix_word(a, std::bit_cast<std::uint32_t>(v.y));
  a = mix_word(a, std::bit_cast<std::uint32_t>(v.z));
  return a;
}

inline std::uint64_t digest_health(const Health& h) {
  std::uint64_t a = 0x1000197ull;
  a = mix_word(a, static_cast<std::uint32_t>(h.hp));
  a = mix_word(a, static_cast<std::uint32_t>(h.max_hp));
  return a;
}

inline std::uint64_t digest_tag(const Tag& t) {
  return mix_word(0x100019Dull, t.bits);
}

inline std::uint64_t digest_component(ComponentId c, const ComponentValue& v) {
  switch (c) {
    case ComponentId::Position:
      return digest_position(v.position);
    case ComponentId::Velocity:
      return digest_velocity(v.velocity);
    case ComponentId::Health:
      return digest_health(v.health);
    case ComponentId::Tag:
      return digest_tag(v.tag);
  }
  return 0;
}

// Digest of one entity for a query over `required`. Components are folded in
// ComponentId order, so the result does not depend on storage order.
inline std::uint64_t digest_entity(ComponentMask required, const ComponentValue* values) {
  std::uint64_t a = 0xCBF29CE484222325ull;
  for (int i = 0; i < kComponentCount; ++i) {
    const ComponentMask bit = static_cast<ComponentMask>(1u << i);
    if (required & bit) {
      a = splitmix64(a ^ digest_component(static_cast<ComponentId>(i), values[i]));
    }
  }
  return a;
}

}  // namespace gds
