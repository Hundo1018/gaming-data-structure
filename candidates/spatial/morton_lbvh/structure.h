#ifndef CANDIDATE_SPATIAL_MORTON_LBVH_H
#define CANDIDATE_SPATIAL_MORTON_LBVH_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gds/spatial/knn.h"
#include "gds/spatial/types.h"
#include "gds/vec.h"

/* A leaf is sixteen entities because sixteen floats are one 64-byte cache
 * line: a leaf is one whole line of each of the x, y, z and id arrays, and
 * the distance loop over it is two 256-bit vectors per axis with no
 * remainder. */
#define MORTON_LBVH_LEAF ((uint32_t)16)
/* A node has eight children because eight floats are one 256-bit vector, so
 * the boxes of all of a node's children are tested by one pass of vector
 * arithmetic over six rows of eight floats, one 192-byte group of three
 * cache lines. */
#define MORTON_LBVH_FAN ((uint32_t)8)

/* A node count fits in 28 bits (2^32 ids over leaves of 16), and each level
 * divides it by eight, so no tree has more than eleven levels. */
#define MORTON_LBVH_MAX_LEVELS ((uint32_t)16)
#define MORTON_LBVH_INF INFINITY
#define MORTON_LBVH_NAN NAN

/* One cache line: the alignment of a line and of a box group. */
#define MORTON_LBVH_LINE_ALIGN 64

/* Sixteen consecutive values of one sorted array: one 64-byte line, with no
 * padding, so a vector of lines is a plain array of floats (or ids) in sorted
 * order whose every sixteenth element starts a cache line. Slot i of the
 * sorted order is element i % MORTON_LBVH_LEAF of line i / MORTON_LBVH_LEAF,
 * and leaf j is line j of each of the four arrays. A slot past the end of the
 * population holds NaN coordinates, which fail every <= test, so the distance
 * loop never needs to know how full the last leaf is. */
typedef struct {
  _Alignas(MORTON_LBVH_LINE_ALIGN) float v[MORTON_LBVH_LEAF];
} morton_lbvh_float_line_;
typedef struct {
  _Alignas(MORTON_LBVH_LINE_ALIGN) EntityId v[MORTON_LBVH_LEAF];
} morton_lbvh_id_line_;
_Static_assert(sizeof(morton_lbvh_float_line_) == MORTON_LBVH_LEAF * sizeof(float),
               "a line is its sixteen values");
_Static_assert(sizeof(morton_lbvh_id_line_) == MORTON_LBVH_LEAF * sizeof(EntityId),
               "a line is its sixteen values");

/* The boxes of MORTON_LBVH_FAN sibling nodes, one field per 32-byte row. A
 * lane past the end of its level holds an empty box, +inf to -inf, so a
 * parent's box is the union of all eight lanes without asking which are
 * real. */
typedef struct {
  _Alignas(MORTON_LBVH_LINE_ALIGN) float lo_x[MORTON_LBVH_FAN];
  float lo_y[MORTON_LBVH_FAN];
  float lo_z[MORTON_LBVH_FAN];
  float hi_x[MORTON_LBVH_FAN];
  float hi_y[MORTON_LBVH_FAN];
  float hi_z[MORTON_LBVH_FAN];
} morton_lbvh_box_group_;

typedef struct {
  float d2;
  uint32_t level;
  uint32_t index;
} morton_lbvh_pending_;

static inline bool morton_lbvh_farther_(morton_lbvh_pending_ a, morton_lbvh_pending_ b) {
  return a.d2 > b.d2;
}

/* morton_lbvh_pending_push_heap and _pop_heap: the frontier, a heap under
 * farther, so its top is the nearest pending node. */
#define GDS_SORT_NAME morton_lbvh_pending
#define GDS_SORT_T morton_lbvh_pending_
#define GDS_SORT_LESS(a, b) morton_lbvh_farther_((a), (b))
#include "gds/sort.inc.h"

