// Measurement pieces shared by every track.
//
// The units a track measures in are its own; how a repetition is timed, which
// repetition is reported, and how memory is accounted for are not, or two
// tracks would quietly be measuring different things under the same names.
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "gds/alloc_tracker.hpp"
#include "gds/json.hpp"
#include "gds/pmu.hpp"

namespace gds {

// Stops the optimiser from deleting work whose result is never read.
template <class T>
inline void keep(const T& value) {
  asm volatile("" : : "r,m"(value) : "memory");
}

struct RepetitionResult {
  std::vector<std::uint64_t> step_ns;  // one entry per frame or tick
  std::uint64_t total_ns = 0;
  std::uint64_t checksum = 0;
  AllocStats alloc{};
  std::size_t reported_bytes = 0;
  std::size_t final_entities = 0;
  std::vector<std::uint64_t> pmu_values;
  bool pmu_available = false;
};

inline std::uint64_t percentile(std::vector<std::uint64_t> v, double p) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  const double idx = p * (static_cast<double>(v.size()) - 1.0);
  const std::size_t lo = static_cast<std::size_t>(idx);
  const std::size_t hi = std::min(lo + 1, v.size() - 1);
  const double frac = idx - static_cast<double>(lo);
  return static_cast<std::uint64_t>(static_cast<double>(v[lo]) * (1.0 - frac) +
                                    static_cast<double>(v[hi]) * frac);
}

struct RunArgs {
  std::string workload_path;
  std::string mode = "verify";
  int repeats = 3;
  int warmup = 1;
};

// Returns false when the caller should exit; `exit_code` says with what.
inline bool parse_run_args(int argc, char** argv, RunArgs& out, const char* candidate_name,
                           int& exit_code) {
  exit_code = 0;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&](const char* what) -> std::string {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "missing value for %s\n", what);
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--workload") out.workload_path = next("--workload");
    else if (a == "--mode") out.mode = next("--mode");
    else if (a == "--repeats") out.repeats = std::atoi(next("--repeats").c_str());
    else if (a == "--warmup") out.warmup = std::atoi(next("--warmup").c_str());
    else if (a == "--name") {
      std::printf("%s\n", candidate_name);
      return false;
    } else {
      std::fprintf(stderr, "unknown argument: %s\n", a.c_str());
      exit_code = 2;
      return false;
    }
  }
  if (out.workload_path.empty()) {
    std::fprintf(stderr,
                 "usage: %s --workload FILE [--mode verify|bench] [--repeats N] [--warmup N]\n",
                 argv[0]);
    exit_code = 2;
    return false;
  }
  return true;
}

// The reported repetition is the one with the median total time: the fastest
// run flatters a structure whose cost is variable, and the mean is dragged by
// scheduler noise.
inline const RepetitionResult& median_repetition(const std::vector<RepetitionResult>& reps) {
  std::vector<std::size_t> order(reps.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(),
            [&](std::size_t a, std::size_t b) { return reps[a].total_ns < reps[b].total_ns; });
  return reps[order[order.size() / 2]];
}

// The metric block every track reports, under names that mean the same thing in
// each. `step_label` is the track's word for its unit of latency.
inline void print_common_bench_json(const RepetitionResult& med,
                                    const std::vector<RepetitionResult>& reps,
                                    std::size_t total_ops, const char* step_label) {
  const double total_s = static_cast<double>(med.total_ns) / 1e9;
  const double throughput = total_s > 0 ? static_cast<double>(total_ops) / total_s : 0.0;
  const double bytes_per_entity =
      med.final_entities ? static_cast<double>(med.alloc.live_bytes) /
                               static_cast<double>(med.final_entities)
                         : 0.0;

  std::printf("  \"status\": \"ok\",\n");
  std::printf("  \"step_label\": \"%s\",\n", step_label);
  std::printf("  \"checksum\": \"%llu\",\n", (unsigned long long)med.checksum);
  std::printf("  \"total_ns\": %llu,\n", (unsigned long long)med.total_ns);
  std::printf("  \"ops_per_second\": %.1f,\n", throughput);
  std::printf("  \"step_ns_p50\": %llu,\n", (unsigned long long)percentile(med.step_ns, 0.50));
  std::printf("  \"step_ns_p95\": %llu,\n", (unsigned long long)percentile(med.step_ns, 0.95));
  std::printf("  \"step_ns_p99\": %llu,\n", (unsigned long long)percentile(med.step_ns, 0.99));
  std::printf("  \"step_ns_max\": %llu,\n", (unsigned long long)percentile(med.step_ns, 1.0));
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
}

inline void print_pmu_json(const RepetitionResult& med, const Pmu& pmu) {
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
    std::printf("  \"pmu_unavailable_reason\": \"%s\"\n",
                json_escape(pmu.unavailable_reason()).c_str());
  }
}

}  // namespace gds
