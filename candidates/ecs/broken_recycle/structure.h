#ifndef CANDIDATE_ECS_BROKEN_RECYCLE_H
#define CANDIDATE_ECS_BROKEN_RECYCLE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gds/types.h"
#include "gds/vec.h"

typedef struct {
  ComponentMask mask;
  uint8_t alive;
  ComponentValue values[GDS_COMPONENT_COUNT];
} broken_recycle_record_;

/* NEGATIVE CONTROL. Not a research candidate.
 *
 * Identical to `aos` except that the handle is a bare slot index with no
 * generation counter, so a recycled slot silently answers for a destroyed
 * entity's handle. It exists to demonstrate that the correctness gate rejects
 * a structure that is fast for the wrong reason: it is expected to pass every
 * workload without stale accesses and to fail h03_stale_handles. */
typedef struct {
  GDS_VEC(broken_recycle_record_) records;
  GDS_VEC(uint32_t) free_list;
  size_t live;
} broken_recycle;

static inline void broken_recycle_init(broken_recycle* s) { memset(s, 0, sizeof *s); }

static inline void broken_recycle_free(broken_recycle* s) {
  gds_vec_free(s->records);
  gds_vec_free(s->free_list);
}

static inline broken_recycle_record_* broken_recycle_resolve_(broken_recycle* s, Entity e) {
  const uint32_t i = (uint32_t)e.bits;
  if (i >= s->records.size || !s->records.data[i].alive) return NULL;
  return &s->records.data[i];
}

static inline Entity broken_recycle_create(broken_recycle* s, ComponentMask mask,
                                           const ComponentValue* values) {
  uint32_t index;
  if (s->free_list.size) {
    index = gds_vec_pop(s->free_list);
  } else {
    index = (uint32_t)s->records.size;
    broken_recycle_record_ r0;
    memset(&r0, 0, sizeof r0);
    gds_vec_push(s->records, r0);
  }
  broken_recycle_record_* r = &s->records.data[index];
  r->alive = 1;
  r->mask = mask;
  for (int i = 0; i < GDS_COMPONENT_COUNT; ++i) {
    if (mask & (1u << i)) r->values[i] = values[i];
  }
  ++s->live;
  const Entity e = {index};
  return e;
}

static inline void broken_recycle_destroy(broken_recycle* s, Entity e) {
  broken_recycle_record_* r = broken_recycle_resolve_(s, e);
  if (!r) return;
  r->mask = 0;
  r->alive = 0;
  gds_vec_push(s->free_list, (uint32_t)e.bits);
  --s->live;
}

static inline bool broken_recycle_alive(broken_recycle* s, Entity e) {
  return broken_recycle_resolve_(s, e) != NULL;
}

static inline void broken_recycle_add(broken_recycle* s, Entity e, ComponentId c,
                                      const ComponentValue* v) {
  broken_recycle_record_* r = broken_recycle_resolve_(s, e);
  if (!r) return;
  r->mask |= gds_mask_of(c);
  r->values[(int)c] = *v;
}

static inline void broken_recycle_remove(broken_recycle* s, Entity e, ComponentId c) {
  broken_recycle_record_* r = broken_recycle_resolve_(s, e);
  if (!r) return;
  r->mask &= (ComponentMask)~gds_mask_of(c);
}

static inline bool broken_recycle_get(broken_recycle* s, Entity e, ComponentId c,
                                      ComponentValue* out) {
  const broken_recycle_record_* r = broken_recycle_resolve_(s, e);
  if (!r || !(r->mask & gds_mask_of(c))) return false;
  *out = r->values[(int)c];
  return true;
}

static inline bool broken_recycle_set(broken_recycle* s, Entity e, ComponentId c,
                                      const ComponentValue* v) {
  broken_recycle_record_* r = broken_recycle_resolve_(s, e);
  if (!r || !(r->mask & gds_mask_of(c))) return false;
  r->values[(int)c] = *v;
  return true;
}

static inline ComponentMask broken_recycle_mask(broken_recycle* s, Entity e) {
  const broken_recycle_record_* r = broken_recycle_resolve_(s, e);
  return r ? r->mask : (ComponentMask)0;
}

static inline uint64_t broken_recycle_query(broken_recycle* s, ComponentMask required,
                                            uint64_t salt) {
  uint64_t acc = 0;
  const size_t n = s->records.size;
  for (size_t i = 0; i < n; ++i) {
    const broken_recycle_record_* r = &s->records.data[i];
    if (r->alive && (r->mask & required) == required)
      acc += gds_digest_entity(required, r->values, salt);
  }
  return acc;
}

static inline void broken_recycle_integrate(broken_recycle* s, float dt) {
  const ComponentMask need = GDS_K_POSITION | GDS_K_VELOCITY;
  const size_t n = s->records.size;
  for (size_t i = 0; i < n; ++i) {
    broken_recycle_record_* r = &s->records.data[i];
    if (!r->alive || (r->mask & need) != need) continue;
    Position* p = &r->values[(int)GDS_POSITION].position;
    const Velocity* v = &r->values[(int)GDS_VELOCITY].velocity;
    p->x += v->x * dt;
    p->y += v->y * dt;
    p->z += v->z * dt;
  }
}

static inline void broken_recycle_sync(broken_recycle* s) { (void)s; }
static inline size_t broken_recycle_entity_count(broken_recycle* s) { return s->live; }
static inline size_t broken_recycle_reported_bytes(broken_recycle* s) {
  return gds_vec_bytes(s->records) + gds_vec_bytes(s->free_list);
}

#endif
