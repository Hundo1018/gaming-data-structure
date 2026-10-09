/* Template header: the ECS replay of one op stream against one structure.
 * Include with GDS_S defined to the structure's prefix; it defines
 * GDS_S_replay and its functions. Included once for the oracle and once for
 * the candidate, so both replay through the same code.
 *
 * Every observation is folded into a checksum, which is how verification
 * compares a candidate with the oracle after each operation, and how the
 * benchmark proves it answered the same questions the same way. */
#ifndef GDS_S
#error "define GDS_S to a structure prefix before including gds/replay.inc.h"
#endif

#include "gds/api.h"
#include "gds/workload.h"

#define GDS_RT_ GDS_CAT(GDS_S, _replay)
#define GDS_RF_(n) GDS_CAT(GDS_S, GDS_CAT(_replay_, n))
#define GDS_SF_(n) GDS_FN(GDS_S, n)

typedef struct {
  GDS_S structure;
  Entity* slots;
  size_t n_slots;
  uint64_t checksum;
  uint64_t frame;
} GDS_RT_;

static inline void GDS_RF_(fold)(GDS_RT_* r, uint64_t v) {
  r->checksum = gds_splitmix64(r->checksum ^ v);
}

static inline void GDS_RF_(apply)(GDS_RT_* r, const GdsOp* op) {
  const ComponentId c = (ComponentId)op->comp;
  switch ((GdsOpKind)op->kind) {
    case GDS_OP_CREATE: {
      ComponentValue values[GDS_COMPONENT_COUNT];
      gds_create_values(op->slot, values);
      r->slots[r->n_slots++] = GDS_SF_(create)(&r->structure, op->mask, values);
      break;
    }
    case GDS_OP_DESTROY:
      GDS_SF_(destroy)(&r->structure, r->slots[op->slot]);
      break;
    case GDS_OP_ADD:
      GDS_SF_(add)(&r->structure, r->slots[op->slot], c, &op->value);
      break;
    case GDS_OP_REMOVE:
      GDS_SF_(remove)(&r->structure, r->slots[op->slot], c);
      break;
    case GDS_OP_GET: {
      ComponentValue out;
      memset(&out, 0, sizeof out);
      const bool ok = GDS_SF_(get)(&r->structure, r->slots[op->slot], c, &out);
      GDS_RF_(fold)(r, ok ? gds_digest_component(c, &out) : 0xDEADBEEFull);
      break;
    }
    case GDS_OP_SET:
      GDS_RF_(fold)(r, GDS_SF_(set)(&r->structure, r->slots[op->slot], c, &op->value) ? 3 : 5);
      break;
  }
}

static inline void GDS_RF_(end_of_frame)(GDS_RT_* r, const GdsWorkloadSpec* spec) {
  if (spec->integrate_per_frame) GDS_SF_(integrate)(&r->structure, spec->dt);
  for (int i = 0; i < spec->n_query_masks; ++i) {
    const uint64_t salt = gds_query_salt(spec->seed, r->frame, (uint64_t)i);
    GDS_RF_(fold)(r, GDS_SF_(query)(&r->structure, spec->query_masks[i], salt));
  }
  GDS_RF_(fold)(r, GDS_SF_(entity_count)(&r->structure));
  GDS_SF_(sync)(&r->structure);
  ++r->frame;
}

#undef GDS_RT_
#undef GDS_RF_
#undef GDS_SF_
