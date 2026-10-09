#ifndef CANDIDATE_SPATIAL_AXIS_SORTED_H
#define CANDIDATE_SPATIAL_AXIS_SORTED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gds/spatial/knn.h"
#include "gds/spatial/types.h"
#include "gds/vec.h"

/* One axis only. Entities are kept sorted by x, and a radius query binary
 * searches the slab [c.x - r, c.x + r] and tests everything inside it.
 *
 * There is no cell size to get wrong and no world bounds to configure, and a
 * query is one binary search followed by a single sequential run with no
 * pointer chasing anywhere. The cost is that the slab is thin in one axis and
 * spans the whole world in the other two, so the number of entities examined
 * grows with the world's cross-section rather than with the query volume.
 *
 * Present as the middle of the range: it should be far better than scanning
 * everything and clearly worse than a structure that partitions all three axes,
 * and the size of both gaps is worth having on the record. */
typedef struct {
  Bounds bounds;
  GDS_VEC(Vec3) pos;
  GDS_VEC(uint8_t) live;
  size_t live_count;
  bool dirty;
  GDS_VEC(EntityId) order;
  GDS_VEC(float) sorted_x;
  GDS_VEC(EntityId) sorted_id;
  GDS_VEC(Vec3) sorted_pos;
} axis_sorted;

enum { axis_sorted_native_rewind = 0 };

/* The order the rebuild sorts ids into: by x, then by id. The comparison reads
 * the positions of the structure being rebuilt, which the C++ comparison
 * captured with `this`; a sort.inc.h comparison sees only its two operands, so
 * the positions reach it through this pointer, set just before the sort. */
static const Vec3* axis_sorted_order_pos_;

static inline bool axis_sorted_order_less_(EntityId a, EntityId b) {
  const Vec3* pos = axis_sorted_order_pos_;
  if (pos[a].x != pos[b].x) return pos[a].x < pos[b].x;
  return a < b;
}

/* axis_sorted_order_sort: the ids, ordered by axis_sorted_order_less_. */
#define GDS_SORT_NAME axis_sorted_order
#define GDS_SORT_T EntityId
#define GDS_SORT_LESS(a, b) axis_sorted_order_less_((a), (b))
#include "gds/sort.inc.h"

/* axis_sorted_x_lower_bound and _upper_bound: the binary searches over the
 * sorted x coordinates. */
#define GDS_SORT_NAME axis_sorted_x
#define GDS_SORT_T float
#define GDS_SORT_LESS(a, b) ((a) < (b))
#include "gds/sort.inc.h"

static inline void axis_sorted_init(axis_sorted* s, const WorldConfig* cfg) {
  memset(s, 0, sizeof *s);
  s->bounds = cfg->bounds;
  s->dirty = true;
  const size_t n = (size_t)cfg->max_entity_id + 1;
  const Vec3 zero = {0, 0, 0};
  gds_vec_assign(s->pos, n, zero);
  gds_vec_assign(s->live, n, 0);
}

static inline void axis_sorted_free(axis_sorted* s) {
  gds_vec_free(s->pos);
  gds_vec_free(s->live);
  gds_vec_free(s->order);
  gds_vec_free(s->sorted_x);
  gds_vec_free(s->sorted_id);
  gds_vec_free(s->sorted_pos);
}

static inline void axis_sorted_insert(axis_sorted* s, EntityId id, Vec3 p) {
  if (!s->live.data[id]) ++s->live_count;
  s->live.data[id] = 1;
  s->pos.data[id] = p;
  s->dirty = true;
}

static inline void axis_sorted_remove(axis_sorted* s, EntityId id) {
  if (!s->live.data[id]) return;
  s->live.data[id] = 0;
  --s->live_count;
  s->dirty = true;
}

static inline void axis_sorted_move_by(axis_sorted* s, EntityId id, Vec3 delta) {
  if (!s->live.data[id]) return;
  s->pos.data[id] = gds_wrap_into(gds_add3(s->pos.data[id], delta), &s->bounds);
  s->dirty = true;
}

static inline bool axis_sorted_position_of(axis_sorted* s, EntityId id, Vec3* out) {
  if (id >= s->live.size || !s->live.data[id]) return false;
  *out = s->pos.data[id];
  return true;
}

