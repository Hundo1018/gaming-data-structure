// Correctness and measurement for the spatial track.
//
// Both modes replay the identical op stream through the identical code path.
// Every query result, every position read and every entity count is folded into
// a checksum; verification compares that checksum against the oracle after each
// operation, and measurement reports it so the orchestrator can compare it
// against the oracle's for the same workload.
#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "gds/alloc_tracker.hpp"
#include "gds/measure.hpp"
#include "gds/pmu.hpp"
#include "gds/spatial/api.hpp"
#include "gds/spatial/oracle.hpp"
#include "gds/spatial/workload.hpp"

namespace gds::spatial {

template <class S>
struct Replay {
  S structure;
  std::uint64_t checksum = 0;

  explicit Replay(const WorldConfig& cfg) : structure(cfg) {}

  void fold(std::uint64_t v) { checksum = splitmix64(checksum ^ v); }

  bool apply(const Op& op) {
    switch (op.kind) {
      case SpatialOp::Insert:
        structure.insert(op.id, op.v);
        return true;
      case SpatialOp::Remove:
        structure.remove(op.id);
        return true;
      case SpatialOp::MoveBy:
        structure.move_by(op.id, op.v);
        return true;
      case SpatialOp::QueryRadius:
        fold(structure.query_radius(op.v, op.radius));
        return true;
      case SpatialOp::QueryRadiusOf:
        fold(structure.query_radius_of(op.id, op.radius));
        return true;
      case SpatialOp::QueryKnn:
        fold(structure.query_knn(op.v, op.k));
        return true;
      case SpatialOp::Rewind:
        return structure.rewind_to(op.k);
    }
    return true;
  }

  void end_of_tick(std::uint64_t tick) {
    fold(structure.entity_count());
    structure.end_tick(tick);
  }
};

struct VerifyResult {
  bool passed = true;
  std::string failure;
  std::uint64_t ops_checked = 0;
  std::uint64_t sweeps = 0;
  std::uint64_t rewinds = 0;
  std::uint64_t checksum = 0;
};

template <class S>
VerifyResult run_verify(const SpatialWorkload& w, const WorldConfig& cfg) {
  VerifyResult r;
  Replay<S> cand(cfg);
  Replay<BruteForceOracle> oracle(cfg);

  auto fail = [&](const std::string& msg, std::uint32_t tick, std::uint32_t op_index) {
    r.passed = false;
    r.failure = "tick " + std::to_string(tick) + ", op " + std::to_string(op_index) + ": " + msg;
  };

  // Every id ever issued, live or not: a structure that forgets to drop an
  // entity on rewind, or resurrects one, fails here rather than only on a
  // query that happens to reach it.
  auto sweep = [&](std::uint32_t tick) {
    if (cand.structure.entity_count() != oracle.structure.entity_count()) {
      fail("entity_count " + std::to_string(cand.structure.entity_count()) + " != oracle " +
               std::to_string(oracle.structure.entity_count()),
           tick, 0);
      return false;
    }
    for (std::uint32_t id = 0; id <= w.max_entity_id; ++id) {
      Vec3 cp{}, op{};
      const bool clive = cand.structure.position_of(id, cp);
      const bool olive = oracle.structure.position_of(id, op);
      if (clive != olive) {
        fail("liveness disagrees for id " + std::to_string(id), tick, 0);
        return false;
      }
      if (clive && (std::bit_cast<std::uint32_t>(cp.x) != std::bit_cast<std::uint32_t>(op.x) ||
                    std::bit_cast<std::uint32_t>(cp.y) != std::bit_cast<std::uint32_t>(op.y) ||
                    std::bit_cast<std::uint32_t>(cp.z) != std::bit_cast<std::uint32_t>(op.z))) {
        fail("position disagrees for id " + std::to_string(id), tick, 0);
        return false;
      }
    }
    ++r.sweeps;
    return true;
  };

  for (std::uint32_t t = 0; t < w.ticks.size(); ++t) {
    const Tick& tk = w.ticks[t];
    for (std::uint32_t i = tk.op_begin; i < tk.op_end; ++i) {
      const Op& op = w.ops[i];
      const bool cok = cand.apply(op);
      const bool ook = oracle.apply(op);
      ++r.ops_checked;
      if (op.kind == SpatialOp::Rewind) ++r.rewinds;
      if (cok != ook) {
        fail("rewind_to(" + std::to_string(op.k) + ") returned " + (cok ? "true" : "false") +
                 " against the oracle's " + (ook ? "true" : "false"),
             t, i - tk.op_begin);
        return r;
      }
      if (cand.checksum != oracle.checksum) {
        fail("observation mismatch on op kind " + std::to_string(static_cast<int>(op.kind)) +
                 " id " + std::to_string(op.id) + " radius " + std::to_string(op.radius) + " k " +
                 std::to_string(op.k),
             t, i - tk.op_begin);
        return r;
      }
    }
    cand.end_of_tick(t);
    oracle.end_of_tick(t);
    if (cand.checksum != oracle.checksum) {
      fail("end-of-tick entity_count mismatch", t, 0);
      return r;
    }
    const std::uint32_t period = std::max<std::uint32_t>(1, w.spec.verify_sweep_ticks);
    if ((t % period) == period - 1 || t + 1 == w.ticks.size()) {
      if (!sweep(t)) return r;
    }
  }
  r.checksum = cand.checksum;
  return r;
}

template <class S>
RepetitionResult run_one_repetition(const SpatialWorkload& w, const WorldConfig& cfg, Pmu& pmu) {
  RepetitionResult rep;
  rep.step_ns.reserve(w.ticks.size());

  alloc_reset();
  {
    Replay<S> replay(cfg);
    pmu.start();
    const auto t_start = std::chrono::steady_clock::now();
    for (std::uint32_t t = 0; t < w.ticks.size(); ++t) {
      const Tick& tk = w.ticks[t];
      const auto t0 = std::chrono::steady_clock::now();
      for (std::uint32_t i = tk.op_begin; i < tk.op_end; ++i) {
        replay.apply(w.ops[i]);
      }
      replay.end_of_tick(t);
      const auto t1 = std::chrono::steady_clock::now();
      keep(replay.checksum);
      rep.step_ns.push_back(static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()));
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

}  // namespace gds::spatial
