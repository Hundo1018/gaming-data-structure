/* Template header: correctness and measurement of one ECS structure. Include
 * with GDS_S defined, after gds/replay.inc.h has been included for GDS_S and
 * for the oracle, `reference`.
 *
 * Both modes replay the identical op stream through the identical code path, so
 * a verified candidate is measured doing exactly the work it was verified on.
 * The benchmark computes a checksum from every observation it makes; the
 * orchestrator compares that checksum against the oracle's, which makes it
 * impossible for a candidate to be fast by answering incorrectly in a mode
 * where nobody is looking. */
#ifndef GDS_S
#error "define GDS_S to a structure prefix before including gds/run.inc.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "gds/alloc.h"
#include "gds/measure.h"
#include "gds/pmu.h"

#define GDS_RT_ GDS_CAT(GDS_S, _replay)
#define GDS_RF_(n) GDS_CAT(GDS_S, GDS_CAT(_replay_, n))
#define GDS_SF_(n) GDS_FN(GDS_S, n)
#define GDS_XF_(n) GDS_CAT(GDS_S, GDS_CAT(_run_, n))

typedef struct {
  bool passed;
  char failure[512];
  uint64_t ops_checked;
  uint64_t sweeps;
  uint64_t checksum;
} GDS_CAT(GDS_S, _verify_result);

/* Compares every component of every slot ever created, including destroyed
 * ones: a candidate that recycles storage without invalidating old handles
 * fails here rather than silently returning another entity's data. */
static bool GDS_XF_(sweep)(GDS_RT_* cand, reference_replay* oracle, uint32_t frame,
                           GDS_CAT(GDS_S, _verify_result) * r) {
  const size_t cn = GDS_SF_(entity_count)(&cand->structure);
  const size_t on = reference_entity_count(&oracle->structure);
  if (cn != on) {
    snprintf(r->failure, sizeof r->failure, "frame %u, op 0: entity_count %zu != oracle %zu",
             frame, cn, on);
    return false;
  }
  for (size_t slot = 0; slot < cand->n_slots; ++slot) {
    const Entity ce = cand->slots[slot];
    const Entity oe = oracle->slots[slot];
    if (GDS_SF_(alive)(&cand->structure, ce) != reference_alive(&oracle->structure, oe)) {
      snprintf(r->failure, sizeof r->failure, "frame %u, op 0: alive() disagrees for slot %zu",
               frame, slot);
      return false;
    }
    if (GDS_SF_(mask)(&cand->structure, ce) != reference_mask(&oracle->structure, oe)) {
      snprintf(r->failure, sizeof r->failure, "frame %u, op 0: mask() disagrees for slot %zu",
               frame, slot);
      return false;
    }
    for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) {
      ComponentValue cv, ov;
      memset(&cv, 0, sizeof cv);
      memset(&ov, 0, sizeof ov);
      const bool cok = GDS_SF_(get)(&cand->structure, ce, (ComponentId)c, &cv);
      const bool ook = reference_get(&oracle->structure, oe, (ComponentId)c, &ov);
      if (cok != ook) {
        snprintf(r->failure, sizeof r->failure,
                 "frame %u, op 0: get() presence disagrees for slot %zu component %d", frame,
                 slot, c);
        return false;
      }
      if (cok && !gds_value_equal((ComponentId)c, &cv, &ov)) {
        snprintf(r->failure, sizeof r->failure,
                 "frame %u, op 0: get() value disagrees for slot %zu component %d", frame, slot,
                 c);
        return false;
      }
    }
  }
  ++r->sweeps;
  return true;
}

static GDS_CAT(GDS_S, _verify_result) GDS_XF_(verify)(const GdsWorkload* w) {
  GDS_CAT(GDS_S, _verify_result) r;
  memset(&r, 0, sizeof r);
  r.passed = true;
  GDS_RT_ cand;
  reference_replay oracle;
  memset(&cand, 0, sizeof cand);
  memset(&oracle, 0, sizeof oracle);
  cand.slots = malloc((w->slot_count + 1) * sizeof(Entity));
  oracle.slots = malloc((w->slot_count + 1) * sizeof(Entity));
  GDS_SF_(init)(&cand.structure);
  reference_init(&oracle.structure);

  for (uint32_t f = 0; f < w->n_frames && r.passed; ++f) {
    const GdsFrame* fr = &w->frames[f];
    for (uint32_t i = fr->op_begin; i < fr->op_end; ++i) {
      const GdsOp* op = &w->ops[i];
      const uint64_t before_c = cand.checksum;
      const uint64_t before_o = oracle.checksum;
      GDS_RF_(apply)(&cand, op);
      reference_replay_apply(&oracle, op);
      ++r.ops_checked;
      if (cand.checksum != oracle.checksum) {
        r.passed = false;
        snprintf(r.failure, sizeof r.failure,
                 "frame %u, op %u: observation mismatch on op kind %d slot %u component %d "
                 "(candidate fold %llu, oracle fold %llu)",
                 f, i - fr->op_begin, (int)op->kind, op->slot, (int)op->comp,
                 (unsigned long long)(cand.checksum ^ before_c),
                 (unsigned long long)(oracle.checksum ^ before_o));
        break;
      }
    }
    if (!r.passed) break;
    GDS_RF_(end_of_frame)(&cand, &w->spec);
    reference_replay_end_of_frame(&oracle, &w->spec);
    if (cand.checksum != oracle.checksum) {
      r.passed = false;
      snprintf(r.failure, sizeof r.failure,
               "frame %u, op 0: end-of-frame observation mismatch (integrate or query)", f);
      break;
    }
    const uint32_t period = w->spec.verify_sweep_frames ? w->spec.verify_sweep_frames : 1;
    if ((f % period) == period - 1 || f + 1 == w->n_frames) {
      if (!GDS_XF_(sweep)(&cand, &oracle, f, &r)) {
        r.passed = false;
        break;
      }
    }
  }
  if (r.passed) r.checksum = cand.checksum;
  GDS_SF_(free)(&cand.structure);
  reference_free(&oracle.structure);
  free(cand.slots);
  free(oracle.slots);
  return r;
}

static void GDS_XF_(one_repetition)(const GdsWorkload* w, GdsPmu* pmu, GdsRepetition* rep) {
  memset(rep, 0, sizeof *rep);
  /* Everything the harness owns is allocated before the reset so that the
   * measured bytes belong to the candidate. */
  rep->step_ns = malloc((w->n_frames + 1) * sizeof(uint64_t));
  Entity* slots = malloc((w->slot_count + 1) * sizeof(Entity));

  gds_alloc_reset();
  GDS_RT_ replay;
  memset(&replay, 0, sizeof replay);
  replay.slots = slots;
  GDS_SF_(init)(&replay.structure);
  gds_pmu_start(pmu);
  const uint64_t t_start = gds_now_ns();
  for (uint32_t f = 0; f < w->n_frames; ++f) {
    const GdsFrame* fr = &w->frames[f];
    const uint64_t t0 = gds_now_ns();
    for (uint32_t i = fr->op_begin; i < fr->op_end; ++i) GDS_RF_(apply)(&replay, &w->ops[i]);
    GDS_RF_(end_of_frame)(&replay, &w->spec);
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
  free(slots);
  rep->pmu_available = pmu->available;
  if (pmu->available) memcpy(rep->pmu_values, pmu->values, sizeof rep->pmu_values);
}

#undef GDS_RT_
#undef GDS_RF_
#undef GDS_SF_
#undef GDS_XF_
