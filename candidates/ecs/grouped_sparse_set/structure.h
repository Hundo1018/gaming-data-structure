#ifndef CANDIDATE_ECS_GROUPED_SPARSE_SET_H
#define CANDIDATE_ECS_GROUPED_SPARSE_SET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gds/types.h"
#include "gds/vec.h"

typedef struct {
  GDS_VEC(uint32_t) sparse;       /* entity index -> dense position */
  GDS_VEC(uint32_t) dense;        /* dense position -> entity index */
  GDS_VEC(ComponentValue) values; /* parallel to dense */
} grouped_sparse_set_set_;

/* sparse_set with one owning group over {Position, Velocity}.
 *
 * Every entity holding both components sits in the first G positions of both
 * Position's and Velocity's dense arrays, at the same position in each. The
 * group is therefore two aligned dense prefixes: integrate walks [0, G) of both
 * value arrays in lockstep with no sparse lookup, and a query over a mask that
 * contains both can be driven from the group the same way. Entities holding
 * only one of the pair sit at position G or later in that component's array;
 * join_group and leave_group both rely on position G holding a non-member.
 *
 * The parent's code is reproduced here rather than included by path. In the C++
 * it had to be: every member of SparseSet is private there and it has no
 * extension point, and the group has to change what its insert and erase do to
 * the dense order, which neither a derived class nor a wrapper around its
 * public interface can reach. C hides nothing, but the port keeps the
 * reproduction, as it keeps every structure's C++ layout. The registry
 * (generation, mask, liveness, free list), the handle layout, get, set, mask
 * and the swap-erase are the parent's, unchanged; what differs is marked where
 * it happens. */
typedef struct {
  GDS_VEC(uint32_t) generation;
  GDS_VEC(ComponentMask) mask;
  GDS_VEC(uint8_t) alive;
  GDS_VEC(uint32_t) free_list;
  grouped_sparse_set_set_ sets[GDS_COMPONENT_COUNT];
  uint32_t group_size;
  size_t live;
} grouped_sparse_set;

#define GROUPED_SPARSE_SET_INVALID (~0u)
#define GROUPED_SPARSE_SET_POS ((int)GDS_POSITION)
#define GROUPED_SPARSE_SET_VEL ((int)GDS_VELOCITY)
#define GROUPED_SPARSE_SET_PAIR ((ComponentMask)(GDS_K_POSITION | GDS_K_VELOCITY))

static inline void grouped_sparse_set_init(grouped_sparse_set* s) { memset(s, 0, sizeof *s); }

static inline void grouped_sparse_set_free(grouped_sparse_set* s) {
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

static inline uint64_t grouped_sparse_set_query_group_(const grouped_sparse_set* s,
                                                       ComponentMask required, uint64_t salt) {
  const ComponentMask rest = (ComponentMask)(required & ~GROUPED_SPARSE_SET_PAIR);
  const grouped_sparse_set_set_* ps = &s->sets[GROUPED_SPARSE_SET_POS];
  const grouped_sparse_set_set_* vs = &s->sets[GROUPED_SPARSE_SET_VEL];
  uint64_t acc = 0;
  ComponentValue v[GDS_COMPONENT_COUNT];
  for (size_t k = 0; k < s->group_size; ++k) {
    if (rest) {
      const uint32_t i = ps->dense.data[k];
      if ((s->mask.data[i] & rest) != rest) continue;
      for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) {
        if (rest & (1u << c)) v[c] = s->sets[c].values.data[s->sets[c].sparse.data[i]];
      }
    }
    v[GROUPED_SPARSE_SET_POS] = ps->values.data[k];
    v[GROUPED_SPARSE_SET_VEL] = vs->values.data[k];
    acc += gds_digest_entity(required, v, salt);
  }
  return acc;
}

/* Driven from one component's set. When both of the pair are still to be
 * probed, a qualifying entity is a group member, so its Velocity sits at the
 * same position as its Position and one sparse lookup serves both.
 *
 * The mask test and the driver's read below differ from the parent for
 * reasons that do not depend on the group: the test covers only the
 * components still to be probed and is skipped when there are none, and the
 * driver's value is read at k instead of back through its own sparse array.
 * Restoring the parent's form of those two lines leaves a group-only variant,
 * which is the comparison that attributes a query-side gain to the group. */