/* morton_sorted's order, with its lookup replaced by a hierarchy of bounding
 * boxes built over that order. Positions are written into flat arrays as they
 * change, and the searchable form is thrown away and rebuilt by the first query
 * that follows any change, as in the parent.
 *
 * The rebuild sorts live entities by a 30-bit Morton code with a radix sort,
 * gathers ids and positions into that order as four parallel arrays, and
 * groups the result bottom-up: a leaf is a run of MORTON_LBVH_LEAF consecutive
 * entities, an internal node is a run of MORTON_LBVH_FAN consecutive nodes of
 * the level below, and every node keeps the box of what lies beneath it.
 * Morton order is spatially coherent, so a consecutive run is a compact
 * region, and a run of sixteen entities is small where the world is crowded
 * and large where it is empty. The hierarchy adapts to density with nothing
 * allocated per unit of world volume, where a grid's cell array is sized by
 * the world.
 *
 * The code quantises each axis to 1024 steps of its own extent, so its cells,
 * and on average the regions a run covers, have the proportions of the world:
 * in a world four times wider than it is tall they are four times longer in x
 * and y than in z.
 *
 * Nothing about a node is stored except its box. Its entity range is implied
 * by its position: node j of level l covers entities [j*LEAF*FAN^l,
 * (j+1)*LEAF*FAN^l), clipped to the population, and its children are nodes
 * [j*FAN, (j+1)*FAN) of level l-1.
 *
 * Nothing of the parent is reused by including it: its members were private
 * in the C++ original, and its code is also a different one, 21 bits of cell
 * index per axis where this one is 10 bits of position per axis. */
typedef struct {
  Bounds bounds;
  float scale_x;
  float scale_y;
  float scale_z;
  GDS_VEC(Vec3) pos;
  GDS_VEC(uint8_t) live;
  size_t live_count;

  bool dirty;
  GDS_VEC(uint64_t) keys;
  GDS_VEC(uint64_t) spare;
  GDS_VEC(morton_lbvh_float_line_) sx;
  GDS_VEC(morton_lbvh_float_line_) sy;
  GDS_VEC(morton_lbvh_float_line_) sz;
  GDS_VEC(morton_lbvh_id_line_) sid;
  GDS_VEC(morton_lbvh_box_group_) boxes;
  uint32_t level_count[MORTON_LBVH_MAX_LEVELS];
  uint32_t level_group[MORTON_LBVH_MAX_LEVELS];
  uint32_t levels;
  GDS_VEC(morton_lbvh_pending_) frontier;
  GDS_VEC(Neighbour) best;
} morton_lbvh;

enum { morton_lbvh_native_rewind = 0 };

/* gds_vec_resize for a vector of lines or box groups. Their type is
 * over-aligned, which std::vector honoured by allocating through the aligned
 * operator new; gds_vec's blocks come from malloc, which promises only 16
 * bytes, and the compiler is entitled to use aligned vector loads on these
 * types. So this is gds_vec_resize step for step, std::vector's growth and a
 * zero-filled tail, with the block taken from aligned_alloc. The allocation
 * tracker counts the bytes asked for, as the C++ one did, so the figures are
 * unaffected by the alignment. */
static inline void* morton_lbvh_regrow_aligned_(void* data, size_t keep, size_t new_cap,
                                                size_t elem) {
  void* fresh = new_cap ? aligned_alloc(MORTON_LBVH_LINE_ALIGN, new_cap * elem) : NULL;
  if (!fresh && new_cap) abort();
  if (keep) memcpy(fresh, data, keep * elem);
  free(data);
  return fresh;
}

#define MORTON_LBVH_RESIZE_ALIGNED_(v, n)                                                 \
  do {                                                                                    \
    size_t morton_lbvh_n_ = (n);                                                          \
    if (morton_lbvh_n_ > (v).cap) {                                                       \
      size_t morton_lbvh_c_ = (v).size + gds_max_size((v).size, morton_lbvh_n_ - (v).size); \
      (v).data = morton_lbvh_regrow_aligned_((v).data, (v).size, morton_lbvh_c_,          \
                                             sizeof(*(v).data));                          \
      (v).cap = morton_lbvh_c_;                                                           \
    }                                                                                     \
    if (morton_lbvh_n_ > (v).size)                                                        \
      memset((v).data + (v).size, 0, (morton_lbvh_n_ - (v).size) * sizeof(*(v).data));    \
    (v).size = morton_lbvh_n_;                                                            \
  } while (0)

/* std::max and std::min on floats as libstdc++ defines them, which decides
 * which of two equal values, or of a NaN and a number, comes out: max(a, b)
 * is (a < b) ? b : a and min(a, b) is (b < a) ? b : a. */
static inline float morton_lbvh_max_(float a, float b) { return a < b ? b : a; }
static inline float morton_lbvh_min_(float a, float b) { return b < a ? b : a; }

