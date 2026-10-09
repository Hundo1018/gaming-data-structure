#ifndef CANDIDATE_SPATIAL_DELTA_GRID_H
#define CANDIDATE_SPATIAL_DELTA_GRID_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gds/spatial/knn.h"
#include "gds/spatial/types.h"
#include "gds/vec.h"
#include "spatial/uniform_grid/structure.h"

/* A grid that keeps its own history as data and keeps its index derived.
 *
 * Two parents, each contributing one half. From grid_undo_log, the record of
 * what changed: the first change to an entity within a tick saves its previous
 * liveness and position. From cell_sorted, the index: a counting sort into
 * contiguous per-cell ranges with a directory. What is new is that neither
 * choice those parents made for every tick is made here in advance.
 *
 * History. A closed frame is kept either as its records or, when the records
 * pass an eighth of the id space, as a full pre-image of the arrays as they were
 * when the tick began. Unwinding writes the positions and liveness arrays and
 * nothing else: the index is never touched while history is replayed, which is
 * the cost that made grid_undo_log lose when everything moves (a remove and an
 * insert into the grid per record).
 *
 * Index. Every change, and every record unwound, lists its id as dirty. The
 * first query after any change either rebuilds the whole index by counting sort
 * or, when few ids are listed, moves just those into a delta layer: a held
 * uniform_grid's intrusive per-cell lists, over the same cells, holding exactly
 * the entities changed since the last rebuild. An entity in the delta is
 * tombstoned in the base, so every live entity is in exactly one of the two and
 * its stored position there is its current one.
 *
 * The held grid supplies the cell geometry, the lists and the authoritative
 * positions and liveness; none of its mutators are used, and the lists hold
 * only the delta. */

/* See hypothesis.md for the cost argument behind both fractions.
 *
 * DELTA_GRID_ALPHA: the first query after changes rebuilds when the listed ids
 * exceed this share of the live population, and otherwise moves them into the
 * delta. */
#define DELTA_GRID_ALPHA 0.5
/* DELTA_GRID_BETA: a tick whose records exceed this share of the id space is
 * kept as a pre-image instead. */
#define DELTA_GRID_BETA 0.125

/* cell_sorted's block width and padding, for the same reason: GCC's -O2 cost
 * model vectorises the distance test only over a trip count that is a known
 * multiple of the vector width, and a block starting at the last entity reads
 * this far past it. */
#define DELTA_GRID_LANES ((size_t)8)
#define DELTA_GRID_PAD (DELTA_GRID_LANES - 1)
#define DELTA_GRID_NO_SLOT (~0u)

/* The query box is widened on each side by this share of |c| + r, and by
 * DELTA_GRID_BOX_FLOOR, so that it holds every point the shared test accepts.
 * Computing c - r and c + r rounds them by up to one ulp of |c| + r, which can
 * move a bound onto a cell boundary past a point dist2 accepts at exactly r*r,
 * and dist2 itself, rounded, accepts points up to about three roundings of r
 * beyond r. A millionth is about seventeen float roundings, so it covers both
 * with margin, and it moves a bound across a cell boundary only when the bound
 * lies within a millionth of |c| + r of that boundary. The floor covers a
 * squared difference that underflows to zero, which happens below 2^-75. */
#define DELTA_GRID_BOX_SLACK 1e-6f
#define DELTA_GRID_BOX_FLOOR 1e-20f

/* A record packs the previous liveness into the top bit of the id. Ids are
 * dense array indices, and an id space of 2^31 would need tens of gigabytes of
 * per-id arrays before this mattered. */
#define DELTA_GRID_LIVE_BIT (1u << 31)

typedef struct {
  uint32_t key;
  Vec3 pos;
} delta_grid_record_;

/* A frame's three vectors, named so that end_tick can swap them with the open
 * frame's as std::swap did: the structs are exchanged, no buffer moves. */
typedef GDS_VEC(delta_grid_record_) delta_grid_records_;
typedef GDS_VEC(Vec3) delta_grid_positions_;
typedef GDS_VEC(uint8_t) delta_grid_liveness_;

/* A frame is the history of one tick: its records, or, when `full`, the
 * positions and liveness as the tick began and the live count then. A slot
 * keeps the buffers of both kinds once it has used them. */
typedef struct {
  uint64_t tick;
  bool full;
  size_t live_count;
  delta_grid_records_ rec;
  delta_grid_positions_ pos;
  delta_grid_liveness_ live;
} delta_grid_frame_;

/* What the k-nearest gather collects into, named so that the out-of-line walks
 * below can take it by pointer. */
