/* Hardware counters via perf_event_open.
 *
 * Availability is a property of the machine, not of the benchmark. When the
 * kernel refuses (no PMU exposed to the guest, or perf_event_paranoid too
 * high) every counter is reported as unavailable. It is never estimated,
 * scaled, or filled in from a model. */
#ifndef GDS_PMU_H
#define GDS_PMU_H

#include <stdbool.h>
#include <stdint.h>

enum { GDS_PMU_COUNTERS = 6 };

typedef struct {
  bool available; /* true only if every requested counter was opened */
  char reason[256];
  int fds[GDS_PMU_COUNTERS];
  uint64_t values[GDS_PMU_COUNTERS];
} GdsPmu;

/* The counters, in the order of GdsPmu.values. */
const char* gds_pmu_counter_name(int index);

void gds_pmu_open(GdsPmu* pmu);
void gds_pmu_close(GdsPmu* pmu);
void gds_pmu_start(GdsPmu* pmu);
void gds_pmu_stop(GdsPmu* pmu);

#endif
