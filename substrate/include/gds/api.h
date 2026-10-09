/* The candidate contract for the ECS track.
 *
 * This is deliberately the smallest set of operations that a game needs from an
 * entity store. It names no array, no index, no chunk and no pointer. Anything
 * that satisfies these semantics is a legal candidate.
 *
 * A candidate is a prefix P: a type named P and the functions below, named
 * P_<operation>, defined `static inline` in its structure.h. Its structure.c
 * defines GDS_CANDIDATE to P and includes "gds/entry.h", which checks every
 * function against the prototypes in GDS_ECS_CONTRACT: a missing function or a
 * different signature fails the build and names the function.
 *
 * Semantics every candidate must honour:
 *
 *  1. create(mask, values) returns a handle that is alive until destroy().
 *  2. A handle passed to destroy() must never be reported alive again, and must
 *     never observe another entity's data, even if storage is recycled.
 *  3. get/set/add/remove on a dead handle are no-ops and report failure.
 *  4. add() on a component already present overwrites its value.
 *     remove() of an absent component is a no-op.
 *  5. query(required, salt) returns the sum (mod 2^64) of
 *     gds_digest_entity(required, v, salt) over every live entity whose mask is
 *     a superset of `required`. Order of iteration is unconstrained. The salt
 *     changes on every call, so an answer cannot be carried from one call to
 *     the next; work inside one call may still be deferred or fused.
 *  6. integrate(dt) applies p += v*dt for every live entity holding both
 *     Position and Velocity, in float arithmetic, per entity independently.
 *  7. Every observation (alive/get/mask/query/entity_count) must be correct at
 *     the moment it is called. Deferring work is allowed; answering with stale
 *     data is not. sync() is called at the end of every frame and exists so a
 *     batching candidate has a declared point to do maintenance.
 *
 * Every operation takes a non-const pointer, observations included: a
 * structure that defers work does it on the first observation that needs it,
 * and C has no `mutable`. What an observation may not change is anything
 * observable.
 *
 * Nothing here requires contiguous storage, stable addresses, an index space,
 * or that components physically exist. A candidate may reconstruct a component
 * on read as long as the observable answers match. */
#ifndef GDS_API_H
#define GDS_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gds/common.h"
#include "gds/types.h"

#define GDS_ECS_CONTRACT(P)                                                                  \
  static inline void GDS_FN(P, init)(P * s);                                                 \
  static inline void GDS_FN(P, free)(P * s);                                                 \
  static inline Entity GDS_FN(P, create)(P * s, ComponentMask mask,                          \
                                         const ComponentValue* values);                      \
  static inline void GDS_FN(P, destroy)(P * s, Entity e);                                    \
  static inline bool GDS_FN(P, alive)(P * s, Entity e);                                      \
  static inline void GDS_FN(P, add)(P * s, Entity e, ComponentId c, const ComponentValue* v);\
  static inline void GDS_FN(P, remove)(P * s, Entity e, ComponentId c);                      \
  static inline bool GDS_FN(P, get)(P * s, Entity e, ComponentId c, ComponentValue* out);    \
  static inline bool GDS_FN(P, set)(P * s, Entity e, ComponentId c, const ComponentValue* v);\
  static inline ComponentMask GDS_FN(P, mask)(P * s, Entity e);                              \
  static inline uint64_t GDS_FN(P, query)(P * s, ComponentMask required, uint64_t salt);     \
  static inline void GDS_FN(P, integrate)(P * s, float dt);                                  \
  static inline void GDS_FN(P, sync)(P * s);                                                 \
  static inline size_t GDS_FN(P, entity_count)(P * s);                                       \
  static inline size_t GDS_FN(P, reported_bytes)(P * s);

/* A fresh value for every query call, identical on the oracle's side and the
 * candidate's because both replay the same frames. */
static inline uint64_t gds_query_salt(uint64_t seed, uint64_t frame, uint64_t index) {
  return gds_splitmix64(gds_splitmix64(seed ^ 0x5A17ull) ^ (frame << 8) ^ index);
}

#endif