typedef GDS_VEC(Neighbour) delta_grid_neighbours_;

typedef struct {
  /* The geometry, the authoritative arrays and the delta lists.
   *
   * uniform_grid, held rather than embedded as a base. Its lists are derived
   * state here, which the first query after a change repairs like the rest of
   * the index. The C++ held it as a mutable member so that its const queries
   * could do that, and opened its protected members by friendship; in C every
   * operation takes a non-const pointer and the grid's fields are open, so
   * neither is needed. */
  uniform_grid grid;

  /* Base: cell_sorted's directory and cell-ordered arrays, plus where each id
   * sits in them. */
  GDS_VEC(uint32_t) start;
  GDS_VEC(EntityId) sorted_id;
  GDS_VEC(float) sorted_x;
  GDS_VEC(float) sorted_y;
  GDS_VEC(float) sorted_z;
  GDS_VEC(uint32_t) slot_of;
  /* Delta: the held grid's lists; this counts their members. */
  size_t delta_size;

  /* Ids changed since the last query, the first dirty_count entries of
   * dirty, and whether the next query must rebuild regardless. */
  GDS_VEC(EntityId) dirty;
  size_t dirty_count;
  GDS_VEC(uint32_t) listed;
  uint32_t pass;
  size_t dirty_limit;
  bool full;

  /* History: up to keep closed frames in a ring starting at oldest, and the
   * open one. Without history, the tick last closed and whether anything has
   * changed since. */
  bool keeping;
  size_t keep;
  GDS_VEC(uint32_t) stamp;
  uint32_t epoch;
  size_t record_limit;
  delta_grid_frame_ open;
  GDS_VEC(delta_grid_frame_) ring;
  size_t oldest;
  size_t count;
  uint64_t closed;
  bool has_closed;
  bool changed;
} delta_grid;

enum { delta_grid_native_rewind = 1 };

static inline void delta_grid_init(delta_grid* s, const WorldConfig* cfg) {
  memset(s, 0, sizeof *s);
  uniform_grid_init(&s->grid, cfg);
  s->pass = 1;
  s->epoch = 1;
  s->keeping = cfg->history_ticks > 0;
  s->keep = (size_t)cfg->history_ticks + 1;
  const size_t n = s->grid.pos.size;
  gds_vec_assign(s->start, s->grid.head.size + 1, 0u);
  gds_vec_assign(s->sorted_id, DELTA_GRID_PAD, GDS_NO_ENTITY);
  gds_vec_assign(s->sorted_x, DELTA_GRID_PAD, 0.0f);
  gds_vec_assign(s->sorted_y, DELTA_GRID_PAD, 0.0f);
  gds_vec_assign(s->sorted_z, DELTA_GRID_PAD, 0.0f);
  gds_vec_assign(s->slot_of, n, DELTA_GRID_NO_SLOT);
  gds_vec_assign(s->listed, n, 0u);
  s->dirty_limit = (size_t)(DELTA_GRID_ALPHA * (double)n);
  gds_vec_assign(s->dirty, s->dirty_limit + 1, GDS_NO_ENTITY);
  if (s->keeping) {
    gds_vec_assign(s->stamp, n, 0u);
    s->record_limit = (size_t)(DELTA_GRID_BETA * (double)n);
  }
}

static inline void delta_grid_frame_free_(delta_grid_frame_* f) {
  gds_vec_free(f->rec);
  gds_vec_free(f->pos);
  gds_vec_free(f->live);
}

static inline void delta_grid_free(delta_grid* s) {
  uniform_grid_free(&s->grid);
  gds_vec_free(s->start);
  gds_vec_free(s->sorted_id);
  gds_vec_free(s->sorted_x);
  gds_vec_free(s->sorted_y);
  gds_vec_free(s->sorted_z);
  gds_vec_free(s->slot_of);
  gds_vec_free(s->dirty);
  gds_vec_free(s->listed);
  gds_vec_free(s->stamp);
  delta_grid_frame_free_(&s->open);
  for (size_t i = 0; i < s->ring.size; ++i) delta_grid_frame_free_(&s->ring.data[i]);
  gds_vec_free(s->ring);
}

static inline size_t delta_grid_frame_bytes_(const delta_grid_frame_* f) {
  return gds_vec_bytes(f->rec) + gds_vec_bytes(f->pos) + gds_vec_bytes(f->live);
}

static inline delta_grid_frame_* delta_grid_frame_at_(delta_grid* s, size_t j) {
  return &s->ring.data[(s->oldest + j) % s->ring.size];
}