/* The ten low bits of x moved to every third bit. One macro per step, each
 * reading its operand twice, so that the table below can be built from the
 * same steps at compile time. */
#define MORTON_LBVH_SB0_(x) ((uint32_t)(x) & 0x000003FFu)
#define MORTON_LBVH_SB1_(x) ((MORTON_LBVH_SB0_(x) ^ (MORTON_LBVH_SB0_(x) << 16)) & 0xFF0000FFu)
#define MORTON_LBVH_SB2_(x) ((MORTON_LBVH_SB1_(x) ^ (MORTON_LBVH_SB1_(x) << 8)) & 0x0300F00Fu)
#define MORTON_LBVH_SB3_(x) ((MORTON_LBVH_SB2_(x) ^ (MORTON_LBVH_SB2_(x) << 4)) & 0x030C30C3u)
#define MORTON_LBVH_SPREAD_BITS_(x) \
  ((MORTON_LBVH_SB3_(x) ^ (MORTON_LBVH_SB3_(x) << 2)) & 0x09249249u)

#define MORTON_LBVH_T4_(i)                                                         \
  MORTON_LBVH_SPREAD_BITS_(i), MORTON_LBVH_SPREAD_BITS_((i) + 1),                  \
      MORTON_LBVH_SPREAD_BITS_((i) + 2), MORTON_LBVH_SPREAD_BITS_((i) + 3)
#define MORTON_LBVH_T16_(i) \
  MORTON_LBVH_T4_(i), MORTON_LBVH_T4_((i) + 4), MORTON_LBVH_T4_((i) + 8), MORTON_LBVH_T4_((i) + 12)
#define MORTON_LBVH_T64_(i)                                                          \
  MORTON_LBVH_T16_(i), MORTON_LBVH_T16_((i) + 16), MORTON_LBVH_T16_((i) + 32),       \
      MORTON_LBVH_T16_((i) + 48)
#define MORTON_LBVH_T256_(i)                                                         \
  MORTON_LBVH_T64_(i), MORTON_LBVH_T64_((i) + 64), MORTON_LBVH_T64_((i) + 128),      \
      MORTON_LBVH_T64_((i) + 192)

/* The rebuild computes three spreads per entity, and as shifts and masks
 * they were most of the cost of computing the codes. As a 4 KB table built
 * at compile time they are three loads that stay in L1. The table is static
 * data, not a heap allocation, and is the same for every instance. */
static inline uint32_t morton_lbvh_spread_(uint32_t x) {
  static const uint32_t table[1024] = {MORTON_LBVH_T256_(0), MORTON_LBVH_T256_(256),
                                       MORTON_LBVH_T256_(512), MORTON_LBVH_T256_(768)};
  return table[x];
}

#undef MORTON_LBVH_T4_
#undef MORTON_LBVH_T16_
#undef MORTON_LBVH_T64_
#undef MORTON_LBVH_T256_

/* A negative or NaN extent is treated as none. An extent so small that the
 * scale overflows to infinity is harmless: step() maps the product to 0 or
 * 1023, and the code only orders. */
static inline float morton_lbvh_axis_scale_(float extent) {
  return extent > 0.0f ? 1024.0f / extent : 0.0f;
}

/* The code decides only the order, never an answer: boxes are built from the
 * positions themselves. So clamping a value off either end of the world, or
 * a NaN, to a valid step changes nothing but where in the order it lands. */
static inline uint32_t morton_lbvh_step_(float v, float lo, float scale) {
  float t = (v - lo) * scale;
  t = t > 0.0f ? t : 0.0f;
  t = t < 1023.0f ? t : 1023.0f;
  return (uint32_t)t;
}

static inline uint32_t morton_lbvh_code_of_(const morton_lbvh* s, Vec3 p) {
  return morton_lbvh_spread_(morton_lbvh_step_(p.x, s->bounds.min.x, s->scale_x)) |
         (morton_lbvh_spread_(morton_lbvh_step_(p.y, s->bounds.min.y, s->scale_y)) << 1) |
         (morton_lbvh_spread_(morton_lbvh_step_(p.z, s->bounds.min.z, s->scale_z)) << 2);
}

