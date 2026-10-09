#ifndef CANDIDATE_SPATIAL_MORTON_SORTED_H
#define CANDIDATE_SPATIAL_MORTON_SORTED_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gds/spatial/knn.h"
#include "gds/spatial/types.h"
#include "gds/vec.h"

typedef struct {
  uint64_t code;
  EntityId id;
} morton_sorted_entry_;

/* No incremental update at all. Positions are written into a flat array as they
 * change; the searchable form is thrown away and rebuilt from scratch the next
 * time anything is asked.
 *
 * The rebuilt form sorts entities by the Morton code of their cell, so entities
 * that are near each other in the world are near each other in memory, and the
 * scan for a cell is a contiguous run rather than a pointer chase. The bet is
 * that a query pays so much less that it covers rebuilding the whole array once
 * per tick. */
typedef struct {
  Bounds bounds;
  float cell;
  float inv_cell;
  GDS_VEC(Vec3) pos;
  GDS_VEC(uint8_t) live;
  size_t live_count;
  bool dirty;
  GDS_VEC(morton_sorted_entry_) sorted;
  GDS_VEC(uint64_t) sorted_code;
  GDS_VEC(EntityId) sorted_id;
  GDS_VEC(Vec3) sorted_pos;
} morton_sorted;

enum { morton_sorted_native_rewind = 0 };

static inline bool morton_sorted_entry_less_(morton_sorted_entry_ a, morton_sorted_entry_ b) {
  if (a.code != b.code) return a.code < b.code;
  return a.id < b.id;
}

/* morton_sorted_entry_sort: the entries, by code and then by id. */
#define GDS_SORT_NAME morton_sorted_entry
#define GDS_SORT_T morton_sorted_entry_
#define GDS_SORT_LESS(a, b) morton_sorted_entry_less_((a), (b))
#include "gds/sort.inc.h"

/* morton_sorted_code_lower_bound: the first entry of a cell in the sorted
 * codes. */
#define GDS_SORT_NAME morton_sorted_code
#define GDS_SORT_T uint64_t
#define GDS_SORT_LESS(a, b) ((a) < (b))
#include "gds/sort.inc.h"

static inline uint64_t morton_sorted_spread_(uint64_t x) {
  x &= 0x1FFFFFull;
  x = (x | (x << 32)) & 0x1F00000000FFFFull;
  x = (x | (x << 16)) & 0x1F0000FF0000FFull;
  x = (x | (x << 8)) & 0x100F00F00F00F00Full;
  x = (x | (x << 4)) & 0x10C30C30C30C30C3ull;
  x = (x | (x << 2)) & 0x1249249249249249ull;
  return x;
}

static inline uint64_t morton_sorted_morton_(uint32_t x, uint32_t y, uint32_t z) {
  return morton_sorted_spread_(x) | (morton_sorted_spread_(y) << 1) |
         (morton_sorted_spread_(z) << 2);
}

static inline uint32_t morton_sorted_axis_cell_(const morton_sorted* s, float v, float lo) {
  const long long i = (long long)floorf((v - lo) * s->inv_cell);
  if (i < 0) return 0;
  if (i > 0x1FFFFF) return 0x1FFFFF;
  return (uint32_t)i;
}

static inline uint64_t morton_sorted_code_of_(const morton_sorted* s, Vec3 p) {
  return morton_sorted_morton_(morton_sorted_axis_cell_(s, p.x, s->bounds.min.x),
                               morton_sorted_axis_cell_(s, p.y, s->bounds.min.y),
                               morton_sorted_axis_cell_(s, p.z, s->bounds.min.z));
}

static inline void morton_sorted_init(morton_sorted* s, const WorldConfig* cfg) {
  memset(s, 0, sizeof *s);
  s->bounds = cfg->bounds;
  s->dirty = true;
  float floor_cell = gds_bounds_largest_extent(&s->bounds) / 256.0f;
  if (floor_cell < 1e-4f) floor_cell = 1e-4f;
  s->cell = cfg->typical_query_radius < floor_cell ? floor_cell : cfg->typical_query_radius;
  s->inv_cell = 1.0f / s->cell;
  const size_t n = (size_t)cfg->max_entity_id + 1;
  const Vec3 zero = {0, 0, 0};
  gds_vec_assign(s->pos, n, zero);
  gds_vec_assign(s->live, n, 0);
}

