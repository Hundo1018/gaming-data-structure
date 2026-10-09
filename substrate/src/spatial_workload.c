#include "gds/spatial/workload.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gds/vec.h"
#include "parse_util.h"

const char* gds_placement_name(GdsPlacement p) {
  return p == GDS_PLACE_UNIFORM ? "uniform" : "clustered";
}

const char* gds_movement_name(GdsMovement m) {
  return m == GDS_MOVE_INDEPENDENT ? "independent" : "flock";
}

void gds_spatial_spec_defaults(GdsSpatialSpec* s) {
  memset(s, 0, sizeof *s);
  gds_copy_str(s->id, sizeof s->id, "unnamed");
  gds_copy_str(s->visibility, sizeof s->visibility, "public");
  gds_copy_str(s->track, sizeof s->track, "spatial");
  s->seed = 1;
  s->world_size = 1024.0f;
  s->world_height = 1024.0f;
  s->initial_entities = 20000;
  s->ticks = 300;
  s->inserts_per_tick = 0;
  s->removes_per_tick = 0;
  s->move_fraction = 1.0f;
  s->moves_per_tick = 0;
  s->speed_min = 0.0f;
  s->speed_max = 2.0f;
  s->teleport_ratio = 0.0f;
  s->movement = GDS_MOVE_INDEPENDENT;
  s->flock_speed = 0.0f;
  s->placement = GDS_PLACE_UNIFORM;
  s->clusters = 16;
  s->cluster_radius = 32.0f;
  s->radius_queries_per_tick = 64;
  s->entity_radius_queries_per_tick = 64;
  s->knn_queries_per_tick = 16;
  s->query_radius_min = 8.0f;
  s->query_radius_max = 16.0f;
  s->knn_k = 8;
  s->query_focus = GDS_PLACE_UNIFORM;
  s->rewind_every = 0;
  s->rewind_depth = 8;
  s->history_ticks = 0;
  s->verify_sweep_ticks = 16;
}

static bool parse_movement(const char* v, GdsMovement* out) {
  if (!strcmp(v, "independent")) { *out = GDS_MOVE_INDEPENDENT; return true; }
  if (!strcmp(v, "flock")) { *out = GDS_MOVE_FLOCK; return true; }
  return false;
}

static bool parse_placement(const char* v, GdsPlacement* out) {
  if (!strcmp(v, "uniform")) { *out = GDS_PLACE_UNIFORM; return true; }
  if (!strcmp(v, "clustered")) { *out = GDS_PLACE_CLUSTERED; return true; }
  return false;
}

typedef struct { uint64_t state; } Rng;

static inline uint64_t rng_next(Rng* r) {
  r->state = gds_splitmix64(r->state + 0x9E3779B97F4A7C15ull);
  return r->state;
}
static inline uint32_t rng_below(Rng* r, uint32_t n) {
  return n ? (uint32_t)(rng_next(r) % n) : 0;
}
static inline float rng_unit(Rng* r) {
  return (float)((double)(rng_next(r) >> 40) * (1.0 / 16777216.0));
}
static inline float rng_range(Rng* r, float lo, float hi) { return lo + rng_unit(r) * (hi - lo); }
/* Sum of three uniforms: a cheap bell shape, so a cluster has a dense middle
 * and a thin edge instead of a hard rim. The draws are separate statements
 * because C leaves the order of a + b + c's operands unspecified. */
static inline float rng_bell(Rng* r) {
  const float a = rng_unit(r);
  const float b = rng_unit(r);
  const float c = rng_unit(r);
  return (a + b + c) * (2.0f / 3.0f) - 1.0f;
}

