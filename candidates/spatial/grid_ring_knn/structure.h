#ifndef CANDIDATE_SPATIAL_GRID_RING_KNN_H
#define CANDIDATE_SPATIAL_GRID_RING_KNN_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gds/spatial/knn.h"
#include "gds/spatial/types.h"
#include "gds/vec.h"
#include "spatial/uniform_grid/structure.h"

/* The table of cell offsets the k-nearest walk follows, nearest first.
 *
 * It is sorted by the least distance any point of an offset cell can be from
 * any point of the query's cell: cell * sqrt(Q), Q = sum over axes of
 * max(0, |d| - 1)^2. Q is an integer, so the order is exact and offsets of equal
 * Q form a shell sharing one bound. The bound depends on |d| alone, so only one
 * octant is stored and each entry stands for its sign reflections.
 *
 * The table depends on nothing but its reach, so it is built once, by the
 * first instance, into static storage, and every instance shares it. A
 * per-instance table would be rebuilt with every snapshot-and-rebuild rewind,
 * which constructs a new structure, and the rewind would then pay for
 * something the parent's does not. */

/* Offsets whose bound is under this many cell edges are in the table. See
 * hypothesis.md: it covers the sparsest public k-nearest configuration with
 * margin, for 3134 four-byte entries in each of two orders. */
enum {
  GRID_RING_KNN_REACH = 16,
  GRID_RING_KNN_Q_LIMIT = GRID_RING_KNN_REACH * GRID_RING_KNN_REACH,
};

/* Every bound is shortened by this much of a cell edge. A point can sit
 * outside the cell it is filed in by the rounding of (v - lo) * inv_cell,
 * under 6.2e-5 of a cell edge with at most 258 cells per axis, and both the
 * query and the entity carry that error. This is over ten times that. */
#define GRID_RING_KNN_SLACK (1.0f / 256.0f)

/* One octant offset in cells, |dx|, |dy|, |dz|, and the key its order sorts a
 * shell's steps by. Four bytes so each order stays near twelve kilobytes. */
typedef struct {
  uint8_t x, y, z, lead;
} grid_ring_knn_step_;

/* A run of steps of equal Q, ending at `end`; sqrt(Q) less the slack, which is
 * the bound they share in cell edges; and the largest |d| any of them has on
 * any axis. */
typedef struct {
  uint32_t end;
  float root;
  int reach;
} grid_ring_knn_shell_;

static inline int grid_ring_knn_q_of_(int a) { return a > 1 ? (a - 1) * (a - 1) : 0; }

/* Every octant offset with Q under the limit, z then y then x, running the
 * statements given with x, y, z and q in scope. Q only grows with each
 * component and q_of(GRID_RING_KNN_REACH + 1) is the limit, so no component of
 * a kept offset exceeds the reach and each run of x stops at its first offset
 * past. */
#define GRID_RING_KNN_FOR_EACH_OFFSET_(...)                                   \
  for (int z = 0; z <= GRID_RING_KNN_REACH; ++z)                              \
    for (int y = 0; y <= GRID_RING_KNN_REACH; ++y) {                          \
      const int qyz = grid_ring_knn_q_of_(y) + grid_ring_knn_q_of_(z);        \
      for (int x = 0; x <= GRID_RING_KNN_REACH; ++x) {                        \
        const int q = qyz + grid_ring_knn_q_of_(x);                           \
        if (q >= GRID_RING_KNN_Q_LIMIT) break;                                \
        __VA_ARGS__                                                           \
      }                                                                       \
    }

static inline size_t grid_ring_knn_count_steps_(void) {
  size_t n = 0;
  GRID_RING_KNN_FOR_EACH_OFFSET_(++n;)
  return n;
}

static inline size_t grid_ring_knn_count_shells_(void) {
  bool used[GRID_RING_KNN_Q_LIMIT];
  memset(used, 0, sizeof used);
  GRID_RING_KNN_FOR_EACH_OFFSET_(used[(size_t)q] = true;)
  size_t n = 0;
  for (size_t i = 0; i < GRID_RING_KNN_Q_LIMIT; ++i) n += used[i] ? 1 : 0;
  return n;
}

