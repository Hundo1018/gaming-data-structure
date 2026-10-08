// Shared geometry and observation digests for the spatial track.
//
// Every candidate uses these exact functions for the narrow phase and for the
// digest. A structure may cull as loosely as it likes — a broad phase is
// allowed to admit points that turn out not to match — but the final accept
// test and the digest must come from here, or two structures would be
// answering slightly different questions and their timings would not be
// comparable.
#pragma once

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "gds/types.hpp"  // splitmix64, mix_word

namespace gds::spatial {

using EntityId = std::uint32_t;
inline constexpr EntityId kNoEntity = ~EntityId(0);

struct Vec3 {
  float x, y, z;
};

struct Bounds {
  Vec3 min, max;

  float extent_x() const { return max.x - min.x; }
  float extent_y() const { return max.y - min.y; }
  float extent_z() const { return max.z - min.z; }
  float largest_extent() const {
    float e = extent_x();
    if (extent_y() > e) e = extent_y();
    if (extent_z() > e) e = extent_z();
    return e;
  }
};

// What any engine would hand a spatial index at construction. A structure is
// free to ignore all of it.
struct WorldConfig {
  Bounds bounds{};
  std::uint32_t expected_entities = 0;
  std::uint32_t max_entity_id = 0;      // ids are dense and below this
  float typical_query_radius = 1.0f;
  std::uint32_t history_ticks = 0;      // deepest rewind the workload will ask for
};

inline float dist2(Vec3 a, Vec3 b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return dx * dx + dy * dy + dz * dz;
}

// Toroidal wrap. `move_by` is defined as wrap(current + delta), and this is the
// only wrap in the system, so every structure lands on bit-identical positions.
inline float wrap_axis(float v, float lo, float hi) {
  const float span = hi - lo;
  if (!(span > 0.0f)) return lo;
  float t = v - lo;
  t = t - span * std::floor(t / span);
  if (!(t >= 0.0f)) t = 0.0f;
  if (t >= span) t = 0.0f;
  return lo + t;
}

inline Vec3 wrap_into(Vec3 p, const Bounds& b) {
  return Vec3{wrap_axis(p.x, b.min.x, b.max.x), wrap_axis(p.y, b.min.y, b.max.y),
              wrap_axis(p.z, b.min.z, b.max.z)};
}

inline Vec3 add(Vec3 a, Vec3 b) { return Vec3{a.x + b.x, a.y + b.y, a.z + b.z}; }

// ---------------------------------------------------------------------------
// Observation digests
// ---------------------------------------------------------------------------

inline std::uint64_t digest_hit(EntityId id, Vec3 p, std::uint64_t salt) {
  std::uint64_t a = (0x9E3779B97F4A7C15ull ^ salt) ^ static_cast<std::uint64_t>(id);
  a = mix_word(a, std::bit_cast<std::uint32_t>(p.x));
  a = mix_word(a, std::bit_cast<std::uint32_t>(p.y));
  a = mix_word(a, std::bit_cast<std::uint32_t>(p.z));
  return splitmix64(a);
}

// The salt of a radius query, taken from the query itself. A radius answer is a
// sum, and an unsalted sum is a quantity a structure could keep per cell as a
// running total and hand back for every cell a query swallows whole, without
// visiting anything inside it — `candidates/ecs/query_memo` showed the same
// hole in the ECS track at 2.6x. Salting each hit by the query that found it
// makes a precomputed total worthless, because no two queries share a salt.
inline std::uint64_t radius_salt(Vec3 c, float r) {
  std::uint64_t a = 0x7AD1u;
  a = mix_word(a, std::bit_cast<std::uint32_t>(c.x));
  a = mix_word(a, std::bit_cast<std::uint32_t>(c.y));
  a = mix_word(a, std::bit_cast<std::uint32_t>(c.z));
  a = mix_word(a, std::bit_cast<std::uint32_t>(r));
  return splitmix64(a);
}

// A radius query is a set, so its digest is a sum and the iteration order of
// the structure is unconstrained. It is constructed from the query it answers.
// query_radius_of(id, r) is the query centred on the entity's position, so it
// uses RadiusDigest(position, r) and excludes the entity's own term.
struct RadiusDigest {
  std::uint64_t salt;
  std::uint64_t acc = 0;
  RadiusDigest(Vec3 c, float r) : salt(radius_salt(c, r)) {}
  void hit(EntityId id, Vec3 p) { acc += digest_hit(id, p, salt); }
  std::uint64_t term(EntityId id, Vec3 p) const { return digest_hit(id, p, salt); }
  std::uint64_t value() const { return acc; }
};

// A k-nearest query is a sequence, so its digest is an ordered fold. Ties are
// broken by ascending id, which makes the sequence unique. It needs no salt: an
// ordered fold over a query-specific sequence has no algebra to precompute.
struct KnnDigest {
  std::uint64_t acc = 0xCBF29CE484222325ull;
  void push(EntityId id, Vec3 p) { acc = splitmix64(acc * 31u ^ digest_hit(id, p, 0)); }
  std::uint64_t value() const { return acc; }
};

// The ordering a k-nearest result must be in before it is folded.
struct Neighbour {
  float d2;
  EntityId id;
};

inline bool nearer(const Neighbour& a, const Neighbour& b) {
  if (a.d2 != b.d2) return a.d2 < b.d2;
  return a.id < b.id;
}

}  // namespace gds::spatial