/* Called only while the ring is short of keep slots and every slot holds a
 * retained frame. The oldest frame is then still in slot 0, because the ring
 * turns only once it is full, so appending a slot keeps the frames in order.
 * Capacity is grown towards keep but never past it. */
static inline void delta_grid_grow_ring_(delta_grid* s) {
  if (s->ring.size == s->ring.cap) {
    gds_vec_reserve(s->ring, gds_min_size(s->keep, gds_max_size(4, 2 * s->ring.size)));
  }
  delta_grid_frame_ empty;
  memset(&empty, 0, sizeof empty);
  gds_vec_push(s->ring, empty);
}

/* Records only grow within a tick, so a tick that will end with more than the
 * limit can be turned into a pre-image the moment it crosses it, and the frame
 * is the same one end_tick would have built. Copy the arrays as they are now
 * and apply the records backwards onto the copy. */
static inline void delta_grid_take_pre_image_(delta_grid* s) {
  delta_grid_frame_* open = &s->open;
  gds_vec_assign_from(open->pos, s->grid.pos);
  gds_vec_assign_from(open->live, s->grid.live);
  size_t lc = s->grid.live_count;
  for (size_t i = open->rec.size; i-- > 0;) {
    const delta_grid_record_ rr = open->rec.data[i];
    const EntityId id = rr.key & ~DELTA_GRID_LIVE_BIT;
    const uint8_t was = (rr.key & DELTA_GRID_LIVE_BIT) ? 1 : 0;
    if (open->live.data[id] && !was) --lc;
    if (!open->live.data[id] && was) ++lc;
    open->live.data[id] = was;
    open->pos.data[id] = rr.pos;
  }
  open->live_count = lc;
  gds_vec_clear(open->rec);
  open->full = true;
}

/* Only the first change to an entity within a tick is recorded: the state to
 * restore is the one at the tick boundary. Once the open tick has become a
 * pre-image nothing further is recorded, because the pre-image already holds
 * every entity as the tick began. With no history kept, a change only clears
 * the way back to the tick just closed. */
static inline void delta_grid_note_(delta_grid* s, EntityId id) {
  if (!s->keeping) {
    s->changed = true;
    return;
  }
  if (s->open.full || s->stamp.data[id] == s->epoch) return;
  s->stamp.data[id] = s->epoch;
  const delta_grid_record_ rec = {id | (s->grid.live.data[id] ? DELTA_GRID_LIVE_BIT : 0u),
                                  s->grid.pos.data[id]};
  gds_vec_push(s->open.rec, rec);
  if (s->open.rec.size > s->record_limit) delta_grid_take_pre_image_(s);
}

/* An id is listed once per pass between two queries; `listed` holds the pass
 * it was last listed in, so starting a new pass clears every mark at once.
 * Past the limit, the list exceeds DELTA_GRID_ALPHA of every id slot, and so of
 * the live population whatever happens before the next query: that query will
 * rebuild, and listing more ids would be wasted. The list therefore never holds
 * more than limit + 1 ids, and is a fixed array of that many with a count,
 * which keeps this function small enough to be inlined into every mutation. */
static inline void delta_grid_mark_dirty_(delta_grid* s, EntityId id) {
  if (s->full || s->listed.data[id] == s->pass) return;
  s->listed.data[id] = s->pass;
  s->dirty.data[s->dirty_count++] = id;
  if (s->dirty_count > s->dirty_limit) s->full = true;
}

static inline void delta_grid_undo_records_(delta_grid* s, const delta_grid_frame_* f) {
  for (size_t i = f->rec.size; i-- > 0;) {
    const delta_grid_record_ rr = f->rec.data[i];
    const EntityId id = rr.key & ~DELTA_GRID_LIVE_BIT;
    const uint8_t was = (rr.key & DELTA_GRID_LIVE_BIT) ? 1 : 0;
    if (s->grid.live.data[id] && !was) --s->grid.live_count;
    if (!s->grid.live.data[id] && was) ++s->grid.live_count;
    s->grid.live.data[id] = was;
    s->grid.pos.data[id] = rr.pos;
    delta_grid_mark_dirty_(s, id);
  }
}

/* Copying a pre-image changes an unknown share of the world, so it lists
 * nothing and asks for a rebuild. */
static inline void delta_grid_restore_image_(delta_grid* s, const delta_grid_frame_* f) {
  memcpy(s->grid.pos.data, f->pos.data, f->pos.size * sizeof *f->pos.data);
  memcpy(s->grid.live.data, f->live.data, f->live.size * sizeof *f->live.data);
  s->grid.live_count = f->live_count;
  s->full = true;
}