static inline void morton_lbvh_init(morton_lbvh* s, const WorldConfig* cfg) {
  memset(s, 0, sizeof *s);
  s->bounds = cfg->bounds;
  s->dirty = true;
  /* 1024 steps over each axis's own extent of the world bounds, so every
   * axis contributes all ten of its bits to the code. A code cell therefore
   * has the world's proportions rather than a cube's. An axis with no extent
   * gets no scale and every entity lands on its step 0. */
  s->scale_x = morton_lbvh_axis_scale_(gds_bounds_extent_x(&s->bounds));
  s->scale_y = morton_lbvh_axis_scale_(gds_bounds_extent_y(&s->bounds));
  s->scale_z = morton_lbvh_axis_scale_(gds_bounds_extent_z(&s->bounds));
  const size_t n = cfg->max_entity_id + 1;
  const Vec3 zero = {0, 0, 0};
  gds_vec_assign(s->pos, n, zero);
  gds_vec_assign(s->live, n, 0);
}

static inline void morton_lbvh_free(morton_lbvh* s) {
  gds_vec_free(s->pos);
  gds_vec_free(s->live);
  gds_vec_free(s->keys);
  gds_vec_free(s->spare);
  gds_vec_free(s->sx);
  gds_vec_free(s->sy);
  gds_vec_free(s->sz);
  gds_vec_free(s->sid);
  gds_vec_free(s->boxes);
  gds_vec_free(s->frontier);
  gds_vec_free(s->best);
}

static inline void morton_lbvh_insert(morton_lbvh* s, EntityId id, Vec3 p) {
  if (!s->live.data[id]) ++s->live_count;
  s->live.data[id] = 1;
  s->pos.data[id] = p;
  s->dirty = true;
}

static inline void morton_lbvh_remove(morton_lbvh* s, EntityId id) {
  if (!s->live.data[id]) return;
  s->live.data[id] = 0;
  --s->live_count;
  s->dirty = true;
}

static inline void morton_lbvh_move_by(morton_lbvh* s, EntityId id, Vec3 delta) {
  if (!s->live.data[id]) return;
  s->pos.data[id] = gds_wrap_into(gds_add3(s->pos.data[id], delta), &s->bounds);
  s->dirty = true;
}

static inline bool morton_lbvh_position_of(morton_lbvh* s, EntityId id, Vec3* out) {
  if (id >= s->live.size || !s->live.data[id]) return false;
  *out = s->pos.data[id];
  return true;
}

/* Lower bounds on the squared distance from c to each of the eight boxes.
 *
 * Per axis the gap is max(lo - c, c - hi, 0), and the three squared gaps are
 * summed in the order gds_dist2 sums its terms. For an entity p in the box,
 * lo <= p <= hi, and rounding is monotonic, so each rounded gap is no larger
 * in magnitude than the rounded p - c that gds_dist2 computes, each square is
 * no larger, and the sum is no larger. The bound can therefore only
 * under-estimate gds_dist2(p, c) as the substrate computes it, never exceed
 * it, and pruning on it cannot drop an entity the accept test would keep. The
 * order is an association: commuting two terms gives the same float, but
 * grouping the last two first can round above gds_dist2 for a point on a
 * face. */
static inline void morton_lbvh_box_dist2_(const morton_lbvh_box_group_* g, Vec3 c, float* out) {
  for (uint32_t j = 0; j < MORTON_LBVH_FAN; ++j) {
    const float ex = morton_lbvh_max_(morton_lbvh_max_(g->lo_x[j] - c.x, c.x - g->hi_x[j]), 0.0f);
    const float ey = morton_lbvh_max_(morton_lbvh_max_(g->lo_y[j] - c.y, c.y - g->hi_y[j]), 0.0f);
    const float ez = morton_lbvh_max_(morton_lbvh_max_(g->lo_z[j] - c.z, c.z - g->hi_z[j]), 0.0f);
    out[j] = ex * ex + ey * ey + ez * ez;
  }
}

/* The accept test is gds_dist2(p, c) <= r*r for every entity, including those
 * of a leaf whose box lies wholly inside the sphere. The distances are
 * computed for the whole leaf first, a fixed-length loop the compiler
 * vectorises, and only then tested. */
static inline void morton_lbvh_leaf_dist2_(const morton_lbvh* s, uint32_t leaf, Vec3 c,
                                           float* out) {
  const morton_lbvh_float_line_* x = &s->sx.data[leaf];
  const morton_lbvh_float_line_* y = &s->sy.data[leaf];
  const morton_lbvh_float_line_* z = &s->sz.data[leaf];
  for (uint32_t j = 0; j < MORTON_LBVH_LEAF; ++j)
    out[j] = gds_dist2(gds_vec3(x->v[j], y->v[j], z->v[j]), c);
}

