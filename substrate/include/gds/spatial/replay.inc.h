/* Template header: the spatial replay of one op stream against one structure.
 * Include with GDS_S defined to the structure's prefix; it defines
 * GDS_S_replay and its functions. Included once for the oracle and once for
 * each way the candidate is run, so all of them replay through the same code.
 *
 * Every query result, every position read and every entity count is folded
 * into a checksum; verification compares that checksum against the oracle
 * after each operation, and measurement reports it so the orchestrator can
 * compare it against the oracle's for the same workload. */
#ifndef GDS_S
#error "define GDS_S to a structure prefix before including gds/spatial/replay.inc.h"
#endif

#include "gds/spatial/api.h"
#include "gds/spatial/workload.h"

#define GDS_RT_ GDS_CAT(GDS_S, _replay)
#define GDS_RF_(n) GDS_CAT(GDS_S, GDS_CAT(_replay_, n))
#define GDS_SF_(n) GDS_FN(GDS_S, n)

typedef struct {
  GDS_S structure;
  uint64_t checksum;
} GDS_RT_;

static inline void GDS_RF_(fold)(GDS_RT_* r, uint64_t v) {
  r->checksum = gds_splitmix64(r->checksum ^ v);
}

/* Returns what a rewind returned, and true for every other op. */
static inline bool GDS_RF_(apply)(GDS_RT_* r, const GdsSpatialOp* op) {
  switch ((GdsSpatialOpKind)op->kind) {
    case GDS_SP_INSERT:
      GDS_SF_(insert)(&r->structure, op->id, op->v);
      return true;
    case GDS_SP_REMOVE:
      GDS_SF_(remove)(&r->structure, op->id);
      return true;
    case GDS_SP_MOVE_BY:
      GDS_SF_(move_by)(&r->structure, op->id, op->v);
      return true;
    case GDS_SP_QUERY_RADIUS:
      GDS_RF_(fold)(r, GDS_SF_(query_radius)(&r->structure, op->v, op->radius));
      return true;
    case GDS_SP_QUERY_RADIUS_OF:
      GDS_RF_(fold)(r, GDS_SF_(query_radius_of)(&r->structure, op->id, op->radius));
      return true;
    case GDS_SP_QUERY_KNN:
      GDS_RF_(fold)(r, GDS_SF_(query_knn)(&r->structure, op->v, op->k));
      return true;
    case GDS_SP_REWIND:
      return GDS_SF_(rewind_to)(&r->structure, op->k);
  }
  return true;
}

static inline void GDS_RF_(end_of_tick)(GDS_RT_* r, uint64_t tick) {
  GDS_RF_(fold)(r, GDS_SF_(entity_count)(&r->structure));
  GDS_SF_(end_tick)(&r->structure, tick);
}

#undef GDS_RT_
#undef GDS_RF_
#undef GDS_SF_