static inline void morton_sorted_free(morton_sorted* s) {
  gds_vec_free(s->pos);
  gds_vec_free(s->live);
  gds_vec_free(s->sorted);
  gds_vec_free(s->sorted_code);
  gds_vec_free(s->sorted_id);
  gds_vec_free(s->sorted_pos);
}

static inline void morton_sorted_insert(morton_sorted* s, EntityId id, Vec3 p) {
  if (!s->live.data[id]) ++s->live_count;
  s->live.data[id] = 1;
  s->pos.data[id] = p;
  s->dirty = true;
}

static inline void morton_sorted_remove(morton_sorted* s, EntityId id) {
  if (!s->live.data[id]) return;
  s->live.data[id] = 0;
  --s->live_count;
  s->dirty = true;
}

static inline void morton_sorted_move_by(morton_sorted* s, EntityId id, Vec3 delta) {
  if (!s->live.data[id]) return;
  s->pos.data[id] = gds_wrap_into(gds_add3(s->pos.data[id], delta), &s->bounds);
  s->dirty = true;
}

static inline bool morton_sorted_position_of(morton_sorted* s, EntityId id, Vec3* out) {
  if (id >= s->live.size || !s->live.data[id]) return false;
  *out = s->pos.data[id];
  return true;
}

static inline void morton_sorted_rebuild_if_needed_(morton_sorted* s) {
  if (!s->dirty) return;
  gds_vec_clear(s->sorted);
  gds_vec_reserve(s->sorted, s->live_count);
  for (size_t i = 0; i < s->live.size; ++i) {
    if (s->live.data[i]) {
      const morton_sorted_entry_ e = {morton_sorted_code_of_(s, s->pos.data[i]), (EntityId)i};
      gds_vec_push(s->sorted, e);
    }
  }
  morton_sorted_entry_sort(s->sorted.data, s->sorted.size);
  gds_vec_resize(s->sorted_code, s->sorted.size);
  gds_vec_resize(s->sorted_id, s->sorted.size);
  gds_vec_resize(s->sorted_pos, s->sorted.size);
  for (size_t i = 0; i < s->sorted.size; ++i) {
    s->sorted_code.data[i] = s->sorted.data[i].code;
    s->sorted_id.data[i] = s->sorted.data[i].id;
    s->sorted_pos.data[i] = s->pos.data[s->sorted.data[i].id];
  }
  s->dirty = false;
}

/* The cell range of the axis-aligned box of half-width r around c.
 *
 * One binary search per cell in the query box. The cells of a box are not
 * contiguous in Morton order, so this is the cost of the ordering: locality
 * inside a cell, a search to find each one. */
typedef struct {
  uint32_t x0, x1, y0, y1, z0, z1;
} morton_sorted_box_;

static inline morton_sorted_box_ morton_sorted_box_of_(const morton_sorted* s, Vec3 c, float r) {
  morton_sorted_box_ b;
  b.x0 = morton_sorted_axis_cell_(s, c.x - r, s->bounds.min.x);
  b.x1 = morton_sorted_axis_cell_(s, c.x + r, s->bounds.min.x);
  b.y0 = morton_sorted_axis_cell_(s, c.y - r, s->bounds.min.y);
  b.y1 = morton_sorted_axis_cell_(s, c.y + r, s->bounds.min.y);
  b.z0 = morton_sorted_axis_cell_(s, c.z - r, s->bounds.min.z);
  b.z1 = morton_sorted_axis_cell_(s, c.z + r, s->bounds.min.z);
  return b;
}

