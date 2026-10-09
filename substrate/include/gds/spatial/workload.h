/* Spatial workloads.
 *
 * The dimensions that decide a spatial index are how fast things move relative
 * to how far a query reaches, how unevenly they are spread, where the queries
 * are aimed, and how often the world is asked to go back. Each is a field
 * here. */
#ifndef GDS_SPATIAL_WORKLOAD_H
#define GDS_SPATIAL_WORKLOAD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gds/spatial/types.h"

typedef enum {
  GDS_SP_INSERT = 0,
  GDS_SP_REMOVE,
  GDS_SP_MOVE_BY,
  GDS_SP_QUERY_RADIUS,
  GDS_SP_QUERY_RADIUS_OF,
  GDS_SP_QUERY_KNN,
  GDS_SP_REWIND,
} GdsSpatialOpKind;

typedef struct {
  uint8_t kind;   /* GdsSpatialOpKind */
  EntityId id;    /* Insert/Remove/MoveBy/QueryRadiusOf; GDS_NO_ENTITY otherwise */
  Vec3 v;         /* Insert position, MoveBy delta, query centre */
  float radius;   /* QueryRadius / QueryRadiusOf */
  uint32_t k;     /* QueryKnn: neighbours. Rewind: target tick. */
} GdsSpatialOp;

typedef struct {
  uint32_t op_begin;
  uint32_t op_end;
} GdsTick;

typedef enum { GDS_PLACE_UNIFORM, GDS_PLACE_CLUSTERED } GdsPlacement;

/* How movers choose their displacement. Independent movers each take their own
 * random step, so a clump diffuses. Flocking movers share their cluster's
 * drift and add a small step of their own, so a clump stays a clump while it
 * travels: the case of a crowd, a herd, an army on the march. */
typedef enum { GDS_MOVE_INDEPENDENT, GDS_MOVE_FLOCK } GdsMovement;

typedef struct {
  char id[128];
  char visibility[16];
  char track[16];
  char note[512];

  uint64_t seed;
  float world_size;   /* cube side; the world is centred on the origin */
  float world_height; /* z extent, so a flat world can be expressed */

  uint32_t initial_entities;
  uint32_t ticks;
  uint32_t inserts_per_tick;
  uint32_t removes_per_tick;

  /* How many entities move each tick, given either as a share of the live
   * population or as an absolute count. A scaling experiment needs the
   * absolute form: holding the operation count fixed while the population
   * grows is what separates the cost of one operation from the number of
   * them. */
  float move_fraction;
  uint32_t moves_per_tick; /* 0 leaves move_fraction in charge */
  float speed_min;         /* per-tick displacement, world units */
  float speed_max;
  float teleport_ratio;    /* share of moves that jump anywhere in the world */
  GdsMovement movement;
  float flock_speed;       /* per-tick drift of each cluster under movement: flock */

  GdsPlacement placement;
  uint32_t clusters;
  float cluster_radius;

  uint32_t radius_queries_per_tick;
  uint32_t entity_radius_queries_per_tick;
  uint32_t knn_queries_per_tick;
  float query_radius_min;
  float query_radius_max;
  uint32_t knn_k;
  GdsPlacement query_focus;

  uint32_t rewind_every;  /* 0 disables rewinds */
  uint32_t rewind_depth;
  uint32_t history_ticks; /* retained history; 0 means none is needed */

  uint32_t verify_sweep_ticks;
} GdsSpatialSpec;

typedef struct {
  GdsSpatialSpec spec;
  GdsSpatialOp* ops;
  size_t n_ops;
  GdsTick* ticks;
  size_t n_ticks;
  uint32_t max_entity_id;
  uint32_t rewind_count;
  Bounds bounds;
  float typical_query_radius;
} GdsSpatialWorkload;

void gds_spatial_spec_defaults(GdsSpatialSpec* out);
/* Both start from the defaults. */
bool gds_parse_spatial_file(const char* path, GdsSpatialSpec* out, char* error, size_t error_len);
bool gds_parse_spatial_text(const char* text, GdsSpatialSpec* out, char* error, size_t error_len);
void gds_generate_spatial_workload(const GdsSpatialSpec* spec, GdsSpatialWorkload* out);
void gds_free_spatial_workload(GdsSpatialWorkload* w);
const char* gds_placement_name(GdsPlacement p);
const char* gds_movement_name(GdsMovement m);

#endif