static inline void delta_grid_insert(delta_grid* s, EntityId id, Vec3 p) {
  delta_grid_note_(s, id);
  if (!s->grid.live.data[id]) ++s->grid.live_count;
  s->grid.live.data[id] = 1;
  s->grid.pos.data[id] = p;
  delta_grid_mark_dirty_(s, id);
}

/* Removing or moving a dead id changes nothing, so it is neither recorded nor
 * listed. */
static inline void delta_grid_remove(delta_grid* s, EntityId id) {
  if (!s->grid.live.data[id]) return;
  delta_grid_note_(s, id);
  s->grid.live.data[id] = 0;
  --s->grid.live_count;
  delta_grid_mark_dirty_(s, id);
}

static inline void delta_grid_move_by(delta_grid* s, EntityId id, Vec3 delta) {
  if (!s->grid.live.data[id]) return;
  delta_grid_note_(s, id);
  s->grid.pos.data[id] = gds_wrap_into(gds_add3(s->grid.pos.data[id], delta), &s->grid.bounds);
  delta_grid_mark_dirty_(s, id);
}

/* These read the authoritative arrays and need no index. */
static inline bool delta_grid_position_of(delta_grid* s, EntityId id, Vec3* out) {
  return uniform_grid_position_of(&s->grid, id, out);
}

static inline size_t delta_grid_entity_count(delta_grid* s) {
  return uniform_grid_entity_count(&s->grid);
}

/* Each listed id leaves the base, if it was there, by tombstoning its slot, and
 * is then linked into, moved within, or unlinked from the delta lists according
 * to its current state. */
static inline void delta_grid_apply_dirty_(delta_grid* s) {
  uniform_grid* g = &s->grid;
  for (size_t q = 0; q < s->dirty_count; ++q) {
    const EntityId id = s->dirty.data[q];
    const uint32_t sl = s->slot_of.data[id];
    if (sl != DELTA_GRID_NO_SLOT) {
      s->sorted_x.data[sl] = NAN;
      s->sorted_id.data[sl] = GDS_NO_ENTITY;
      s->slot_of.data[id] = DELTA_GRID_NO_SLOT;
    }
    const uint32_t was = g->cell_of.data[id];
    if (g->live.data[id]) {
      const uint32_t c = uniform_grid_cell_index_(g, g->pos.data[id]);
      if (was == c) continue;
      if (was == UNIFORM_GRID_NO_CELL) ++s->delta_size;
      else uniform_grid_unlink_(g, id);
      uniform_grid_link_(g, id, c);
    } else if (was != UNIFORM_GRID_NO_CELL) {
      uniform_grid_unlink_(g, id);
      --s->delta_size;
    }
  }
}

/* cell_sorted's counting sort, with `slot_of` as both the per-id cell scratch
 * of the counting pass and, after the scatter, the slot each id landed in,
 * which is what tombstoning needs. A rebuild empties the delta first; each
 * non-empty list has a member whose cell names its head. */
static inline void delta_grid_rebuild_(delta_grid* s) {
  uniform_grid* g = &s->grid;
  const uint8_t* live = g->live.data;
  const Vec3* pos = g->pos.data;
  const size_t n = g->live.size;
  if (s->delta_size > 0) {
    for (size_t id = 0; id < n; ++id) {
      const uint32_t c = g->cell_of.data[id];
      if (c == UNIFORM_GRID_NO_CELL) continue;
      g->head.data[c] = GDS_NO_ENTITY;
      g->cell_of.data[id] = UNIFORM_GRID_NO_CELL;
    }
    s->delta_size = 0;
  }
  const size_t cells = s->start.size - 1;
  uint32_t* start = s->start.data;
  uint32_t* slot_of = s->slot_of.data;
  memset(start, 0, s->start.size * sizeof *start);
  for (size_t id = 0; id < n; ++id) {
    if (!live[id]) {
      slot_of[id] = DELTA_GRID_NO_SLOT;
      continue;
    }
    const uint32_t c = uniform_grid_cell_index_(g, pos[id]);
    slot_of[id] = c;
    ++start[c];
  }
  uint32_t end = 0;
  for (size_t c = 0; c < cells; ++c) {
    end += start[c];
    start[c] = end;
  }
  start[cells] = end;
  const size_t padded = (size_t)end + DELTA_GRID_PAD;
  gds_vec_resize(s->sorted_id, padded);
  gds_vec_resize(s->sorted_x, padded);
  gds_vec_resize(s->sorted_y, padded);
  gds_vec_resize(s->sorted_z, padded);
  for (size_t id = n; id-- > 0;) {
    if (!live[id]) continue;
    const uint32_t slot = --start[slot_of[id]];
    s->sorted_id.data[slot] = (EntityId)id;
    s->sorted_x.data[slot] = pos[id].x;
    s->sorted_y.data[slot] = pos[id].y;
    s->sorted_z.data[slot] = pos[id].z;
    slot_of[id] = slot;
  }
}

