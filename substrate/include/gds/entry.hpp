// Per-candidate executable entry point.
//
// Each candidate builds into its own binary, so a candidate that fails to
// compile removes only itself from the run. One process handles exactly one
// (candidate, workload, mode) triple, which keeps the allocation high-water
// mark attributable.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "gds/api.hpp"
#include "gds/harness.hpp"
#include "gds/json.hpp"
#include "gds/measure.hpp"
#include "gds/workload.hpp"

namespace gds {

template <class S>
int candidate_main(int argc, char** argv) {
  static_assert(CandidateStructure<S>, "candidate does not satisfy the ECS candidate contract");

  RunArgs args;
  int exit_code = 0;
  if (!parse_run_args(argc, argv, args, S::name(), exit_code)) return exit_code;
  const std::string& workload_path = args.workload_path;
  const std::string& mode = args.mode;
  const int repeats = args.repeats;
  const int warmup = args.warmup;

  WorkloadSpec spec;
  std::string error;
  if (!parse_workload_file(workload_path, spec, error)) {
    std::printf("{\"status\":\"workload_error\",\"error\":\"%s\"}\n", json_escape(error).c_str());
    return 3;
  }
  const Workload w = generate_workload(spec);

  std::printf("{\n");
  std::printf("  \"candidate\": \"%s\",\n", json_escape(S::name()).c_str());
  std::printf("  \"track\": \"ecs\",\n");
  std::printf("  \"workload\": \"%s\",\n", json_escape(spec.id).c_str());
  std::printf("  \"visibility\": \"%s\",\n", json_escape(spec.visibility).c_str());
  std::printf("  \"mode\": \"%s\",\n", json_escape(mode).c_str());
  std::printf("  \"total_ops\": %zu,\n", w.ops.size());
  std::printf("  \"frames\": %zu,\n", w.frames.size());
  std::printf("  \"slots\": %u,\n", w.slot_count);

  if (mode == "verify") {
    const VerifyResult v = run_verify<S>(w);
    std::printf("  \"status\": \"%s\",\n", v.passed ? "passed" : "failed");
    std::printf("  \"ops_checked\": %llu,\n", (unsigned long long)v.ops_checked);
    std::printf("  \"sweeps\": %llu,\n", (unsigned long long)v.sweeps);
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
    RepetitionResult discard = run_one_repetition<S>(w, pmu);
    keep(discard.checksum);
  }

  std::vector<RepetitionResult> reps;
  reps.reserve(static_cast<std::size_t>(repeats));
  for (int i = 0; i < repeats; ++i) {
    reps.push_back(run_one_repetition<S>(w, pmu));
  }

  const RepetitionResult& med = median_repetition(reps);
  print_common_bench_json(med, reps, w.ops.size(), "frame");
  print_pmu_json(med, pmu);
  std::printf("}\n");
  return 0;
}

}  // namespace gds

#define GDS_CANDIDATE_MAIN(Type)                       \
  int main(int argc, char** argv) {                    \
    return ::gds::candidate_main<Type>(argc, argv);    \
  }
