// Correctness and measurement.
//
// Both modes replay the identical op stream through the identical code path, so
// a verified candidate is measured doing exactly the work it was verified on.
// The benchmark computes a checksum from every observation it makes; the
// orchestrator compares that checksum against the oracle's, which makes it
// impossible for a candidate to be fast by answering incorrectly in a mode
// where nobody is looking.
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "gds/alloc_tracker.hpp"
#include "gds/api.hpp"
#include "gds/measure.hpp"
#include "gds/pmu.hpp"
#include "gds/reference.hpp"
#include "gds/types.hpp"
#include "gds/workload.hpp"

namespace gds {

// Replays the op stream against one structure and folds every observation into
// a checksum.
template <class S>
struct Replay {
  S structure;
  std::vector<Entity> slots;
  std::uint64_t checksum = 0;

  explicit Replay(std::uint32_t slot_capacity) { slots.reserve(slot_capacity); }

  void fold(std::uint64_t v) { checksum = splitmix64(checksum ^ v); }

  void apply(const Op& op) {
    switch (op.kind) {
      case OpKind::Create: {
        ComponentValue values[kComponentCount];
        create_values(op.slot, values);
        slots.push_back(structure.create(op.mask, values));
        break;
      }
      case OpKind::Destroy:
        structure.destroy(slots[op.slot]);
        break;
      case OpKind::Add:
        structure.add(slots[op.slot], op.comp, op.value);
        break;
      case OpKind::Remove:
        structure.remove(slots[op.slot], op.comp);
        break;
      case OpKind::Get: {
        ComponentValue out{};
        const bool ok = structure.get(slots[op.slot], op.comp, out);
        fold(ok ? digest_component(op.comp, out) : 0xDEADBEEFull);
        break;
      }
      case OpKind::Set:
        fold(structure.set(slots[op.slot], op.comp, op.value) ? 3 : 5);
        break;
    }
  }

