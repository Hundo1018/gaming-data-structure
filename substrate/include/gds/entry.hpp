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
#include "gds/workload.hpp"

namespace gds {

std::string json_escape(const std::string& s);

template <class S>
int candidate_main(int argc, char** argv) {
  static_assert(CandidateStructure<S>, "candidate does not satisfy the ECS candidate contract");

  std::string workload_path;
  std::string mode = "verify";
  int repeats = 3;
  int warmup = 1;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&](const char* what) -> std::string {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "missing value for %s\n", what);
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--workload") workload_path = next("--workload");
    else if (a == "--mode") mode = next("--mode");
    else if (a == "--repeats") repeats = std::atoi(next("--repeats").c_str());
    else if (a == "--warmup") warmup = std::atoi(next("--warmup").c_str());
    else if (a == "--name") { std::printf("%s\n", S::name()); return 0; }
    else {
      std::fprintf(stderr, "unknown argument: %s\n", a.c_str());
      return 2;
    }
  }

  if (workload_path.empty()) {
    std::fprintf(stderr, "usage: %s --workload FILE [--mode verify|bench] [--repeats N]\n",
                 argv[0]);
    return 2;
  }

  WorkloadSpec spec;
  std::string error;
  if (!parse_workload_file(workload_path, spec, error)) {
    std::printf("{\"status\":\"workload_error\",\"error\":\"%s\"}\n", json_escape(error).c_str());
    return 3;
  }
  const Workload w = generate_workload(spec);

  std::printf("{\n");
  std::printf("  \"candidate\": \"%s\",\n", json_escape(S::name()).c_str());
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

  // The reported repetition is the one with the median total time: the fastest
  // run flatters a structure whose cost is variable, the mean is dragged by
  // scheduler noise.
  std::vector<std::size_t> order(reps.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(),
            [&](std::size_t a, std::size_t b) { return reps[a].total_ns < reps[b].total_ns; });
  const RepetitionResult& med = reps[order[order.size() / 2]];

  const double total_s = static_cast<double>(med.total_ns) / 1e9;
  const double throughput = total_s > 0 ? static_cast<double>(w.ops.size()) / total_s : 0.0;
  // Divides the footprint standing at the end of the run by the population
  // standing at the end of the run. Dividing the high-water mark by the final
  // count would mix a peak from one moment with a population from another,
  // which on a bursty workload is not a quantity that describes anything.
  const double bytes_per_entity =
      med.final_entities ? static_cast<double>(med.alloc.live_bytes) /
                               static_cast<double>(med.final_entities)
                         : 0.0;

  std::printf("  \"status\": \"ok\",\n");
  std::printf("  \"repeats\": %d,\n", repeats);
  std::printf("  \"checksum\": \"%llu\",\n", (unsigned long long)med.checksum);
  std::printf("  \"total_ns\": %llu,\n", (unsigned long long)med.total_ns);
  std::printf("  \"ops_per_second\": %.1f,\n", throughput);
  std::printf("  \"frame_ns_p50\": %llu,\n", (unsigned long long)percentile(med.frame_ns, 0.50));
  std::printf("  \"frame_ns_p95\": %llu,\n", (unsigned long long)percentile(med.frame_ns, 0.95));
  std::printf("  \"frame_ns_p99\": %llu,\n", (unsigned long long)percentile(med.frame_ns, 0.99));
  std::printf("  \"frame_ns_max\": %llu,\n", (unsigned long long)percentile(med.frame_ns, 1.0));
  std::printf("  \"peak_bytes\": %lld,\n", (long long)med.alloc.peak_bytes);
  std::printf("  \"live_bytes\": %lld,\n", (long long)med.alloc.live_bytes);
  std::printf("  \"reported_bytes\": %zu,\n", med.reported_bytes);
  std::printf("  \"bytes_per_entity\": %.2f,\n", bytes_per_entity);
  std::printf("  \"alloc_count\": %llu,\n", (unsigned long long)med.alloc.alloc_count);
  std::printf("  \"free_count\": %llu,\n", (unsigned long long)med.alloc.free_count);
  std::printf("  \"alloc_total_bytes\": %llu,\n", (unsigned long long)med.alloc.total_bytes);
  std::printf("  \"final_entities\": %zu,\n", med.final_entities);

  std::printf("  \"repetition_total_ns\": [");
  for (std::size_t i = 0; i < reps.size(); ++i) {
    std::printf("%s%llu", i ? ", " : "", (unsigned long long)reps[i].total_ns);
  }
  std::printf("],\n");

  std::printf("  \"pmu_available\": %s,\n", med.pmu_available ? "true" : "false");
  if (med.pmu_available) {
    std::printf("  \"pmu\": {");
    const auto& names = Pmu::counters();
    for (std::size_t i = 0; i < names.size() && i < med.pmu_values.size(); ++i) {
      std::printf("%s\"%s\": %llu", i ? ", " : "", pmu_counter_name(names[i]),
                  (unsigned long long)med.pmu_values[i]);
    }
    std::printf("}\n");
  } else {
    std::printf("  \"pmu\": null,\n");
    std::printf("  \"pmu_unavailable_reason\": \"%s\"\n", json_escape(pmu.unavailable_reason()).c_str());
  }
  std::printf("}\n");
  return 0;
}

}  // namespace gds

#define GDS_CANDIDATE_MAIN(Type)                       \
  int main(int argc, char** argv) {                    \
    return ::gds::candidate_main<Type>(argc, argv);    \
  }
