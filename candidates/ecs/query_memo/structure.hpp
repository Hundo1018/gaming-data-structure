#pragma once

#include <cstddef>
#include <cstdint>

#include "ecs/soa/structure.hpp"
#include "gds/types.hpp"

namespace gds::candidates {

// soa with every query answer kept as a running total instead of recomputed.
//
// A query returns the sum of digest_entity over the matching entities, and a
// sum can be maintained: subtract an entity's digest before it changes, add it
// back after. Once a mask has been asked for, its query costs nothing, and each
// mutation costs two digests per tracked mask it touches. integrate changes
// every position at once, so it marks the masks it touches stale and the next
// query recomputes them with one ordinary pass.
//
// This is not a data structure anyone would ship. It answers a question about
// the harness: whether a candidate can be fast by never finding the answer at
// all, as long as it can compute what the answer's digest would have been.
class QueryMemo {
 public:
  static const char* name() { return "query_memo"; }

  Entity create(ComponentMask mask, const ComponentValue* values) {
    const Entity e = inner_.create(mask, values);
    credit(e, +1);
    return e;
  }

  void destroy(Entity e) {
    credit(e, -1);
    inner_.destroy(e);
  }

  bool alive(Entity e) const { return inner_.alive(e); }

  void add(Entity e, ComponentId c, const ComponentValue& v) {
    credit(e, -1);
    inner_.add(e, c, v);
    credit(e, +1);
  }

  void remove(Entity e, ComponentId c) {
    credit(e, -1);
    inner_.remove(e, c);
    credit(e, +1);
  }

  bool get(Entity e, ComponentId c, ComponentValue& out) const { return inner_.get(e, c, out); }

  bool set(Entity e, ComponentId c, const ComponentValue& v) {
    credit(e, -1);
    const bool ok = inner_.set(e, c, v);
    credit(e, +1);
    return ok;
  }

  ComponentMask mask(Entity e) const { return inner_.mask(e); }

  std::uint64_t query(ComponentMask required) const {
    Memo& m = memo_[required];
    if (!m.tracked || m.stale) {
      m.sum = inner_.query(required);
      m.tracked = true;
      m.stale = false;
    }
    return m.sum;
  }

  void integrate(float dt) {
    inner_.integrate(dt);
    for (int m = 0; m < kMasks; ++m) {
      if (m & kPosition) memo_[m].stale = true;
    }
  }

  void sync() { inner_.sync(); }

  std::size_t entity_count() const { return inner_.entity_count(); }

  std::size_t reported_bytes() const { return inner_.reported_bytes() + sizeof(memo_); }

 private:
  static constexpr int kMasks = 1 << kComponentCount;

  struct Memo {
    std::uint64_t sum = 0;
    bool tracked = false;
    bool stale = false;
  };

  // Adds (sign +1) or removes (sign -1) the entity's contribution to every
  // tracked, current total whose mask it satisfies.
  void credit(Entity e, int sign) {
    const ComponentMask held = inner_.mask(e);
    if (!held) return;
    for (int m = 1; m < kMasks; ++m) {
      Memo& memo = memo_[m];
      if (!memo.tracked || memo.stale) continue;
      const ComponentMask req = static_cast<ComponentMask>(m);
      if ((held & req) != req) continue;
      ComponentValue v[kComponentCount];
      for (int i = 0; i < kComponentCount; ++i) {
        if (req & (1u << i)) inner_.get(e, static_cast<ComponentId>(i), v[i]);
      }
      const std::uint64_t d = digest_entity(req, v);
      memo.sum = sign > 0 ? memo.sum + d : memo.sum - d;
    }
  }

  Soa inner_;
  mutable Memo memo_[kMasks];
};

}  // namespace gds::candidates
