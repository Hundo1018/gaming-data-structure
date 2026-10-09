/* Template header: rewind for a structure that does not keep history. It
 * snapshots the world every tick, and on rewind clears the index and
 * re-inserts everything.
 *
 * This is not a strawman. It is what an engine does today: the game state is
 * the authority, the spatial index is derived, and rollback restores the state
 * and rebuilds the index. Any candidate declaring P_native_rewind = 0 is
 * measured inside this wrapper on temporal workloads, so "keep history inside
 * the index" has something real to beat.
 *
 * The wrapper's own memory is charged to the candidate, because it is part of
 * what that approach costs.
 *
 * Include with GDS_RR_INNER (the wrapped structure's prefix) and GDS_RR_SELF
 * (the wrapper's prefix) defined; both are undefined again at the end. */
#if !defined(GDS_RR_INNER) || !defined(GDS_RR_SELF)
#error "define GDS_RR_INNER and GDS_RR_SELF before including gds/spatial/rebuild_rewind.inc.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "gds/spatial/types.h"
#include "gds/vec.h"

#define GDS_RR_F_(n) GDS_FN(GDS_RR_SELF, n)
#define GDS_RR_I_(n) GDS_FN(GDS_RR_INNER, n)
#define GDS_RR_FRAME_ GDS_CAT(GDS_RR_SELF, _frame)

typedef struct {
  uint64_t tick;
  GDS_VEC(Vec3) pos;
  GDS_VEC(uint8_t) live;
  size_t live_count;
} GDS_RR_FRAME_;

typedef struct {
  WorldConfig cfg;
  GDS_RR_INNER inner;
  GDS_VEC(Vec3) pos;
  GDS_VEC(uint8_t) live;
  size_t live_count;
  GDS_VEC(GDS_RR_FRAME_) frames;
} GDS_RR_SELF;

enum { GDS_RR_F_(native_rewind) = 1 };

static inline void GDS_RR_F_(init)(GDS_RR_SELF* s, const WorldConfig* cfg) {
  memset(s, 0, sizeof *s);
  s->cfg = *cfg;
  GDS_RR_I_(init)(&s->inner, cfg);
  const Vec3 zero = {0, 0, 0};
  gds_vec_assign(s->pos, (size_t)cfg->max_entity_id + 1, zero);
  gds_vec_assign(s->live, (size_t)cfg->max_entity_id + 1, 0);
}

static inline void GDS_CAT(GDS_RR_SELF, _frame_free_)(GDS_RR_FRAME_* f) {
  gds_vec_free(f->pos);
  gds_vec_free(f->live);
}

static inline void GDS_RR_F_(free)(GDS_RR_SELF* s) {
  GDS_RR_I_(free)(&s->inner);
  for (size_t i = 0; i < s->frames.size; ++i)
    GDS_CAT(GDS_RR_SELF, _frame_free_)(&s->frames.data[i]);
  gds_vec_free(s->frames);
  gds_vec_free(s->pos);
  gds_vec_free(s->live);
}

static inline void GDS_RR_F_(insert)(GDS_RR_SELF* s, EntityId id, Vec3 p) {
  if (!s->live.data[id]) ++s->live_count;
  s->live.data[id] = 1;
  s->pos.data[id] = p;
  GDS_RR_I_(insert)(&s->inner, id, p);
}

static inline void GDS_RR_F_(remove)(GDS_RR_SELF* s, EntityId id) {
  if (s->live.data[id]) --s->live_count;
  s->live.data[id] = 0;
  GDS_RR_I_(remove)(&s->inner, id);
}

static inline void GDS_RR_F_(move_by)(GDS_RR_SELF* s, EntityId id, Vec3 delta) {
  if (id < s->live.size && s->live.data[id])
    s->pos.data[id] = gds_wrap_into(gds_add3(s->pos.data[id], delta), &s->cfg.bounds);
  GDS_RR_I_(move_by)(&s->inner, id, delta);
}

static inline bool GDS_RR_F_(position_of)(GDS_RR_SELF* s, EntityId id, Vec3* out) {
  return GDS_RR_I_(position_of)(&s->inner, id, out);
}
static inline uint64_t GDS_RR_F_(query_radius)(GDS_RR_SELF* s, Vec3 c, float r) {
  return GDS_RR_I_(query_radius)(&s->inner, c, r);
}
static inline uint64_t GDS_RR_F_(query_radius_of)(GDS_RR_SELF* s, EntityId id, float r) {
  return GDS_RR_I_(query_radius_of)(&s->inner, id, r);
}
static inline uint64_t GDS_RR_F_(query_knn)(GDS_RR_SELF* s, Vec3 c, uint32_t k) {
  return GDS_RR_I_(query_knn)(&s->inner, c, k);
}

static inline void GDS_RR_F_(end_tick)(GDS_RR_SELF* s, uint64_t tick) {
  GDS_RR_I_(end_tick)(&s->inner, tick);
  if (s->cfg.history_ticks == 0) return;
  GDS_RR_FRAME_ f;
  f.tick = tick;
  gds_vec_copy(f.pos, s->pos);
  gds_vec_copy(f.live, s->live);
  f.live_count = s->live_count;
  gds_vec_push(s->frames, f);
  const size_t keep = (size_t)s->cfg.history_ticks + 1;
  while (s->frames.size > keep) {
    GDS_CAT(GDS_RR_SELF, _frame_free_)(&s->frames.data[0]);
    memmove(s->frames.data, s->frames.data + 1, (s->frames.size - 1) * sizeof *s->frames.data);
    --s->frames.size;
  }
}

static inline bool GDS_RR_F_(rewind_to)(GDS_RR_SELF* s, uint64_t tick) {
  for (size_t i = s->frames.size; i-- > 0;) {
    if (s->frames.data[i].tick != tick) continue;
    GDS_RR_FRAME_* f = &s->frames.data[i];
    gds_vec_assign_from(s->pos, f->pos);
    gds_vec_assign_from(s->live, f->live);
    s->live_count = f->live_count;
    for (size_t j = i + 1; j < s->frames.size; ++j)
      GDS_CAT(GDS_RR_SELF, _frame_free_)(&s->frames.data[j]);
    s->frames.size = i + 1;
    /* The fresh index exists before the old one is released, as it did when
     * this was `inner_ = S(cfg_)`, so the peak charges both. */
    GDS_RR_INNER fresh;
    GDS_RR_I_(init)(&fresh, &s->cfg);
    GDS_RR_I_(free)(&s->inner);
    s->inner = fresh;
    for (size_t id = 0; id < s->live.size; ++id)
      if (s->live.data[id]) GDS_RR_I_(insert)(&s->inner, (EntityId)id, s->pos.data[id]);
    return true;
  }
  return false;
}

static inline size_t GDS_RR_F_(entity_count)(GDS_RR_SELF* s) {
  return GDS_RR_I_(entity_count)(&s->inner);
}

static inline size_t GDS_RR_F_(reported_bytes)(GDS_RR_SELF* s) {
  size_t b = GDS_RR_I_(reported_bytes)(&s->inner) + gds_vec_bytes(s->pos) + gds_vec_bytes(s->live);
  for (size_t i = 0; i < s->frames.size; ++i)
    b += gds_vec_bytes(s->frames.data[i].pos) + gds_vec_bytes(s->frames.data[i].live);
  return b;
}

#undef GDS_RR_F_
#undef GDS_RR_I_
#undef GDS_RR_FRAME_
#undef GDS_RR_INNER
#undef GDS_RR_SELF