/* Keys are (code << 32 | id), so one 8-byte word carries both through the
 * sort. Ids are visited in ascending order and every pass is stable, so
 * equal codes stay in id order and the result is the same every time.
 *
 * Least significant digit first, three digits of ten bits: a 1024-bucket
 * histogram is 4 KB, so all three fit in L1 together and are filled in the
 * same pass that computes the codes. A digit every key shares is skipped,
 * which happens when the whole population lies inside one cell of the
 * code's coarser levels. */
static inline void morton_lbvh_sort_live_(morton_lbvh* s, uint32_t n) {
  gds_vec_resize(s->keys, n);
  gds_vec_resize(s->spare, n);
  uint32_t count[3][1024];
  memset(count, 0, sizeof count);
  uint32_t k = 0;
  for (size_t id = 0; id < s->live.size; ++id) {
    if (!s->live.data[id]) continue;
    const uint32_t code = morton_lbvh_code_of_(s, s->pos.data[id]);
    ++count[0][code & 1023u];
    ++count[1][(code >> 10) & 1023u];
    ++count[2][code >> 20];
    s->keys.data[k++] = ((uint64_t)code << 32) | (uint64_t)id;
  }
  for (uint32_t pass = 0; pass < 3; ++pass) {
    const uint32_t shift = 32 + 10 * pass;
    uint32_t* c = count[pass];
    if (c[(s->keys.data[0] >> shift) & 1023u] == n) continue;
    uint32_t total = 0;
    for (uint32_t slot = 0; slot < 1024; ++slot) {
      const uint32_t here = c[slot];
      c[slot] = total;
      total += here;
    }
    for (uint32_t i = 0; i < n; ++i) {
      const uint64_t key = s->keys.data[i];
      s->spare.data[c[(key >> shift) & 1023u]++] = key;
    }
    /* The two vectors swap, field by field: two GDS_VEC types never match. */
    uint64_t* const data = s->keys.data;
    const size_t size = s->keys.size;
    const size_t cap = s->keys.cap;
    s->keys.data = s->spare.data;
    s->keys.size = s->spare.size;
    s->keys.cap = s->spare.cap;
    s->spare.data = data;
    s->spare.size = size;
    s->spare.cap = cap;
  }
}

static inline void morton_lbvh_gather_(morton_lbvh* s, uint32_t n) {
  const uint32_t nleaves = (n + MORTON_LBVH_LEAF - 1) / MORTON_LBVH_LEAF;
  MORTON_LBVH_RESIZE_ALIGNED_(s->sx, nleaves);
  MORTON_LBVH_RESIZE_ALIGNED_(s->sy, nleaves);
  MORTON_LBVH_RESIZE_ALIGNED_(s->sz, nleaves);
  MORTON_LBVH_RESIZE_ALIGNED_(s->sid, nleaves);
  for (uint32_t i = 0; i < n; ++i) {
    const EntityId id = (EntityId)s->keys.data[i];
    const Vec3 p = s->pos.data[id];
    const uint32_t line = i / MORTON_LBVH_LEAF;
    const uint32_t j = i % MORTON_LBVH_LEAF;
    s->sx.data[line].v[j] = p.x;
    s->sy.data[line].v[j] = p.y;
    s->sz.data[line].v[j] = p.z;
    s->sid.data[line].v[j] = id;
  }
  const uint32_t last = nleaves - 1;
  for (uint32_t j = n - last * MORTON_LBVH_LEAF; j < MORTON_LBVH_LEAF; ++j) {
    s->sx.data[last].v[j] = MORTON_LBVH_NAN;
    s->sy.data[last].v[j] = MORTON_LBVH_NAN;
    s->sz.data[last].v[j] = MORTON_LBVH_NAN;
    s->sid.data[last].v[j] = GDS_NO_ENTITY;
  }
}

static inline void morton_lbvh_set_empty_(morton_lbvh_box_group_* g, uint32_t lane) {
  g->lo_x[lane] = g->lo_y[lane] = g->lo_z[lane] = MORTON_LBVH_INF;
  g->hi_x[lane] = g->hi_y[lane] = g->hi_z[lane] = -MORTON_LBVH_INF;
}

