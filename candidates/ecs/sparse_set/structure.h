#ifndef CANDIDATE_ECS_SPARSE_SET_H
#define CANDIDATE_ECS_SPARSE_SET_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gds/types.h"
#include "gds/vec.h"

typedef struct {
  GDS_VEC(uint32_t) sparse;       /* entity index -> dense position */
  GDS_VEC(uint32_t) dense;        /* dense position -> entity index */
  GDS_VEC(ComponentValue) values; /* parallel to dense */
} sparse_set_set_;

/* One sparse set per component: a dense, packed array of values plus a sparse
 * index from entity index to dense position. Component values are contiguous
 * with no holes, so a query pays only for entities that actually carry the
 * component; adding or removing a component touches one component's arrays and
 * never relocates the entity. */
typedef struct {
  GDS_VEC(uint32_t) generation;
  GDS_VEC(ComponentMask) mask;
  GDS_VEC(uint8_t) alive;
  GDS_VEC(uint32_t) free_list;
  sparse_set_set_ sets[GDS_COMPONENT_COUNT];
  size_t live;
} sparse_set;

#define SPARSE_SET_INVALID (~0u)

static inline void sparse_set_init(sparse_set* s) { memset(s, 0, sizeof *s); }

static inline void sparse_set_free(sparse_set* s) {
  gds_vec_free(s->generation);
  gds_vec_free(s->mask);
  gds_vec_free(s->alive);
  gds_vec_free(s->free_list);
  for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) {
    gds_vec_free(s->sets[c].sparse);
    gds_vec_free(s->sets[c].dense);
    gds_vec_free(s->sets[c].values);
  }
}

static inline uint32_t sparse_set_resolve_(const sparse_set* s, Entity e) {
  const uint32_t i = (uint32_t)(e.bits & 0xFFFFFFFFu);
  if (i >= s->generation.size) return SPARSE_SET_INVALID;
  if (s->generation.data[i] != (uint32_t)(e.bits >> 32)) return SPARSE_SET_INVALID;
  if (!s->alive.data[i]) return SPARSE_SET_INVALID;
  return i;
}

static inline void sparse_set_insert_(sparse_set* s, int c, uint32_t i, const ComponentValue* v) {
  sparse_set_set_* set = &s->sets[c];
  set->sparse.data[i] = (uint32_t)set->dense.size;
  gds_vec_push(set->dense, i);
  gds_vec_push(set->values, *v);
}

static inline void sparse_set_erase_(sparse_set* s, int c, uint32_t i) {
  sparse_set_set_* set = &s->sets[c];
  const uint32_t pos = set->sparse.data[i];
  const uint32_t last = gds_vec_back(set->dense);
  set->dense.data[pos] = last;
  set->values.data[pos] = gds_vec_back(set->values);
  set->sparse.data[last] = pos;
  (void)gds_vec_pop(set->dense);
  (void)gds_vec_pop(set->values);
  set->sparse.data[i] = SPARSE_SET_INVALID;
}

static inline Entity sparse_set_create(sparse_set* s, ComponentMask mask,
                                       const ComponentValue* values) {
  uint32_t index;
  if (s->free_list.size) {
    index = gds_vec_pop(s->free_list);
  } else {
    index = (uint32_t)s->generation.size;
    gds_vec_push(s->generation, 1u);
    gds_vec_push(s->mask, 0);
    gds_vec_push(s->alive, 0);
    for (int c = 0; c < GDS_COMPONENT_COUNT; ++c)
      gds_vec_push(s->sets[c].sparse, SPARSE_SET_INVALID);
  }
  s->mask.data[index] = mask;
  s->alive.data[index] = 1;
  for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) {
    if (mask & (1u << c)) sparse_set_insert_(s, c, index, &values[c]);
  }
  ++s->live;
  const Entity e = {((uint64_t)s->generation.data[index] << 32) | index};
  return e;
}

static inline void sparse_set_destroy(sparse_set* s, Entity e) {
  const uint32_t i = sparse_set_resolve_(s, e);
  if (i == SPARSE_SET_INVALID) return;
  for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) {
    if (s->mask.data[i] & (1u << c)) sparse_set_erase_(s, c, i);
  }
  s->mask.data[i] = 0;
  s->alive.data[i] = 0;
  ++s->generation.data[i];
  gds_vec_push(s->free_list, i);
  --s->live;
}

static inline bool sparse_set_alive(sparse_set* s, Entity e) {
  return sparse_set_resolve_(s, e) != SPARSE_SET_INVALID;
}