static inline uint64_t grouped_sparse_set_query_set_(const grouped_sparse_set* s, int d,
                                                     ComponentMask required, uint64_t salt) {
  const ComponentMask rest = (ComponentMask)(required & ~(1u << d));
  const bool pair_rest = (rest & GROUPED_SPARSE_SET_PAIR) == GROUPED_SPARSE_SET_PAIR;
  const ComponentMask lookups =
      pair_rest ? (ComponentMask)(rest & ~GROUPED_SPARSE_SET_PAIR) : rest;
  const grouped_sparse_set_set_* ds = &s->sets[d];
  const grouped_sparse_set_set_* ps = &s->sets[GROUPED_SPARSE_SET_POS];
  const grouped_sparse_set_set_* vs = &s->sets[GROUPED_SPARSE_SET_VEL];
  uint64_t acc = 0;
  ComponentValue v[GDS_COMPONENT_COUNT];
  for (size_t k = 0; k < ds->dense.size; ++k) {
    const uint32_t i = ds->dense.data[k];
    if (rest && (s->mask.data[i] & rest) != rest) continue;
    v[d] = ds->values.data[k];
    if (pair_rest) {
      const uint32_t pos = ps->sparse.data[i];
      v[GROUPED_SPARSE_SET_POS] = ps->values.data[pos];
      v[GROUPED_SPARSE_SET_VEL] = vs->values.data[pos];
    }
    for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) {
      if (lookups & (1u << c)) v[c] = s->sets[c].values.data[s->sets[c].sparse.data[i]];
    }
    acc += gds_digest_entity(required, v, salt);
  }
  return acc;
}

static inline uint32_t grouped_sparse_set_resolve_(const grouped_sparse_set* s, Entity e) {
  const uint32_t i = (uint32_t)(e.bits & 0xFFFFFFFFu);
  if (i >= s->generation.size) return GROUPED_SPARSE_SET_INVALID;
  if (s->generation.data[i] != (uint32_t)(e.bits >> 32)) return GROUPED_SPARSE_SET_INVALID;
  if (!s->alive.data[i]) return GROUPED_SPARSE_SET_INVALID;
  return i;
}

/* Appends past the group, as the parent does. A new member of the pair is
 * brought into the prefix afterwards by join_group. */
static inline void grouped_sparse_set_insert_(grouped_sparse_set* s, int c, uint32_t i,
                                              const ComponentValue* v) {
  grouped_sparse_set_set_* set = &s->sets[c];
  set->sparse.data[i] = (uint32_t)set->dense.size;
  gds_vec_push(set->dense, i);
  gds_vec_push(set->values, *v);
}

/* The parent's swap-erase. For Position and Velocity, callers guarantee the
 * entity is not a group member here, so its position and the last position
 * are both at or past G and the element moved into the hole is not a member
 * either. */
static inline void grouped_sparse_set_erase_(grouped_sparse_set* s, int c, uint32_t i) {
  grouped_sparse_set_set_* set = &s->sets[c];
  const uint32_t pos = set->sparse.data[i];
  const uint32_t last = gds_vec_back(set->dense);
  set->dense.data[pos] = last;
  set->values.data[pos] = gds_vec_back(set->values);
  set->sparse.data[last] = pos;
  (void)gds_vec_pop(set->dense);
  (void)gds_vec_pop(set->values);
  set->sparse.data[i] = GROUPED_SPARSE_SET_INVALID;
}

static inline void grouped_sparse_set_swap_positions_(grouped_sparse_set_set_* set, uint32_t a,
                                                      uint32_t b) {
  if (a == b) return;
  const uint32_t ea = set->dense.data[a];
  const uint32_t eb = set->dense.data[b];
  set->dense.data[a] = eb;
  set->dense.data[b] = ea;
  const ComponentValue t = set->values.data[a];
  set->values.data[a] = set->values.data[b];
  set->values.data[b] = t;
  set->sparse.data[eb] = a;
  set->sparse.data[ea] = b;
}

/* The entity has just come to hold both components and sits at or past G in
 * each array; so does whatever is at position G, which is not a member. One
 * swap per array puts the entity at G in both, and the prefix grows over it. */