static inline void morton_lbvh_build_boxes_(morton_lbvh* s, uint32_t n) {
  /* Level 0 holds one box per leaf; each level above holds one per
   * MORTON_LBVH_FAN boxes of the level below, until a single root. */
  uint32_t count = (n + MORTON_LBVH_LEAF - 1) / MORTON_LBVH_LEAF;
  uint32_t groups = 0;
  s->levels = 0;
  for (;;) {
    s->level_count[s->levels] = count;
    s->level_group[s->levels] = groups;
    ++s->levels;
    groups += (count + MORTON_LBVH_FAN - 1) / MORTON_LBVH_FAN;
    if (count == 1) break;
    count = (count + MORTON_LBVH_FAN - 1) / MORTON_LBVH_FAN;
  }
  MORTON_LBVH_RESIZE_ALIGNED_(s->boxes, groups);
  for (uint32_t l = 0; l < s->levels; ++l) {
    morton_lbvh_box_group_* tail =
        &s->boxes.data[s->level_group[l] + (s->level_count[l] - 1) / MORTON_LBVH_FAN];
    for (uint32_t lane = (s->level_count[l] - 1) % MORTON_LBVH_FAN + 1; lane < MORTON_LBVH_FAN;
         ++lane) {
      morton_lbvh_set_empty_(tail, lane);
    }
  }

  /* Leaf boxes, over the real slots only: the padding is NaN. */
  for (uint32_t leaf = 0; leaf < s->level_count[0]; ++leaf) {
    const morton_lbvh_float_line_* x = &s->sx.data[leaf];
    const morton_lbvh_float_line_* y = &s->sy.data[leaf];
    const morton_lbvh_float_line_* z = &s->sz.data[leaf];
    const uint32_t rest = n - leaf * MORTON_LBVH_LEAF;
    const uint32_t used = rest < MORTON_LBVH_LEAF ? rest : MORTON_LBVH_LEAF;
    float lx = x->v[0], ly = y->v[0], lz = z->v[0];
    float hx = lx, hy = ly, hz = lz;
    for (uint32_t j = 1; j < used; ++j) {
      lx = morton_lbvh_min_(lx, x->v[j]);
      ly = morton_lbvh_min_(ly, y->v[j]);
      lz = morton_lbvh_min_(lz, z->v[j]);
      hx = morton_lbvh_max_(hx, x->v[j]);
      hy = morton_lbvh_max_(hy, y->v[j]);
      hz = morton_lbvh_max_(hz, z->v[j]);
    }
    morton_lbvh_box_group_* g = &s->boxes.data[s->level_group[0] + leaf / MORTON_LBVH_FAN];
    const uint32_t lane = leaf % MORTON_LBVH_FAN;
    g->lo_x[lane] = lx;
    g->lo_y[lane] = ly;
    g->lo_z[lane] = lz;
    g->hi_x[lane] = hx;
    g->hi_y[lane] = hy;
    g->hi_z[lane] = hz;
  }

  /* A node's box is the union of its children's group, all eight lanes. */
  for (uint32_t l = 1; l < s->levels; ++l) {
    for (uint32_t node = 0; node < s->level_count[l]; ++node) {
      const morton_lbvh_box_group_* kids = &s->boxes.data[s->level_group[l - 1] + node];
      float lx = kids->lo_x[0], ly = kids->lo_y[0], lz = kids->lo_z[0];
      float hx = kids->hi_x[0], hy = kids->hi_y[0], hz = kids->hi_z[0];
      for (uint32_t j = 1; j < MORTON_LBVH_FAN; ++j) {
        lx = morton_lbvh_min_(lx, kids->lo_x[j]);
        ly = morton_lbvh_min_(ly, kids->lo_y[j]);
        lz = morton_lbvh_min_(lz, kids->lo_z[j]);
        hx = morton_lbvh_max_(hx, kids->hi_x[j]);
        hy = morton_lbvh_max_(hy, kids->hi_y[j]);
        hz = morton_lbvh_max_(hz, kids->hi_z[j]);
      }
      morton_lbvh_box_group_* g = &s->boxes.data[s->level_group[l] + node / MORTON_LBVH_FAN];
      const uint32_t lane = node % MORTON_LBVH_FAN;
      g->lo_x[lane] = lx;
      g->lo_y[lane] = ly;
      g->lo_z[lane] = lz;
      g->hi_x[lane] = hx;
      g->hi_y[lane] = hy;
      g->hi_z[lane] = hz;
    }
  }
}