/* What grid_ring_knn_count_steps_() and grid_ring_knn_count_shells_() return.
 * The arrays below are sized by them, and C has no constant expression that
 * can run the enumeration, so the counts are written out here and the
 * one-time build checks them against the enumeration before it writes
 * anything. */
enum {
  GRID_RING_KNN_STEPS = 3134,
  GRID_RING_KNN_SHELLS = 214,
};

/* The C++ built this table in a constant expression, where std::sqrt is not
 * usable, and the same iteration is kept here so that every bound is the one
 * it computed. Newton's iteration from x = q decreases monotonically towards
 * sqrt(q) and stops when rounding stops it, within an ulp of a double; the
 * slack it is compared against is ten orders of magnitude larger. */
static inline double grid_ring_knn_root_of_(int q) {
  if (q == 0) return 0.0;
  double x = q;
  for (int i = 0; i < 64; ++i) {
    const double next = 0.5 * (x + q / x);
    if (!(next < x)) break;
    x = next;
  }
  return x;
}

/* The same offsets in two orders. Both are sorted by Q first, so they share
 * their shells; within a shell one ascends in |dz| and the other in
 * max(|dx|, |dy|). A grid shorter in z than across walks the first and a grid
 * narrower across than in z the second, so that in either the first step of a
 * shell too long for the grid along the leading key ends the shell, and the
 * steps after it are never read. */
typedef struct {
  grid_ring_knn_step_ by_z[GRID_RING_KNN_STEPS];
  grid_ring_knn_step_ by_xy[GRID_RING_KNN_STEPS];
  grid_ring_knn_shell_ shells[GRID_RING_KNN_SHELLS];
} grid_ring_knn_octant_;

/* The leading key of an order: |dz| for by_z, max(|dx|, |dy|) for by_xy. */
static inline int grid_ring_knn_lead_(bool by_xy, int x, int y, int z) {
  return by_xy ? (x < y ? y : x) : z;
}

/* Counting sort on (Q, lead). It is stable, so the offsets of each (Q, lead)
 * stay in the order they were enumerated. */
static inline void grid_ring_knn_sort_steps_(grid_ring_knn_step_* out, bool by_xy) {
  enum { leads = GRID_RING_KNN_REACH + 1 };
  uint32_t next[GRID_RING_KNN_Q_LIMIT * leads + 1];
  memset(next, 0, sizeof next);
  const size_t buckets = sizeof next / sizeof *next;
  GRID_RING_KNN_FOR_EACH_OFFSET_(
      ++next[(size_t)q * leads + (size_t)grid_ring_knn_lead_(by_xy, x, y, z) + 1];)
  for (size_t b = 0; b + 1 < buckets; ++b) next[b + 1] += next[b];
  GRID_RING_KNN_FOR_EACH_OFFSET_(
      const int lead = grid_ring_knn_lead_(by_xy, x, y, z);
      const grid_ring_knn_step_ st = {(uint8_t)x, (uint8_t)y, (uint8_t)z, (uint8_t)lead};
      out[next[(size_t)q * leads + (size_t)lead]++] = st;)
}

static inline void grid_ring_knn_build_octant_(grid_ring_knn_octant_* t) {
  memset(t, 0, sizeof *t);
  grid_ring_knn_sort_steps_(t->by_z, false);
  grid_ring_knn_sort_steps_(t->by_xy, true);
  uint32_t end[GRID_RING_KNN_Q_LIMIT + 1];
  memset(end, 0, sizeof end);
  GRID_RING_KNN_FOR_EACH_OFFSET_(++end[(size_t)q + 1];)
  for (size_t q = 0; q < GRID_RING_KNN_Q_LIMIT; ++q) end[q + 1] += end[q];
  size_t s = 0;
  for (int q = 0; q < GRID_RING_KNN_Q_LIMIT; ++q) {
    if (end[(size_t)q + 1] == end[(size_t)q]) continue;
    /* q_of(d) <= Q on every axis, so |d| <= isqrt(Q) + 1. */
    int root = 0;
    while ((root + 1) * (root + 1) <= q) ++root;
    const float bound = q == 0 ? 0.0f : (float)(grid_ring_knn_root_of_(q) - 1.0 / 256.0);
    const int reach = root + 1 < GRID_RING_KNN_REACH ? root + 1 : GRID_RING_KNN_REACH;
    const grid_ring_knn_shell_ shell = {end[(size_t)q + 1], bound, reach};
    t->shells[s++] = shell;
  }
}

