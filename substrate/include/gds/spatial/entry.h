/* Per-candidate executable entry point for the spatial track.
 *
 * A candidate that does not keep its own history is measured wrapped in the
 * rebuild wrapper, but only on workloads that actually rewind: on a workload
 * with no rewinds the wrapper would charge it for snapshots nobody asked for.
 *
 * A candidate's structure.c defines GDS_CANDIDATE to its prefix and includes
 * this file once, after its structure.h. Both ways of running it are compiled
 * in, as the candidate itself and as <prefix>_rr inside the rebuild wrapper,
 * and the workload decides at run time which one is measured. */
#ifndef GDS_CANDIDATE
#error "define GDS_CANDIDATE to the candidate's prefix before including gds/spatial/entry.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gds/json.h"
#include "gds/measure.h"
#include "gds/spatial/api.h"
#include "gds/spatial/oracle.h"
#include "gds/spatial/workload.h"

/* Redeclaring every contract function with its required prototype is the
 * compile-time contract check: a missing function is an undefined static
 * function, a different signature is a conflicting declaration. */
GDS_SPATIAL_CONTRACT(GDS_CANDIDATE)

#ifndef GDS_CANDIDATE_IS_ORACLE
#define GDS_S brute_force
#include "gds/spatial/replay.inc.h"
#undef GDS_S
#endif

#define GDS_S GDS_CANDIDATE
#include "gds/spatial/replay.inc.h"
#include "gds/spatial/run.inc.h"
#undef GDS_S

#define GDS_RR_INNER GDS_CANDIDATE
#define GDS_RR_SELF GDS_CAT(GDS_CANDIDATE, _rr)
#include "gds/spatial/rebuild_rewind.inc.h"

#define GDS_S GDS_CAT(GDS_CANDIDATE, _rr)
#include "gds/spatial/replay.inc.h"
#include "gds/spatial/run.inc.h"
#undef GDS_S

int main(int argc, char** argv) {
  const char* name = GDS_STR(GDS_CANDIDATE);
  GdsRunArgs args;
  int exit_code = 0;
  if (!gds_parse_run_args(argc, argv, &args, name, &exit_code)) return exit_code;

  GdsSpatialSpec spec;
  char error[512] = {0};
  if (!gds_parse_spatial_file(args.workload_path, &spec, error, sizeof error)) {
    printf("{\"status\":\"workload_error\",\"error\":\"");
    gds_json_write_escaped(stdout, error);
    printf("\"}\n");
    return 3;
  }
  GdsSpatialWorkload w;
  gds_generate_spatial_workload(&spec, &w);

  WorldConfig cfg;
  memset(&cfg, 0, sizeof cfg);
  cfg.bounds = w.bounds;
  cfg.expected_entities = spec.initial_entities;
  cfg.max_entity_id = w.max_entity_id;
  cfg.typical_query_radius = w.typical_query_radius;
  cfg.history_ticks = w.rewind_count ? spec.history_ticks : 0;

  printf("{\n");
  printf("  \"candidate\": \"");
  gds_json_write_escaped(stdout, name);
  printf("\",\n");
  printf("  \"track\": \"spatial\",\n");
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
  printf("  \"ticks\": %zu,\n", w.n_ticks);
  printf("  \"rewinds\": %u,\n", w.rewind_count);
  printf("  \"max_entity_id\": %u,\n", w.max_entity_id);

  const bool needs_history = w.rewind_count > 0;
  int rc;
  if (!needs_history || GDS_FN(GDS_CANDIDATE, native_rewind)) {
    rc = GDS_CAT(GDS_CANDIDATE, _run_and_report)(&w, &cfg, &args,
                                                 needs_history ? "native" : "none");
  } else {
    rc = GDS_CAT(GDS_CANDIDATE, _rr_run_and_report)(&w, &cfg, &args, "snapshot_rebuild");
  }
  gds_free_spatial_workload(&w);
  return rc;
}