static inline void morton_lbvh_rebuild_if_needed_(morton_lbvh* s) {
  if (!s->dirty) return;
  s->dirty = false;
  const uint32_t n = (uint32_t)s->live_count;
  s->levels = 0;
  if (n == 0) return;
  morton_lbvh_sort_live_(s, n);
  morton_lbvh_gather_(s, n);
  morton_lbvh_build_boxes_(s, n);
}

static inline void morton_lbvh_scan_leaf_radius_(const morton_lbvh* s, uint32_t leaf, Vec3 c,
                                                 float r2, RadiusDigest* d) {
  const morton_lbvh_float_line_* x = &s->sx.data[leaf];
  const morton_lbvh_float_line_* y = &s->sy.data[leaf];
  const morton_lbvh_float_line_* z = &s->sz.data[leaf];
  const morton_lbvh_id_line_* ids = &s->sid.data[leaf];
  float d2[MORTON_LBVH_LEAF];
  morton_lbvh_leaf_dist2_(s, leaf, c, d2);
  for (uint32_t j = 0; j < MORTON_LBVH_LEAF; ++j) {
    if (d2[j] <= r2) gds_radius_hit(d, ids->v[j], gds_vec3(x->v[j], y->v[j], z->v[j]));
  }
}

static inline void morton_lbvh_collect_radius_(morton_lbvh* s, Vec3 c, float r, RadiusDigest* d) {
  morton_lbvh_rebuild_if_needed_(s);
  if (s->live_count == 0) return;
  const float r2 = r * r;
  typedef struct {
    uint32_t level;
    uint32_t index;
  } morton_lbvh_visit_;
  /* Depth first. Each expanded node leaves at most MORTON_LBVH_FAN - 1
   * siblings waiting per level, so the stack never exceeds MORTON_LBVH_FAN
   * entries per level. */
  morton_lbvh_visit_ stack[MORTON_LBVH_FAN * MORTON_LBVH_MAX_LEVELS];
  size_t sp = 0;
  if (s->levels == 1) {
    morton_lbvh_scan_leaf_radius_(s, 0, c, r2, d);
    return;
  }
  const morton_lbvh_visit_ root = {s->levels - 1, 0};
  stack[sp++] = root;
  while (sp > 0) {
    const morton_lbvh_visit_ at = stack[--sp];
    const uint32_t below = at.level - 1;
    const uint32_t first = at.index * MORTON_LBVH_FAN;
    const uint32_t rest = s->level_count[below] - first;
    const uint32_t valid = rest < MORTON_LBVH_FAN ? rest : MORTON_LBVH_FAN;
    float bd[MORTON_LBVH_FAN];
    morton_lbvh_box_dist2_(&s->boxes.data[s->level_group[below] + at.index], c, bd);
    for (uint32_t j = 0; j < valid; ++j) {
      if (bd[j] > r2) continue;
      if (below == 0) {
        morton_lbvh_scan_leaf_radius_(s, first + j, c, r2, d);
      } else {
        const morton_lbvh_visit_ child = {below, first + j};
        stack[sp++] = child;
      }
    }
  }
}

/* s->best is a max-heap under gds_nearer, so its front is the kth best so
 * far. */
static inline void morton_lbvh_scan_leaf_knn_(morton_lbvh* s, uint32_t leaf, Vec3 c, size_t want) {
  const morton_lbvh_id_line_* ids = &s->sid.data[leaf];
  float d2[MORTON_LBVH_LEAF];
  morton_lbvh_leaf_dist2_(s, leaf, c, d2);
  float limit = s->best.size == want ? s->best.data[0].d2 : MORTON_LBVH_INF;
  for (uint32_t j = 0; j < MORTON_LBVH_LEAF; ++j) {
    /* Equal distance still competes on id; NaN padding fails here. */
    if (!(d2[j] <= limit)) continue;
    const Neighbour nb = {d2[j], ids->v[j]};
    if (s->best.size < want) {
      gds_vec_push(s->best, nb);
      gds_neighbour_push_heap(s->best.data, s->best.size);
    } else if (gds_nearer(nb, s->best.data[0])) {
      gds_neighbour_pop_heap(s->best.data, s->best.size);
      gds_vec_back(s->best) = nb;
      gds_neighbour_push_heap(s->best.data, s->best.size);
    } else {
      continue;
    }
    if (s->best.size == want) limit = s->best.data[0].d2;
  }
}