/* The walk relies on this: in both orders every step of a shell has the
 * shell's Q, and its lead never decreases within the shell. */
static inline bool grid_ring_knn_well_ordered_(const grid_ring_knn_step_* steps,
                                               const grid_ring_knn_octant_* t) {
  uint32_t e = 0;
  for (size_t si = 0; si < GRID_RING_KNN_SHELLS; ++si) {
    const grid_ring_knn_shell_* shell = &t->shells[si];
    const grid_ring_knn_step_ first = steps[e];
    const int q = grid_ring_knn_q_of_(first.x) + grid_ring_knn_q_of_(first.y) +
                  grid_ring_knn_q_of_(first.z);
    for (uint32_t i = e; i < shell->end; ++i) {
      const grid_ring_knn_step_ st = steps[i];
      if (grid_ring_knn_q_of_(st.x) + grid_ring_knn_q_of_(st.y) + grid_ring_knn_q_of_(st.z) != q)
        return false;
      if (i > e && st.lead < steps[i - 1].lead) return false;
    }
    e = shell->end;
  }
  return e == GRID_RING_KNN_STEPS;
}

/* The table every instance walks, and whether it has been built yet. */
static grid_ring_knn_octant_ grid_ring_knn_k_octant_;
static bool grid_ring_knn_k_octant_built_;

/* Builds the table on the first call and does nothing after. The C++ checked
 * the counts and the ordering at compile time; C cannot, so they are checked
 * here, once, and a table that fails them stops the program before any query
 * could read it. */
static inline void grid_ring_knn_build_table_(void) {
  if (grid_ring_knn_k_octant_built_) return;
  if (grid_ring_knn_count_steps_() != GRID_RING_KNN_STEPS ||
      grid_ring_knn_count_shells_() != GRID_RING_KNN_SHELLS) {
    fprintf(stderr, "grid_ring_knn: GRID_RING_KNN_STEPS or GRID_RING_KNN_SHELLS is wrong\n");
    abort();
  }
  grid_ring_knn_build_octant_(&grid_ring_knn_k_octant_);
  if (!(grid_ring_knn_well_ordered_(grid_ring_knn_k_octant_.by_z, &grid_ring_knn_k_octant_) &&
        grid_ring_knn_well_ordered_(grid_ring_knn_k_octant_.by_xy, &grid_ring_knn_k_octant_))) {
    fprintf(stderr, "grid_ring_knn: the offset table is not well ordered\n");
    abort();
  }
  grid_ring_knn_k_octant_built_ = true;
}

/* uniform_grid with its k-nearest search replaced and nothing else changed.
 *
 * The parent widens a box by doubling its half-width and re-walks every cell of
 * it each round. This visits each cell at most once, nearest first, in the
 * order of the table above, and stops as soon as no unvisited cell can hold
 * anything nearer than the k-th best found so far. */
typedef struct {
  uniform_grid base;
  /* The largest step component that can fit inside the grid on each axis, and
   * the least of the three: a shell whose reach is within it needs no check. */
  int ex, ey, ez, fit;
  /* The order this grid walks, and the largest lead in it that can fit. */
  const grid_ring_knn_step_* order;
  int lead;
  float tail_bound2;
  uint32_t cell_count;
  bool covers_grid;
  /* Scratch for the best k of one query, kept so a query allocates nothing once
   * it has grown to the largest k asked for. */
  GDS_VEC(Neighbour) heap;
} grid_ring_knn;

enum { grid_ring_knn_native_rewind = uniform_grid_native_rewind };

/* Every squared bound is multiplied by this. dist2 can round below the real
 * squared distance by a relative 5 * 2^-24, and computing a bound can round
 * up by about as much; 2^-16 is 256 * 2^-24. */
#define GRID_RING_KNN_SHRINK (1.0f - 1.0f / 65536.0f)

/* The query's position along one axis, in the coordinates that file points
 * into cells: cell i + a starts (a - up) cell edges above it and cell i - a
 * ends (a - down) below it, both already shortened by the slack. */
typedef struct {
  int i;
  int n;
  float up;
  float down;
} grid_ring_knn_axis_;

