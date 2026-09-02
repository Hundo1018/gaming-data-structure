// The candidate contract for the ECS track.
//
// This is deliberately the smallest set of operations that a game needs from an
// entity store. It names no array, no index, no chunk and no pointer. Anything
// that satisfies these semantics is a legal candidate.
//
// Semantics every candidate must honour:
//
//  1. create(mask, values) returns a handle that is alive until destroy().
//  2. A handle passed to destroy() must never be reported alive again, and must
//     never observe another entity's data, even if storage is recycled.
//  3. get/set/add/remove on a dead handle are no-ops and report failure.
//  4. add() on a component already present overwrites its value.
//     remove() of an absent component is a no-op.
//  5. query(required) returns the sum (mod 2^64) of digest_entity(required, v)
//     over every live entity whose mask is a superset of `required`.
//     Order of iteration is unconstrained.
//  6. integrate(dt) applies p += v*dt for every live entity holding both
//     Position and Velocity, in float arithmetic, per entity independently.
//  7. Every observation (alive/get/mask/query/entity_count) must be correct at
//     the moment it is called. Deferring work is allowed; answering with stale
//     data is not. sync() is called at the end of every frame and exists so a
//     batching candidate has a declared point to do maintenance.
//
// Nothing here requires contiguous storage, stable addresses, an index space,
// or that components physically exist. A candidate may reconstruct a component
// on read as long as the observable answers match.
#pragma once

#include <concepts>
#include <cstddef>

#include "gds/types.hpp"

namespace gds {

template <class T>
concept CandidateStructure = requires(T s, const T cs, Entity e, ComponentMask m, ComponentId c,
                                      const ComponentValue& v, ComponentValue& out, float dt) {
  { s.create(m, static_cast<const ComponentValue*>(nullptr)) } -> std::same_as<Entity>;
  { s.destroy(e) };
  { cs.alive(e) } -> std::same_as<bool>;
  { s.add(e, c, v) };
  { s.remove(e, c) };
  { cs.get(e, c, out) } -> std::same_as<bool>;
  { s.set(e, c, v) } -> std::same_as<bool>;
  { cs.mask(e) } -> std::same_as<ComponentMask>;
  { cs.query(m) } -> std::same_as<std::uint64_t>;
  { s.integrate(dt) };
  { s.sync() };
  { cs.entity_count() } -> std::same_as<std::size_t>;
  { cs.reported_bytes() } -> std::same_as<std::size_t>;
  { T::name() } -> std::convertible_to<const char*>;
};

}  // namespace gds