static inline uint64_t morton_sorted_query_radius(morton_sorted* s, Vec3 c, float r) {
  morton_sorted_rebuild_if_needed_(s);
  RadiusDigest d = gds_radius_digest(c, r);
  const float r2 = r * r;
  const morton_sorted_box_ b = morton_sorted_box_of_(s, c, r);
  const uint64_t* sorted_code = s->sorted_code.data;
  const size_t n = s->sorted_code.size;
  const EntityId* sorted_id = s->sorted_id.data;
  const Vec3* sorted_pos = s->sorted_pos.data;
  for (uint32_t z = b.z0; z <= b.z1; ++z) {
    for (uint32_t y = b.y0; y <= b.y1; ++y) {
      for (uint32_t x = b.x0; x <= b.x1; ++x) {
        const uint64_t code = morton_sorted_morton_(x, y, z);
        const size_t begin = morton_sorted_code_lower_bound(sorted_code, n, code);
        for (size_t slot = begin; slot != n && sorted_code[slot] == code; ++slot) {
          if (gds_dist2(sorted_pos[slot], c) <= r2)
            gds_radius_hit(&d, sorted_id[slot], sorted_pos[slot]);
        }
      }
    }
  }
  return d.acc;
}

static inline uint64_t morton_sorted_query_radius_of(morton_sorted* s, EntityId self, float r) {
  if (self >= s->live.size || !s->live.data[self]) return 0;
  const Vec3 c = s->pos.data[self];
  const RadiusDigest own = gds_radius_digest(c, r);
  return morton_sorted_query_radius(s, c, r) - gds_radius_term(&own, self, c);
}

static inline uint64_t morton_sorted_query_knn(morton_sorted* s, Vec3 c, uint32_t k) {
  morton_sorted_rebuild_if_needed_(s);
  KnnDigest d = gds_knn_digest();
  if (k == 0 || s->live_count == 0) return d.acc;
  const float limit = gds_bounds_largest_extent(&s->bounds) * 2.0f;
  const uint64_t* sorted_code = s->sorted_code.data;
  const size_t count = s->sorted_code.size;
  const EntityId* sorted_id = s->sorted_id.data;
  const Vec3* sorted_pos = s->sorted_pos.data;
  GDS_VEC(Neighbour) found;
  gds_vec_init(found);
  for (float r = s->cell;; r *= 2.0f) {
    gds_vec_clear(found);
    const morton_sorted_box_ b = morton_sorted_box_of_(s, c, r);
    for (uint32_t z = b.z0; z <= b.z1; ++z) {
      for (uint32_t y = b.y0; y <= b.y1; ++y) {
        for (uint32_t x = b.x0; x <= b.x1; ++x) {
          const uint64_t code = morton_sorted_morton_(x, y, z);
          const size_t begin = morton_sorted_code_lower_bound(sorted_code, count, code);
          for (size_t slot = begin; slot != count && sorted_code[slot] == code; ++slot) {
            const Neighbour nb = {gds_dist2(sorted_pos[slot], c), sorted_id[slot]};
            gds_vec_push(found, nb);
          }
        }
      }
    }
    const size_t n = found.size;
    const size_t want = k < n ? k : n;
    if (want > 0) gds_neighbour_partial_sort(found.data, want, n);
    if (n >= k || r > limit) {
      if (want == 0 || found.data[want - 1].d2 <= r * r || r > limit) {
        for (size_t i = 0; i < want; ++i)
          gds_knn_push(&d, found.data[i].id, s->pos.data[found.data[i].id]);
        gds_vec_free(found);
        return d.acc;
      }
    }
  }
}

static inline void morton_sorted_end_tick(morton_sorted* s, uint64_t tick) {
  (void)s;
  (void)tick;
}

static inline bool morton_sorted_rewind_to(morton_sorted* s, uint64_t tick) {
  (void)s;
  (void)tick;
  return false;
}

static inline size_t morton_sorted_entity_count(morton_sorted* s) { return s->live_count; }

static inline size_t morton_sorted_reported_bytes(morton_sorted* s) {
  return gds_vec_bytes(s->pos) + gds_vec_bytes(s->live) + gds_vec_bytes(s->sorted) +
         gds_vec_bytes(s->sorted_id) + gds_vec_bytes(s->sorted_pos) +
         gds_vec_bytes(s->sorted_code);
}

#endif