/* Per query and per axis, indexed by GRID_RING_KNN_REACH + d: the change in
 * linear cell index for moving d cells along the axis, or
 * GRID_RING_KNN_OUTSIDE if that leaves the grid, and the squared gap from the
 * query to that slab of cells, in world units. Filled only as far as the walk
 * has reached. */
typedef struct {
  int32_t step[2 * GRID_RING_KNN_REACH + 1];
  float gap2[2 * GRID_RING_KNN_REACH + 1];
} grid_ring_knn_lookup_;

/* Added to a linear index, any one of these makes it negative, and three
 * together still do not overflow: an index past every grid this parent can
 * build (at most 258 cells per axis) is rejected by one unsigned comparison. */
#define GRID_RING_KNN_OUTSIDE (INT32_MIN / 4)
#define GRID_RING_KNN_FAR INFINITY

typedef struct {
  Vec3 c;
  size_t k;
  size_t seen;
  float worst; /* the k-th best dist2 once k are held, infinity before */
} grid_ring_knn_search_;

/* A length in world units, squared and shrunk. Every bound is scaled into
 * world units before it is squared: squaring the cell edge first overflows
 * float once the edge passes 2^64, and a bound of infinity would then prune
 * cells whose entities are a fraction of an edge away at a finite dist2.
 * Squared after scaling, a bound overflows only where the distance it bounds
 * is larger still, so dist2 of anything it prunes is infinite as well. */
static inline float grid_ring_knn_squared_(float length) {
  return length * length * GRID_RING_KNN_SHRINK;
}

/* Which of the table's steps can fit inside this grid, and what is left
 * beyond the table. Constant time: nothing here depends on the population,
 * and the table itself is not rebuilt. */
static inline void grid_ring_knn_fit_grid_(grid_ring_knn* s) {
  const uniform_grid* g = &s->base;
  s->ex = (int)g->nx - 1 < GRID_RING_KNN_REACH ? (int)g->nx - 1 : GRID_RING_KNN_REACH;
  s->ey = (int)g->ny - 1 < GRID_RING_KNN_REACH ? (int)g->ny - 1 : GRID_RING_KNN_REACH;
  s->ez = (int)g->nz - 1 < GRID_RING_KNN_REACH ? (int)g->nz - 1 : GRID_RING_KNN_REACH;
  const int fit_yz = s->ez < s->ey ? s->ez : s->ey;
  s->fit = fit_yz < s->ex ? fit_yz : s->ex;
  /* Of the two orders, the one whose leading key this grid cuts shorter. */
  const int across = s->ex < s->ey ? s->ey : s->ex;
  const bool by_xy = across < s->ez;
  s->order = by_xy ? grid_ring_knn_k_octant_.by_xy : grid_ring_knn_k_octant_.by_z;
  s->lead = by_xy ? across : s->ez;
  /* Anything not in the table is outside the grid or has Q of at least the
   * limit, so this bounds every cell the table walk did not reach. When the
   * largest offset the grid can hold is in the table, nothing is unreached. */
  s->tail_bound2 =
      grid_ring_knn_squared_(((float)GRID_RING_KNN_REACH - GRID_RING_KNN_SLACK) * g->cell);
  s->covers_grid = grid_ring_knn_q_of_((int)g->nx - 1) + grid_ring_knn_q_of_((int)g->ny - 1) +
                       grid_ring_knn_q_of_((int)g->nz - 1) <
                   GRID_RING_KNN_Q_LIMIT;
  s->cell_count = (uint32_t)g->head.size;
}

static inline void grid_ring_knn_init(grid_ring_knn* s, const WorldConfig* cfg) {
  memset(s, 0, sizeof *s);
  grid_ring_knn_build_table_();
  uniform_grid_init(&s->base, cfg);
  grid_ring_knn_fit_grid_(s);
}

static inline void grid_ring_knn_free(grid_ring_knn* s) {
  gds_vec_free(s->heap);
  uniform_grid_free(&s->base);
}

static inline void grid_ring_knn_insert(grid_ring_knn* s, EntityId id, Vec3 p) {
  uniform_grid_insert(&s->base, id, p);
}

static inline void grid_ring_knn_remove(grid_ring_knn* s, EntityId id) {
  uniform_grid_remove(&s->base, id);
}