static inline void delta_grid_repair_(delta_grid* s) {
  if (s->full || (double)s->dirty_count > DELTA_GRID_ALPHA * (double)s->grid.live_count) {
    delta_grid_rebuild_(s);
  } else {
    delta_grid_apply_dirty_(s);
  }
  s->dirty_count = 0;
  s->full = false;
  ++s->pass;
}

/* The index is derived state, repaired by the first query after a change. The
 * test is kept apart from the repair, so that a query whose index is current
 * pays only the test. */
static inline void delta_grid_sync_(delta_grid* s) {
  if (s->full || s->dirty_count != 0) delta_grid_repair_(s);
}

/* One bound of the query box on one axis: the cell holding v, clamped to the
 * axis. The clamp is made on the float before it is converted, so an infinite
 * bound lands on an end of the axis rather than converting to an undefined
 * integer. A comparison with NaN is false, so the order of the two clamps
 * decides where a NaN bound lands: the lower bound clamps from below first and
 * lands on the first cell, the upper from above first and lands on the last,
 * and the box widens either way. For a finite bound this is uniform_grid's
 * axis_index, which is monotone in v, so a lower bound below a point's
 * coordinate never names a cell above the point's, nor an upper bound above it
 * one below. */
static inline uint32_t delta_grid_box_lower_(const delta_grid* s, float v, float lo, uint32_t n) {
  float f = floorf((v - lo) * s->grid.inv_cell);
  const float last = (float)(n - 1);
  f = f > 0.0f ? f : 0.0f;
  f = f < last ? f : last;
  return (uint32_t)f;
}

static inline uint32_t delta_grid_box_upper_(const delta_grid* s, float v, float lo, uint32_t n) {
  float f = floorf((v - lo) * s->grid.inv_cell);
  const float last = (float)(n - 1);
  f = f < last ? f : last;
  f = f > 0.0f ? f : 0.0f;
  return (uint32_t)f;
}

/* cell_sorted's exact test over one base range, DELTA_GRID_LANES slots at a
 * time. A tombstone holds NaN in x, so its distance is NaN and `<=` rejects it
 * in its lane like any point outside the radius; lanes past the range are
 * masked by index. */
