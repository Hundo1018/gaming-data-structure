/* Template header: rewind by remembering only what changed.
 *
 * The other strategy, the rebuild wrapper, copies the whole world every tick
 * and rebuilds the index on rewind. This one records the previous position of
 * each entity the first time it changes within a tick, and rewinds by
 * replaying those records backwards.
 *
 * Which wins is not obvious and is the point of having both: the log is small
 * when few entities move and large when they all do, and replaying it costs
 * two index operations per record against the rebuild's one insert per
 * entity. A workload where every entity moves every tick should favour the
 * rebuild; one where a handful move should favour the log.
 *
 * Include with GDS_UL_INNER (the wrapped structure's prefix) and GDS_UL_SELF
 * (the wrapper's prefix) defined; both are undefined again at the end. */
#if !defined(GDS_UL_INNER) || !defined(GDS_UL_SELF)
#error "define GDS_UL_INNER and GDS_UL_SELF before including gds/spatial/undo_log_rewind.inc.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "gds/spatial/types.h"
#include "gds/vec.h"

#define GDS_UL_F_(n) GDS_FN(GDS_UL_SELF, n)
#define GDS_UL_I_(n) GDS_FN(GDS_UL_INNER, n)
#define GDS_UL_REC_ GDS_CAT(GDS_UL_SELF, _record)
#define GDS_UL_FRAME_ GDS_CAT(GDS_UL_SELF, _frame)

typedef struct {
  EntityId id;
  uint8_t was_live;
  Vec3 pos;
} GDS_UL_REC_;

typedef struct {
  uint64_t tick;
  GDS_VEC(GDS_UL_REC_) records;
} GDS_UL_FRAME_;

typedef struct {
  WorldConfig cfg;
  GDS_UL_INNER inner;
  bool keeping;
  GDS_VEC(Vec3) pos;
  GDS_VEC(uint8_t) live;
  GDS_VEC(uint64_t) stamp;
  size_t live_count;
  uint64_t epoch;
  GDS_UL_FRAME_ open;
  GDS_VEC(GDS_UL_FRAME_) frames;
} GDS_UL_SELF;

enum { GDS_UL_F_(native_rewind) = 1 };

static inline void GDS_UL_F_(init)(GDS_UL_SELF* s, const WorldConfig* cfg) {
  memset(s, 0, sizeof *s);
  s->cfg = *cfg;
  GDS_UL_I_(init)(&s->inner, cfg);
  s->keeping = cfg->history_ticks > 0;
  if (!s->keeping) return; /* the mirror exists only to feed the log */
  const Vec3 zero = {0, 0, 0};
  const size_t n = (size_t)cfg->max_entity_id + 1;
  gds_vec_assign(s->pos, n, zero);
  gds_vec_assign(s->live, n, 0);
  /* Epochs never repeat, not even after a rewind reissues a tick number, so a
   * stale stamp can never be mistaken for one from the current tick. */
  gds_vec_assign(s->stamp, n, 0);
}

static inline void GDS_UL_F_(free)(GDS_UL_SELF* s) {
  GDS_UL_I_(free)(&s->inner);
  gds_vec_free(s->open.records);
  for (size_t i = 0; i < s->frames.size; ++i) gds_vec_free(s->frames.data[i].records);
  gds_vec_free(s->frames);
  gds_vec_free(s->pos);
  gds_vec_free(s->live);
  gds_vec_free(s->stamp);
}

/* Only the first change to an entity within a tick is worth recording: the
 * state being restored is the one at the tick boundary, not each step of it. */
static inline void GDS_CAT(GDS_UL_SELF, _note_)(GDS_UL_SELF* s, EntityId id) {
  if (s->cfg.history_ticks == 0 || id >= s->stamp.size) return;
  if (s->stamp.data[id] == s->epoch + 1) return;
  s->stamp.data[id] = s->epoch + 1;
  GDS_UL_REC_ rec;
  memset(&rec, 0, sizeof rec);
  rec.id = id;
  rec.was_live = s->live.data[id];
  rec.pos = s->pos.data[id];
  gds_vec_push(s->open.records, rec);
}

static inline void GDS_CAT(GDS_UL_SELF, _undo_)(GDS_UL_SELF* s, const GDS_UL_FRAME_* f) {
  for (size_t i = f->records.size; i-- > 0;) {
    const GDS_UL_REC_* rec = &f->records.data[i];
    const bool now_live = s->live.data[rec->id] != 0;
    if (now_live) GDS_UL_I_(remove)(&s->inner, rec->id);
    if (rec->was_live) GDS_UL_I_(insert)(&s->inner, rec->id, rec->pos);
    if (now_live && !rec->was_live) --s->live_count;
    if (!now_live && rec->was_live) ++s->live_count;
    s->live.data[rec->id] = rec->was_live;
    s->pos.data[rec->id] = rec->pos;
  }
}

