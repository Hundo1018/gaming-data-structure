#ifndef CANDIDATE_SPATIAL_CELL_SORTED_H
#define CANDIDATE_SPATIAL_CELL_SORTED_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gds/spatial/knn.h"
#include "gds/spatial/types.h"
#include "gds/vec.h"

/* morton_sorted with its sort key and its lookup replaced, and nothing else
 * changed in kind. Positions are still written into flat arrays as they change,
 * and the searchable form is still thrown away and rebuilt by the first query
 * that follows any change.
 *
 * The key is uniform_grid's dense cell index instead of a Morton code. A dense
 * integer key admits a counting sort, which orders the population with no
 * comparison, and the running totals that sort computes are a directory: the
 * entities of cell c are the range [start[c], start[c+1]). Finding a cell is
 * one array read, as it is in the grid, instead of the binary search per cell
 * that made the parent's k-nearest search slower than a linear scan.
 *
 * The cell geometry is uniform_grid's, reproduced here rather than included:
 * its helpers are reachable only by embedding its linked lists, which this
 * structure does not have, and the parent keeps its fields to itself. */
typedef struct {
  Bounds bounds;
  float cell;
  float inv_cell;
  uint32_t nx, ny, nz;
  GDS_VEC(Vec3) pos;
  GDS_VEC(uint8_t) live;
  size_t live_count;
  bool dirty;
  GDS_VEC(uint32_t) cell_of;
  GDS_VEC(uint32_t) start;
  GDS_VEC(EntityId) sorted_id;
  GDS_VEC(float) sorted_x;
  GDS_VEC(float) sorted_y;
  GDS_VEC(float) sorted_z;
} cell_sorted;

enum { cell_sorted_native_rewind = 0 };

/* Everything from here on is for a descendant as much as for this file, so
 * that it can change one thing and include the rest by path; cell_rows does,
 * for the walk.
 *
 * One 256-bit vector of floats. GCC's -O2 cost model vectorises a loop only
 * when its trip count is a known multiple of the vector width, so the
 * distance test runs over blocks of exactly this many slots and masks the
 * lanes past the end of a cell's range instead of stopping at it. */
#define CELL_SORTED_LANES ((size_t)8)
/* A block starting at the last entity reads this far past it. */
#define CELL_SORTED_PAD (CELL_SORTED_LANES - 1)

static inline uint32_t cell_sorted_axis_cells_(const cell_sorted* s, float extent) {
  const int n = (int)floorf(extent * s->inv_cell) + 1;
  return (uint32_t)(1 < n ? n : 1);
}

static inline uint32_t cell_sorted_axis_index_(const cell_sorted* s, float v, float lo,
                                               uint32_t n) {
  int i = (int)floorf((v - lo) * s->inv_cell);
  if (i < 0) i = 0;
  if (i >= (int)n) i = (int)n - 1;
  return (uint32_t)i;
}

static inline uint32_t cell_sorted_cell_index_(const cell_sorted* s, Vec3 p) {
  const uint32_t ix = cell_sorted_axis_index_(s, p.x, s->bounds.min.x, s->nx);
  const uint32_t iy = cell_sorted_axis_index_(s, p.y, s->bounds.min.y, s->ny);
  const uint32_t iz = cell_sorted_axis_index_(s, p.z, s->bounds.min.z, s->nz);
  return (iz * s->ny + iy) * s->nx + ix;
}

static inline void cell_sorted_init(cell_sorted* s, const WorldConfig* cfg) {
  memset(s, 0, sizeof *s);
  s->bounds = cfg->bounds;
  float floor_cell = gds_bounds_largest_extent(&s->bounds) / 256.0f;
  if (floor_cell < 1e-4f) floor_cell = 1e-4f;
  s->cell = cfg->typical_query_radius < floor_cell ? floor_cell : cfg->typical_query_radius;
  s->inv_cell = 1.0f / s->cell;
  s->nx = cell_sorted_axis_cells_(s, gds_bounds_extent_x(&s->bounds));
  s->ny = cell_sorted_axis_cells_(s, gds_bounds_extent_y(&s->bounds));
  s->nz = cell_sorted_axis_cells_(s, gds_bounds_extent_z(&s->bounds));
  s->dirty = true;
  gds_vec_assign(s->start, (size_t)s->nx * s->ny * s->nz + 1, 0u);
  const size_t n = cfg->max_entity_id + 1;
  const Vec3 zero = {0, 0, 0};
  gds_vec_assign(s->pos, n, zero);
  gds_vec_assign(s->live, n, 0);
  gds_vec_assign(s->cell_of, n, 0u);
}

static inline void cell_sorted_free(cell_sorted* s) {
  gds_vec_free(s->pos);
  gds_vec_free(s->live);
  gds_vec_free(s->cell_of);
  gds_vec_free(s->start);
  gds_vec_free(s->sorted_id);
  gds_vec_free(s->sorted_x);
  gds_vec_free(s->sorted_y);
  gds_vec_free(s->sorted_z);
}

