#ifndef CANDIDATE_ECS_SOA_H
#define CANDIDATE_ECS_SOA_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gds/types.h"
#include "gds/vec.h"

/* Same index space as `aos`, but each component lives in its own array.
 * A query over Position+Velocity streams three arrays (mask, position,
 * velocity) and never loads Health or Tag, so the bytes moved per matching
 * entity fall while the number of streams rises. */
typedef struct {
  GDS_VEC(uint32_t) generation;
  GDS_VEC(ComponentMask) mask;
  GDS_VEC(uint8_t) alive;
  GDS_VEC(Position) position;
  GDS_VEC(Velocity) velocity;
  GDS_VEC(Health) health;
  GDS_VEC(Tag) tag;
  GDS_VEC(uint32_t) free_list;
  size_t live;
} soa;

#define SOA_INVALID (~0u)

static inline void soa_init(soa* s) { memset(s, 0, sizeof *s); }

static inline void soa_free(soa* s) {
  gds_vec_free(s->generation);
  gds_vec_free(s->mask);
  gds_vec_free(s->alive);
  gds_vec_free(s->position);
  gds_vec_free(s->velocity);
  gds_vec_free(s->health);
  gds_vec_free(s->tag);
  gds_vec_free(s->free_list);
}

static inline Entity soa_make_handle_(uint32_t index, uint32_t generation) {
  Entity e = {((uint64_t)generation << 32) | index};
  return e;
}

static inline uint32_t soa_resolve_(const soa* s, Entity e) {
  const uint32_t i = (uint32_t)(e.bits & 0xFFFFFFFFu);
  if (i >= s->generation.size) return SOA_INVALID;
  if (s->generation.data[i] != (uint32_t)(e.bits >> 32)) return SOA_INVALID;
  if (!s->alive.data[i]) return SOA_INVALID;
  return i;
}

static inline void soa_store_(soa* s, uint32_t i, ComponentId c, const ComponentValue* v) {
  switch (c) {
    case GDS_POSITION: s->position.data[i] = v->position; break;
    case GDS_VELOCITY: s->velocity.data[i] = v->velocity; break;
    case GDS_HEALTH: s->health.data[i] = v->health; break;
    case GDS_TAG: s->tag.data[i] = v->tag; break;
  }
}

static inline void soa_load_(const soa* s, uint32_t i, ComponentId c, ComponentValue* out) {
  switch (c) {
    case GDS_POSITION: out->position = s->position.data[i]; break;
    case GDS_VELOCITY: out->velocity = s->velocity.data[i]; break;
    case GDS_HEALTH: out->health = s->health.data[i]; break;
    case GDS_TAG: out->tag = s->tag.data[i]; break;
  }
}

static inline Entity soa_create(soa* s, ComponentMask mask, const ComponentValue* values) {
  uint32_t index;
  if (s->free_list.size) {
    index = gds_vec_pop(s->free_list);
  } else {
    index = (uint32_t)s->generation.size;
    const Position p0 = {0, 0, 0};
    const Velocity v0 = {0, 0, 0};
    const Health h0 = {0, 0};
    const Tag t0 = {0};
    gds_vec_push(s->generation, 1u);
    gds_vec_push(s->mask, 0);
    gds_vec_push(s->alive, 0);
    gds_vec_push(s->position, p0);
    gds_vec_push(s->velocity, v0);
    gds_vec_push(s->health, h0);
    gds_vec_push(s->tag, t0);
  }
  s->mask.data[index] = mask;
  s->alive.data[index] = 1;
  if (mask & GDS_K_POSITION) s->position.data[index] = values[0].position;
  if (mask & GDS_K_VELOCITY) s->velocity.data[index] = values[1].velocity;
  if (mask & GDS_K_HEALTH) s->health.data[index] = values[2].health;
  if (mask & GDS_K_TAG) s->tag.data[index] = values[3].tag;
  ++s->live;
  return soa_make_handle_(index, s->generation.data[index]);
}

static inline void soa_destroy(soa* s, Entity e) {
  const uint32_t i = soa_resolve_(s, e);
  if (i == SOA_INVALID) return;
  s->mask.data[i] = 0;
  s->alive.data[i] = 0;
  ++s->generation.data[i];
  gds_vec_push(s->free_list, i);
  --s->live;
}

static inline bool soa_alive(soa* s, Entity e) { return soa_resolve_(s, e) != SOA_INVALID; }

static inline void soa_add(soa* s, Entity e, ComponentId c, const ComponentValue* v) {
  const uint32_t i = soa_resolve_(s, e);
  if (i == SOA_INVALID) return;
  s->mask.data[i] |= gds_mask_of(c);
  soa_store_(s, i, c, v);
}

static inline void soa_remove(soa* s, Entity e, ComponentId c) {
  const uint32_t i = soa_resolve_(s, e);
  if (i == SOA_INVALID) return;
  s->mask.data[i] &= (ComponentMask)~gds_mask_of(c);
}

static inline bool soa_get(soa* s, Entity e, ComponentId c, ComponentValue* out) {
  const uint32_t i = soa_resolve_(s, e);
  if (i == SOA_INVALID || !(s->mask.data[i] & gds_mask_of(c))) return false;
  soa_load_(s, i, c, out);
  return true;
}

static inline bool soa_set(soa* s, Entity e, ComponentId c, const ComponentValue* v) {
  const uint32_t i = soa_resolve_(s, e);
  if (i == SOA_INVALID || !(s->mask.data[i] & gds_mask_of(c))) return false;
  soa_store_(s, i, c, v);
  return true;
}

static inline ComponentMask soa_mask(soa* s, Entity e) {
  const uint32_t i = soa_resolve_(s, e);
  return i == SOA_INVALID ? (ComponentMask)0 : s->mask.data[i];
}

static inline uint64_t soa_query(soa* s, ComponentMask required, uint64_t salt) {
  uint64_t acc = 0;
  const size_t n = s->mask.size;
  ComponentValue v[GDS_COMPONENT_COUNT];
  for (size_t i = 0; i < n; ++i) {
    if (!s->alive.data[i] || (s->mask.data[i] & required) != required) continue;
    if (required & GDS_K_POSITION) v[0].position = s->position.data[i];
    if (required & GDS_K_VELOCITY) v[1].velocity = s->velocity.data[i];
    if (required & GDS_K_HEALTH) v[2].health = s->health.data[i];
    if (required & GDS_K_TAG) v[3].tag = s->tag.data[i];
    acc += gds_digest_entity(required, v, salt);
  }
  return acc;
}

static inline void soa_integrate(soa* s, float dt) {
  const ComponentMask need = GDS_K_POSITION | GDS_K_VELOCITY;
  const size_t n = s->mask.size;
  for (size_t i = 0; i < n; ++i) {
    if (!s->alive.data[i] || (s->mask.data[i] & need) != need) continue;
    Position* p = &s->position.data[i];
    const Velocity* v = &s->velocity.data[i];
    p->x += v->x * dt;
    p->y += v->y * dt;
    p->z += v->z * dt;
  }
}

static inline void soa_sync(soa* s) { (void)s; }

static inline size_t soa_entity_count(soa* s) { return s->live; }

static inline size_t soa_reported_bytes(soa* s) {
  return gds_vec_bytes(s->generation) + gds_vec_bytes(s->mask) + gds_vec_bytes(s->alive) +
         gds_vec_bytes(s->position) + gds_vec_bytes(s->velocity) + gds_vec_bytes(s->health) +
         gds_vec_bytes(s->tag) + gds_vec_bytes(s->free_list);
}

#endif
