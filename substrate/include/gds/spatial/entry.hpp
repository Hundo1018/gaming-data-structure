// Per-candidate executable entry point for the spatial track.
//
// A candidate that does not keep its own history is measured wrapped in
// RebuildRewind, but only on workloads that actually rewind: on a workload with
// no rewinds the wrapper would charge it for snapshots nobody asked for.
#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "gds/json.hpp"
#include "gds/measure.hpp"
#include "gds/spatial/api.hpp"
#include "gds/spatial/harness.hpp"
#include "gds/spatial/rebuild_rewind.hpp"
#include "gds/spatial/workload.hpp"

namespace gds::spatial {

template <class Runner>
int run_and_report(const SpatialWorkload& w, const WorldConfig& cfg, const std::string& mode,
                   int repeats, int warmup, const char* strategy) {
  std::printf("  \"rewind_strategy\": \"%s\",\n", strategy);

  if (mode == "verify") {
    const VerifyResult v = run_verify<Runner>(w, cfg);
    std::printf("  \"status\": \"%s\",\n", v.passed ? "passed" : "failed");
    std::printf("  \"ops_checked\": %llu,\n", (unsigned long long)v.ops_checked);
    std::printf("  \"sweeps\": %llu,\n", (unsigned long long)v.sweeps);
    std::printf("  \"rewinds\": %llu,\n", (unsigned long long)v.rewinds);
    std::printf("  \"checksum\": \"%llu\",\n", (unsigned long long)v.checksum);
    std::printf("  \"failure\": \"%s\"\n", json_escape(v.failure).c_str());
    std::printf("}\n");
    return v.passed ? 0 : 1;
  }
  if (mode != "bench") {
    std::printf("  \"status\": \"bad_mode\"\n}\n");
    return 2;
  }

  Pmu pmu;
  for (int i = 0; i < warmup; ++i) {
    RepetitionResult discard = run_one_repetition<Runner>(w, cfg, pmu);
    keep(discard.checksum);
  }
  std::vector<RepetitionResult> reps;
  reps.reserve(static_cast<std::size_t>(repeats));
  for (int i = 0; i < repeats; ++i) reps.push_back(run_one_repetition<Runner>(w, cfg, pmu));

  const RepetitionResult& med = median_repetition(reps);
  print_common_bench_json(med, reps, w.ops.size(), "tick");
  print_pmu_json(med, pmu);
  std::printf("}\n");
  return 0;
}

template <class S>
int candidate_main(int argc, char** argv) {
  static_assert(SpatialStructure<S>, "candidate does not satisfy the spatial candidate contract");

  RunArgs args;
  int exit_code = 0;
  if (!parse_run_args(argc, argv, args, S::name(), exit_code)) return exit_code;

  SpatialSpec spec;
  std::string error;
  if (!parse_spatial_file(args.workload_path, spec, error)) {
    std::printf("{\"status\":\"workload_error\",\"error\":\"%s\"}\n", json_escape(error).c_str());
    return 3;
  }
  const SpatialWorkload w = generate_spatial_workload(spec);

  WorldConfig cfg;
  cfg.bounds = w.bounds;
  cfg.expected_entities = spec.initial_entities;
  cfg.max_entity_id = w.max_entity_id;
  cfg.typical_query_radius = w.typical_query_radius;
  cfg.history_ticks = w.rewind_count ? spec.history_ticks : 0;

  std::printf("{\n");
  std::printf("  \"candidate\": \"%s\",\n", json_escape(S::name()).c_str());
  std::printf("  \"track\": \"spatial\",\n");
  std::printf("  \"workload\": \"%s\",\n", json_escape(spec.id).c_str());
  std::printf("  \"visibility\": \"%s\",\n", json_escape(spec.visibility).c_str());
  std::printf("  \"mode\": \"%s\",\n", json_escape(args.mode).c_str());
  std::printf("  \"total_ops\": %zu,\n", w.ops.size());
  std::printf("  \"ticks\": %zu,\n", w.ticks.size());
  std::printf("  \"rewinds\": %u,\n", w.rewind_count);
  std::printf("  \"max_entity_id\": %u,\n", w.max_entity_id);

  const bool needs_history = w.rewind_count > 0;
  if (!needs_history || S::kNativeRewind) {
    return run_and_report<S>(w, cfg, args.mode, args.repeats, args.warmup,
                             needs_history ? "native" : "none");
  }
  return run_and_report<RebuildRewind<S>>(w, cfg, args.mode, args.repeats, args.warmup,
                                          "snapshot_rebuild");
}

}  // namespace gds::spatial

#define GDS_SPATIAL_CANDIDATE_MAIN(Type)                     \
  int main(int argc, char** argv) {                          \
    return ::gds::spatial::candidate_main<Type>(argc, argv);  \
  }