  void end_of_frame(const WorkloadSpec& spec) {
    if (spec.integrate_per_frame) structure.integrate(spec.dt);
    for (ComponentMask m : spec.query_masks) {
      fold(structure.query(m));
    }
    fold(structure.entity_count());
    structure.sync();
  }
};

// ---------------------------------------------------------------------------
// Verification
// ---------------------------------------------------------------------------

struct VerifyResult {
  bool passed = true;
  std::string failure;
  std::uint64_t ops_checked = 0;
  std::uint64_t sweeps = 0;
  std::uint64_t checksum = 0;
};

template <class S>
VerifyResult run_verify(const Workload& w) {
  VerifyResult r;
  Replay<S> cand(w.slot_count);
  Replay<ReferenceStructure> oracle(w.slot_count);

  auto fail = [&](const std::string& msg, std::uint32_t frame, std::uint32_t op_index) {
    r.passed = false;
    r.failure = "frame " + std::to_string(frame) + ", op " + std::to_string(op_index) + ": " + msg;
  };

  // Compares every component of every slot ever created, including destroyed
  // ones: a candidate that recycles storage without invalidating old handles
  // fails here rather than silently returning another entity's data.
  auto sweep = [&](std::uint32_t frame) {
    if (cand.structure.entity_count() != oracle.structure.entity_count()) {
      fail("entity_count " + std::to_string(cand.structure.entity_count()) + " != oracle " +
               std::to_string(oracle.structure.entity_count()),
           frame, 0);
      return false;
    }
    for (std::uint32_t slot = 0; slot < cand.slots.size(); ++slot) {
      const Entity ce = cand.slots[slot];
      const Entity oe = oracle.slots[slot];
      if (cand.structure.alive(ce) != oracle.structure.alive(oe)) {
        fail("alive() disagrees for slot " + std::to_string(slot), frame, 0);
        return false;
      }
      if (cand.structure.mask(ce) != oracle.structure.mask(oe)) {
        fail("mask() disagrees for slot " + std::to_string(slot), frame, 0);
        return false;
      }
      for (int c = 0; c < kComponentCount; ++c) {
        const ComponentId id = static_cast<ComponentId>(c);
        ComponentValue cv{}, ov{};
        const bool cok = cand.structure.get(ce, id, cv);
        const bool ook = oracle.structure.get(oe, id, ov);
        if (cok != ook) {
          fail("get() presence disagrees for slot " + std::to_string(slot) + " component " +
                   std::to_string(c),
               frame, 0);
          return false;
        }
        if (cok && !value_equal(id, cv, ov)) {
          fail("get() value disagrees for slot " + std::to_string(slot) + " component " +
                   std::to_string(c),
               frame, 0);
          return false;
        }
      }
    }
    ++r.sweeps;
    return true;
  };

  for (std::uint32_t f = 0; f < w.frames.size(); ++f) {
    const Frame& fr = w.frames[f];
    for (std::uint32_t i = fr.op_begin; i < fr.op_end; ++i) {
      const Op& op = w.ops[i];
      const std::uint64_t before_c = cand.checksum;
      const std::uint64_t before_o = oracle.checksum;
      cand.apply(op);
      oracle.apply(op);
      ++r.ops_checked;
      if (cand.checksum != oracle.checksum) {
        fail("observation mismatch on op kind " + std::to_string(static_cast<int>(op.kind)) +
                 " slot " + std::to_string(op.slot) + " component " +
                 std::to_string(static_cast<int>(op.comp)) + " (candidate fold " +
                 std::to_string(cand.checksum ^ before_c) + ", oracle fold " +
                 std::to_string(oracle.checksum ^ before_o) + ")",
             f, i - fr.op_begin);
        return r;
      }
    }
    cand.end_of_frame(w.spec);
    oracle.end_of_frame(w.spec);
    if (cand.checksum != oracle.checksum) {
      fail("end-of-frame observation mismatch (integrate or query)", f, 0);
      return r;
    }
    const std::uint32_t period = std::max<std::uint32_t>(1, w.spec.verify_sweep_frames);
    if ((f % period) == period - 1 || f + 1 == w.frames.size()) {
      if (!sweep(f)) return r;
    }
  }
  r.checksum = cand.checksum;
  return r;
}

// ---------------------------------------------------------------------------
// Measurement
// ---------------------------------------------------------------------------

template <class S>
RepetitionResult run_one_repetition(const Workload& w, Pmu& pmu) {
  RepetitionResult rep;
  rep.step_ns.reserve(w.frames.size());

  // Everything the harness owns is allocated before the reset so that the
  // measured bytes belong to the candidate.
  std::vector<Entity> slots;
  slots.reserve(w.slot_count);

  alloc_reset();
  {
    Replay<S> replay(0);
    replay.slots.swap(slots);
    pmu.start();
    const auto t_start = std::chrono::steady_clock::now();
    for (std::uint32_t f = 0; f < w.frames.size(); ++f) {
      const Frame& fr = w.frames[f];
      const auto t0 = std::chrono::steady_clock::now();
      for (std::uint32_t i = fr.op_begin; i < fr.op_end; ++i) {
        replay.apply(w.ops[i]);
      }
      replay.end_of_frame(w.spec);
      const auto t1 = std::chrono::steady_clock::now();
      keep(replay.checksum);
      rep.step_ns.push_back(
          static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)
                                         .count()));
    }
    const auto t_end = std::chrono::steady_clock::now();
    pmu.stop();
    rep.total_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count());
    rep.checksum = replay.checksum;
    rep.reported_bytes = replay.structure.reported_bytes();
    rep.final_entities = replay.structure.entity_count();
    rep.alloc = alloc_snapshot();
  }
  rep.pmu_available = pmu.available();
  if (pmu.available()) rep.pmu_values = pmu.values();
  return rep;
}

}  // namespace gds