static inline void cell_sorted_insert(cell_sorted* s, EntityId id, Vec3 p) {
  if (!s->live.data[id]) ++s->live_count;
  s->live.data[id] = 1;
  s->pos.data[id] = p;
  s->dirty = true;
}

static inline void cell_sorted_remove(cell_sorted* s, EntityId id) {
  if (!s->live.data[id]) return;
  s->live.data[id] = 0;
  --s->live_count;
  s->dirty = true;
}

static inline void cell_sorted_move_by(cell_sorted* s, EntityId id, Vec3 delta) {
  if (!s->live.data[id]) return;
  s->pos.data[id] = gds_wrap_into(gds_add3(s->pos.data[id], delta), &s->bounds);
  s->dirty = true;
}

static inline bool cell_sorted_position_of(cell_sorted* s, EntityId id, Vec3* out) {
  if (id >= s->live.size || !s->live.data[id]) return false;
  *out = s->pos.data[id];
  return true;
}

/* A counting sort on the cell index. The running totals leave each entry of
 * the directory at the end of its cell's range; the scatter walks ids
 * downwards and pre-decrements, so each entry finishes at the start of its
 * range and no separate array of write cursors is needed. Within a cell,
 * entities end up in ascending id order, which nothing depends on. */
static inline void cell_sorted_rebuild_if_needed_(cell_sorted* s) {
  if (!s->dirty) return;
  const size_t cells = s->start.size - 1;
  uint32_t* start = s->start.data;
  memset(start, 0, s->start.size * sizeof *start);
  const size_t n = s->live.size;
  for (size_t id = 0; id < n; ++id) {
    if (!s->live.data[id]) continue;
    const uint32_t c = cell_sorted_cell_index_(s, s->pos.data[id]);
    s->cell_of.data[id] = c;
    ++start[c];
  }
  uint32_t end = 0;
  for (size_t c = 0; c < cells; ++c) {
    end += start[c];
    start[c] = end;
  }
  start[cells] = end;
  const size_t padded = (size_t)end + CELL_SORTED_PAD;
  gds_vec_resize(s->sorted_id, padded);
  gds_vec_resize(s->sorted_x, padded);
  gds_vec_resize(s->sorted_y, padded);
  gds_vec_resize(s->sorted_z, padded);
  for (size_t id = n; id-- > 0;) {
    if (!s->live.data[id]) continue;
    const uint32_t slot = --start[s->cell_of.data[id]];
    s->sorted_id.data[slot] = (EntityId)id;
    s->sorted_x.data[slot] = s->pos.data[id].x;
    s->sorted_y.data[slot] = s->pos.data[id].y;
    s->sorted_z.data[slot] = s->pos.data[id].z;
  }
  s->dirty = false;
}

/* The cells overlapping the axis-aligned box of half-width r. A caller walks
 * every one of them, in uniform_grid's order, each as its range of the sorted
 * arrays. The box is not clipped to a sphere, so the caller does the exact
 * test.
 *
 * The cells of one row are consecutive in the cell order, so their ranges
 * are adjacent and a row could be read as one run from two directory reads.
 * That is a second change to the lookup, which uniform_grid's layout cannot
 * make, and it is left out so that the pair differs only in the key and the
 * directory; each cell is read on its own, as the grid reads its list heads. */
typedef struct {
  uint32_t x0, x1, y0, y1, z0, z1;
} cell_sorted_box_;

static inline cell_sorted_box_ cell_sorted_box_of_(const cell_sorted* s, Vec3 c, float r) {
  cell_sorted_box_ b;
  b.x0 = cell_sorted_axis_index_(s, c.x - r, s->bounds.min.x, s->nx);
  b.x1 = cell_sorted_axis_index_(s, c.x + r, s->bounds.min.x, s->nx);
  b.y0 = cell_sorted_axis_index_(s, c.y - r, s->bounds.min.y, s->ny);
  b.y1 = cell_sorted_axis_index_(s, c.y + r, s->bounds.min.y, s->ny);
  b.z0 = cell_sorted_axis_index_(s, c.z - r, s->bounds.min.z, s->nz);
  b.z1 = cell_sorted_axis_index_(s, c.z + r, s->bounds.min.z, s->nz);
  return b;
}

/* The exact test over one cell's range, a block of CELL_SORTED_LANES slots at
 * a time. The block is indexed from its own base pointer so the compiler sees
 * contiguous loads; each lane's accept bit is the shared dist2 test and its
 * index being inside the range, and the accepted lanes are then folded one by
 * one. A cell with the cell edge at the query radius usually holds fewer
 * entities than a block, so its range is one block with the tail masked. */
