/* Per-candidate executable entry point for the ECS track.
 *
 * Each candidate builds into its own binary, so a candidate that fails to
 * compile removes only itself from the run. One process handles exactly one
 * (candidate, workload, mode) triple, which keeps the allocation high-water
 * mark attributable.
 *
 * A candidate's structure.c defines GDS_CANDIDATE to its prefix and includes
 * this file once, after its structure.h. */
#ifndef GDS_CANDIDATE
#error "define GDS_CANDIDATE to the candidate's prefix before including gds/entry.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gds/api.h"
#include "gds/json.h"
#include "gds/measure.h"
#include "gds/pmu.h"
#include "gds/reference.h"
#include "gds/workload.h"

/* Redeclaring every contract function with its required prototype is the
 * compile-time contract check: a missing function is an undefined static
 * function, a different signature is a conflicting declaration. */
GDS_ECS_CONTRACT(GDS_CANDIDATE)

#ifndef GDS_CANDIDATE_IS_ORACLE
#define GDS_S reference
#include "gds/replay.inc.h"
#undef GDS_S
#endif

#define GDS_S GDS_CANDIDATE
#include "gds/replay.inc.h"
#include "gds/run.inc.h"
#undef GDS_S

#define GDS_E_(n) GDS_CAT(GDS_CANDIDATE, GDS_CAT(_run_, n))

int main(int argc, char** argv) {
  const char* name = GDS_STR(GDS_CANDIDATE);
  GdsRunArgs args;
  int exit_code = 0;
  if (!gds_parse_run_args(argc, argv, &args, name, &exit_code)) return exit_code;

  GdsWorkloadSpec spec;
  char error[512] = {0};
  if (!gds_parse_workload_file(args.workload_path, &spec, error, sizeof error)) {
    printf("{\"status\":\"workload_error\",\"error\":\"");
    gds_json_write_escaped(stdout, error);
    printf("\"}\n");
    return 3;
  }
  GdsWorkload w;
  gds_generate_workload(&spec, &w);

  printf("{\n");
  printf("  \"candidate\": \"");
  gds_json_write_escaped(stdout, name);
  printf("\",\n");
  printf("  \"track\": \"ecs\",\n");
  printf("  \"workload\": \"");
  gds_json_write_escaped(stdout, spec.id);
  printf("\",\n");
  printf("  \"visibility\": \"");
  gds_json_write_escaped(stdout, spec.visibility);
  printf("\",\n");
  printf("  \"mode\": \"");
  gds_json_write_escaped(stdout, args.mode);
  printf("\",\n");
  printf("  \"total_ops\": %zu,\n", w.n_ops);
  printf("  \"frames\": %zu,\n", w.n_frames);
  printf("  \"slots\": %u,\n", w.slot_count);

  if (!strcmp(args.mode, "verify")) {
    const GDS_CAT(GDS_CANDIDATE, _verify_result) v = GDS_E_(verify)(&w);
    printf("  \"status\": \"%s\",\n", v.passed ? "passed" : "failed");
    printf("  \"ops_checked\": %llu,\n", (unsigned long long)v.ops_checked);
    printf("  \"sweeps\": %llu,\n", (unsigned long long)v.sweeps);
    printf("  \"checksum\": \"%llu\",\n", (unsigned long long)v.checksum);
    printf("  \"failure\": \"");
    gds_json_write_escaped(stdout, v.failure);
    printf("\"\n");
    printf("}\n");
    gds_free_workload(&w);
    return v.passed ? 0 : 1;
  }

  if (strcmp(args.mode, "bench")) {
    printf("  \"status\": \"bad_mode\"\n}\n");
    gds_free_workload(&w);
    return 2;
  }

  GdsPmu pmu;
  gds_pmu_open(&pmu);
  for (int i = 0; i < args.warmup; ++i) {
    GdsRepetition discard;
    GDS_E_(one_repetition)(&w, &pmu, &discard);
    gds_keep_u64(discard.checksum);
    free(discard.step_ns);
  }

  const int repeats = args.repeats > 0 ? args.repeats : 0;
  GdsRepetition* reps = calloc((size_t)(repeats ? repeats : 1), sizeof *reps);
  for (int i = 0; i < repeats; ++i) GDS_E_(one_repetition)(&w, &pmu, &reps[i]);

  const GdsRepetition* med = gds_median_repetition(reps, repeats);
  gds_print_common_bench_json(med, reps, repeats, w.n_ops, "frame");
  gds_print_pmu_json(med, &pmu);
  printf("}\n");

  for (int i = 0; i < repeats; ++i) free(reps[i].step_ns);
  free(reps);
  gds_pmu_close(&pmu);
  gds_free_workload(&w);
  return 0;
}

#undef GDS_E_