static inline void axis_sorted_rebuild_if_needed_(axis_sorted* s) {
  if (!s->dirty) return;
  gds_vec_clear(s->order);
  gds_vec_reserve(s->order, s->live_count);
  for (size_t i = 0; i < s->live.size; ++i) {
    if (s->live.data[i]) gds_vec_push(s->order, (EntityId)i);
  }
  axis_sorted_order_pos_ = s->pos.data;
  axis_sorted_order_sort(s->order.data, s->order.size);
  gds_vec_resize(s->sorted_x, s->order.size);
  gds_vec_resize(s->sorted_id, s->order.size);
  gds_vec_resize(s->sorted_pos, s->order.size);
  for (size_t i = 0; i < s->order.size; ++i) {
    s->sorted_id.data[i] = s->order.data[i];
    s->sorted_pos.data[i] = s->pos.data[s->order.data[i]];
    s->sorted_x.data[i] = s->sorted_pos.data[i].x;
  }
  s->dirty = false;
}

/* The index range [lo, hi) of the sorted entries whose x lies in the slab
 * [cx - r, cx + r]. A caller walks it and does the exact test itself. */
typedef struct {
  size_t lo, hi;
} axis_sorted_slab_;

static inline axis_sorted_slab_ axis_sorted_slab_of_(const axis_sorted* s, float cx, float r) {
  axis_sorted_slab_ b;
  b.lo = axis_sorted_x_lower_bound(s->sorted_x.data, s->sorted_x.size, cx - r);
  b.hi = axis_sorted_x_upper_bound(s->sorted_x.data, s->sorted_x.size, cx + r);
  return b;
}

static inline uint64_t axis_sorted_query_radius(axis_sorted* s, Vec3 c, float r) {
  axis_sorted_rebuild_if_needed_(s);
  RadiusDigest d = gds_radius_digest(c, r);
  const float r2 = r * r;
  const axis_sorted_slab_ b = axis_sorted_slab_of_(s, c.x, r);
  const EntityId* sorted_id = s->sorted_id.data;
  const Vec3* sorted_pos = s->sorted_pos.data;
  for (size_t i = b.lo; i != b.hi; ++i) {
    if (gds_dist2(sorted_pos[i], c) <= r2) gds_radius_hit(&d, sorted_id[i], sorted_pos[i]);
  }
  return d.acc;
}

static inline uint64_t axis_sorted_query_radius_of(axis_sorted* s, EntityId self, float r) {
  if (self >= s->live.size || !s->live.data[self]) return 0;
  const Vec3 c = s->pos.data[self];
  const RadiusDigest own = gds_radius_digest(c, r);
  return axis_sorted_query_radius(s, c, r) - gds_radius_term(&own, self, c);
}

static inline uint64_t axis_sorted_query_knn(axis_sorted* s, Vec3 c, uint32_t k) {
  axis_sorted_rebuild_if_needed_(s);
  KnnDigest d = gds_knn_digest();
  if (k == 0 || s->live_count == 0) return d.acc;
  const float limit = gds_bounds_largest_extent(&s->bounds) * 2.0f;
  const float first_r = gds_bounds_largest_extent(&s->bounds) / 256.0f;
  const EntityId* sorted_id = s->sorted_id.data;
  const Vec3* sorted_pos = s->sorted_pos.data;
  GDS_VEC(Neighbour) found;
  gds_vec_init(found);
  for (float r = 1e-3f < first_r ? first_r : 1e-3f;; r *= 2.0f) {
    gds_vec_clear(found);
    const axis_sorted_slab_ b = axis_sorted_slab_of_(s, c.x, r);
    for (size_t i = b.lo; i != b.hi; ++i) {
      const Neighbour nb = {gds_dist2(sorted_pos[i], c), sorted_id[i]};
      gds_vec_push(found, nb);
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

static inline void axis_sorted_end_tick(axis_sorted* s, uint64_t tick) {
  (void)s;
  (void)tick;
}

static inline bool axis_sorted_rewind_to(axis_sorted* s, uint64_t tick) {
  (void)s;
  (void)tick;
  return false;
}

static inline size_t axis_sorted_entity_count(axis_sorted* s) { return s->live_count; }

static inline size_t axis_sorted_reported_bytes(axis_sorted* s) {
  return gds_vec_bytes(s->pos) + gds_vec_bytes(s->live) + gds_vec_bytes(s->sorted_x) +
         gds_vec_bytes(s->sorted_id) + gds_vec_bytes(s->sorted_pos);
}

#endif
