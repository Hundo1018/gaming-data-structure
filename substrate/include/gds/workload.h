/* A workload is a research object, not a benchmark parameter.
 *
 * It is generated once from a seed into a concrete op stream, then replayed
 * byte-identically by the oracle and by every candidate. Generation cost is
 * therefore never inside a measurement, and two candidates are always compared
 * on exactly the same sequence of operations. */
#ifndef GDS_WORKLOAD_H
#define GDS_WORKLOAD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gds/types.h"

typedef enum {
  GDS_OP_CREATE = 0,
  GDS_OP_DESTROY,
  GDS_OP_ADD,
  GDS_OP_REMOVE,
  GDS_OP_GET,
  GDS_OP_SET,
} GdsOpKind;

typedef struct {
  uint8_t kind;        /* GdsOpKind */
  uint8_t comp;        /* ComponentId */
  ComponentMask mask;  /* Create only */
  uint32_t slot;       /* index into the harness slot table */
  ComponentValue value;
} GdsOp;

/* Initial component values for a created entity. A pure function of the slot
 * index so that the oracle and every candidate build identical entities from an
 * op record that carries no payload. */
static inline float gds_create_value_f(uint64_t bits, float lo, float hi) {
  const float u = (float)((double)(bits >> 40) * (1.0 / 16777216.0));
  return lo + u * (hi - lo);
}

static inline void gds_create_values(uint32_t slot, ComponentValue* out) {
  const uint64_t a = gds_splitmix64(0x9E3779B97F4A7C15ull ^ slot);
  const uint64_t b = gds_splitmix64(a);
  const uint64_t c = gds_splitmix64(b);
  memset(out, 0, sizeof(ComponentValue) * GDS_COMPONENT_COUNT);
  out[0].position.x = gds_create_value_f(a, -512.f, 512.f);
  out[0].position.y = gds_create_value_f(a >> 20, -64.f, 64.f);
  out[0].position.z = gds_create_value_f(b, -512.f, 512.f);
  out[1].velocity.x = gds_create_value_f(b >> 20, -8.f, 8.f);
  out[1].velocity.y = gds_create_value_f(c, -2.f, 2.f);
  out[1].velocity.z = gds_create_value_f(c >> 20, -8.f, 8.f);
  out[2].health.hp = (int32_t)(1 + (a % 100));
  out[2].health.max_hp = 100;
  out[3].tag.bits = (uint32_t)(c >> 32);
}

typedef struct {
  uint32_t op_begin;
  uint32_t op_end;
} GdsFrame;

/* Access distribution over the currently live slots. */
typedef enum {
  GDS_ACCESS_UNIFORM, /* every live entity equally likely */
  GDS_ACCESS_ZIPF,    /* heavy head: a few entities take most of the traffic */
  GDS_ACCESS_RECENT,  /* temporal locality: entities created most recently */
} GdsAccess;

enum { GDS_MAX_QUERY_MASKS = 16 };

typedef struct {
  char id[128];
  char visibility[16]; /* public | hidden */
  char track[16];
  char note[512];

  uint64_t seed;
  uint32_t initial_entities;
  uint32_t max_entities;
  uint32_t frames;
  uint32_t ops_per_frame;

  /* Relative weights, normalised at generation time. */
  float w_create, w_destroy, w_add, w_remove, w_get, w_set;

  GdsAccess access;
  double zipf_exponent;
  uint32_t recency_window;

  float burst_frame_ratio; /* fraction of frames that carry a burst */
  uint32_t burst_multiplier;

  float stale_access_ratio; /* fraction of accesses aimed at dead handles */

  /* How many components one point access reads or writes. Gameplay code that
   * touches an entity usually touches several of its components at once; with
   * a width of N, a drawn Get or Set becomes N consecutive accesses of the same
   * kind to the same entity, over N distinct components starting at the drawn
   * one, and counts as N operations of the frame. */
  uint32_t access_width;

  /* Probability that a newly created entity carries each component. */
  float p_position, p_velocity, p_health, p_tag;

  bool integrate_per_frame;
  float dt;
  ComponentMask query_masks[GDS_MAX_QUERY_MASKS];
  int n_query_masks;

  uint32_t verify_sweep_frames; /* full oracle sweep every N frames */
} GdsWorkloadSpec;

typedef struct {
  GdsWorkloadSpec spec;
  GdsOp* ops;
  size_t n_ops;
  GdsFrame* frames;
  size_t n_frames;
  uint32_t slot_count; /* total slots the replay will allocate */
} GdsWorkload;

void gds_workload_spec_defaults(GdsWorkloadSpec* out);

/* Parses the flat `key: value` workload format into `out`, which the caller has
 * filled with defaults. Returns false and fills `error` on an unknown key or an
 * unparsable value: a silently ignored field would make two different
 * experiments look like the same one. */
bool gds_parse_workload_file(const char* path, GdsWorkloadSpec* out, char* error, size_t error_len);
bool gds_parse_workload_text(const char* text, GdsWorkloadSpec* out, char* error, size_t error_len);

void gds_generate_workload(const GdsWorkloadSpec* spec, GdsWorkload* out);
void gds_free_workload(GdsWorkload* w);

const char* gds_access_name(GdsAccess a);

#endif
