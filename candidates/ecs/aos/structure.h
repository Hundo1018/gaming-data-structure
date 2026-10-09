#ifndef CANDIDATE_ECS_AOS_H
#define CANDIDATE_ECS_AOS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gds/types.h"
#include "gds/vec.h"

/* An entity with no components is still a live entity, so liveness is a
 * separate bit and never inferred from the mask. */
typedef struct {
  uint32_t generation;
  ComponentMask mask;
  uint8_t alive;
  ComponentValue values[GDS_COMPONENT_COUNT];
} aos_record_;

/* One record per slot, every component inline, whether present or not.
 * Storage is a flat array indexed by the handle's index field; a generation
 * counter in the handle invalidates stale references. Nothing is ever moved,
 * so a handle stays valid for the whole life of the entity. */
typedef struct {
  GDS_VEC(aos_record_) records;
  GDS_VEC(uint32_t) free_list;
  size_t live;
} aos;

static inline void aos_init(aos* s) { memset(s, 0, sizeof *s); }

static inline void aos_free(aos* s) {
  gds_vec_free(s->records);
  gds_vec_free(s->free_list);
}

static inline Entity aos_make_handle_(uint32_t index, uint32_t generation) {
  Entity e = {((uint64_t)generation << 32) | index};
  return e;
}
static inline uint32_t aos_index_of_(Entity e) { return (uint32_t)(e.bits & 0xFFFFFFFFu); }
static inline uint32_t aos_generation_of_(Entity e) { return (uint32_t)(e.bits >> 32); }

static inline aos_record_* aos_resolve_(aos* s, Entity e) {
  const uint32_t i = aos_index_of_(e);
  if (i >= s->records.size) return NULL;
  aos_record_* r = &s->records.data[i];
  if (r->generation != aos_generation_of_(e) || !r->alive) return NULL;
  return r;
}

static inline Entity aos_create(aos* s, ComponentMask mask, const ComponentValue* values) {
  uint32_t index;
  if (s->free_list.size) {
    index = gds_vec_pop(s->free_list);
  } else {
    index = (uint32_t)s->records.size;
    aos_record_ r0;
    memset(&r0, 0, sizeof r0);
    gds_vec_push(s->records, r0);
    gds_vec_back(s->records).generation = 1;
  }
  aos_record_* r = &s->records.data[index];
  r->alive = 1;
  r->mask = mask;
  for (int i = 0; i < GDS_COMPONENT_COUNT; ++i) {
    if (mask & (1u << i)) r->values[i] = values[i];
  }
  ++s->live;
  return aos_make_handle_(index, r->generation);
}

static inline void aos_destroy(aos* s, Entity e) {
  aos_record_* r = aos_resolve_(s, e);
  if (!r) return;
  r->mask = 0;
  r->alive = 0;
  ++r->generation; /* every outstanding handle to this slot is now stale */
  gds_vec_push(s->free_list, aos_index_of_(e));
  --s->live;
}

static inline bool aos_alive(aos* s, Entity e) { return aos_resolve_(s, e) != NULL; }

static inline void aos_add(aos* s, Entity e, ComponentId c, const ComponentValue* v) {
  aos_record_* r = aos_resolve_(s, e);
  if (!r) return;
  r->mask |= gds_mask_of(c);
  r->values[(int)c] = *v;
}

static inline void aos_remove(aos* s, Entity e, ComponentId c) {
  aos_record_* r = aos_resolve_(s, e);
  if (!r) return;
  r->mask &= (ComponentMask)~gds_mask_of(c);
}

static inline bool aos_get(aos* s, Entity e, ComponentId c, ComponentValue* out) {
  const aos_record_* r = aos_resolve_(s, e);
  if (!r || !(r->mask & gds_mask_of(c))) return false;
  *out = r->values[(int)c];
  return true;
}

static inline bool aos_set(aos* s, Entity e, ComponentId c, const ComponentValue* v) {
  aos_record_* r = aos_resolve_(s, e);
  if (!r || !(r->mask & gds_mask_of(c))) return false;
  r->values[(int)c] = *v;
  return true;
}

static inline ComponentMask aos_mask(aos* s, Entity e) {
  const aos_record_* r = aos_resolve_(s, e);
  return r ? r->mask : (ComponentMask)0;
}

static inline uint64_t aos_query(aos* s, ComponentMask required, uint64_t salt) {
  uint64_t acc = 0;
  const size_t n = s->records.size;
  for (size_t i = 0; i < n; ++i) {
    const aos_record_* r = &s->records.data[i];
    if (r->alive && (r->mask & required) == required) {
      acc += gds_digest_entity(required, r->values, salt);
    }
  }
  return acc;
}

static inline void aos_integrate(aos* s, float dt) {
  const ComponentMask need = GDS_K_POSITION | GDS_K_VELOCITY;
  const size_t n = s->records.size;
  for (size_t i = 0; i < n; ++i) {
    aos_record_* r = &s->records.data[i];
    if (!r->alive || (r->mask & need) != need) continue;
    Position* p = &r->values[(int)GDS_POSITION].position;
    const Velocity* v = &r->values[(int)GDS_VELOCITY].velocity;
    p->x += v->x * dt;
    p->y += v->y * dt;
    p->z += v->z * dt;
  }
}

static inline void aos_sync(aos* s) { (void)s; }

static inline size_t aos_entity_count(aos* s) { return s->live; }

static inline size_t aos_reported_bytes(aos* s) {
  return gds_vec_bytes(s->records) + gds_vec_bytes(s->free_list);
}

#endif
