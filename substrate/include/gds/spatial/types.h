/* Shared geometry and observation digests for the spatial track.
 *
 * Every candidate uses these exact functions for the narrow phase and for the
 * digest. A structure may cull as loosely as it likes — a broad phase is
 * allowed to admit points that turn out not to match — but the final accept
 * test and the digest must come from here, or two structures would be
 * answering slightly different questions and their timings would not be
 * comparable. */
#ifndef GDS_SPATIAL_TYPES_H
#define GDS_SPATIAL_TYPES_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gds/common.h"

typedef uint32_t EntityId;
#define GDS_NO_ENTITY ((EntityId)~(EntityId)0)

typedef struct {
  float x, y, z;
} Vec3;

typedef struct {
  Vec3 min, max;
} Bounds;

static inline Vec3 gds_vec3(float x, float y, float z) {
  Vec3 v = {x, y, z};
  return v;
}

static inline float gds_bounds_extent_x(const Bounds* b) { return b->max.x - b->min.x; }
static inline float gds_bounds_extent_y(const Bounds* b) { return b->max.y - b->min.y; }
static inline float gds_bounds_extent_z(const Bounds* b) { return b->max.z - b->min.z; }
static inline float gds_bounds_largest_extent(const Bounds* b) {
  float e = gds_bounds_extent_x(b);
  if (gds_bounds_extent_y(b) > e) e = gds_bounds_extent_y(b);
  if (gds_bounds_extent_z(b) > e) e = gds_bounds_extent_z(b);
  return e;
}

/* What any engine would hand a spatial index at construction. A structure is
 * free to ignore all of it. */
typedef struct {
  Bounds bounds;
  uint32_t expected_entities;
  uint32_t max_entity_id;     /* ids are dense and no greater than this */
  float typical_query_radius;
  uint32_t history_ticks;     /* deepest rewind the workload will ask for */
} WorldConfig;

static inline float gds_dist2(Vec3 a, Vec3 b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return dx * dx + dy * dy + dz * dz;
}

/* Toroidal wrap. move_by is defined as wrap(current + delta), and this is the
 * only wrap in the system, so every structure lands on bit-identical
 * positions. */
static inline float gds_wrap_axis(float v, float lo, float hi) {
  const float span = hi - lo;
  if (!(span > 0.0f)) return lo;
  float t = v - lo;
  t = t - span * floorf(t / span);
  if (!(t >= 0.0f)) t = 0.0f;
  if (t >= span) t = 0.0f;
  return lo + t;
}

static inline Vec3 gds_wrap_into(Vec3 p, const Bounds* b) {
  Vec3 r;
  r.x = gds_wrap_axis(p.x, b->min.x, b->max.x);
  r.y = gds_wrap_axis(p.y, b->min.y, b->max.y);
  r.z = gds_wrap_axis(p.z, b->min.z, b->max.z);
  return r;
}

static inline Vec3 gds_add3(Vec3 a, Vec3 b) {
  Vec3 r = {a.x + b.x, a.y + b.y, a.z + b.z};
  return r;
}

static inline bool gds_vec3_bits_equal(Vec3 a, Vec3 b) {
  return gds_float_bits(a.x) == gds_float_bits(b.x) && gds_float_bits(a.y) == gds_float_bits(b.y) &&
         gds_float_bits(a.z) == gds_float_bits(b.z);
}

/* ------------------------------------------------------------------------
 * Observation digests
 * ------------------------------------------------------------------------ */

static inline uint64_t gds_digest_hit(EntityId id, Vec3 p, uint64_t salt) {
  uint64_t a = (0x9E3779B97F4A7C15ull ^ salt) ^ (uint64_t)id;
  a = gds_mix_word(a, gds_float_bits(p.x));
  a = gds_mix_word(a, gds_float_bits(p.y));
  a = gds_mix_word(a, gds_float_bits(p.z));
  return gds_splitmix64(a);
}

/* The salt of a radius query, taken from the query itself. A radius answer is
 * a sum, and an unsalted sum is a quantity a structure could keep per cell as
 * a running total and hand back for every cell a query swallows whole, without
 * visiting anything inside it — candidates/ecs/query_memo showed the same hole
 * in the ECS track at 2.6x. Salting each hit by the query that found it makes
 * a precomputed total worthless, because no two queries share a salt. */
static inline uint64_t gds_radius_salt(Vec3 c, float r) {
  uint64_t a = 0x7AD1u;
  a = gds_mix_word(a, gds_float_bits(c.x));
  a = gds_mix_word(a, gds_float_bits(c.y));
  a = gds_mix_word(a, gds_float_bits(c.z));
  a = gds_mix_word(a, gds_float_bits(r));
  return gds_splitmix64(a);
}

/* A radius query is a set, so its digest is a sum and the iteration order of
 * the structure is unconstrained. It is constructed from the query it answers.
 * query_radius_of(id, r) is the query centred on the entity's position, so it
 * uses gds_radius_digest(position, r) and excludes the entity's own term. */
typedef struct {
  uint64_t salt;
  uint64_t acc;
} RadiusDigest;

static inline RadiusDigest gds_radius_digest(Vec3 c, float r) {
  RadiusDigest d = {gds_radius_salt(c, r), 0};
  return d;
}
static inline void gds_radius_hit(RadiusDigest* d, EntityId id, Vec3 p) {
  d->acc += gds_digest_hit(id, p, d->salt);
}
static inline uint64_t gds_radius_term(const RadiusDigest* d, EntityId id, Vec3 p) {
  return gds_digest_hit(id, p, d->salt);
}

/* A k-nearest query is a sequence, so its digest is an ordered fold. Ties are
 * broken by ascending id, which makes the sequence unique. It needs no salt:
 * an ordered fold over a query-specific sequence has no algebra to
 * precompute. */
typedef struct {
  uint64_t acc;
} KnnDigest;

static inline KnnDigest gds_knn_digest(void) {
  KnnDigest d = {0xCBF29CE484222325ull};
  return d;
}
static inline void gds_knn_push(KnnDigest* d, EntityId id, Vec3 p) {
  d->acc = gds_splitmix64(d->acc * 31u ^ gds_digest_hit(id, p, 0));
}

/* The ordering a k-nearest result must be in before it is folded. */
typedef struct {
  float d2;
  EntityId id;
} Neighbour;

static inline bool gds_nearer(Neighbour a, Neighbour b) {
  if (a.d2 != b.d2) return a.d2 < b.d2;
  return a.id < b.id;
}

#endif
