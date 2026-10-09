/* The ECS oracle.
 *
 * Slow, obvious, and written so that its correctness is easy to see by reading
 * it. Every candidate is checked against this, never against another candidate.
 *
 * It is a chained hash map from a never-reused id to one separately allocated
 * record per entity: the shape of the std::unordered_map it was before the
 * port, kept because the `reference` candidate is measured too and its notes
 * are about exactly that shape — a node per entity and a dependent chain of
 * loads to reach it. */
#ifndef GDS_REFERENCE_H
#define GDS_REFERENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "gds/types.h"

typedef struct reference_node {
  uint64_t key;
  ComponentMask mask;
  ComponentValue values[GDS_COMPONENT_COUNT];
  struct reference_node* next;
} reference_node;

typedef struct reference {
  reference_node** buckets;
  size_t n_buckets; /* a power of two */
  size_t size;
  uint64_t next_id;
} reference;

static inline void reference_init(reference* s) {
  s->n_buckets = 16;
  s->buckets = calloc(s->n_buckets, sizeof *s->buckets);
  s->size = 0;
  s->next_id = 1;
}

static inline void reference_free(reference* s) {
  for (size_t b = 0; b < s->n_buckets; ++b) {
    reference_node* n = s->buckets[b];
    while (n) {
      reference_node* next = n->next;
      free(n);
      n = next;
    }
  }
  free(s->buckets);
  s->buckets = NULL;
  s->n_buckets = s->size = 0;
}

static inline reference_node* reference_find_(reference* s, uint64_t key) {
  for (reference_node* n = s->buckets[key & (s->n_buckets - 1)]; n; n = n->next)
    if (n->key == key) return n;
  return NULL;
}

/* Doubles the table when it holds more records than buckets. */
static inline void reference_grow_(reference* s) {
  const size_t nb = s->n_buckets * 2;
  reference_node** fresh = calloc(nb, sizeof *fresh);
  for (size_t b = 0; b < s->n_buckets; ++b) {
    reference_node* n = s->buckets[b];
    while (n) {
      reference_node* next = n->next;
      const size_t i = n->key & (nb - 1);
      n->next = fresh[i];
      fresh[i] = n;
      n = next;
    }
  }
  free(s->buckets);
  s->buckets = fresh;
  s->n_buckets = nb;
}

static inline Entity reference_create(reference* s, ComponentMask mask,
                                      const ComponentValue* values) {
  const uint64_t id = s->next_id++;
  reference_node* n = calloc(1, sizeof *n);
  n->key = id;
  n->mask = mask;
  for (int i = 0; i < GDS_COMPONENT_COUNT; ++i)
    if (mask & (1u << i)) n->values[i] = values[i];
  if (s->size + 1 > s->n_buckets) reference_grow_(s);
  const size_t b = id & (s->n_buckets - 1);
  n->next = s->buckets[b];
  s->buckets[b] = n;
  ++s->size;
  Entity e = {id};
  return e;
}

static inline void reference_destroy(reference* s, Entity e) {
  reference_node** link = &s->buckets[e.bits & (s->n_buckets - 1)];
  for (reference_node* n = *link; n; link = &n->next, n = n->next) {
    if (n->key == e.bits) {
      *link = n->next;
      free(n);
      --s->size;
      return;
    }
  }
}

static inline bool reference_alive(reference* s, Entity e) { return reference_find_(s, e.bits) != NULL; }

static inline void reference_add(reference* s, Entity e, ComponentId c, const ComponentValue* v) {
  reference_node* n = reference_find_(s, e.bits);
  if (!n) return;
  n->mask |= gds_mask_of(c);
  n->values[c] = *v;
}

static inline void reference_remove(reference* s, Entity e, ComponentId c) {
  reference_node* n = reference_find_(s, e.bits);
  if (!n) return;
  n->mask &= (ComponentMask)~gds_mask_of(c);
}

static inline bool reference_get(reference* s, Entity e, ComponentId c, ComponentValue* out) {
  reference_node* n = reference_find_(s, e.bits);
  if (!n || !(n->mask & gds_mask_of(c))) return false;
  *out = n->values[c];
  return true;
}

static inline bool reference_set(reference* s, Entity e, ComponentId c, const ComponentValue* v) {
  reference_node* n = reference_find_(s, e.bits);
  if (!n || !(n->mask & gds_mask_of(c))) return false;
  n->values[c] = *v;
  return true;
}

static inline ComponentMask reference_mask(reference* s, Entity e) {
  reference_node* n = reference_find_(s, e.bits);
  return n ? n->mask : 0;
}

static inline uint64_t reference_query(reference* s, ComponentMask required, uint64_t salt) {
  uint64_t acc = 0;
  for (size_t b = 0; b < s->n_buckets; ++b)
    for (reference_node* n = s->buckets[b]; n; n = n->next)
      if ((n->mask & required) == required) acc += gds_digest_entity(required, n->values, salt);
  return acc;
}

static inline void reference_integrate(reference* s, float dt) {
  const ComponentMask need = GDS_K_POSITION | GDS_K_VELOCITY;
  for (size_t b = 0; b < s->n_buckets; ++b) {
    for (reference_node* n = s->buckets[b]; n; n = n->next) {
      if ((n->mask & need) != need) continue;
      Position* p = &n->values[GDS_POSITION].position;
      const Velocity* v = &n->values[GDS_VELOCITY].velocity;
      p->x += v->x * dt;
      p->y += v->y * dt;
      p->z += v->z * dt;
    }
  }
}

static inline void reference_sync(reference* s) { (void)s; }

static inline size_t reference_entity_count(reference* s) { return s->size; }

static inline size_t reference_reported_bytes(reference* s) {
  return s->size * sizeof(reference_node) + s->n_buckets * sizeof(reference_node*);
}

#endif