static inline void cell_sorted_scan_run_(const cell_sorted* s, size_t b, size_t e, Vec3 c,
                                         float r2, RadiusDigest* d) {
  for (size_t i = b; i < e; i += CELL_SORTED_LANES) {
    const float* px = s->sorted_x.data + i;
    const float* py = s->sorted_y.data + i;
    const float* pz = s->sorted_z.data + i;
    const size_t left = e - i;
    uint32_t hits = 0;
    for (size_t j = 0; j < CELL_SORTED_LANES; ++j) {
      const bool in = gds_dist2(gds_vec3(px[j], py[j], pz[j]), c) <= r2;
      hits |= (uint32_t)(in & (j < left)) << j;
    }
    while (hits != 0) {
      const size_t j = (size_t)__builtin_ctz(hits);
      hits &= hits - 1;
      gds_radius_hit(d, s->sorted_id.data[i + j], gds_vec3(px[j], py[j], pz[j]));
    }
  }
}

static inline uint64_t cell_sorted_query_radius(cell_sorted* s, Vec3 c, float r) {
  cell_sorted_rebuild_if_needed_(s);
  RadiusDigest d = gds_radius_digest(c, r);
  const float r2 = r * r;
  const cell_sorted_box_ bx = cell_sorted_box_of_(s, c, r);
  const uint32_t* start = s->start.data;
  for (uint32_t z = bx.z0; z <= bx.z1; ++z) {
    for (uint32_t y = bx.y0; y <= bx.y1; ++y) {
      const size_t row = ((size_t)z * s->ny + y) * s->nx;
      for (uint32_t x = bx.x0; x <= bx.x1; ++x) {
        const size_t b = start[row + x];
        const size_t e = start[row + x + 1];
        if (b < e) cell_sorted_scan_run_(s, b, e, c, r2, &d);
      }
    }
  }
  return d.acc;
}

static inline uint64_t cell_sorted_query_radius_of(cell_sorted* s, EntityId self, float r) {
  if (self >= s->live.size || !s->live.data[self]) return 0;
  const Vec3 c = s->pos.data[self];
  const RadiusDigest own = gds_radius_digest(c, r);
  return cell_sorted_query_radius(s, c, r) - gds_radius_term(&own, self, c);
}

/* uniform_grid's search, unaltered: widen a box from one cell edge, doubling,
 * until the kth gathered neighbour is inside its half-width. Only where the
 * candidates come from differs. */
static inline uint64_t cell_sorted_query_knn(cell_sorted* s, Vec3 c, uint32_t k) {
  KnnDigest d = gds_knn_digest();
  if (k == 0 || s->live_count == 0) return d.acc;
  cell_sorted_rebuild_if_needed_(s);
  const float limit = gds_bounds_largest_extent(&s->bounds) * 2.0f;
  const uint32_t* start = s->start.data;
  GDS_VEC(Neighbour) found;
  gds_vec_init(found);
  for (float r = s->cell;; r *= 2.0f) {
    gds_vec_clear(found);
    const cell_sorted_box_ bx = cell_sorted_box_of_(s, c, r);
    for (uint32_t z = bx.z0; z <= bx.z1; ++z) {
      for (uint32_t y = bx.y0; y <= bx.y1; ++y) {
        const size_t row = ((size_t)z * s->ny + y) * s->nx;
        for (uint32_t x = bx.x0; x <= bx.x1; ++x) {
          const size_t b = start[row + x];
          const size_t e = start[row + x + 1];
          if (b < e) {
            for (size_t i = b; i < e; ++i) {
              const Vec3 p = {s->sorted_x.data[i], s->sorted_y.data[i], s->sorted_z.data[i]};
              const Neighbour nb = {gds_dist2(p, c), s->sorted_id.data[i]};
              gds_vec_push(found, nb);
            }
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
          gds_knn_push(&d, found.data[i].id, s->pos.data[found.data[i].id]);
        gds_vec_free(found);
        return d.acc;
      }
    }
  }
}

static inline void cell_sorted_end_tick(cell_sorted* s, uint64_t tick) {
  (void)s;
  (void)tick;
}

static inline bool cell_sorted_rewind_to(cell_sorted* s, uint64_t tick) {
  (void)s;
  (void)tick;
  return false;
}

static inline size_t cell_sorted_entity_count(cell_sorted* s) { return s->live_count; }

static inline size_t cell_sorted_reported_bytes(cell_sorted* s) {
  return gds_vec_bytes(s->pos) + gds_vec_bytes(s->live) + gds_vec_bytes(s->cell_of) +
         gds_vec_bytes(s->start) + gds_vec_bytes(s->sorted_id) + gds_vec_bytes(s->sorted_x) +
         gds_vec_bytes(s->sorted_y) + gds_vec_bytes(s->sorted_z);
}

#endif
