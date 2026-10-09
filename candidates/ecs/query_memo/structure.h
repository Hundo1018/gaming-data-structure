#ifndef CANDIDATE_ECS_QUERY_MEMO_H
#define CANDIDATE_ECS_QUERY_MEMO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ecs/soa/structure.h"
#include "gds/types.h"

enum { QUERY_MEMO_MASKS = 1 << GDS_COMPONENT_COUNT };

typedef struct {
  uint64_t sum;
  uint64_t salt;
  bool tracked;
  bool stale;
} query_memo_memo_;

/* soa with every query answer kept as a running total instead of recomputed.
 *
 * A query returns the sum of gds_digest_entity over the matching entities, and
 * a sum can be maintained: subtract an entity's digest before it changes, add
 * it back after. Once a mask has been asked for, its query costs nothing, and
 * each mutation costs two digests per tracked mask it touches. integrate
 * changes every position at once, so it marks the masks it touches stale and
 * the next query recomputes them with one ordinary pass.
 *
 * This is not a data structure anyone would ship. It answers a question about
 * the harness: whether a candidate can be fast by never finding the answer at
 * all, as long as it can compute what the answer's digest would have been. */
typedef struct {
  soa inner;
  query_memo_memo_ memo[QUERY_MEMO_MASKS];
} query_memo;

static inline void query_memo_init(query_memo* s) {
  memset(s, 0, sizeof *s);
  soa_init(&s->inner);
}

static inline void query_memo_free(query_memo* s) { soa_free(&s->inner); }

/* Adds (sign +1) or removes (sign -1) the entity's contribution to every
 * tracked, current total whose mask it satisfies. */
static inline void query_memo_credit_(query_memo* s, Entity e, int sign) {
  const ComponentMask held = soa_mask(&s->inner, e);
  if (!held) return;
  for (int m = 1; m < QUERY_MEMO_MASKS; ++m) {
    query_memo_memo_* memo = &s->memo[m];
    if (!memo->tracked || memo->stale) continue;
    const ComponentMask req = (ComponentMask)m;
    if ((held & req) != req) continue;
    ComponentValue v[GDS_COMPONENT_COUNT];
    for (int i = 0; i < GDS_COMPONENT_COUNT; ++i) {
      if (req & (1u << i)) soa_get(&s->inner, e, (ComponentId)i, &v[i]);
    }
    const uint64_t d = gds_digest_entity(req, v, memo->salt);
    memo->sum = sign > 0 ? memo->sum + d : memo->sum - d;
  }
}

static inline Entity query_memo_create(query_memo* s, ComponentMask mask,
                                       const ComponentValue* values) {
  const Entity e = soa_create(&s->inner, mask, values);
  query_memo_credit_(s, e, +1);
  return e;
}

static inline void query_memo_destroy(query_memo* s, Entity e) {
  query_memo_credit_(s, e, -1);
  soa_destroy(&s->inner, e);
}

static inline bool query_memo_alive(query_memo* s, Entity e) { return soa_alive(&s->inner, e); }

static inline void query_memo_add(query_memo* s, Entity e, ComponentId c, const ComponentValue* v) {
  query_memo_credit_(s, e, -1);
  soa_add(&s->inner, e, c, v);
  query_memo_credit_(s, e, +1);
}

static inline void query_memo_remove(query_memo* s, Entity e, ComponentId c) {
  query_memo_credit_(s, e, -1);
  soa_remove(&s->inner, e, c);
  query_memo_credit_(s, e, +1);
}

static inline bool query_memo_get(query_memo* s, Entity e, ComponentId c, ComponentValue* out) {
  return soa_get(&s->inner, e, c, out);
}

static inline bool query_memo_set(query_memo* s, Entity e, ComponentId c, const ComponentValue* v) {
  query_memo_credit_(s, e, -1);
  const bool ok = soa_set(&s->inner, e, c, v);
  query_memo_credit_(s, e, +1);
  return ok;
}

static inline ComponentMask query_memo_mask(query_memo* s, Entity e) {
  return soa_mask(&s->inner, e);
}

/* The running total is kept under the salt of the call that last computed
 * it. Returning it for a later call is the whole idea, and since the salted
 * contract it is also exactly what the gate rejects. */
static inline uint64_t query_memo_query(query_memo* s, ComponentMask required, uint64_t salt) {
  query_memo_memo_* m = &s->memo[required];
  if (!m->tracked || m->stale) {
    m->sum = soa_query(&s->inner, required, salt);
    m->salt = salt;
    m->tracked = true;
    m->stale = false;
  }
  return m->sum;
}

static inline void query_memo_integrate(query_memo* s, float dt) {
  soa_integrate(&s->inner, dt);
  for (int m = 0; m < QUERY_MEMO_MASKS; ++m) {
    if (m & GDS_K_POSITION) s->memo[m].stale = true;
  }
}

static inline void query_memo_sync(query_memo* s) { soa_sync(&s->inner); }

static inline size_t query_memo_entity_count(query_memo* s) { return soa_entity_count(&s->inner); }

static inline size_t query_memo_reported_bytes(query_memo* s) {
  return soa_reported_bytes(&s->inner) + sizeof(s->memo);
}

#endif