static inline void GDS_UL_F_(insert)(GDS_UL_SELF* s, EntityId id, Vec3 p) {
  if (s->keeping) {
    GDS_CAT(GDS_UL_SELF, _note_)(s, id);
    if (!s->live.data[id]) ++s->live_count;
    s->live.data[id] = 1;
    s->pos.data[id] = p;
  }
  GDS_UL_I_(insert)(&s->inner, id, p);
}

static inline void GDS_UL_F_(remove)(GDS_UL_SELF* s, EntityId id) {
  if (s->keeping) {
    GDS_CAT(GDS_UL_SELF, _note_)(s, id);
    if (s->live.data[id]) --s->live_count;
    s->live.data[id] = 0;
  }
  GDS_UL_I_(remove)(&s->inner, id);
}

static inline void GDS_UL_F_(move_by)(GDS_UL_SELF* s, EntityId id, Vec3 delta) {
  if (s->keeping && id < s->live.size && s->live.data[id]) {
    GDS_CAT(GDS_UL_SELF, _note_)(s, id);
    s->pos.data[id] = gds_wrap_into(gds_add3(s->pos.data[id], delta), &s->cfg.bounds);
  }
  GDS_UL_I_(move_by)(&s->inner, id, delta);
}

static inline bool GDS_UL_F_(position_of)(GDS_UL_SELF* s, EntityId id, Vec3* out) {
  return GDS_UL_I_(position_of)(&s->inner, id, out);
}
static inline uint64_t GDS_UL_F_(query_radius)(GDS_UL_SELF* s, Vec3 c, float r) {
  return GDS_UL_I_(query_radius)(&s->inner, c, r);
}
static inline uint64_t GDS_UL_F_(query_radius_of)(GDS_UL_SELF* s, EntityId id, float r) {
  return GDS_UL_I_(query_radius_of)(&s->inner, id, r);
}
static inline uint64_t GDS_UL_F_(query_knn)(GDS_UL_SELF* s, Vec3 c, uint32_t k) {
  return GDS_UL_I_(query_knn)(&s->inner, c, k);
}

static inline void GDS_UL_F_(end_tick)(GDS_UL_SELF* s, uint64_t tick) {
  GDS_UL_I_(end_tick)(&s->inner, tick);
  if (s->cfg.history_ticks == 0) return;
  s->open.tick = tick;
  gds_vec_push(s->frames, s->open);
  gds_vec_init(s->open.records);
  s->open.tick = 0;
  ++s->epoch;
  const size_t keep = (size_t)s->cfg.history_ticks + 1;
  while (s->frames.size > keep) {
    gds_vec_free(s->frames.data[0].records);
    memmove(s->frames.data, s->frames.data + 1, (s->frames.size - 1) * sizeof *s->frames.data);
    --s->frames.size;
  }
}

static inline bool GDS_UL_F_(rewind_to)(GDS_UL_SELF* s, uint64_t tick) {
  size_t target = s->frames.size;
  for (size_t i = s->frames.size; i-- > 0;) {
    if (s->frames.data[i].tick == tick) {
      target = i;
      break;
    }
  }
  if (target == s->frames.size) return false;
  /* Anything changed since the checkpoint but not yet closed into a frame must
   * be undone first, newest change first. */
  GDS_CAT(GDS_UL_SELF, _undo_)(s, &s->open);
  gds_vec_free(s->open.records);
  s->open.tick = 0;
  for (size_t i = s->frames.size; i-- > target + 1;) GDS_CAT(GDS_UL_SELF, _undo_)(s, &s->frames.data[i]);
  for (size_t i = target + 1; i < s->frames.size; ++i) gds_vec_free(s->frames.data[i].records);
  s->frames.size = target + 1;
  ++s->epoch;
  return true;
}

static inline size_t GDS_UL_F_(entity_count)(GDS_UL_SELF* s) {
  return GDS_UL_I_(entity_count)(&s->inner);
}

static inline size_t GDS_UL_F_(reported_bytes)(GDS_UL_SELF* s) {
  size_t b = GDS_UL_I_(reported_bytes)(&s->inner) + gds_vec_bytes(s->pos) +
             gds_vec_bytes(s->live) + gds_vec_bytes(s->stamp) + gds_vec_bytes(s->open.records);
  for (size_t i = 0; i < s->frames.size; ++i) b += gds_vec_bytes(s->frames.data[i].records);
  return b;
}

#undef GDS_UL_F_
#undef GDS_UL_I_
#undef GDS_UL_REC_
#undef GDS_UL_FRAME_
#undef GDS_UL_INNER
#undef GDS_UL_SELF
