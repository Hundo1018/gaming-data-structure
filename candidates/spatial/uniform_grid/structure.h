#ifndef CANDIDATE_SPATIAL_UNIFORM_GRID_H
#define CANDIDATE_SPATIAL_UNIFORM_GRID_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gds/spatial/knn.h"
#include "gds/spatial/types.h"
#include "gds/vec.h"

/* A fixed grid of cells over the world box. Each cell holds an intrusive
 * doubly-linked list threaded through per-entity arrays, so a cell costs four
 * bytes whether or not anything is in it, and moving between cells is two
 * unlinks and two links with no allocation and no search.
 *
 * Cell size is set to the workload's typical query radius, floored so the grid
 * cannot exceed a few million cells. At that size a radius query touches at
 * most twenty-seven cells regardless of how many entities exist. */
typedef struct {
  Bounds bounds;
  float cell;
  float inv_cell;
  uint32_t nx, ny, nz;
  GDS_VEC(EntityId) head;
  GDS_VEC(EntityId) next;
  GDS_VEC(EntityId) prev;
  GDS_VEC(uint32_t) cell_of;
  GDS_VEC(Vec3) pos;
  GDS_VEC(uint8_t) live;
  size_t live_count;
} uniform_grid;

enum { uniform_grid_native_rewind = 0 };

#define UNIFORM_GRID_NO_CELL (~0u)

static inline uint32_t uniform_grid_axis_cells_(const uniform_grid* s, float extent) {
  const int n = (int)floorf(extent * s->inv_cell) + 1;
  return (uint32_t)(n > 1 ? n : 1);
}

static inline uint32_t uniform_grid_axis_index_(const uniform_grid* s, float v, float lo,
                                                uint32_t n) {
  int i = (int)floorf((v - lo) * s->inv_cell);
  if (i < 0) i = 0;
  if (i >= (int)n) i = (int)n - 1;
  return (uint32_t)i;
}

static inline uint32_t uniform_grid_cell_index_(const uniform_grid* s, Vec3 p) {
  const uint32_t ix = uniform_grid_axis_index_(s, p.x, s->bounds.min.x, s->nx);
  const uint32_t iy = uniform_grid_axis_index_(s, p.y, s->bounds.min.y, s->ny);
  const uint32_t iz = uniform_grid_axis_index_(s, p.z, s->bounds.min.z, s->nz);
  return (iz * s->ny + iy) * s->nx + ix;
}

static inline void uniform_grid_init(uniform_grid* s, const WorldConfig* cfg) {
  memset(s, 0, sizeof *s);
  s->bounds = cfg->bounds;
  float floor_cell = gds_bounds_largest_extent(&s->bounds) / 256.0f;
  if (floor_cell < 1e-4f) floor_cell = 1e-4f;
  s->cell = cfg->typical_query_radius > floor_cell ? cfg->typical_query_radius : floor_cell;
  s->inv_cell = 1.0f / s->cell;
  s->nx = uniform_grid_axis_cells_(s, gds_bounds_extent_x(&s->bounds));
  s->ny = uniform_grid_axis_cells_(s, gds_bounds_extent_y(&s->bounds));
  s->nz = uniform_grid_axis_cells_(s, gds_bounds_extent_z(&s->bounds));
  gds_vec_assign(s->head, (size_t)s->nx * s->ny * s->nz, GDS_NO_ENTITY);
  const size_t n = (size_t)cfg->max_entity_id + 1;
  const Vec3 zero = {0, 0, 0};
  gds_vec_assign(s->next, n, GDS_NO_ENTITY);
  gds_vec_assign(s->prev, n, GDS_NO_ENTITY);
  gds_vec_assign(s->cell_of, n, UNIFORM_GRID_NO_CELL);
  gds_vec_assign(s->pos, n, zero);
  gds_vec_assign(s->live, n, 0);
}

static inline void uniform_grid_free(uniform_grid* s) {
  gds_vec_free(s->head);
  gds_vec_free(s->next);
  gds_vec_free(s->prev);
  gds_vec_free(s->cell_of);
  gds_vec_free(s->pos);
  gds_vec_free(s->live);
}

static inline void uniform_grid_link_(uniform_grid* s, EntityId id, uint32_t cell) {
  s->cell_of.data[id] = cell;
  const EntityId h = s->head.data[cell];
  s->next.data[id] = h;
  s->prev.data[id] = GDS_NO_ENTITY;
  if (h != GDS_NO_ENTITY) s->prev.data[h] = id;
  s->head.data[cell] = id;
}

static inline void uniform_grid_unlink_(uniform_grid* s, EntityId id) {
  const uint32_t cell = s->cell_of.data[id];
  if (cell == UNIFORM_GRID_NO_CELL) return;
  const EntityId p = s->prev.data[id];
  const EntityId n = s->next.data[id];
  if (p != GDS_NO_ENTITY) s->next.data[p] = n;
  else s->head.data[cell] = n;
  if (n != GDS_NO_ENTITY) s->prev.data[n] = p;
  s->cell_of.data[id] = UNIFORM_GRID_NO_CELL;
  s->next.data[id] = GDS_NO_ENTITY;
  s->prev.data[id] = GDS_NO_ENTITY;
}

static inline void uniform_grid_insert(uniform_grid* s, EntityId id, Vec3 p) {
  if (s->live.data[id]) uniform_grid_unlink_(s, id);
  else ++s->live_count;
  s->live.data[id] = 1;
  s->pos.data[id] = p;
  uniform_grid_link_(s, id, uniform_grid_cell_index_(s, p));
}