bool gds_parse_spatial_text(const char* text, GdsSpatialSpec* out, char* error,
                            size_t error_len) {
  gds_spatial_spec_defaults(out);
  char* buf = strdup(text);
  char* cursor = buf;
  char *key = NULL, *val = NULL;
  int line_no = 0;
  bool ok = true;
  bool seen_move_fraction = false;
  bool seen_moves_per_tick = false;
  int r;
  while ((r = gds_next_pair(&cursor, &key, &val)) != 0) {
    ++line_no;
    if (r == 1) continue;
    if (r < 0) {
      snprintf(error, error_len, "line %d: expected 'key: value'", line_no);
      ok = false;
      break;
    }
    uint64_t u = 0;
    double d = 0;
#define NEED_U(dst)                                                          \
  do {                                                                       \
    if (!gds_parse_u64(val, &u)) {                                           \
      snprintf(error, error_len, "%s: expected an integer", key);            \
      ok = false;                                                            \
    } else {                                                                 \
      (dst) = (uint32_t)u;                                                   \
    }                                                                        \
  } while (0)
#define NEED_F(dst)                                                          \
  do {                                                                       \
    if (!gds_parse_double(val, &d)) {                                        \
      snprintf(error, error_len, "%s: expected a number", key);              \
      ok = false;                                                            \
    } else {                                                                 \
      (dst) = (float)d;                                                      \
    }                                                                        \
  } while (0)
#define FAIL(msg)                                                            \
  do {                                                                       \
    snprintf(error, error_len, "%s", msg);                                   \
    ok = false;                                                              \
  } while (0)

    if (!strcmp(key, "id")) gds_copy_str(out->id, sizeof out->id, val);
    else if (!strcmp(key, "visibility")) {
      if (strcmp(val, "public") && strcmp(val, "hidden")) FAIL("visibility must be public or hidden");
      else gds_copy_str(out->visibility, sizeof out->visibility, val);
    }
    else if (!strcmp(key, "track")) {
      if (strcmp(val, "spatial")) FAIL("this binary only runs track: spatial");
      else gds_copy_str(out->track, sizeof out->track, val);
    }
    else if (!strcmp(key, "note")) gds_copy_str(out->note, sizeof out->note, val);
    else if (!strcmp(key, "seed")) {
      if (!gds_parse_u64(val, &u)) FAIL("seed: expected an integer");
      else out->seed = u;
    }
    else if (!strcmp(key, "world_size")) NEED_F(out->world_size);
    else if (!strcmp(key, "world_height")) NEED_F(out->world_height);
    else if (!strcmp(key, "initial_entities")) NEED_U(out->initial_entities);
    else if (!strcmp(key, "ticks")) NEED_U(out->ticks);
    else if (!strcmp(key, "inserts_per_tick")) NEED_U(out->inserts_per_tick);
    else if (!strcmp(key, "removes_per_tick")) NEED_U(out->removes_per_tick);
    else if (!strcmp(key, "move_fraction")) { seen_move_fraction = true; NEED_F(out->move_fraction); }
    else if (!strcmp(key, "moves_per_tick")) { seen_moves_per_tick = true; NEED_U(out->moves_per_tick); }
    else if (!strcmp(key, "speed_min")) NEED_F(out->speed_min);
    else if (!strcmp(key, "speed_max")) NEED_F(out->speed_max);
    else if (!strcmp(key, "teleport_ratio")) NEED_F(out->teleport_ratio);
    else if (!strcmp(key, "movement")) {
      if (!parse_movement(val, &out->movement)) FAIL("movement must be independent or flock");
    }
    else if (!strcmp(key, "flock_speed")) NEED_F(out->flock_speed);
    else if (!strcmp(key, "placement")) {
      if (!parse_placement(val, &out->placement)) FAIL("placement must be uniform or clustered");
    }
    else if (!strcmp(key, "clusters")) NEED_U(out->clusters);
    else if (!strcmp(key, "cluster_radius")) NEED_F(out->cluster_radius);
    else if (!strcmp(key, "radius_queries_per_tick")) NEED_U(out->radius_queries_per_tick);
    else if (!strcmp(key, "entity_radius_queries_per_tick")) NEED_U(out->entity_radius_queries_per_tick);
    else if (!strcmp(key, "knn_queries_per_tick")) NEED_U(out->knn_queries_per_tick);
    else if (!strcmp(key, "query_radius_min")) NEED_F(out->query_radius_min);
    else if (!strcmp(key, "query_radius_max")) NEED_F(out->query_radius_max);
    else if (!strcmp(key, "knn_k")) NEED_U(out->knn_k);
    else if (!strcmp(key, "query_focus")) {
      if (!parse_placement(val, &out->query_focus)) FAIL("query_focus must be uniform or clustered");
    }
    else if (!strcmp(key, "rewind_every")) NEED_U(out->rewind_every);
    else if (!strcmp(key, "rewind_depth")) NEED_U(out->rewind_depth);
    else if (!strcmp(key, "history_ticks")) NEED_U(out->history_ticks);
    else if (!strcmp(key, "verify_sweep_ticks")) NEED_U(out->verify_sweep_ticks);
    else {
      snprintf(error, error_len, "line %d: unknown key '%s'", line_no, key);
      ok = false;
    }
#undef NEED_U
#undef NEED_F
#undef FAIL
    if (!ok) break;
  }
  free(buf);
  if (!ok) return false;
  if (seen_move_fraction && seen_moves_per_tick) {
    snprintf(error, error_len,
             "set move_fraction or moves_per_tick, not both: they are two ways of saying the "
             "same thing and a file that sets both does not say which it means");
    return false;
  }
  if (out->rewind_every > 0 && out->history_ticks < out->rewind_depth) {
    out->history_ticks = out->rewind_depth;
  }
  return true;
}

