// Hardware counters via perf_event_open.
//
// Availability is a property of the machine, not of the benchmark. When the
// kernel refuses (no PMU exposed to the guest, or perf_event_paranoid too
// high) every counter is reported as unavailable. It is never estimated,
// scaled, or filled in from a model.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gds {

enum class PmuCounter {
  Cycles,
  Instructions,
  CacheReferences,
  CacheMisses,
  BranchInstructions,
  BranchMisses,
};

const char* pmu_counter_name(PmuCounter c);

class Pmu {
 public:
  Pmu();
  ~Pmu();
  Pmu(const Pmu&) = delete;
  Pmu& operator=(const Pmu&) = delete;

  // True only if every requested counter was successfully opened.
  bool available() const { return available_; }
  const std::string& unavailable_reason() const { return reason_; }

  void start();
  void stop();

  // Valid only when available(). Indexed in the order of counters().
  const std::vector<std::uint64_t>& values() const { return values_; }
  static const std::vector<PmuCounter>& counters();

 private:
  bool available_ = false;
  std::string reason_;
  std::vector<int> fds_;
  std::vector<std::uint64_t> values_;
};

}  // namespace gds