static inline void grid_ring_knn_move_by(grid_ring_knn* s, EntityId id, Vec3 delta) {
  uniform_grid_move_by(&s->base, id, delta);
}

static inline bool grid_ring_knn_position_of(grid_ring_knn* s, EntityId id, Vec3* out) {
  return uniform_grid_position_of(&s->base, id, out);
}

static inline uint64_t grid_ring_knn_query_radius(grid_ring_knn* s, Vec3 c, float r) {
  return uniform_grid_query_radius(&s->base, c, r);
}

static inline uint64_t grid_ring_knn_query_radius_of(grid_ring_knn* s, EntityId self, float r) {
  return uniform_grid_query_radius_of(&s->base, self, r);
}

static inline grid_ring_knn_axis_ grid_ring_knn_axis_frame_(const grid_ring_knn* s, float v,
                                                            float lo, uint32_t n) {
  const uniform_grid* g = &s->base;
  const int i = (int)uniform_grid_axis_index_(g, v, lo, n);
  /* The same expression axis_index floors. Clamping a query that lies
   * outside the grid toward it only shortens its gaps, which keeps every
   * bound below the truth. */
  float scaled = (v - lo) * g->inv_cell;
  if (!(scaled >= -1.0f)) scaled = -1.0f;
  if (scaled > (float)n + 1.0f) scaled = (float)n + 1.0f;
  const float f = scaled - (float)i;
  const grid_ring_knn_axis_ a = {i, (int)n, f + GRID_RING_KNN_SLACK,
                                 (1.0f - f) + GRID_RING_KNN_SLACK};
  return a;
}

/* The squared world-unit gap to a slab `cells` cell edges away, or none. */
static inline float grid_ring_knn_slab_gap2_(const grid_ring_knn* s, float cells) {
  return cells > 0.0f ? grid_ring_knn_squared_(cells * s->base.cell) : 0.0f;
}

/* Fills offsets from..to, both signs, of one axis's lookup. The query's own
 * slab has no gap: if the query lies outside the grid, the edge cell it was
 * clamped into also holds whatever entities were clamped there with it. */
static inline void grid_ring_knn_extend_(const grid_ring_knn* s, grid_ring_knn_lookup_* t,
                                         const grid_ring_knn_axis_* a, int32_t stride, int from,
                                         int to) {
  for (int off = from; off <= to; ++off) {
    t->step[GRID_RING_KNN_REACH + off] = a->i + off < a->n ? off * stride : GRID_RING_KNN_OUTSIDE;
    t->step[GRID_RING_KNN_REACH - off] = a->i - off >= 0 ? -off * stride : GRID_RING_KNN_OUTSIDE;
    const float d = (float)off;
    t->gap2[GRID_RING_KNN_REACH + off] = off == 0 ? 0.0f : grid_ring_knn_slab_gap2_(s, d - a->up);
    t->gap2[GRID_RING_KNN_REACH - off] =
        off == 0 ? 0.0f : grid_ring_knn_slab_gap2_(s, d - a->down);
  }
}

/* The heap's top is the farthest of the best k. Replacing it and sifting down
 * is one pass where a pop and a push would be two. */
static inline void grid_ring_knn_replace_top_(grid_ring_knn* s, Neighbour nb) {
  Neighbour* h = s->heap.data;
  const size_t n = s->heap.size;
  size_t i = 0;
  for (;;) {
    size_t child = 2 * i + 1;
    if (child >= n) break;
    if (child + 1 < n && gds_nearer(h[child], h[child + 1])) ++child;
    if (!gds_nearer(nb, h[child])) break;
    h[i] = h[child];
    i = child;
  }
  h[i] = nb;
}

static inline void grid_ring_knn_walk_(grid_ring_knn* s, EntityId id, float gap2,
                                       grid_ring_knn_search_* sr) {
  /* The exact gap from the query point to this cell's box, tested only for
   * cells that hold something. Once k are held, a cell strictly beyond the
   * k-th best cannot change the answer; equal is walked, for the same reason
   * as at a shell boundary. */
  if (gap2 > sr->worst) return;
  const EntityId* next = s->base.next.data;
  const Vec3* pos = s->base.pos.data;
  for (; id != GDS_NO_ENTITY; id = next[id]) {
    ++sr->seen;
    const Neighbour nb = {gds_dist2(pos[id], sr->c), id};
    if (s->heap.size < sr->k) {
      gds_vec_push(s->heap, nb);
      gds_neighbour_push_heap(s->heap.data, s->heap.size);
      if (s->heap.size == sr->k) sr->worst = s->heap.data[0].d2;
    } else if (gds_nearer(nb, s->heap.data[0])) {
      grid_ring_knn_replace_top_(s, nb);
      sr->worst = s->heap.data[0].d2;
    }
  }
}