bool gds_parse_spatial_file(const char* path, GdsSpatialSpec* out, char* error,
                            size_t error_len) {
  char* text = gds_read_file(path);
  if (!text) {
    snprintf(error, error_len, "cannot open workload file: %s", path);
    return false;
  }
  const bool ok = gds_parse_spatial_text(text, out, error, error_len);
  free(text);
  return ok;
}

/* ------------------------------------------------------------------------
 * Generation
 * ------------------------------------------------------------------------ */

typedef GDS_VEC(Vec3) Vec3Vec;
typedef GDS_VEC(EntityId) IdVec;
typedef GDS_VEC(GdsSpatialOp) SpOpVec;
typedef GDS_VEC(GdsTick) TickVec;

typedef struct {
  IdVec inserted;
  IdVec removed;
} TickDelta;

typedef struct {
  const GdsSpatialSpec* spec;
  Rng rng;
  float hx, hz;
  Bounds bounds;
  uint32_t ncl;
  bool flocking;
  Vec3Vec centres;
  Vec3Vec drift;
  /* Simulated time along the current branch. It advances one per tick and is
   * set back by a rewind, so after rolling back the clusters are where they
   * were. */
  uint32_t sim_time;
  uint32_t last_cluster;
  uint8_t* live;
  uint32_t* cluster_of;
  IdVec live_list;
  uint32_t next_id;
  TickDelta* deltas;
  SpOpVec ops;
} SpGen;

static Vec3 centre_now(SpGen* g, uint32_t c) {
  if (!g->flocking) return g->centres.data[c];
  const float s = (float)g->sim_time;
  const Vec3 base = g->centres.data[c];
  const Vec3 dr = g->drift.data[c];
  Vec3 p = {base.x + dr.x * s, base.y + dr.y * s, base.z + dr.z * s};
  return gds_wrap_into(p, &g->bounds);
}

static Vec3 sample_point(SpGen* g, GdsPlacement mode) {
  Vec3 p;
  if (mode == GDS_PLACE_UNIFORM) {
    /* Drawn only when it is used, so that every workload that does not flock
     * generates exactly the op stream it generated before flocking existed. */
    if (g->flocking) g->last_cluster = rng_below(&g->rng, g->ncl);
    p.x = rng_range(&g->rng, -g->hx, g->hx);
    p.y = rng_range(&g->rng, -g->hx, g->hx);
    p.z = rng_range(&g->rng, -g->hz, g->hz);
    return p;
  }
  g->last_cluster = rng_below(&g->rng, g->ncl);
  const Vec3 c = centre_now(g, g->last_cluster);
  const float r = g->spec->cluster_radius;
  p.x = c.x + rng_bell(&g->rng) * r;
  p.y = c.y + rng_bell(&g->rng) * r;
  p.z = c.z + rng_bell(&g->rng) * r;
  return gds_wrap_into(p, &g->bounds);
}

