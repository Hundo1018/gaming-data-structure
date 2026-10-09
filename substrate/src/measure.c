#include "gds/measure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gds/json.h"

static int cmp_u64(const void* a, const void* b) {
  const uint64_t x = *(const uint64_t*)a, y = *(const uint64_t*)b;
  return (x > y) - (x < y);
}

uint64_t gds_percentile(const uint64_t* v, size_t n, double p) {
  if (n == 0) return 0;
  uint64_t* s = malloc(n * sizeof *s);
  memcpy(s, v, n * sizeof *s);
  qsort(s, n, sizeof *s, cmp_u64);
  const double idx = p * ((double)n - 1.0);
  const size_t lo = (size_t)idx;
  const size_t hi = lo + 1 < n ? lo + 1 : n - 1;
  const double frac = idx - (double)lo;
  const uint64_t out = (uint64_t)((double)s[lo] * (1.0 - frac) + (double)s[hi] * frac);
  free(s);
  return out;
}

bool gds_parse_run_args(int argc, char** argv, GdsRunArgs* out, const char* candidate_name,
                        int* exit_code) {
  *exit_code = 0;
  out->workload_path = NULL;
  out->mode = "verify";
  out->repeats = 3;
  out->warmup = 1;
  for (int i = 1; i < argc; ++i) {
    const char* a = argv[i];
    const bool has_value = i + 1 < argc;
    if (!strcmp(a, "--workload") || !strcmp(a, "--mode") || !strcmp(a, "--repeats") ||
        !strcmp(a, "--warmup")) {
      if (!has_value) {
        fprintf(stderr, "missing value for %s\n", a);
        exit(2);
      }
      const char* v = argv[++i];
      if (!strcmp(a, "--workload")) out->workload_path = v;
      else if (!strcmp(a, "--mode")) out->mode = v;
      else if (!strcmp(a, "--repeats")) out->repeats = atoi(v);
      else out->warmup = atoi(v);
    } else if (!strcmp(a, "--name")) {
      printf("%s\n", candidate_name);
      return false;
    } else {
      fprintf(stderr, "unknown argument: %s\n", a);
      *exit_code = 2;
      return false;
    }
  }
  if (!out->workload_path) {
    fprintf(stderr,
            "usage: %s --workload FILE [--mode verify|bench] [--repeats N] [--warmup N]\n",
            argv[0]);
    *exit_code = 2;
    return false;
  }
  return true;
}

const GdsRepetition* gds_median_repetition(const GdsRepetition* reps, int n) {
  int order[256];
  if (n > 256) n = 256;
  for (int i = 0; i < n; ++i) order[i] = i;
  /* Insertion sort on total time: a handful of repetitions, and stable. */
  for (int i = 1; i < n; ++i) {
    const int x = order[i];
    int j = i - 1;
    while (j >= 0 && reps[order[j]].total_ns > reps[x].total_ns) {
      order[j + 1] = order[j];
      --j;
    }
    order[j + 1] = x;
  }
  return &reps[order[n / 2]];
}

/* Step 0 of every workload in both tracks is the load step: it creates the
 * whole initial population and nothing else. It is a different question from
 * steady state, and keeping it in the percentiles made the p99 of a short run
 * the load step itself. The step percentiles are taken over every step after
 * it, and the load step is reported on its own as `load_step_ns`. A run of one
 * step has no steady state, and its one step is what is reported. */
static const uint64_t* steady(const GdsRepetition* r, size_t* n) {
  const size_t first = r->steps > 1 ? 1 : 0;
  *n = r->steps - first;
  return r->step_ns + first;
}

static uint64_t steady_percentile(const GdsRepetition* r, double p) {
  size_t n;
  const uint64_t* s = steady(r, &n);
  return gds_percentile(s, n, p);
}

void gds_print_common_bench_json(const GdsRepetition* med, const GdsRepetition* reps, int nreps,
                                 size_t total_ops, const char* step_label) {
  const double total_s = (double)med->total_ns / 1e9;
  const double throughput = total_s > 0 ? (double)total_ops / total_s : 0.0;
  const double bytes_per_entity =
      med->final_entities ? (double)med->alloc.live_bytes / (double)med->final_entities : 0.0;

  printf("  \"status\": \"ok\",\n");
  printf("  \"step_label\": \"%s\",\n", step_label);
  printf("  \"checksum\": \"%llu\",\n", (unsigned long long)med->checksum);
  printf("  \"total_ns\": %llu,\n", (unsigned long long)med->total_ns);
  printf("  \"ops_per_second\": %.1f,\n", throughput);
  printf("  \"step_ns_p50\": %llu,\n", (unsigned long long)steady_percentile(med, 0.50));
  printf("  \"step_ns_p95\": %llu,\n", (unsigned long long)steady_percentile(med, 0.95));
  printf("  \"step_ns_p99\": %llu,\n", (unsigned long long)steady_percentile(med, 0.99));
  printf("  \"step_ns_max\": %llu,\n", (unsigned long long)steady_percentile(med, 1.0));
  printf("  \"load_step_ns\": %llu,\n",
         (unsigned long long)(med->steps ? med->step_ns[0] : 0));
  printf("  \"peak_bytes\": %lld,\n", (long long)med->alloc.peak_bytes);
  printf("  \"live_bytes\": %lld,\n", (long long)med->alloc.live_bytes);
  printf("  \"reported_bytes\": %zu,\n", med->reported_bytes);
  printf("  \"bytes_per_entity\": %.2f,\n", bytes_per_entity);
  printf("  \"alloc_count\": %llu,\n", (unsigned long long)med->alloc.alloc_count);
  printf("  \"free_count\": %llu,\n", (unsigned long long)med->alloc.free_count);
  printf("  \"alloc_total_bytes\": %llu,\n", (unsigned long long)med->alloc.total_bytes);
  printf("  \"final_entities\": %zu,\n", med->final_entities);
  printf("  \"repetition_total_ns\": [");
  for (int i = 0; i < nreps; ++i) printf("%s%llu", i ? ", " : "", (unsigned long long)reps[i].total_ns);
  printf("],\n");
  /* The step percentiles of every repetition, not only the median one, so that
   * a reader can tell a difference between two candidates from the spread of
   * one candidate against itself. runner/predictions.py uses them. */
  printf("  \"repetition_step_ns_p50\": [");
  for (int i = 0; i < nreps; ++i)
    printf("%s%llu", i ? ", " : "", (unsigned long long)steady_percentile(&reps[i], 0.50));
  printf("],\n");
  printf("  \"repetition_step_ns_p99\": [");
  for (int i = 0; i < nreps; ++i)
    printf("%s%llu", i ? ", " : "", (unsigned long long)steady_percentile(&reps[i], 0.99));
  printf("],\n");
}

void gds_print_pmu_json(const GdsRepetition* med, const GdsPmu* pmu) {
  printf("  \"pmu_available\": %s,\n", med->pmu_available ? "true" : "false");
  if (med->pmu_available) {
    printf("  \"pmu\": {");
    for (int i = 0; i < GDS_PMU_COUNTERS; ++i)
      printf("%s\"%s\": %llu", i ? ", " : "", gds_pmu_counter_name(i),
             (unsigned long long)med->pmu_values[i]);
    printf("}\n");
  } else {
    printf("  \"pmu\": null,\n");
    printf("  \"pmu_unavailable_reason\": \"");
    gds_json_write_escaped(stdout, pmu->reason);
    printf("\"\n");
  }
}
