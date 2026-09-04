// The candidate contract for the spatial track.
//
// The question is: given a point, or given an entity, which entities are near
// it — while every entity is free to move anywhere in the world every tick, and
// while the world may be asked to go back to how it was N ticks ago.
//
// Semantics every candidate must honour:
//
//  1. Ids are given, not chosen. Identity is part of the answer to "what is
//     near me", so the harness assigns dense ids and the structure returns
//     them. It may store them, or reconstruct them, or not store them at all.
//  2. move_by(id, delta) sets the position to wrap_into(current + delta,
//     bounds), using the shared wrap. Deltas may be small or may cross the
//     world; nothing distinguishes a step from a teleport.
//  3. query_radius(c, r) returns the sum of digest_hit over every live entity
//     with dist2(position, c) <= r*r. Iteration order is unconstrained. A broad
//     phase may over-admit; the accept test must be the shared dist2.
//  4. query_radius_of(id, r) is the same query centred on that entity's own
//     position and excluding it. A dead id yields 0.
//  5. query_knn(c, k) returns an ordered fold over the k nearest live entities,
//     ordered by (dist2, id) ascending. Fewer than k live entities folds what
//     there is.
//  6. end_tick(t) is called once at the end of every tick. A structure that
//     batches its work has a declared point to do it; a structure that keeps
//     history has a declared point to record it.
//  7. rewind_to(t) restores the observable state to the end of tick t, which
//     will never be deeper than WorldConfig::history_ticks. Everything after t
//     is discarded: the workload then continues down a different branch, as
//     rollback re-simulation does. Returns false if the structure cannot.
//  8. Every observation must be correct at the moment it is called. Deferring
//     work is allowed; answering with stale data is not.
//
// kNativeRewind declares whether the structure keeps its own history. A
// structure that does not is measured wrapped in RebuildRewind, which
// snapshots the world every tick and rebuilds the index on rewind — what an
// engine does today when the game state is the authority and the index is
// derived. Whether keeping history inside the index beats that is the
// experiment, not an assumption.
#pragma once

#include <concepts>
#include <cstddef>

#include "gds/spatial/types.hpp"

namespace gds::spatial {

template <class T>
concept SpatialStructure =
    requires(T s, const T cs, const WorldConfig& cfg, EntityId id, Vec3 p, Vec3& out, float r,
             std::uint32_t k, std::uint64_t tick) {
      { T(cfg) };
      { s.insert(id, p) };
      { s.remove(id) };
      { s.move_by(id, p) };
      { cs.position_of(id, out) } -> std::same_as<bool>;
      { cs.query_radius(p, r) } -> std::same_as<std::uint64_t>;
      { cs.query_radius_of(id, r) } -> std::same_as<std::uint64_t>;
      { cs.query_knn(p, k) } -> std::same_as<std::uint64_t>;
      { s.end_tick(tick) };
      { s.rewind_to(tick) } -> std::same_as<bool>;
      { cs.entity_count() } -> std::same_as<std::size_t>;
      { cs.reported_bytes() } -> std::same_as<std::size_t>;
      { T::kNativeRewind } -> std::convertible_to<bool>;
      { T::name() } -> std::convertible_to<const char*>;
    };

}  // namespace gds::spatial