static GdsSpatialOp blank_op(GdsSpatialOpKind kind) {
  GdsSpatialOp op;
  memset(&op, 0, sizeof op);
  op.kind = (uint8_t)kind;
  op.id = GDS_NO_ENTITY;
  return op;
}

static void do_insert(SpGen* g, uint32_t tick) {
  const EntityId id = g->next_id++;
  g->live[id] = 1;
  gds_vec_push(g->live_list, id);
  gds_vec_push(g->deltas[tick].inserted, id);
  GdsSpatialOp op = blank_op(GDS_SP_INSERT);
  op.id = id;
  op.v = sample_point(g, g->spec->placement);
  if (g->flocking) g->cluster_of[id] = g->last_cluster;
  gds_vec_push(g->ops, op);
}

static void do_remove(SpGen* g, uint32_t tick) {
  if (g->live_list.size == 0) return;
  const uint32_t idx = rng_below(&g->rng, (uint32_t)g->live_list.size);
  const EntityId id = g->live_list.data[idx];
  g->live_list.data[idx] = gds_vec_back(g->live_list);
  --g->live_list.size;
  g->live[id] = 0;
  gds_vec_push(g->deltas[tick].removed, id);
  GdsSpatialOp op = blank_op(GDS_SP_REMOVE);
  op.id = id;
  gds_vec_push(g->ops, op);
}

static void rebuild_live_list(SpGen* g) {
  gds_vec_clear(g->live_list);
  for (uint32_t i = 0; i < g->next_id; ++i)
    if (g->live[i]) gds_vec_push(g->live_list, i);
}

/* A random step of the given length in a direction from three bells. */
static Vec3 random_step(SpGen* g, float speed) {
  const float dx = rng_bell(&g->rng);
  const float dy = rng_bell(&g->rng);
  const float dz = rng_bell(&g->rng);
  const float len = sqrtf(dx * dx + dy * dy + dz * dz);
  const float scale = len > 1e-6f ? speed / len : 0.0f;
  Vec3 v = {dx * scale, dy * scale, dz * scale};
  return v;
}

