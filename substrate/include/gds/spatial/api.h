/* The candidate contract for the spatial track.
 *
 * The question is: given a point, or given an entity, which entities are near
 * it — while every entity is free to move anywhere in the world every tick, and
 * while the world may be asked to go back to how it was N ticks ago.
 *
 * A candidate is a prefix P: a type named P, the functions below named
 * P_<operation> and defined `static inline`, and an enumeration constant
 * P_native_rewind. Its structure.c defines GDS_CANDIDATE to P and includes
 * "gds/spatial/entry.h", which checks every function against the prototypes in
 * GDS_SPATIAL_CONTRACT: a missing function or a different signature fails the
 * build and names the function.
 *
 * Semantics every candidate must honour:
 *
 *  1. Ids are given, not chosen. Identity is part of the answer to "what is
 *     near me", so the harness assigns dense ids and the structure returns
 *     them. It may store them, or reconstruct them, or not store them at all.
 *  2. move_by(id, delta) sets the position to gds_wrap_into(current + delta,
 *     bounds), using the shared wrap. Deltas may be small or may cross the
 *     world; nothing distinguishes a step from a teleport.
 *  3. query_radius(c, r) returns gds_radius_digest(c, r) folded over every
 *     live entity with gds_dist2(position, c) <= r*r. Iteration order is
 *     unconstrained. A broad phase may over-admit; the accept test must be the
 *     shared gds_dist2. The digest is salted by the query, so a total kept
 *     from earlier work cannot stand in for visiting the entities.
 *  4. query_radius_of(id, r) is the same query centred on that entity's own
 *     position and excluding it. A dead id yields 0.
 *  5. query_knn(c, k) returns an ordered fold over the k nearest live
 *     entities, ordered by (dist2, id) ascending. Fewer than k live entities
 *     folds what there is.
 *  6. end_tick(t) is called once at the end of every tick. A structure that
 *     batches its work has a declared point to do it; a structure that keeps
 *     history has a declared point to record it.
 *  7. rewind_to(t) restores the observable state to the end of tick t, which
 *     will never be deeper than WorldConfig.history_ticks. Everything after t
 *     is discarded: the workload then continues down a different branch, as
 *     rollback re-simulation does. Returns false if the structure cannot.
 *  8. Every observation must be correct at the moment it is called. Deferring
 *     work is allowed; answering with stale data is not.
 *
 * Every operation takes a non-const pointer, observations included: a
 * structure that defers work does it on the first observation that needs it,
 * and C has no `mutable`. What an observation may not change is anything
 * observable.
 *
 * A structure must be relocatable: copying its struct to another address and
 * using the copy must work, so it may not hold a pointer into itself. The
 * rebuild wrapper replaces its index by initialising a fresh one and moving it
 * into place.
 *
 * P_native_rewind declares whether the structure keeps its own history. A
 * structure that does not is measured wrapped in the rebuild wrapper
 * (gds/spatial/rebuild_rewind.inc.h), which snapshots the world every tick and
 * rebuilds the index on rewind — what an engine does today when the game state
 * is the authority and the index is derived. Whether keeping history inside
 * the index beats that is the experiment, not an assumption. */
#ifndef GDS_SPATIAL_API_H
#define GDS_SPATIAL_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gds/common.h"
#include "gds/spatial/types.h"

#define GDS_SPATIAL_CONTRACT(P)                                                           \
  static inline void GDS_FN(P, init)(P * s, const WorldConfig* cfg);                      \
  static inline void GDS_FN(P, free)(P * s);                                              \
  static inline void GDS_FN(P, insert)(P * s, EntityId id, Vec3 p);                       \
  static inline void GDS_FN(P, remove)(P * s, EntityId id);                               \
  static inline void GDS_FN(P, move_by)(P * s, EntityId id, Vec3 delta);                  \
  static inline bool GDS_FN(P, position_of)(P * s, EntityId id, Vec3 * out);              \
  static inline uint64_t GDS_FN(P, query_radius)(P * s, Vec3 c, float r);                 \
  static inline uint64_t GDS_FN(P, query_radius_of)(P * s, EntityId id, float r);         \
  static inline uint64_t GDS_FN(P, query_knn)(P * s, Vec3 c, uint32_t k);                 \
  static inline void GDS_FN(P, end_tick)(P * s, uint64_t tick);                           \
  static inline bool GDS_FN(P, rewind_to)(P * s, uint64_t tick);                          \
  static inline size_t GDS_FN(P, entity_count)(P * s);                                    \
  static inline size_t GDS_FN(P, reported_bytes)(P * s);                                  \
  _Static_assert(GDS_FN(P, native_rewind) == 0 || GDS_FN(P, native_rewind) == 1,          \
                 "<prefix>_native_rewind must be an enumeration constant, 0 or 1");

#endif