static inline void grouped_sparse_set_join_group_(grouped_sparse_set* s, uint32_t i) {
  grouped_sparse_set_swap_positions_(&s->sets[GROUPED_SPARSE_SET_POS],
                                     s->sets[GROUPED_SPARSE_SET_POS].sparse.data[i],
                                     s->group_size);
  grouped_sparse_set_swap_positions_(&s->sets[GROUPED_SPARSE_SET_VEL],
                                     s->sets[GROUPED_SPARSE_SET_VEL].sparse.data[i],
                                     s->group_size);
  ++s->group_size;
}

/* The reverse: the last member of the prefix is swapped into the leaving
 * entity's position in both arrays, which keeps the two aligned, and the
 * prefix shrinks off the leaving entity, which is left at G in both. */
static inline void grouped_sparse_set_leave_group_(grouped_sparse_set* s, uint32_t i) {
  --s->group_size;
  const uint32_t pos = s->sets[GROUPED_SPARSE_SET_POS].sparse.data[i];
  grouped_sparse_set_swap_positions_(&s->sets[GROUPED_SPARSE_SET_POS], pos, s->group_size);
  grouped_sparse_set_swap_positions_(&s->sets[GROUPED_SPARSE_SET_VEL], pos, s->group_size);
}

static inline Entity grouped_sparse_set_create(grouped_sparse_set* s, ComponentMask mask,
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
      gds_vec_push(s->sets[c].sparse, GROUPED_SPARSE_SET_INVALID);
  }
  s->mask.data[index] = mask;
  s->alive.data[index] = 1;
  for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) {
    if (mask & (1u << c)) grouped_sparse_set_insert_(s, c, index, &values[c]);
  }
  if ((mask & GROUPED_SPARSE_SET_PAIR) == GROUPED_SPARSE_SET_PAIR)
    grouped_sparse_set_join_group_(s, index);
  ++s->live;
  const Entity e = {((uint64_t)s->generation.data[index] << 32) | index};
  return e;
}

static inline void grouped_sparse_set_destroy(grouped_sparse_set* s, Entity e) {
  const uint32_t i = grouped_sparse_set_resolve_(s, e);
  if (i == GROUPED_SPARSE_SET_INVALID) return;
  /* Leaving first puts the entity at position G of both arrays, outside the
   * prefix, so the swap-erases below never move a group member. */
  if ((s->mask.data[i] & GROUPED_SPARSE_SET_PAIR) == GROUPED_SPARSE_SET_PAIR)
    grouped_sparse_set_leave_group_(s, i);
  for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) {
    if (s->mask.data[i] & (1u << c)) grouped_sparse_set_erase_(s, c, i);
  }
  s->mask.data[i] = 0;
  s->alive.data[i] = 0;
  ++s->generation.data[i];
  gds_vec_push(s->free_list, i);
  --s->live;
}

static inline bool grouped_sparse_set_alive(grouped_sparse_set* s, Entity e) {
  return grouped_sparse_set_resolve_(s, e) != GROUPED_SPARSE_SET_INVALID;
}

static inline void grouped_sparse_set_add(grouped_sparse_set* s, Entity e, ComponentId c,
                                          const ComponentValue* v) {
  const uint32_t i = grouped_sparse_set_resolve_(s, e);
  if (i == GROUPED_SPARSE_SET_INVALID) return;
  const int ci = (int)c;
  const ComponentMask bit = gds_mask_of(c);
  if (s->mask.data[i] & bit) {
    s->sets[ci].values.data[s->sets[ci].sparse.data[i]] = *v;
    return;
  }
  s->mask.data[i] |= bit;
  grouped_sparse_set_insert_(s, ci, i, v);
  if ((bit & GROUPED_SPARSE_SET_PAIR) &&
      (s->mask.data[i] & GROUPED_SPARSE_SET_PAIR) == GROUPED_SPARSE_SET_PAIR)
    grouped_sparse_set_join_group_(s, i);
}