/* Small enough to inline at all eight call sites, which keeps an empty cell
 * to a load and a compare; the walk of a cell that holds something is not. */
static inline void grid_ring_knn_visit_(grid_ring_knn* s, int32_t index, float gap2,
                                        grid_ring_knn_search_* sr) {
  const uint32_t cell = (uint32_t)index;
  const bool inside = cell < s->cell_count;
  EntityId id = s->base.head.data[inside ? cell : 0u];
  id = inside ? id : GDS_NO_ENTITY;
  if (id != GDS_NO_ENTITY) grid_ring_knn_walk_(s, id, gap2, sr);
}

/* The up to eight sign reflections of one octant step, unrolled. Reflections
 * that leave the grid, and the minus reflection of a zero component (which is
 * the plus one again), carry GRID_RING_KNN_OUTSIDE and are rejected inside
 * visit without a branch of their own: a walk whose branches follow the table
 * rather than the data lets the cell loads overlap. */
static inline void grid_ring_knn_visit_step_(grid_ring_knn* s, grid_ring_knn_step_ st,
                                             const grid_ring_knn_lookup_* lx,
                                             const grid_ring_knn_lookup_* ly,
                                             const grid_ring_knn_lookup_* lz, int32_t base,
                                             grid_ring_knn_search_* sr) {
  const int px = GRID_RING_KNN_REACH + st.x, mx = GRID_RING_KNN_REACH - st.x;
  const int py = GRID_RING_KNN_REACH + st.y, my = GRID_RING_KNN_REACH - st.y;
  const int pz = GRID_RING_KNN_REACH + st.z, mz = GRID_RING_KNN_REACH - st.z;
  const int32_t xp = lx->step[px], xm = st.x ? lx->step[mx] : GRID_RING_KNN_OUTSIDE;
  const int32_t yp = ly->step[py], ym = st.y ? ly->step[my] : GRID_RING_KNN_OUTSIDE;
  const int32_t zp = lz->step[pz], zm = st.z ? lz->step[mz] : GRID_RING_KNN_OUTSIDE;
  const float gxp = lx->gap2[px], gxm = lx->gap2[mx];
  const float gpp = ly->gap2[py] + lz->gap2[pz], gmp = ly->gap2[my] + lz->gap2[pz];
  const float gpm = ly->gap2[py] + lz->gap2[mz], gmm = ly->gap2[my] + lz->gap2[mz];
  grid_ring_knn_visit_(s, base + xp + yp + zp, gxp + gpp, sr);
  grid_ring_knn_visit_(s, base + xm + yp + zp, gxm + gpp, sr);
  grid_ring_knn_visit_(s, base + xp + ym + zp, gxp + gmp, sr);
  grid_ring_knn_visit_(s, base + xm + ym + zp, gxm + gmp, sr);
  grid_ring_knn_visit_(s, base + xp + yp + zm, gxp + gpm, sr);
  grid_ring_knn_visit_(s, base + xm + yp + zm, gxm + gpm, sr);
  grid_ring_knn_visit_(s, base + xp + ym + zm, gxp + gmm, sr);
  grid_ring_knn_visit_(s, base + xm + ym + zm, gxm + gmm, sr);
}

static inline uint64_t grid_ring_knn_finish_(grid_ring_knn* s) {
  gds_neighbour_sort_heap(s->heap.data, s->heap.size);
  KnnDigest d = gds_knn_digest();
  for (size_t i = 0; i < s->heap.size; ++i) {
    const Neighbour nb = s->heap.data[i];
    gds_knn_push(&d, nb.id, s->base.pos.data[nb.id]);
  }
  return d.acc;
}

