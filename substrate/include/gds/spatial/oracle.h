/* The spatial oracle.
 *
 * Linear scan, full-copy history, no cleverness anywhere. It is written to be
 * checked by reading it. Deliberately shares no code with the rebuild wrapper:
 * if the oracle's history and the wrapper's history were the same
 * implementation, a bug in it would cancel out and neither would be verified. */
#ifndef GDS_SPATIAL_ORACLE_H
#define GDS_SPATIAL_ORACLE_H

#include <stdlib.h>
#include <string.h>

#include "gds/spatial/knn.h"
#include "gds/spatial/types.h"
#include "gds/vec.h"

typedef struct {
  uint64_t tick;
  GDS_VEC(Vec3) pos;
  GDS_VEC(uint8_t) live;
  size_t live_count;
} brute_force_frame;

typedef struct {
  WorldConfig cfg;
  GDS_VEC(Vec3) pos;
  GDS_VEC(uint8_t) live;
  size_t live_count;
  GDS_VEC(brute_force_frame) history;
} brute_force;

enum { brute_force_native_rewind = 1 };

static inline void brute_force_init(brute_force* s, const WorldConfig* cfg) {
  memset(s, 0, sizeof *s);
  s->cfg = *cfg;
  const Vec3 zero = {0, 0, 0};
  gds_vec_assign(s->pos, (size_t)cfg->max_entity_id + 1, zero);
  gds_vec_assign(s->live, (size_t)cfg->max_entity_id + 1, 0);
}

static inline void brute_force_frame_free_(brute_force_frame* f) {
  gds_vec_free(f->pos);
  gds_vec_free(f->live);
}

static inline void brute_force_free(brute_force* s) {
  for (size_t i = 0; i < s->history.size; ++i) brute_force_frame_free_(&s->history.data[i]);
  gds_vec_free(s->history);
  gds_vec_free(s->pos);
  gds_vec_free(s->live);
}

static inline void brute_force_insert(brute_force* s, EntityId id, Vec3 p) {
  if (!s->live.data[id]) ++s->live_count;
  s->live.data[id] = 1;
  s->pos.data[id] = p;
}

static inline void brute_force_remove(brute_force* s, EntityId id) {
  if (s->live.data[id]) --s->live_count;
  s->live.data[id] = 0;
}

static inline void brute_force_move_by(brute_force* s, EntityId id, Vec3 delta) {
  if (!s->live.data[id]) return;
  s->pos.data[id] = gds_wrap_into(gds_add3(s->pos.data[id], delta), &s->cfg.bounds);
}

static inline bool brute_force_position_of(brute_force* s, EntityId id, Vec3* out) {
  if (id >= s->live.size || !s->live.data[id]) return false;
  *out = s->pos.data[id];
  return true;
}

static inline uint64_t brute_force_query_radius(brute_force* s, Vec3 c, float r) {
  const float r2 = r * r;
  RadiusDigest d = gds_radius_digest(c, r);
  for (size_t i = 0; i < s->live.size; ++i)
    if (s->live.data[i] && gds_dist2(s->pos.data[i], c) <= r2)
      gds_radius_hit(&d, (EntityId)i, s->pos.data[i]);
  return d.acc;
}

static inline uint64_t brute_force_query_radius_of(brute_force* s, EntityId self, float r) {
  if (self >= s->live.size || !s->live.data[self]) return 0;
  const Vec3 c = s->pos.data[self];
  const float r2 = r * r;
  RadiusDigest d = gds_radius_digest(c, r);
  for (size_t i = 0; i < s->live.size; ++i) {
    if (i == self) continue;
    if (s->live.data[i] && gds_dist2(s->pos.data[i], c) <= r2)
      gds_radius_hit(&d, (EntityId)i, s->pos.data[i]);
  }
  return d.acc;
}

static inline uint64_t brute_force_query_knn(brute_force* s, Vec3 c, uint32_t k) {
  Neighbour* all = s->live_count ? malloc(s->live_count * sizeof *all) : NULL;
  size_t n = 0;
  for (size_t i = 0; i < s->live.size; ++i) {
    if (s->live.data[i]) {
      Neighbour nb = {gds_dist2(s->pos.data[i], c), (EntityId)i};
      all[n++] = nb;
    }
  }
  const size_t want = k < n ? k : n;
  gds_neighbour_partial_sort(all, want, n);
  KnnDigest d = gds_knn_digest();
  for (size_t i = 0; i < want; ++i) gds_knn_push(&d, all[i].id, s->pos.data[all[i].id]);
  free(all);
  return d.acc;
}

static inline void brute_force_end_tick(brute_force* s, uint64_t tick) {
  brute_force_frame f;
  f.tick = tick;
  gds_vec_copy(f.pos, s->pos);
  gds_vec_copy(f.live, s->live);
  f.live_count = s->live_count;
  gds_vec_push(s->history, f);
  const size_t keep = (size_t)s->cfg.history_ticks + 1;
  while (s->history.size > keep) {
    brute_force_frame_free_(&s->history.data[0]);
    memmove(s->history.data, s->history.data + 1,
            (s->history.size - 1) * sizeof *s->history.data);
    --s->history.size;
  }
}

static inline bool brute_force_rewind_to(brute_force* s, uint64_t tick) {
  for (size_t i = s->history.size; i-- > 0;) {
    if (s->history.data[i].tick == tick) {
      brute_force_frame* f = &s->history.data[i];
      gds_vec_assign_from(s->pos, f->pos);
      gds_vec_assign_from(s->live, f->live);
      s->live_count = f->live_count;
      for (size_t j = i + 1; j < s->history.size; ++j)
        brute_force_frame_free_(&s->history.data[j]);
      s->history.size = i + 1;
      return true;
    }
  }
  return false;
}

static inline size_t brute_force_entity_count(brute_force* s) { return s->live_count; }

static inline size_t brute_force_reported_bytes(brute_force* s) {
  size_t b = gds_vec_bytes(s->pos) + gds_vec_bytes(s->live);
  for (size_t i = 0; i < s->history.size; ++i)
    b += gds_vec_bytes(s->history.data[i].pos) + gds_vec_bytes(s->history.data[i].live);
  return b;
}

#endif
