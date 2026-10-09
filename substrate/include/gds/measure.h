/* Measurement pieces shared by every track.
 *
 * The units a track measures in are its own; how a repetition is timed, which
 * repetition is reported, and how memory is accounted for are not, or two
 * tracks would quietly be measuring different things under the same names. */
#ifndef GDS_MEASURE_H
#define GDS_MEASURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "gds/alloc.h"
#include "gds/pmu.h"

typedef struct {
  uint64_t* step_ns; /* one entry per frame or tick */
  size_t steps;
  uint64_t total_ns;
  uint64_t checksum;
  GdsAllocStats alloc;
  size_t reported_bytes;
  size_t final_entities;
  uint64_t pmu_values[GDS_PMU_COUNTERS];
  bool pmu_available;
} GdsRepetition;

typedef struct {
  const char* workload_path;
  const char* mode;
  int repeats;
  int warmup;
} GdsRunArgs;

static inline uint64_t gds_now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* Linear interpolation between the two nearest ranks of the sorted values. */
uint64_t gds_percentile(const uint64_t* v, size_t n, double p);

/* Returns false when the caller should exit; *exit_code says with what. */
bool gds_parse_run_args(int argc, char** argv, GdsRunArgs* out, const char* candidate_name,
                        int* exit_code);

/* The reported repetition is the one with the median total time: the fastest
 * run flatters a structure whose cost is variable, and the mean is dragged by
 * scheduler noise. */
const GdsRepetition* gds_median_repetition(const GdsRepetition* reps, int n);

/* The metric block every track reports, under names that mean the same thing
 * in each. `step_label` is the track's word for its unit of latency. */
void gds_print_common_bench_json(const GdsRepetition* med, const GdsRepetition* reps, int nreps,
                                 size_t total_ops, const char* step_label);
void gds_print_pmu_json(const GdsRepetition* med, const GdsPmu* pmu);

#endif