static inline void delta_grid_scan_run_(const delta_grid* s, size_t b, size_t e, Vec3 c,
                                        float r2, RadiusDigest* d) {
  for (size_t i = b; i < e; i += DELTA_GRID_LANES) {
    const float* px = s->sorted_x.data + i;
    const float* py = s->sorted_y.data + i;
    const float* pz = s->sorted_z.data + i;
    const size_t left = e - i;
    uint32_t hits = 0;
    for (size_t j = 0; j < DELTA_GRID_LANES; ++j) {
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

/* Every cell overlapping the box of half-width r around c, in uniform_grid's
 * order, with the box widened as DELTA_GRID_BOX_SLACK describes so that it is
 * conservative for the shared test. The box is not clipped to a sphere, so the
 * caller does the exact test. Runs the statements given once per cell with
 * `cell` in scope, then sets `whole` to whether the box spans every cell.
 *
 * The C++ wrote this walk once, as a template over the work done per cell;
 * here it is a macro, expanded once in each of the four walks below. */
#define DELTA_GRID_FOR_EACH_CELL_IN_BOX_(s, c, r, whole, ...)                            \
  do {                                                                                   \
    const Bounds* bnd_ = &(s)->grid.bounds;                                              \
    const uint32_t nx_ = (s)->grid.nx, ny_ = (s)->grid.ny, nz_ = (s)->grid.nz;           \
    const float mx_ = (fabsf((c).x) + (r)) * DELTA_GRID_BOX_SLACK + DELTA_GRID_BOX_FLOOR; \
    const float my_ = (fabsf((c).y) + (r)) * DELTA_GRID_BOX_SLACK + DELTA_GRID_BOX_FLOOR; \
    const float mz_ = (fabsf((c).z) + (r)) * DELTA_GRID_BOX_SLACK + DELTA_GRID_BOX_FLOOR; \
    const uint32_t x0_ = delta_grid_box_lower_((s), ((c).x - (r)) - mx_, bnd_->min.x, nx_); \
    const uint32_t x1_ = delta_grid_box_upper_((s), ((c).x + (r)) + mx_, bnd_->min.x, nx_); \
    const uint32_t y0_ = delta_grid_box_lower_((s), ((c).y - (r)) - my_, bnd_->min.y, ny_); \
    const uint32_t y1_ = delta_grid_box_upper_((s), ((c).y + (r)) + my_, bnd_->min.y, ny_); \
    const uint32_t z0_ = delta_grid_box_lower_((s), ((c).z - (r)) - mz_, bnd_->min.z, nz_); \
    const uint32_t z1_ = delta_grid_box_upper_((s), ((c).z + (r)) + mz_, bnd_->min.z, nz_); \
    for (uint32_t z_ = z0_; z_ <= z1_; ++z_) {                                           \
      for (uint32_t y_ = y0_; y_ <= y1_; ++y_) {                                         \
        const size_t row_ = ((size_t)z_ * ny_ + y_) * nx_;                               \
        for (uint32_t x_ = x0_; x_ <= x1_; ++x_) {                                       \
          const size_t cell = row_ + x_;                                                 \
          __VA_ARGS__                                                                    \
        }                                                                                \
      }                                                                                  \
    }                                                                                    \
    (whole) = x0_ == 0 && y0_ == 0 && z0_ == 0 && x1_ + 1 == nx_ && y1_ + 1 == ny_ &&    \
              z1_ + 1 == nz_;                                                            \
  } while (0)

/* The walk, kept out of line. Inlined, the walk shares its registers with the
 * digest and the k-nearest bookkeeping of the query around it, and the queries
 * of a tick of s01 took about 8 us longer; the version before the review, whose
 * box was computed inside the walk, was compiled out of line by the compiler's
 * own choice.
 *
 * The C++ template was compiled out of line once per query and delta flag; the
 * four copies are written out here, the radius scan and the k-nearest gather,
 * each without and with the delta lists, with the work per cell written into
 * each. (GCC kept the per-cell lambda of the C++ k-nearest gather with delta
 * lists out of line and called it once per cell; paired timing of the two
 * builds on hs03, hs02 and s01 showed no difference from writing it in.) Each
 * returns whether the box spans every cell. */
static __attribute__((noinline)) bool delta_grid_radius_walk_(const delta_grid* s, Vec3 c,
                                                              float r, float r2,
                                                              RadiusDigest* d) {
  const uint32_t* start = s->start.data;
  bool whole;
  DELTA_GRID_FOR_EACH_CELL_IN_BOX_(s, c, r, whole, {
    const size_t b = start[cell];
    const size_t e = start[cell + 1];
    if (b < e) delta_grid_scan_run_(s, b, e, c, r2, d);
  });
  return whole;
}

static __attribute__((noinline)) bool delta_grid_radius_walk_delta_(const delta_grid* s,
                                                                    Vec3 c, float r, float r2,
                                                                    RadiusDigest* d) {
  const uint32_t* start = s->start.data;
  const EntityId* head = s->grid.head.data;
  const EntityId* next = s->grid.next.data;
  const Vec3* pos = s->grid.pos.data;
  bool whole;
  DELTA_GRID_FOR_EACH_CELL_IN_BOX_(s, c, r, whole, {
    const size_t b = start[cell];
    const size_t e = start[cell + 1];
    if (b < e) delta_grid_scan_run_(s, b, e, c, r2, d);
    for (EntityId id = head[cell]; id != GDS_NO_ENTITY; id = next[id]) {
      if (gds_dist2(pos[id], c) <= r2) gds_radius_hit(d, id, pos[id]);
    }
  });
  return whole;
}

static __attribute__((noinline)) bool delta_grid_gather_walk_(const delta_grid* s, Vec3 c,
                                                              float r,
                                                              delta_grid_neighbours_* found) {
  const uint32_t* start = s->start.data;
  const EntityId* sorted_id = s->sorted_id.data;
  const float* sx = s->sorted_x.data;
  const float* sy = s->sorted_y.data;
  const float* sz = s->sorted_z.data;
  bool whole;
  DELTA_GRID_FOR_EACH_CELL_IN_BOX_(s, c, r, whole, {
    for (size_t i = start[cell], e = start[cell + 1]; i < e; ++i) {
      const EntityId id = sorted_id[i];
      if (id == GDS_NO_ENTITY) continue;
      const Neighbour nb = {gds_dist2(gds_vec3(sx[i], sy[i], sz[i]), c), id};
      gds_vec_push(*found, nb);
    }
  });
  return whole;
}

static __attribute__((noinline)) bool delta_grid_gather_walk_delta_(const delta_grid* s,
                                                                    Vec3 c, float r,
                                                                    delta_grid_neighbours_* found) {
  const uint32_t* start = s->start.data;
  const EntityId* sorted_id = s->sorted_id.data;
  const float* sx = s->sorted_x.data;
  const float* sy = s->sorted_y.data;
  const float* sz = s->sorted_z.data;
  const EntityId* head = s->grid.head.data;
  const EntityId* next = s->grid.next.data;
  const Vec3* pos = s->grid.pos.data;
  bool whole;
  DELTA_GRID_FOR_EACH_CELL_IN_BOX_(s, c, r, whole, {
    for (size_t i = start[cell], e = start[cell + 1]; i < e; ++i) {
      const EntityId id = sorted_id[i];
      if (id == GDS_NO_ENTITY) continue;
      const Neighbour nb = {gds_dist2(gds_vec3(sx[i], sy[i], sz[i]), c), id};
      gds_vec_push(*found, nb);
    }
    for (EntityId id = head[cell]; id != GDS_NO_ENTITY; id = next[id]) {
      const Neighbour nb = {gds_dist2(pos[id], c), id};
      gds_vec_push(*found, nb);
    }
  });
  return whole;
}

static inline uint64_t delta_grid_radius_search_(delta_grid* s, Vec3 c, float r,
                                                 const bool walk_delta) {
  RadiusDigest d = gds_radius_digest(c, r);
  const float r2 = r * r;
  if (walk_delta) delta_grid_radius_walk_delta_(s, c, r, r2, &d);
  else delta_grid_radius_walk_(s, c, r, r2, &d);
  return d.acc;
}

/* uniform_grid's box doubling; candidates are gathered from each cell's base
 * range and its delta list. A tombstone is skipped by its id rather than left
 * to the distance: its NaN would compare equal to every distance under
 * `nearer` and break the ordering the partial sort needs.
 *
 * The search stops early, before the kth neighbour is inside the box, only
 * once the box has passed twice the largest extent and spans every cell, so
 * that everything has been gathered. For a centre inside the world the first
 * implies the second and this is uniform_grid's stop exactly; for a centre
 * outside it, a box that wide can still miss the far side of the world. A
 * half-width that has overflowed to infinity ends the search whatever the box,
 * so the doubling always ends. */
static inline uint64_t delta_grid_knn_search_(delta_grid* s, Vec3 c, uint32_t k,
                                              const bool walk_delta) {
  KnnDigest d = gds_knn_digest();
  const float limit = gds_bounds_largest_extent(&s->grid.bounds) * 2.0f;
  const Vec3* pos = s->grid.pos.data;
  delta_grid_neighbours_ found;
  gds_vec_init(found);
  for (float r = s->grid.cell;; r *= 2.0f) {
    gds_vec_clear(found);
    const bool whole = walk_delta ? delta_grid_gather_walk_delta_(s, c, r, &found)
                                  : delta_grid_gather_walk_(s, c, r, &found);
    const bool done = (r > limit && whole) || !(r < INFINITY);
    const size_t n = found.size;
    const size_t want = (size_t)k < n ? (size_t)k : n;
    if (want > 0) gds_neighbour_partial_sort(found.data, want, n);
    if (n >= k || done) {
      if (want == 0 || found.data[want - 1].d2 <= r * r || done) {
        for (size_t i = 0; i < want; ++i)
          gds_knn_push(&d, found.data[i].id, pos[found.data[i].id]);
        gds_vec_free(found);
        return d.acc;
      }
    }
  }
}

/* Whether a query walks the delta lists is decided by which of two out-of-line
 * walks it calls, rather than tested in the per-cell loop: the k-nearest gather
 * visits many mostly empty cells, and a test there is reloaded after every push
 * into the gather vector, which measurably slowed it. The C++ made it a
 * template argument, decided once per query; here a k-nearest query tests it
 * once per doubling of its box. */
static inline uint64_t delta_grid_query_radius(delta_grid* s, Vec3 c, float r) {
  delta_grid_sync_(s);
  return s->delta_size > 0 ? delta_grid_radius_search_(s, c, r, true)
                           : delta_grid_radius_search_(s, c, r, false);
}

static inline uint64_t delta_grid_query_radius_of(delta_grid* s, EntityId self, float r) {
  if (self >= s->grid.live.size || !s->grid.live.data[self]) return 0;
  const Vec3 c = s->grid.pos.data[self];
  const RadiusDigest own = gds_radius_digest(c, r);
  return delta_grid_query_radius(s, c, r) - gds_radius_term(&own, self, c);
}

static inline uint64_t delta_grid_query_knn(delta_grid* s, Vec3 c, uint32_t k) {
  if (k == 0 || s->grid.live_count == 0) return gds_knn_digest().acc;
  delta_grid_sync_(s);
  return s->delta_size > 0 ? delta_grid_knn_search_(s, c, k, true)
                           : delta_grid_knn_search_(s, c, k, false);
}

/* Closes the open frame into the ring. The ring grows by one slot per tick
 * closed until it holds history_ticks + 1, as the oracle's history does, so a
 * run shorter than the declared history never pays for frames it cannot have;
 * from then on it is circular. Slots keep their buffers, so once the ring is
 * full a tick allocates nothing: a pre-image swaps its arrays with the slot's,
 * and a records frame swaps its record vector.
 *
 * With no history kept, only the tick just closed and whether anything has
 * changed since are remembered, for rewind_to. */
static inline void delta_grid_end_tick(delta_grid* s, uint64_t tick) {
  if (!s->keeping) {
    s->closed = tick;
    s->has_closed = true;
    s->changed = false;
    return;
  }
  if (s->count == s->keep) {
    s->oldest = (s->oldest + 1) % s->keep;
    --s->count;
  } else if (s->count == s->ring.size) {
    delta_grid_grow_ring_(s);
  }
  delta_grid_frame_* f = delta_grid_frame_at_(s, s->count);
  f->tick = tick;
  f->full = s->open.full;
  f->live_count = s->open.live_count;
  if (s->open.full) {
    const delta_grid_positions_ p = f->pos;
    f->pos = s->open.pos;
    s->open.pos = p;
    const delta_grid_liveness_ l = f->live;
    f->live = s->open.live;
    s->open.live = l;
  } else {
    const delta_grid_records_ rec = f->rec;
    f->rec = s->open.rec;
    s->open.rec = rec;
  }
  ++s->count;
  gds_vec_clear(s->open.rec);
  s->open.full = false;
  ++s->epoch;
}

/* Undoing a frame restores the state at the end of the frame before it, so
 * reaching tick T means undoing the open tick and then every frame newer than
 * T, newest first. A pre-image among them makes everything newer than it
 * irrelevant: the oldest one is copied and only the record frames older than it
 * are undone. The frame of T itself is never undone; it is kept so that T is
 * known to be inside the window, as the oracle's snapshot of T is.
 *
 * With no history kept, the one rewind the contract allows is to the tick just
 * closed, and it is honoured exactly when nothing has changed since, because
 * the state is then already the one asked for. That is the rewind a workload
 * of depth 0 issues, as the first operation of its tick. */
static inline bool delta_grid_rewind_to(delta_grid* s, uint64_t tick) {
  if (!s->keeping) return s->has_closed && tick == s->closed && !s->changed;
  size_t target = s->count;
  for (size_t j = s->count; j-- > 0;) {
    if (delta_grid_frame_at_(s, j)->tick == tick) {
      target = j;
      break;
    }
  }
  if (target == s->count) return false;
  size_t jump = s->count;
  for (size_t j = target + 1; j < s->count; ++j) {
    if (delta_grid_frame_at_(s, j)->full) {
      jump = j;
      break;
    }
  }
  if (jump < s->count) {
    delta_grid_restore_image_(s, delta_grid_frame_at_(s, jump));
  } else if (s->open.full) {
    delta_grid_restore_image_(s, &s->open);
  } else {
    delta_grid_undo_records_(s, &s->open);
  }
  for (size_t j = jump; j-- > target + 1;) delta_grid_undo_records_(s, delta_grid_frame_at_(s, j));
  s->count = target + 1;
  gds_vec_clear(s->open.rec);
  s->open.full = false;
  /* A fresh epoch, so the stamps of the discarded open tick cannot stop the
   * continuing tick from recording its own first changes. */
  ++s->epoch;
  return true;
}

static inline size_t delta_grid_reported_bytes(delta_grid* s) {
  size_t b = uniform_grid_reported_bytes(&s->grid) + gds_vec_bytes(s->start) +
             gds_vec_bytes(s->sorted_id) + gds_vec_bytes(s->sorted_x) +
             gds_vec_bytes(s->sorted_y) + gds_vec_bytes(s->sorted_z) + gds_vec_bytes(s->slot_of) +
             gds_vec_bytes(s->listed) + gds_vec_bytes(s->dirty) + gds_vec_bytes(s->stamp) +
             gds_vec_bytes(s->ring) + delta_grid_frame_bytes_(&s->open);
  for (size_t i = 0; i < s->ring.size; ++i) b += delta_grid_frame_bytes_(&s->ring.data[i]);
  return b;
}

#endif
