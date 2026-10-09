/* Template header: correctness, measurement and the report for one way of
 * running a spatial structure. Include with GDS_S defined, after
 * gds/spatial/replay.inc.h has been included for GDS_S and for the oracle,
 * `brute_force`.
 *
 * Both modes replay the identical op stream through the identical code path. */
#ifndef GDS_S
#error "define GDS_S to a structure prefix before including gds/spatial/run.inc.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "gds/alloc.h"
#include "gds/json.h"
#include "gds/measure.h"
#include "gds/pmu.h"
#include "gds/spatial/oracle.h"

#define GDS_RT_ GDS_CAT(GDS_S, _replay)
#define GDS_RF_(n) GDS_CAT(GDS_S, GDS_CAT(_replay_, n))
#define GDS_SF_(n) GDS_FN(GDS_S, n)
#define GDS_XF_(n) GDS_CAT(GDS_S, GDS_CAT(_run_, n))
#define GDS_VR_ GDS_CAT(GDS_S, _verify_result)

typedef struct {
  bool passed;
  char failure[512];
  uint64_t ops_checked;
  uint64_t sweeps;
  uint64_t rewinds;
  uint64_t checksum;
} GDS_VR_;

/* Every id ever issued, live or not: a structure that forgets to drop an
 * entity on rewind, or resurrects one, fails here rather than only on a query
 * that happens to reach it. */
static inline bool GDS_XF_(sweep)(GDS_RT_* cand, brute_force_replay* oracle,
                                  const GdsSpatialWorkload* w, uint32_t tick, GDS_VR_* r) {
  const size_t cn = GDS_SF_(entity_count)(&cand->structure);
  const size_t on = brute_force_entity_count(&oracle->structure);
  if (cn != on) {
    snprintf(r->failure, sizeof r->failure, "tick %u, op 0: entity_count %zu != oracle %zu",
             tick, cn, on);
    return false;
  }
  for (uint32_t id = 0; id <= w->max_entity_id; ++id) {
    Vec3 cp = {0, 0, 0}, op = {0, 0, 0};
    const bool clive = GDS_SF_(position_of)(&cand->structure, id, &cp);
    const bool olive = brute_force_position_of(&oracle->structure, id, &op);
    if (clive != olive) {
      snprintf(r->failure, sizeof r->failure, "tick %u, op 0: liveness disagrees for id %u",
               tick, id);
      return false;
    }
    if (clive && !gds_vec3_bits_equal(cp, op)) {
      snprintf(r->failure, sizeof r->failure, "tick %u, op 0: position disagrees for id %u",
               tick, id);
      return false;
    }
  }
  ++r->sweeps;
  return true;
}

static inline GDS_VR_ GDS_XF_(verify)(const GdsSpatialWorkload* w, const WorldConfig* cfg) {
  GDS_VR_ r;
  memset(&r, 0, sizeof r);
  r.passed = true;
  GDS_RT_ cand;
  brute_force_replay oracle;
  memset(&cand, 0, sizeof cand);
  memset(&oracle, 0, sizeof oracle);
  GDS_SF_(init)(&cand.structure, cfg);
  brute_force_init(&oracle.structure, cfg);

  for (uint32_t t = 0; t < w->n_ticks && r.passed; ++t) {
    const GdsTick* tk = &w->ticks[t];
    for (uint32_t i = tk->op_begin; i < tk->op_end; ++i) {
      const GdsSpatialOp* op = &w->ops[i];
      const bool cok = GDS_RF_(apply)(&cand, op);
      const bool ook = brute_force_replay_apply(&oracle, op);
      ++r.ops_checked;
      if (op->kind == GDS_SP_REWIND) ++r.rewinds;
      if (cok != ook) {
        r.passed = false;
        snprintf(r.failure, sizeof r.failure,
                 "tick %u, op %u: rewind_to(%u) returned %s against the oracle's %s", t,
                 i - tk->op_begin, op->k, cok ? "true" : "false", ook ? "true" : "false");
        break;
      }
      if (cand.checksum != oracle.checksum) {
        r.passed = false;
        snprintf(r.failure, sizeof r.failure,
                 "tick %u, op %u: observation mismatch on op kind %d id %u radius %f k %u", t,
                 i - tk->op_begin, (int)op->kind, op->id, (double)op->radius, op->k);
        break;
      }
    }
    if (!r.passed) break;
    GDS_RF_(end_of_tick)(&cand, t);
    brute_force_replay_end_of_tick(&oracle, t);
    if (cand.checksum != oracle.checksum) {
      r.passed = false;
      snprintf(r.failure, sizeof r.failure, "tick %u, op 0: end-of-tick entity_count mismatch",
               t);
      break;
    }
    const uint32_t period = w->spec.verify_sweep_ticks ? w->spec.verify_sweep_ticks : 1;
    if ((t % period) == period - 1 || t + 1 == w->n_ticks) {
      if (!GDS_XF_(sweep)(&cand, &oracle, w, t, &r)) {
        r.passed = false;
        break;
      }
    }
  }
  if (r.passed) r.checksum = cand.checksum;
  GDS_SF_(free)(&cand.structure);
  brute_force_free(&oracle.structure);
  return r;
}

