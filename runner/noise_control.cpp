// A control for runner/noise_probe.py: work whose cost does not depend on any
// candidate, timed the way the harness times a repetition. If its repetitions
// spread as much as a candidate's do, the spread comes from the machine and
// not from the code under test.
//
//   --workload compute   a dependent integer chain, no memory traffic
//   --workload memory    a dependent random walk over a 64 MiB array (DRAM)
//   --workload cache     the same walk over 6 MiB, about the working set of
//                        w01_steady_uniform, which fits a shared L3 cache
//
// Output follows the bench JSON of the suite as far as noise_probe reads it.
// Each repetition is split into 100 steps, as a workload is split into frames.
//
//   g++ -O2 -std=c++20 -o build/noise_control runner/noise_control.cpp

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <vector>

static std::uint64_t now_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
  const char* kind = "compute";
  int repeats = 5, warmup = 1;
  for (int i = 1; i + 1 < argc; i += 2) {
    if (!std::strcmp(argv[i], "--workload")) kind = argv[i + 1];
    else if (!std::strcmp(argv[i], "--repeats")) repeats = std::atoi(argv[i + 1]);
    else if (!std::strcmp(argv[i], "--warmup")) warmup = std::atoi(argv[i + 1]);
  }
  // Accept a workload path too: the kind is the file's stem.
  if (std::strstr(kind, "memory")) kind = "memory";
  else if (std::strstr(kind, "cache")) kind = "cache";
  else kind = "compute";
  const bool memory = std::strcmp(kind, "compute") != 0;

  const std::size_t mib = !std::strcmp(kind, "cache") ? 6 : 64;
  const std::size_t n = (mib << 20) / sizeof(std::uint32_t);
  std::vector<std::uint32_t> next;
  if (memory) {
    // One cycle through every slot (Sattolo), so the walk never short-circuits.
    next.resize(n);
    std::iota(next.begin(), next.end(), 0u);
    std::uint64_t s = 88172645463325252ull;
    for (std::size_t i = n - 1; i > 0; --i) {
      s ^= s << 13; s ^= s >> 7; s ^= s << 17;
      std::swap(next[i], next[s % i]);
    }
  }
  const int steps = 100;
  const std::uint64_t per_step = !memory ? 4000000 : 40000;

  std::uint64_t sink = 1;
  std::vector<std::uint64_t> totals, p50s;
  for (int r = 0; r < warmup + repeats; ++r) {
    std::vector<std::uint64_t> step_ns;
    const std::uint64_t t0 = now_ns();
    std::uint32_t at = 0;
    for (int s = 0; s < steps; ++s) {
      const std::uint64_t a = now_ns();
      if (memory) {
        for (std::uint64_t i = 0; i < per_step; ++i) at = next[at];
        sink += at;
      } else {
        std::uint64_t x = sink;
        for (std::uint64_t i = 0; i < per_step; ++i) x = x * 6364136223846793005ull + 1442695040888963407ull;
        sink = x;
      }
      step_ns.push_back(now_ns() - a);
    }
    const std::uint64_t total = now_ns() - t0;
    if (r < warmup) continue;
    totals.push_back(total);
    std::sort(step_ns.begin(), step_ns.end());
    p50s.push_back(step_ns[step_ns.size() / 2]);
  }
  std::printf("{\n  \"candidate\": \"noise_control\",\n  \"workload\": \"%s\",\n", kind);
  std::printf("  \"sink\": %llu,\n  \"repetition_total_ns\": [", (unsigned long long)sink);
  for (std::size_t i = 0; i < totals.size(); ++i) std::printf("%s%llu", i ? ", " : "", (unsigned long long)totals[i]);
  std::printf("],\n  \"repetition_step_ns_p50\": [");
  for (std::size_t i = 0; i < p50s.size(); ++i) std::printf("%s%llu", i ? ", " : "", (unsigned long long)p50s[i]);
  std::printf("]\n}\n");
  return 0;
}
