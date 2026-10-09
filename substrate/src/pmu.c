#define _GNU_SOURCE
#include "gds/pmu.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(__linux__)
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

static const char* const kNames[GDS_PMU_COUNTERS] = {
    "cycles", "instructions", "cache_references",
    "cache_misses", "branch_instructions", "branch_misses",
};

const char* gds_pmu_counter_name(int index) {
  return (index >= 0 && index < GDS_PMU_COUNTERS) ? kNames[index] : "unknown";
}

#if defined(__linux__)

static const uint64_t kConfig[GDS_PMU_COUNTERS] = {
    PERF_COUNT_HW_CPU_CYCLES,          PERF_COUNT_HW_INSTRUCTIONS,
    PERF_COUNT_HW_CACHE_REFERENCES,    PERF_COUNT_HW_CACHE_MISSES,
    PERF_COUNT_HW_BRANCH_INSTRUCTIONS, PERF_COUNT_HW_BRANCH_MISSES,
};

void gds_pmu_open(GdsPmu* pmu) {
  memset(pmu, 0, sizeof *pmu);
  for (int i = 0; i < GDS_PMU_COUNTERS; ++i) pmu->fds[i] = -1;
  int leader = -1;
  for (int i = 0; i < GDS_PMU_COUNTERS; ++i) {
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof attr);
    attr.type = PERF_TYPE_HARDWARE;
    attr.size = sizeof attr;
    attr.config = kConfig[i];
    attr.disabled = (i == 0) ? 1 : 0;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    attr.inherit = 0;
    const int fd = (int)syscall(SYS_perf_event_open, &attr, 0, -1, leader, 0);
    if (fd < 0) {
      snprintf(pmu->reason, sizeof pmu->reason, "perf_event_open(%s) failed: %s", kNames[i],
               strerror(errno));
      for (int j = 0; j < GDS_PMU_COUNTERS; ++j) {
        if (pmu->fds[j] >= 0) close(pmu->fds[j]);
        pmu->fds[j] = -1;
      }
      pmu->available = false;
      return;
    }
    pmu->fds[i] = fd;
    if (i == 0) leader = fd;
  }
  pmu->available = true;
}

void gds_pmu_close(GdsPmu* pmu) {
  for (int i = 0; i < GDS_PMU_COUNTERS; ++i) {
    if (pmu->fds[i] >= 0) close(pmu->fds[i]);
    pmu->fds[i] = -1;
  }
}

/* Zeroed per repetition, so a warmup run never contributes to a reported one. */
void gds_pmu_start(GdsPmu* pmu) {
  if (!pmu->available) return;
  memset(pmu->values, 0, sizeof pmu->values);
  ioctl(pmu->fds[0], PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
  ioctl(pmu->fds[0], PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
}

void gds_pmu_stop(GdsPmu* pmu) {
  if (!pmu->available) return;
  ioctl(pmu->fds[0], PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
  for (int i = 0; i < GDS_PMU_COUNTERS; ++i) {
    uint64_t v = 0;
    if (read(pmu->fds[i], &v, sizeof v) == (ssize_t)sizeof v) pmu->values[i] += v;
  }
}

#else

void gds_pmu_open(GdsPmu* pmu) {
  memset(pmu, 0, sizeof *pmu);
  snprintf(pmu->reason, sizeof pmu->reason, "perf_event_open is Linux-only");
}
void gds_pmu_close(GdsPmu* pmu) { (void)pmu; }
void gds_pmu_start(GdsPmu* pmu) { (void)pmu; }
void gds_pmu_stop(GdsPmu* pmu) { (void)pmu; }

#endif