static inline void uniform_grid_remove(uniform_grid* s, EntityId id) {
  if (!s->live.data[id]) return;
  uniform_grid_unlink_(s, id);
  s->live.data[id] = 0;
  --s->live_count;
}

static inline void uniform_grid_move_by(uniform_grid* s, EntityId id, Vec3 delta) {
  if (!s->live.data[id]) return;
  const Vec3 np = gds_wrap_into(gds_add3(s->pos.data[id], delta), &s->bounds);
  s->pos.data[id] = np;
  const uint32_t nc = uniform_grid_cell_index_(s, np);
  if (nc != s->cell_of.data[id]) {
    uniform_grid_unlink_(s, id);
    uniform_grid_link_(s, id, nc);
  }
}

static inline bool uniform_grid_position_of(uniform_grid* s, EntityId id, Vec3* out) {
  if (id >= s->live.size || !s->live.data[id]) return false;
  *out = s->pos.data[id];
  return true;
}

/* The cell range of the axis-aligned box of half-width r around c. The box is
 * not clipped to a sphere, so a caller sees points outside the radius and does
 * the exact test itself. */
typedef struct {
  uint32_t x0, x1, y0, y1, z0, z1;
} uniform_grid_box_;

static inline uniform_grid_box_ uniform_grid_box_of_(const uniform_grid* s, Vec3 c, float r) {
  uniform_grid_box_ b;
  b.x0 = uniform_grid_axis_index_(s, c.x - r, s->bounds.min.x, s->nx);
  b.x1 = uniform_grid_axis_index_(s, c.x + r, s->bounds.min.x, s->nx);
  b.y0 = uniform_grid_axis_index_(s, c.y - r, s->bounds.min.y, s->ny);
  b.y1 = uniform_grid_axis_index_(s, c.y + r, s->bounds.min.y, s->ny);
  b.z0 = uniform_grid_axis_index_(s, c.z - r, s->bounds.min.z, s->nz);
  b.z1 = uniform_grid_axis_index_(s, c.z + r, s->bounds.min.z, s->nz);
  return b;
}

static inline uint64_t uniform_grid_query_radius(uniform_grid* s, Vec3 c, float r) {
  RadiusDigest d = gds_radius_digest(c, r);
  const float r2 = r * r;
  const uniform_grid_box_ b = uniform_grid_box_of_(s, c, r);
  const EntityId* head = s->head.data;
  const EntityId* next = s->next.data;
  const Vec3* pos = s->pos.data;
  for (uint32_t z = b.z0; z <= b.z1; ++z) {
    for (uint32_t y = b.y0; y <= b.y1; ++y) {
      const uint32_t row = (z * s->ny + y) * s->nx;
      for (uint32_t x = b.x0; x <= b.x1; ++x) {
        for (EntityId id = head[row + x]; id != GDS_NO_ENTITY; id = next[id])
          if (gds_dist2(pos[id], c) <= r2) gds_radius_hit(&d, id, pos[id]);
      }
    }
  }
  return d.acc;
}

static inline uint64_t uniform_grid_query_radius_of(uniform_grid* s, EntityId self, float r) {
  if (self >= s->live.size || !s->live.data[self]) return 0;
  /* The digest is a sum, so removing the centre from its own result is a
   * subtraction rather than a branch inside the inner loop. */
  const Vec3 c = s->pos.data[self];
  const RadiusDigest own = gds_radius_digest(c, r);
  return uniform_grid_query_radius(s, c, r) - gds_radius_term(&own, self, c);
}

static inline uint64_t uniform_grid_query_knn(uniform_grid* s, Vec3 c, uint32_t k) {
  KnnDigest d = gds_knn_digest();
  if (k == 0 || s->live_count == 0) return d.acc;
  /* Everything outside the box of half-width r is farther than r from c, so
   * once the kth gathered neighbour is within r the answer cannot change. */
  const float limit = gds_bounds_largest_extent(&s->bounds) * 2.0f;
  const EntityId* head = s->head.data;
  const EntityId* next = s->next.data;
  const Vec3* pos = s->pos.data;
  GDS_VEC(Neighbour) found;
  gds_vec_init(found);
  for (float r = s->cell;; r *= 2.0f) {
    gds_vec_clear(found);
    const uniform_grid_box_ b = uniform_grid_box_of_(s, c, r);
    for (uint32_t z = b.z0; z <= b.z1; ++z) {
      for (uint32_t y = b.y0; y <= b.y1; ++y) {
        const uint32_t row = (z * s->ny + y) * s->nx;
        for (uint32_t x = b.x0; x <= b.x1; ++x) {
          for (EntityId id = head[row + x]; id != GDS_NO_ENTITY; id = next[id]) {
            const Neighbour nb = {gds_dist2(pos[id], c), id};
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
          gds_knn_push(&d, found.data[i].id, pos[found.data[i].id]);
        gds_vec_free(found);
        return d.acc;
      }
    }
  }
}

static inline void uniform_grid_end_tick(uniform_grid* s, uint64_t tick) {
  (void)s;
  (void)tick;
}

static inline bool uniform_grid_rewind_to(uniform_grid* s, uint64_t tick) {
  (void)s;
  (void)tick;
  return false;
}

static inline size_t uniform_grid_entity_count(uniform_grid* s) { return s->live_count; }

static inline size_t uniform_grid_reported_bytes(uniform_grid* s) {
  return gds_vec_bytes(s->head) + gds_vec_bytes(s->next) + gds_vec_bytes(s->prev) +
         gds_vec_bytes(s->cell_of) + gds_vec_bytes(s->pos) + gds_vec_bytes(s->live);
}

#endif