static inline uint64_t morton_lbvh_query_radius(morton_lbvh* s, Vec3 c, float r) {
  RadiusDigest d = gds_radius_digest(c, r);
  morton_lbvh_collect_radius_(s, c, r, &d);
  return d.acc;
}

static inline uint64_t morton_lbvh_query_radius_of(morton_lbvh* s, EntityId self, float r) {
  if (self >= s->live.size || !s->live.data[self]) return 0;
  const Vec3 p = s->pos.data[self];
  RadiusDigest d = gds_radius_digest(p, r);
  morton_lbvh_collect_radius_(s, p, r, &d);
  /* The scan accepted the centre exactly when this same test passes, and the
   * digest is a sum, so removing it is a subtraction rather than a branch on
   * every hit. */
  if (gds_dist2(p, p) <= r * r) return d.acc - gds_radius_term(&d, self, p);
  return d.acc;
}

/* Best-first: nodes are expanded in order of their distance from c, and the
 * search stops when the nearest unexpanded node is strictly farther than the
 * kth best found. A node at exactly that distance can still hold an entity
 * that ties on distance and wins on id, so it is expanded. */
static inline uint64_t morton_lbvh_query_knn(morton_lbvh* s, Vec3 c, uint32_t k) {
  morton_lbvh_rebuild_if_needed_(s);
  KnnDigest d = gds_knn_digest();
  if (k == 0 || s->live_count == 0) return d.acc;
  const size_t want = s->live_count < (size_t)k ? s->live_count : (size_t)k;
  gds_vec_clear(s->best);
  gds_vec_clear(s->frontier);
  const morton_lbvh_pending_ root = {0.0f, s->levels - 1, 0};
  gds_vec_push(s->frontier, root);
  while (s->frontier.size != 0) {
    morton_lbvh_pending_pop_heap(s->frontier.data, s->frontier.size);
    const morton_lbvh_pending_ at = gds_vec_back(s->frontier);
    (void)gds_vec_pop(s->frontier);
    if (s->best.size == want && at.d2 > s->best.data[0].d2) break;
    if (at.level == 0) {
      morton_lbvh_scan_leaf_knn_(s, at.index, c, want);
      continue;
    }
    const uint32_t below = at.level - 1;
    const uint32_t first = at.index * MORTON_LBVH_FAN;
    const uint32_t rest = s->level_count[below] - first;
    const uint32_t valid = rest < MORTON_LBVH_FAN ? rest : MORTON_LBVH_FAN;
    float bd[MORTON_LBVH_FAN];
    morton_lbvh_box_dist2_(&s->boxes.data[s->level_group[below] + at.index], c, bd);
    for (uint32_t j = 0; j < valid; ++j) {
      if (s->best.size == want && bd[j] > s->best.data[0].d2) continue;
      const morton_lbvh_pending_ next = {bd[j], below, first + j};
      gds_vec_push(s->frontier, next);
      morton_lbvh_pending_push_heap(s->frontier.data, s->frontier.size);
    }
  }
  gds_neighbour_sort_heap(s->best.data, s->best.size);
  for (size_t i = 0; i < s->best.size; ++i)
    gds_knn_push(&d, s->best.data[i].id, s->pos.data[s->best.data[i].id]);
  return d.acc;
}

static inline void morton_lbvh_end_tick(morton_lbvh* s, uint64_t tick) {
  (void)s;
  (void)tick;
}

static inline bool morton_lbvh_rewind_to(morton_lbvh* s, uint64_t tick) {
  (void)s;
  (void)tick;
  return false;
}

static inline size_t morton_lbvh_entity_count(morton_lbvh* s) { return s->live_count; }

static inline size_t morton_lbvh_reported_bytes(morton_lbvh* s) {
  return gds_vec_bytes(s->pos) + s->live.cap + gds_vec_bytes(s->keys) + gds_vec_bytes(s->spare) +
         (s->sx.cap + s->sy.cap + s->sz.cap) * sizeof(morton_lbvh_float_line_) +
         gds_vec_bytes(s->sid) + gds_vec_bytes(s->boxes) + gds_vec_bytes(s->frontier) +
         gds_vec_bytes(s->best);
}

#endif