static inline void sparse_set_add(sparse_set* s, Entity e, ComponentId c, const ComponentValue* v) {
  const uint32_t i = sparse_set_resolve_(s, e);
  if (i == SPARSE_SET_INVALID) return;
  const int ci = (int)c;
  if (s->mask.data[i] & gds_mask_of(c)) {
    s->sets[ci].values.data[s->sets[ci].sparse.data[i]] = *v;
    return;
  }
  s->mask.data[i] |= gds_mask_of(c);
  sparse_set_insert_(s, ci, i, v);
}

static inline void sparse_set_remove(sparse_set* s, Entity e, ComponentId c) {
  const uint32_t i = sparse_set_resolve_(s, e);
  if (i == SPARSE_SET_INVALID || !(s->mask.data[i] & gds_mask_of(c))) return;
  s->mask.data[i] &= (ComponentMask)~gds_mask_of(c);
  sparse_set_erase_(s, (int)c, i);
}

static inline bool sparse_set_get(sparse_set* s, Entity e, ComponentId c, ComponentValue* out) {
  const uint32_t i = sparse_set_resolve_(s, e);
  if (i == SPARSE_SET_INVALID || !(s->mask.data[i] & gds_mask_of(c))) return false;
  *out = s->sets[(int)c].values.data[s->sets[(int)c].sparse.data[i]];
  return true;
}

static inline bool sparse_set_set(sparse_set* s, Entity e, ComponentId c, const ComponentValue* v) {
  const uint32_t i = sparse_set_resolve_(s, e);
  if (i == SPARSE_SET_INVALID || !(s->mask.data[i] & gds_mask_of(c))) return false;
  s->sets[(int)c].values.data[s->sets[(int)c].sparse.data[i]] = *v;
  return true;
}

static inline ComponentMask sparse_set_mask(sparse_set* s, Entity e) {
  const uint32_t i = sparse_set_resolve_(s, e);
  return i == SPARSE_SET_INVALID ? (ComponentMask)0 : s->mask.data[i];
}

/* Iterates the smallest participating set and probes the others. The choice
 * of driving set is what makes the cost proportional to the rarest component
 * rather than to the entity count. */
static inline uint64_t sparse_set_query(sparse_set* s, ComponentMask required, uint64_t salt) {
  if (required == 0) return 0;
  int driver = -1;
  size_t best = ~(size_t)0;
  for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) {
    if (!(required & (1u << c))) continue;
    if (s->sets[c].dense.size < best) {
      best = s->sets[c].dense.size;
      driver = c;
    }
  }
  uint64_t acc = 0;
  ComponentValue v[GDS_COMPONENT_COUNT];
  const sparse_set_set_* d = &s->sets[driver];
  for (size_t k = 0; k < d->dense.size; ++k) {
    const uint32_t i = d->dense.data[k];
    if ((s->mask.data[i] & required) != required) continue;
    for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) {
      if (required & (1u << c)) v[c] = s->sets[c].values.data[s->sets[c].sparse.data[i]];
    }
    acc += gds_digest_entity(required, v, salt);
  }
  return acc;
}

static inline void sparse_set_integrate(sparse_set* s, float dt) {
  const int pi = (int)GDS_POSITION;
  const int vi = (int)GDS_VELOCITY;
  const sparse_set_set_* vs = &s->sets[vi];
  sparse_set_set_* ps = &s->sets[pi];
  const ComponentMask need = GDS_K_POSITION | GDS_K_VELOCITY;
  for (size_t k = 0; k < vs->dense.size; ++k) {
    const uint32_t i = vs->dense.data[k];
    if ((s->mask.data[i] & need) != need) continue;
    Position* p = &ps->values.data[ps->sparse.data[i]].position;
    const Velocity* v = &vs->values.data[k].velocity;
    p->x += v->x * dt;
    p->y += v->y * dt;
    p->z += v->z * dt;
  }
}

static inline void sparse_set_sync(sparse_set* s) { (void)s; }

static inline size_t sparse_set_entity_count(sparse_set* s) { return s->live; }

static inline size_t sparse_set_reported_bytes(sparse_set* s) {
  size_t b = gds_vec_bytes(s->generation) + gds_vec_bytes(s->mask) + gds_vec_bytes(s->alive) +
             gds_vec_bytes(s->free_list);
  for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) {
    const sparse_set_set_* set = &s->sets[c];
    b += gds_vec_bytes(set->sparse) + gds_vec_bytes(set->dense) + gds_vec_bytes(set->values);
  }
  return b;
}

#endif
