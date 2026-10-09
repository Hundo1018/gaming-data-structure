#ifndef CANDIDATE_SPATIAL_CELL_ROWS_H
#define CANDIDATE_SPATIAL_CELL_ROWS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gds/spatial/knn.h"
#include "gds/spatial/types.h"
#include "gds/vec.h"
#include "spatial/cell_sorted/structure.h"

/* cell_sorted with one thing changed: how a query reads the box.
 *
 * The cell index runs x fastest, so the cells of one row of the box are
 * consecutive in the cell order, and in a counting-sorted array their ranges
 * are adjacent. The whole row is then one run, [start[row + x0],
 * start[row + x1 + 1]), found with two directory reads however many cells it
 * spans, and scanned as one sequence of vector blocks. cell_sorted reads each
 * cell on its own, as uniform_grid reads its list heads, so that its
 * comparison with the grid is only about the key and the directory; this pair
 * isolates the run length. A linked grid cannot make this change: its cells are
 * not stored next to each other.
 *
 * Everything else, the rebuild, the arrays and the k-nearest search strategy,
 * is cell_sorted's code, included by path. */
typedef struct {
  cell_sorted base;
} cell_rows;

enum { cell_rows_native_rewind = cell_sorted_native_rewind };

static inline void cell_rows_init(cell_rows* s, const WorldConfig* cfg) {
  cell_sorted_init(&s->base, cfg);
}

static inline void cell_rows_free(cell_rows* s) { cell_sorted_free(&s->base); }

static inline void cell_rows_insert(cell_rows* s, EntityId id, Vec3 p) {
  cell_sorted_insert(&s->base, id, p);
}

static inline void cell_rows_remove(cell_rows* s, EntityId id) {
  cell_sorted_remove(&s->base, id);
}

static inline void cell_rows_move_by(cell_rows* s, EntityId id, Vec3 delta) {
  cell_sorted_move_by(&s->base, id, delta);
}

static inline bool cell_rows_position_of(cell_rows* s, EntityId id, Vec3* out) {
  return cell_sorted_position_of(&s->base, id, out);
}

/* The same box as cell_sorted's walk, every row of it as one run,
 * [start[row + x0], start[row + x1 + 1]). This query and the k-nearest search
 * below each walk it inline. */
static inline uint64_t cell_rows_query_radius(cell_rows* s, Vec3 c, float r) {
  cell_sorted* g = &s->base;
  cell_sorted_rebuild_if_needed_(g);
  RadiusDigest d = gds_radius_digest(c, r);
  const float r2 = r * r;
  const cell_sorted_box_ bx = cell_sorted_box_of_(g, c, r);
  const uint32_t* start = g->start.data;
  for (uint32_t z = bx.z0; z <= bx.z1; ++z) {
    for (uint32_t y = bx.y0; y <= bx.y1; ++y) {
      const size_t row = ((size_t)z * g->ny + y) * g->nx;
      const size_t b = start[row + bx.x0];
      const size_t e = start[row + bx.x1 + 1];
      if (b < e) cell_sorted_scan_run_(g, b, e, c, r2, &d);
    }
  }
  return d.acc;
}

static inline uint64_t cell_rows_query_radius_of(cell_rows* s, EntityId self, float r) {
  cell_sorted* g = &s->base;
  if (self >= g->live.size || !g->live.data[self]) return 0;
  const Vec3 c = g->pos.data[self];
  const RadiusDigest own = gds_radius_digest(c, r);
  return cell_rows_query_radius(s, c, r) - gds_radius_term(&own, self, c);
}

/* cell_sorted's search, gathering from row runs instead of cell ranges. */
static inline uint64_t cell_rows_query_knn(cell_rows* s, Vec3 c, uint32_t k) {
  cell_sorted* g = &s->base;
  KnnDigest d = gds_knn_digest();
  if (k == 0 || g->live_count == 0) return d.acc;
  cell_sorted_rebuild_if_needed_(g);
  const float limit = gds_bounds_largest_extent(&g->bounds) * 2.0f;
  const uint32_t* start = g->start.data;
  GDS_VEC(Neighbour) found;
  gds_vec_init(found);
  for (float r = g->cell;; r *= 2.0f) {
    gds_vec_clear(found);
    const cell_sorted_box_ bx = cell_sorted_box_of_(g, c, r);
    for (uint32_t z = bx.z0; z <= bx.z1; ++z) {
      for (uint32_t y = bx.y0; y <= bx.y1; ++y) {
        const size_t row = ((size_t)z * g->ny + y) * g->nx;
        const size_t b = start[row + bx.x0];
        const size_t e = start[row + bx.x1 + 1];
        if (b < e) {
          for (size_t i = b; i < e; ++i) {
            const Vec3 p = {g->sorted_x.data[i], g->sorted_y.data[i], g->sorted_z.data[i]};
            const Neighbour nb = {gds_dist2(p, c), g->sorted_id.data[i]};
            gds_vec_push(found, nb);
          }
        }
      }
    }
    const size_t n = found.size;
    const size_t want = (size_t)k < n ? (size_t)k : n;
    if (want > 0) gds_neighbour_partial_sort(found.data, want, n);
    if (n >= k || r > limit) {
      if (want == 0 || found.data[want - 1].d2 <= r * r || r > limit) {
        for (size_t i = 0; i < want; ++i)
          gds_knn_push(&d, found.data[i].id, g->pos.data[found.data[i].id]);
        gds_vec_free(found);
        return d.acc;
      }
    }
  }
}

static inline void cell_rows_end_tick(cell_rows* s, uint64_t tick) {
  cell_sorted_end_tick(&s->base, tick);
}

static inline bool cell_rows_rewind_to(cell_rows* s, uint64_t tick) {
  return cell_sorted_rewind_to(&s->base, tick);
}

static inline size_t cell_rows_entity_count(cell_rows* s) {
  return cell_sorted_entity_count(&s->base);
}

static inline size_t cell_rows_reported_bytes(cell_rows* s) {
  return cell_sorted_reported_bytes(&s->base);
}

#endif