void gds_generate_spatial_workload(const GdsSpatialSpec* spec, GdsSpatialWorkload* w) {
  memset(w, 0, sizeof *w);
  w->spec = *spec;
  SpGen g;
  memset(&g, 0, sizeof g);
  g.spec = spec;
  g.hx = spec->world_size * 0.5f;
  g.hz = spec->world_height * 0.5f;
  g.bounds.min = gds_vec3(-g.hx, -g.hx, -g.hz);
  g.bounds.max = gds_vec3(g.hx, g.hx, g.hz);
  w->bounds = g.bounds;
  w->typical_query_radius = 0.5f * (spec->query_radius_min + spec->query_radius_max);
  g.rng.state = gds_splitmix64(spec->seed | 1);

  /* Cluster centres are fixed for the whole run under independent movement, so
   * a clustered world stays clustered where the queries are aimed even as
   * entities drift. Under flocking each cluster also has a drift, and its
   * centre at a given point of simulated time is where that drift has carried
   * it; the queries follow it. */
  g.ncl = spec->clusters > 1 ? spec->clusters : 1;
  gds_vec_reserve(g.centres, g.ncl);
  for (uint32_t i = 0; i < g.ncl; ++i) {
    Vec3 c;
    c.x = rng_range(&g.rng, -g.hx, g.hx);
    c.y = rng_range(&g.rng, -g.hx, g.hx);
    c.z = rng_range(&g.rng, -g.hz, g.hz);
    gds_vec_push(g.centres, c);
  }
  g.flocking = spec->movement == GDS_MOVE_FLOCK;
  if (g.flocking) {
    gds_vec_reserve(g.drift, g.ncl);
    for (uint32_t i = 0; i < g.ncl; ++i) {
      const Vec3 d = random_step(&g, spec->flock_speed);
      gds_vec_push(g.drift, d);
    }
  }

  /* The generator mirrors liveness only. Positions live in the structures, so
   * a rewind needs no position history here. */
  const uint32_t max_ids = spec->initial_entities + spec->inserts_per_tick * spec->ticks + 1;
  g.live = calloc(max_ids, 1);
  /* Which cluster an entity flocks with: the one it was placed in, or a random
   * one under uniform placement. */
  g.cluster_of = calloc(g.flocking ? max_ids : 1, sizeof *g.cluster_of);
  gds_vec_reserve(g.live_list, max_ids);
  g.deltas = calloc(spec->ticks ? spec->ticks : 1, sizeof *g.deltas);

  TickVec ticks;
  gds_vec_init(ticks);
  IdVec flock_order;
  gds_vec_init(flock_order);

  /* Tick 0 is the load tick: it builds the initial population and nothing
   * else. */
  {
    const uint32_t begin = (uint32_t)g.ops.size;
    for (uint32_t i = 0; i < spec->initial_entities; ++i) do_insert(&g, 0);
    GdsTick tk = {begin, (uint32_t)g.ops.size};
    gds_vec_push(ticks, tk);
  }

  for (uint32_t t = 1; t < spec->ticks; ++t) {
    const uint32_t begin = (uint32_t)g.ops.size;
    gds_vec_clear(g.deltas[t].inserted);
    gds_vec_clear(g.deltas[t].removed);
    ++g.sim_time;

    const bool rewinding =
        spec->rewind_every > 0 && (t % spec->rewind_every) == 0 && t > spec->rewind_depth;
    if (rewinding) {
      const uint32_t target = t - spec->rewind_depth - 1;
      GdsSpatialOp op = blank_op(GDS_SP_REWIND);
      op.k = target;
      gds_vec_push(g.ops, op);
      ++w->rewind_count;
      /* Roll the generator's own liveness model back to the end of `target`,
       * then carry on down a different branch: ids created in the discarded
       * ticks are never reissued, so nothing is ambiguous after the branch. */
      for (uint32_t back = t - 1; back > target; --back) {
        TickDelta* dl = &g.deltas[back];
        for (size_t i = 0; i < dl->inserted.size; ++i) g.live[dl->inserted.data[i]] = 0;
        for (size_t i = 0; i < dl->removed.size; ++i) g.live[dl->removed.data[i]] = 1;
        gds_vec_clear(dl->inserted);
        gds_vec_clear(dl->removed);
      }
      rebuild_live_list(&g);
      /* This tick continues the branch from the end of `target`. */
      g.sim_time = target + 1;
    }

    for (uint32_t i = 0; i < spec->inserts_per_tick; ++i) do_insert(&g, t);
    for (uint32_t i = 0; i < spec->removes_per_tick; ++i) do_remove(&g, t);

    const size_t n_live = g.live_list.size;
    uint32_t movers = spec->moves_per_tick > 0
                          ? spec->moves_per_tick
                          : (uint32_t)((double)n_live * spec->move_fraction);
    if (n_live == 0) movers = 0;
    /* Independent movers are drawn with replacement, which is harmless when
     * each takes its own random step. A flocking mover adds its cluster's
     * drift, so an entity drawn twice would run ahead of its cluster and one
     * never drawn would fall behind, and the clump would smear along its
     * drift. Under flocking the movers are therefore distinct: a partial
     * shuffle of the live list. With move_fraction below 1 the entities not
     * chosen still lag. */
    if (g.flocking && n_live > 0) {
      gds_vec_clear(flock_order);
      gds_vec_reserve(flock_order, n_live);
      memcpy(flock_order.data, g.live_list.data, n_live * sizeof(EntityId));
      flock_order.size = n_live;
      if (movers > (uint32_t)n_live) movers = (uint32_t)n_live;
      for (uint32_t i = 0; i < movers; ++i) {
        const uint32_t j = i + rng_below(&g.rng, (uint32_t)n_live - i);
        const EntityId tmp = flock_order.data[i];
        flock_order.data[i] = flock_order.data[j];
        flock_order.data[j] = tmp;
      }
    }
    for (uint32_t i = 0; i < movers; ++i) {
      const EntityId id = g.flocking ? flock_order.data[i]
                                     : g.live_list.data[rng_below(&g.rng, (uint32_t)n_live)];
      GdsSpatialOp op = blank_op(GDS_SP_MOVE_BY);
      op.id = id;
      if (spec->teleport_ratio > 0.0f && rng_unit(&g.rng) < spec->teleport_ratio) {
        /* A delta large enough to land anywhere once wrapped: from the index's
         * point of view nothing distinguishes this from a very fast entity. */
        op.v.x = rng_range(&g.rng, -g.hx * 2.0f, g.hx * 2.0f);
        op.v.y = rng_range(&g.rng, -g.hx * 2.0f, g.hx * 2.0f);
        op.v.z = rng_range(&g.rng, -g.hz * 2.0f, g.hz * 2.0f);
      } else {
        const float speed = rng_range(&g.rng, spec->speed_min, spec->speed_max);
        op.v = random_step(&g, speed);
        if (g.flocking) op.v = gds_add3(op.v, g.drift.data[g.cluster_of[id]]);
      }
      gds_vec_push(g.ops, op);
    }

    for (uint32_t i = 0; i < spec->radius_queries_per_tick; ++i) {
      GdsSpatialOp op = blank_op(GDS_SP_QUERY_RADIUS);
      op.v = sample_point(&g, spec->query_focus);
      op.radius = rng_range(&g.rng, spec->query_radius_min, spec->query_radius_max);
      gds_vec_push(g.ops, op);
    }
    if (g.live_list.size > 0) {
      for (uint32_t i = 0; i < spec->entity_radius_queries_per_tick; ++i) {
        GdsSpatialOp op = blank_op(GDS_SP_QUERY_RADIUS_OF);
        op.id = g.live_list.data[rng_below(&g.rng, (uint32_t)g.live_list.size)];
        op.radius = rng_range(&g.rng, spec->query_radius_min, spec->query_radius_max);
        gds_vec_push(g.ops, op);
      }
    }
    for (uint32_t i = 0; i < spec->knn_queries_per_tick; ++i) {
      GdsSpatialOp op = blank_op(GDS_SP_QUERY_KNN);
      op.v = sample_point(&g, spec->query_focus);
      op.k = spec->knn_k;
      gds_vec_push(g.ops, op);
    }

    GdsTick tk = {begin, (uint32_t)g.ops.size};
    gds_vec_push(ticks, tk);
  }

  w->max_entity_id = g.next_id ? g.next_id - 1 : 0;
  w->ops = g.ops.data;
  w->n_ops = g.ops.size;
  w->ticks = ticks.data;
  w->n_ticks = ticks.size;

  for (uint32_t t = 0; t < spec->ticks; ++t) {
    gds_vec_free(g.deltas[t].inserted);
    gds_vec_free(g.deltas[t].removed);
  }
  free(g.deltas);
  free(g.live);
  free(g.cluster_of);
  gds_vec_free(g.live_list);
  gds_vec_free(g.centres);
  gds_vec_free(g.drift);
  gds_vec_free(flock_order);
}

void gds_free_spatial_workload(GdsSpatialWorkload* w) {
  free(w->ops);
  free(w->ticks);
  w->ops = NULL;
  w->ticks = NULL;
  w->n_ops = w->n_ticks = 0;
}