static inline void grouped_sparse_set_remove(grouped_sparse_set* s, Entity e, ComponentId c) {
  const uint32_t i = grouped_sparse_set_resolve_(s, e);
  const ComponentMask bit = gds_mask_of(c);
  if (i == GROUPED_SPARSE_SET_INVALID || !(s->mask.data[i] & bit)) return;
  if ((bit & GROUPED_SPARSE_SET_PAIR) &&
      (s->mask.data[i] & GROUPED_SPARSE_SET_PAIR) == GROUPED_SPARSE_SET_PAIR)
    grouped_sparse_set_leave_group_(s, i);
  s->mask.data[i] &= (ComponentMask)~bit;
  grouped_sparse_set_erase_(s, (int)c, i);
}

static inline bool grouped_sparse_set_get(grouped_sparse_set* s, Entity e, ComponentId c,
                                          ComponentValue* out) {
  const uint32_t i = grouped_sparse_set_resolve_(s, e);
  if (i == GROUPED_SPARSE_SET_INVALID || !(s->mask.data[i] & gds_mask_of(c))) return false;
  *out = s->sets[(int)c].values.data[s->sets[(int)c].sparse.data[i]];
  return true;
}

static inline bool grouped_sparse_set_set(grouped_sparse_set* s, Entity e, ComponentId c,
                                          const ComponentValue* v) {
  const uint32_t i = grouped_sparse_set_resolve_(s, e);
  if (i == GROUPED_SPARSE_SET_INVALID || !(s->mask.data[i] & gds_mask_of(c))) return false;
  s->sets[(int)c].values.data[s->sets[(int)c].sparse.data[i]] = *v;
  return true;
}

static inline ComponentMask grouped_sparse_set_mask(grouped_sparse_set* s, Entity e) {
  const uint32_t i = grouped_sparse_set_resolve_(s, e);
  return i == GROUPED_SPARSE_SET_INVALID ? (ComponentMask)0 : s->mask.data[i];
}

/* Drives from the smallest of the group, when the mask names both of its
 * components, and each required component's set. Ties go to the group,
 * because it is the only driver from which two components are read without a
 * lookup; since G never exceeds either set of the pair, a mask containing
 * both is never driven from Position's or Velocity's set. Only the required
 * components the driver does not already supply are probed. */
static inline uint64_t grouped_sparse_set_query(grouped_sparse_set* s, ComponentMask required,
                                                uint64_t salt) {
  if (required == 0) return 0;
  const bool from_group = (required & GROUPED_SPARSE_SET_PAIR) == GROUPED_SPARSE_SET_PAIR;
  int driver = -1;
  size_t best = from_group ? (size_t)s->group_size : ~(size_t)0;
  for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) {
    if (!(required & (1u << c))) continue;
    if (s->sets[c].dense.size < best) {
      best = s->sets[c].dense.size;
      driver = c;
    }
  }
  if (driver >= 0) return grouped_sparse_set_query_set_(s, driver, required, salt);
  return from_group ? grouped_sparse_set_query_group_(s, required, salt) : 0;
}

/* The aligned walk: position k of Position's values and position k of
 * Velocity's values belong to the same entity for every k below G, and every
 * entity holding both is below G, so no mask test is needed either. */
static inline void grouped_sparse_set_integrate(grouped_sparse_set* s, float dt) {
  ComponentValue* restrict p = s->sets[GROUPED_SPARSE_SET_POS].values.data;
  const ComponentValue* restrict v = s->sets[GROUPED_SPARSE_SET_VEL].values.data;
  const size_t n = s->group_size;
  for (size_t k = 0; k < n; ++k) {
    p[k].position.x += v[k].velocity.x * dt;
    p[k].position.y += v[k].velocity.y * dt;
    p[k].position.z += v[k].velocity.z * dt;
  }
}

static inline void grouped_sparse_set_sync(grouped_sparse_set* s) { (void)s; }

static inline size_t grouped_sparse_set_entity_count(grouped_sparse_set* s) { return s->live; }

/* The group adds no allocation: it is an ordering of the parent's arrays and
 * one counter. */
static inline size_t grouped_sparse_set_reported_bytes(grouped_sparse_set* s) {
  size_t b = gds_vec_bytes(s->generation) + gds_vec_bytes(s->mask) + gds_vec_bytes(s->alive) +
             gds_vec_bytes(s->free_list);
  for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) {
    const grouped_sparse_set_set_* set = &s->sets[c];
    b += gds_vec_bytes(set->sparse) + gds_vec_bytes(set->dense) + gds_vec_bytes(set->values);
  }
  return b;
}

#endif
