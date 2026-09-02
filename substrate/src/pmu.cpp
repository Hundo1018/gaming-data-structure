#include "gds/pmu.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>

#if defined(__linux__)
#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace gds {

const char* pmu_counter_name(PmuCounter c) {
  switch (c) {
    case PmuCounter::Cycles:
      return "cycles";
    case PmuCounter::Instructions:
      return "instructions";
    case PmuCounter::CacheReferences:
      return "cache_references";
    case PmuCounter::CacheMisses:
      return "cache_misses";
    case PmuCounter::BranchInstructions:
      return "branch_instructions";
    case PmuCounter::BranchMisses:
      return "branch_misses";
  }
  return "unknown";
}

const std::vector<PmuCounter>& Pmu::counters() {
  static const std::vector<PmuCounter> kCounters = {
      PmuCounter::Cycles,           PmuCounter::Instructions,
      PmuCounter::CacheReferences,  PmuCounter::CacheMisses,
      PmuCounter::BranchInstructions, PmuCounter::BranchMisses};
  return kCounters;
}

#if defined(__linux__)

namespace {

std::uint64_t hw_config(PmuCounter c) {
  switch (c) {
    case PmuCounter::Cycles:
      return PERF_COUNT_HW_CPU_CYCLES;
    case PmuCounter::Instructions:
      return PERF_COUNT_HW_INSTRUCTIONS;
    case PmuCounter::CacheReferences:
      return PERF_COUNT_HW_CACHE_REFERENCES;
    case PmuCounter::CacheMisses:
      return PERF_COUNT_HW_CACHE_MISSES;
    case PmuCounter::BranchInstructions:
      return PERF_COUNT_HW_BRANCH_INSTRUCTIONS;
    case PmuCounter::BranchMisses:
      return PERF_COUNT_HW_BRANCH_MISSES;
  }
  return 0;
}

}  // namespace

Pmu::Pmu() {
  const auto& want = counters();
  values_.assign(want.size(), 0);
  fds_.assign(want.size(), -1);
  int leader = -1;
  for (std::size_t i = 0; i < want.size(); ++i) {
    perf_event_attr attr{};
    attr.type = PERF_TYPE_HARDWARE;
    attr.size = sizeof(attr);
    attr.config = hw_config(want[i]);
    attr.disabled = (i == 0) ? 1 : 0;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    attr.inherit = 0;
    const int fd = static_cast<int>(
        syscall(__NR_perf_event_open, &attr, /*pid=*/0, /*cpu=*/-1, leader, 0));
    if (fd < 0) {
      reason_ = std::string("perf_event_open(") + pmu_counter_name(want[i]) +
                ") failed: " + std::strerror(errno);
      for (int open_fd : fds_) {
        if (open_fd >= 0) close(open_fd);
      }
      fds_.assign(want.size(), -1);
      available_ = false;
      return;
    }
    fds_[i] = fd;
    if (i == 0) leader = fd;
  }
  available_ = true;
}

Pmu::~Pmu() {
  for (int fd : fds_) {
    if (fd >= 0) close(fd);
  }
}

void Pmu::start() {
  if (!available_) return;
  // Zeroed per repetition, so a warmup run never contributes to a reported one.
  std::fill(values_.begin(), values_.end(), 0ull);
  ioctl(fds_[0], PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
  ioctl(fds_[0], PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
}

void Pmu::stop() {
  if (!available_) return;
  ioctl(fds_[0], PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
  for (std::size_t i = 0; i < fds_.size(); ++i) {
    std::uint64_t v = 0;
    if (read(fds_[i], &v, sizeof(v)) == static_cast<ssize_t>(sizeof(v))) {
      values_[i] += v;
    }
  }
}

#else

Pmu::Pmu() {
  values_.assign(counters().size(), 0);
  reason_ = "perf_event_open is Linux-only";
  available_ = false;
}
Pmu::~Pmu() = default;
void Pmu::start() {}
void Pmu::stop() {}

#endif

}  // namespace gds