static inline uint64_t grid_ring_knn_query_knn(grid_ring_knn* s, Vec3 c, uint32_t k) {
  const uniform_grid* g = &s->base;
  if (k == 0 || g->live_count == 0) return gds_knn_digest().acc;

  const grid_ring_knn_axis_ ax = grid_ring_knn_axis_frame_(s, c.x, g->bounds.min.x, g->nx);
  const grid_ring_knn_axis_ ay = grid_ring_knn_axis_frame_(s, c.y, g->bounds.min.y, g->ny);
  const grid_ring_knn_axis_ az = grid_ring_knn_axis_frame_(s, c.z, g->bounds.min.z, g->nz);
  grid_ring_knn_lookup_ lx, ly, lz;
  int filled = -1;
  const int32_t base =
      (int32_t)(((uint32_t)az.i * g->ny + (uint32_t)ay.i) * g->nx + (uint32_t)ax.i);

  gds_vec_clear(s->heap);
  const size_t want = g->live_count < (size_t)k ? g->live_count : (size_t)k;
  if (s->heap.cap < want) gds_vec_reserve(s->heap, want);
  grid_ring_knn_search_ sr = {c, k, 0, GRID_RING_KNN_FAR};

  const float cell = g->cell;
  const grid_ring_knn_step_* const order = s->order;
  uint32_t e = 0;
  for (size_t si = 0; si < GRID_RING_KNN_SHELLS; ++si) {
    const grid_ring_knn_shell_* shell = &grid_ring_knn_k_octant_.shells[si];
    /* Strictly greater: an unvisited entity at exactly the k-th best dist2
     * with a smaller id would displace the current k-th, so a shell whose
     * bound equals that distance still has to be walked. */
    if (grid_ring_knn_squared_(shell->root * cell) > sr.worst) return grid_ring_knn_finish_(s);
    if (shell->reach > filled) {
      grid_ring_knn_extend_(s, &lx, &ax, 1, filled + 1, shell->reach);
      grid_ring_knn_extend_(s, &ly, &ay, (int32_t)g->nx, filled + 1, shell->reach);
      grid_ring_knn_extend_(s, &lz, &az, (int32_t)(g->nx * g->ny), filled + 1, shell->reach);
      filled = shell->reach;
    }
    /* A step longer than the grid on some axis has no reflection inside it.
     * Skipping it is only a saving, since its reflections would all be
     * rejected anyway, and only a shell that can hold one tests for it. The
     * first step whose lead is past the grid ends the shell, because the
     * rest of the shell's leads are larger still; the other axes are tested
     * a step at a time. One call site rather than a loop per case keeps
     * visit_step inlined. */
    const bool clip = shell->reach > s->fit;
    for (; e < shell->end; ++e) {
      const grid_ring_knn_step_ st = order[e];
      if (clip) {
        if (st.lead > s->lead) {
          e = shell->end;
          break;
        }
        if (st.x > s->ex || st.y > s->ey || st.z > s->ez) continue;
      }
      grid_ring_knn_visit_step_(s, st, &lx, &ly, &lz, base, &sr);
    }
    if (sr.seen == g->live_count) return grid_ring_knn_finish_(s);
  }
  if (s->covers_grid || s->tail_bound2 > sr.worst) return grid_ring_knn_finish_(s);
  /* The k-th neighbour may lie beyond the table. The parent's widening
   * search answers from scratch rather than this one extending, so the answer
   * is then the parent's, exact wherever the parent's is: its stop test
   * squares a half-width, and once that overflows float (worlds wider than
   * about 1.8e19) neither search is exact. */
  return uniform_grid_query_knn(&s->base, c, k);
}

static inline void grid_ring_knn_end_tick(grid_ring_knn* s, uint64_t tick) {
  uniform_grid_end_tick(&s->base, tick);
}

static inline bool grid_ring_knn_rewind_to(grid_ring_knn* s, uint64_t tick) {
  return uniform_grid_rewind_to(&s->base, tick);
}

static inline size_t grid_ring_knn_entity_count(grid_ring_knn* s) {
  return uniform_grid_entity_count(&s->base);
}

/* The table is static data, not a heap allocation, and the same for every
 * instance; it is counted all the same, because the search cannot work
 * without it. */
static inline size_t grid_ring_knn_reported_bytes(grid_ring_knn* s) {
  return uniform_grid_reported_bytes(&s->base) + sizeof(grid_ring_knn_octant_) +
         gds_vec_bytes(s->heap);
}

#endif