static inline void GDS_XF_(one_repetition)(const GdsSpatialWorkload* w, const WorldConfig* cfg,
                                           GdsPmu* pmu, GdsRepetition* rep) {
  memset(rep, 0, sizeof *rep);
  rep->step_ns = malloc((w->n_ticks + 1) * sizeof(uint64_t));

  gds_alloc_reset();
  GDS_RT_ replay;
  memset(&replay, 0, sizeof replay);
  GDS_SF_(init)(&replay.structure, cfg);
  gds_pmu_start(pmu);
  const uint64_t t_start = gds_now_ns();
  for (uint32_t t = 0; t < w->n_ticks; ++t) {
    const GdsTick* tk = &w->ticks[t];
    const uint64_t t0 = gds_now_ns();
    for (uint32_t i = tk->op_begin; i < tk->op_end; ++i) GDS_RF_(apply)(&replay, &w->ops[i]);
    GDS_RF_(end_of_tick)(&replay, t);
    const uint64_t t1 = gds_now_ns();
    gds_keep_u64(replay.checksum);
    rep->step_ns[rep->steps++] = t1 - t0;
  }
  const uint64_t t_end = gds_now_ns();
  gds_pmu_stop(pmu);
  rep->total_ns = t_end - t_start;
  rep->checksum = replay.checksum;
  rep->reported_bytes = GDS_SF_(reported_bytes)(&replay.structure);
  rep->final_entities = GDS_SF_(entity_count)(&replay.structure);
  rep->alloc = gds_alloc_snapshot();
  GDS_SF_(free)(&replay.structure);
  rep->pmu_available = pmu->available;
  if (pmu->available) memcpy(rep->pmu_values, pmu->values, sizeof rep->pmu_values);
}

/* Prints the rest of the report after the header and returns the exit code. */
static inline int GDS_XF_(and_report)(const GdsSpatialWorkload* w, const WorldConfig* cfg,
                                      const GdsRunArgs* args, const char* strategy) {
  printf("  \"rewind_strategy\": \"%s\",\n", strategy);

  if (!strcmp(args->mode, "verify")) {
    const GDS_VR_ v = GDS_XF_(verify)(w, cfg);
    printf("  \"status\": \"%s\",\n", v.passed ? "passed" : "failed");
    printf("  \"ops_checked\": %llu,\n", (unsigned long long)v.ops_checked);
    printf("  \"sweeps\": %llu,\n", (unsigned long long)v.sweeps);
    printf("  \"rewinds\": %llu,\n", (unsigned long long)v.rewinds);
    printf("  \"checksum\": \"%llu\",\n", (unsigned long long)v.checksum);
    printf("  \"failure\": \"");
    gds_json_write_escaped(stdout, v.failure);
    printf("\"\n");
    printf("}\n");
    return v.passed ? 0 : 1;
  }
  if (strcmp(args->mode, "bench")) {
    printf("  \"status\": \"bad_mode\"\n}\n");
    return 2;
  }

  GdsPmu pmu;
  gds_pmu_open(&pmu);
  for (int i = 0; i < args->warmup; ++i) {
    GdsRepetition discard;
    GDS_XF_(one_repetition)(w, cfg, &pmu, &discard);
    gds_keep_u64(discard.checksum);
    free(discard.step_ns);
  }
  const int repeats = args->repeats > 0 ? args->repeats : 0;
  GdsRepetition* reps = calloc((size_t)(repeats ? repeats : 1), sizeof *reps);
  for (int i = 0; i < repeats; ++i) GDS_XF_(one_repetition)(w, cfg, &pmu, &reps[i]);

  const GdsRepetition* med = gds_median_repetition(reps, repeats);
  gds_print_common_bench_json(med, reps, repeats, w->n_ops, "tick");
  /* The ticks that open with a rewind, separately: the cost of putting the
   * world back is the question a history strategy answers, and in the tick
   * percentiles it is mixed with every ordinary tick. Zero when nothing
   * rewinds. */
  uint64_t* rewind_steps = malloc((w->n_ticks + 1) * sizeof *rewind_steps);
  size_t n_rewind = 0;
  for (size_t t = 0; t < w->n_ticks && t < med->steps; ++t) {
    const GdsTick* tk = &w->ticks[t];
    if (tk->op_begin < tk->op_end && w->ops[tk->op_begin].kind == GDS_SP_REWIND)
      rewind_steps[n_rewind++] = med->step_ns[t];
  }
  printf("  \"rewind_steps\": %zu,\n", n_rewind);
  printf("  \"rewind_step_ns_p50\": %llu,\n",
         (unsigned long long)gds_percentile(rewind_steps, n_rewind, 0.50));
  printf("  \"rewind_step_ns_max\": %llu,\n",
         (unsigned long long)gds_percentile(rewind_steps, n_rewind, 1.0));
  gds_print_pmu_json(med, &pmu);
  printf("}\n");

  free(rewind_steps);
  for (int i = 0; i < repeats; ++i) free(reps[i].step_ns);
  free(reps);
  gds_pmu_close(&pmu);
  return 0;
}

#undef GDS_RT_
#undef GDS_RF_
#undef GDS_SF_
#undef GDS_XF_
#undef GDS_VR_
