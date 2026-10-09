/* The cost no spatial structure can avoid, measured per tick.
 *
 * A candidate's tick time says how fast it is. It does not say how far from
 * possible it is, and that is what decides whether a new representation is
 * worth building for a workload. This tool measures the floor: what a tick
 * would cost if finding every answer were free.
 *
 * Two things are irreducible whatever the structure:
 *
 *   state    applying the tick's inserts, removes and moves to flat position
 *            and liveness arrays — knowing where everything is.
 *   answers  folding the digest of every entity that is in a radius answer,
 *            and of the k results of every k-nearest query, from buffers that
 *            already hold exactly those entities, packed.
 *
 * The floor of a tick is the sum. Everything a candidate spends above it is
 * search: culling, walking, testing points that turn out not to match.
 *
 * It also counts, for every radius query, how many entities the reference
 * grid's query box admits against how many are actually in the answer. That
 * ratio is the over-admission a tighter broad phase could remove, and the
 * answer size is the part nothing can remove.
 *
 * Answers are found with the oracle, outside any timed region. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gds/common.h"
#include "gds/json.h"
#include "gds/measure.h"
#include "gds/spatial/knn.h"
#include "gds/spatial/oracle.h"
#include "gds/spatial/types.h"
#include "gds/spatial/workload.h"
#include "gds/vec.h"
#include "spatial/uniform_grid/structure.h"

/* std::sort over the per-tick timings, for the percentiles. */
#define GDS_SORT_NAME spatial_floor_u64
#define GDS_SORT_T uint64_t
#define GDS_SORT_LESS(a, b) ((a) < (b))
#include "gds/sort.inc.h"

/* The reference grid with one addition: a count of what its broad phase
 * admits. */
typedef struct {
  uniform_grid base;
} counting_grid;

static void counting_grid_init(counting_grid* g, const WorldConfig* cfg) {
  uniform_grid_init(&g->base, cfg);
}

static void counting_grid_free(counting_grid* g) { uniform_grid_free(&g->base); }

static void counting_grid_insert(counting_grid* g, EntityId id, Vec3 p) {
  uniform_grid_insert(&g->base, id, p);
}

static void counting_grid_remove(counting_grid* g, EntityId id) {
  uniform_grid_remove(&g->base, id);
}

static void counting_grid_move_by(counting_grid* g, EntityId id, Vec3 delta) {
  uniform_grid_move_by(&g->base, id, delta);
}

static bool counting_grid_position_of(counting_grid* g, EntityId id, Vec3* out) {
  return uniform_grid_position_of(&g->base, id, out);
}

/* uniform_grid's box walk, counting every entity it visits. */
static uint64_t counting_grid_admitted(const counting_grid* g, Vec3 c, float r) {
  const uniform_grid* s = &g->base;
  uint64_t n = 0;
  const uniform_grid_box_ b = uniform_grid_box_of_(s, c, r);
  for (uint32_t z = b.z0; z <= b.z1; ++z) {
    for (uint32_t y = b.y0; y <= b.y1; ++y) {
      const uint32_t row = (z * s->ny + y) * s->nx;
      for (uint32_t x = b.x0; x <= b.x1; ++x) {
        for (EntityId id = s->head.data[row + x]; id != GDS_NO_ENTITY; id = s->next.data[id]) ++n;
      }
    }
  }
  return n;
}

static uint64_t counting_grid_admitted_of(counting_grid* g, EntityId self, float r) {
  Vec3 p = {0, 0, 0};
  if (!counting_grid_position_of(g, self, &p)) return 0;
  return counting_grid_admitted(g, p, r) - 1;
}

typedef struct {
  EntityId id;
  Vec3 p;
  uint64_t salt;
} Hit;

typedef GDS_VEC(Hit) HitVec;
typedef GDS_VEC(uint64_t) U64Vec;

/* The oracle's own answer, as a list rather than a digest. */
static void collect_radius(brute_force* o, uint32_t max_id, Vec3 c, float r, EntityId exclude,
                           uint64_t salt, HitVec* out, uint64_t* count) {
  const float r2 = r * r;
  for (EntityId id = 0; id <= max_id; ++id) {
    if (id == exclude) continue;
    Vec3 p = {0, 0, 0};
    if (!brute_force_position_of(o, id, &p)) continue;
    if (gds_dist2(p, c) <= r2) {
      const Hit h = {id, p, salt};
      gds_vec_push(*out, h);
      ++*count;
    }
  }
}

static void collect_knn(brute_force* o, uint32_t max_id, Vec3 c, uint32_t k, HitVec* out) {
  GDS_VEC(Neighbour) all;
  gds_vec_init(all);
  for (EntityId id = 0; id <= max_id; ++id) {
    Vec3 p = {0, 0, 0};
    if (brute_force_position_of(o, id, &p)) {
      const Neighbour nb = {gds_dist2(p, c), id};
      gds_vec_push(all, nb);
    }
  }
  const size_t want = (size_t)k < all.size ? (size_t)k : all.size;
  gds_neighbour_partial_sort(all.data, want, all.size);
  for (size_t i = 0; i < want; ++i) {
    Vec3 p = {0, 0, 0};
    brute_force_position_of(o, all.data[i].id, &p);
    /* kNN results are delimited by a sentinel so the timed loop can restart
     * its ordered fold for each query. */
    const Hit h = {all.data[i].id, p, i == 0 ? 1u : 0u};
    gds_vec_push(*out, h);
  }
  gds_vec_free(all);
}

/* The hits gathered for one query must digest to exactly what the oracle
 * answers, or the floor would be measuring a different question. */
static bool same_answer(const HitVec* buf, size_t first, uint64_t oracle_value) {
  uint64_t acc = 0;
  for (size_t i = first; i < buf->size; ++i)
    acc += gds_digest_hit(buf->data[i].id, buf->data[i].p, buf->data[i].salt);
  return acc == oracle_value;
}

/* Linear interpolation between the two nearest ranks. The C++ took the values
 * by value and sorted its copy, which this does too. */
static double percentile(const U64Vec* values, double q) {
  U64Vec v;
  gds_vec_copy(v, *values);
  if (v.size == 0) {
    gds_vec_free(v);
    return 0.0;
  }
  spatial_floor_u64_sort(v.data, v.size);
  const double idx = q * (double)(v.size - 1);
  const size_t lo = (size_t)idx;
  const size_t hi = gds_min_size(lo + 1, v.size - 1);
  const double frac = idx - (double)lo;
  const double result = (double)v.data[lo] * (1.0 - frac) + (double)v.data[hi] * frac;
  gds_vec_free(v);
  return result;
}

/* A snapshot of the floor's own state at the end of one tick. */
typedef struct {
  uint64_t tick;
  GDS_VEC(Vec3) pos;
  GDS_VEC(uint8_t) live;
} Snap;

static void snap_free(Snap* s) {
  gds_vec_free(s->pos);
  gds_vec_free(s->live);
}

int main(int argc, char** argv) {
  const char* path = NULL;
  int reps = 3;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--workload") && i + 1 < argc) path = argv[++i];
    else if (!strcmp(argv[i], "--reps") && i + 1 < argc) reps = atoi(argv[++i]);
  }
  if (path == NULL || path[0] == '\0') {
    fprintf(stderr, "usage: gds_floor_spatial --workload FILE [--reps N]\n");
    return 2;
  }
  GdsSpatialSpec spec;
  char error[512] = {0};
  if (!gds_parse_spatial_file(path, &spec, error, sizeof error)) {
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

  brute_force oracle;
  brute_force_init(&oracle, &cfg);
  counting_grid grid;
  counting_grid_init(&grid, &cfg);

  /* The floor's own state: flat arrays and nothing else. History, when the
   * workload rewinds, is a full copy per tick taken outside the timed region,
   * and restoring it is timed as a copy: what the data alone costs to put
   * back. */
  const size_t n = (size_t)w.max_entity_id + 1;
  const Vec3 zero = {0, 0, 0};
  GDS_VEC(Vec3) pos;
  gds_vec_init(pos);
  gds_vec_assign(pos, n, zero);
  GDS_VEC(uint8_t) live;
  gds_vec_init(live);
  gds_vec_assign(live, n, 0);
  GDS_VEC(Snap) history;
  gds_vec_init(history);

  U64Vec state_ns, answer_ns, floor_ns;
  gds_vec_init(state_ns);
  gds_vec_init(answer_ns);
  gds_vec_init(floor_ns);
  uint64_t radius_queries = 0, radius_hits = 0, radius_admitted = 0, knn_queries = 0;
  uint64_t max_hits = 0;
  HitVec radius_buf, knn_buf;
  gds_vec_init(radius_buf);
  gds_vec_init(knn_buf);
  uint64_t sink = 0;

  for (uint32_t t = 0; t < w.n_ticks; ++t) {
    const GdsTick* tk = &w.ticks[t];
    gds_vec_clear(radius_buf);
    gds_vec_clear(knn_buf);

    /* State, timed, on the flat arrays. */
    uint64_t t_state = 0;
    {
      const uint64_t t0 = gds_now_ns();
      for (uint32_t i = tk->op_begin; i < tk->op_end; ++i) {
        const GdsSpatialOp* op = &w.ops[i];
        switch (op->kind) {
          case GDS_SP_INSERT: live.data[op->id] = 1; pos.data[op->id] = op->v; break;
          case GDS_SP_REMOVE: live.data[op->id] = 0; break;
          case GDS_SP_MOVE_BY:
            if (live.data[op->id])
              pos.data[op->id] = gds_wrap_into(gds_add3(pos.data[op->id], op->v), &w.bounds);
            break;
          case GDS_SP_REWIND:
            for (size_t s = history.size; s-- > 0;) {
              if (history.data[s].tick != op->k) continue;
              memcpy(pos.data, history.data[s].pos.data, n * sizeof(Vec3));
              memcpy(live.data, history.data[s].live.data, n);
              break;
            }
            break;
          default: break;
        }
      }
      const uint64_t t1 = gds_now_ns();
      t_state = t1 - t0;
    }

    /* Answers, found by the oracle and the grid in op order, untimed. */
    for (uint32_t i = tk->op_begin; i < tk->op_end; ++i) {
      const GdsSpatialOp* op = &w.ops[i];
      switch (op->kind) {
        case GDS_SP_INSERT:
          brute_force_insert(&oracle, op->id, op->v);
          counting_grid_insert(&grid, op->id, op->v);
          break;
        case GDS_SP_REMOVE:
          brute_force_remove(&oracle, op->id);
          counting_grid_remove(&grid, op->id);
          break;
        case GDS_SP_MOVE_BY:
          brute_force_move_by(&oracle, op->id, op->v);
          counting_grid_move_by(&grid, op->id, op->v);
          break;
        case GDS_SP_REWIND: {
          brute_force_rewind_to(&oracle, op->k);
          /* A fresh grid moved into place, as the C++ assigned a temporary. */
          counting_grid fresh;
          counting_grid_init(&fresh, &cfg);
          counting_grid_free(&grid);
          grid = fresh;
          for (EntityId id = 0; id <= w.max_entity_id; ++id) {
            Vec3 p = {0, 0, 0};
            if (brute_force_position_of(&oracle, id, &p)) counting_grid_insert(&grid, id, p);
          }
          break;
        }
        case GDS_SP_QUERY_RADIUS: {
          uint64_t c = 0;
          const size_t first = radius_buf.size;
          collect_radius(&oracle, w.max_entity_id, op->v, op->radius, GDS_NO_ENTITY,
                         gds_radius_salt(op->v, op->radius), &radius_buf, &c);
          if (!same_answer(&radius_buf, first,
                           brute_force_query_radius(&oracle, op->v, op->radius))) {
            printf("{\"status\":\"internal_error\",\"tick\":%u}\n", t);
            return 4;
          }
          radius_hits += c;
          max_hits = c > max_hits ? c : max_hits;
          radius_admitted += counting_grid_admitted(&grid, op->v, op->radius);
          ++radius_queries;
          break;
        }
        case GDS_SP_QUERY_RADIUS_OF: {
          Vec3 centre = {0, 0, 0};
          if (!brute_force_position_of(&oracle, op->id, &centre)) {
            ++radius_queries;
            break;
          }
          uint64_t c = 0;
          const size_t first = radius_buf.size;
          collect_radius(&oracle, w.max_entity_id, centre, op->radius, op->id,
                         gds_radius_salt(centre, op->radius), &radius_buf, &c);
          if (!same_answer(&radius_buf, first,
                           brute_force_query_radius_of(&oracle, op->id, op->radius))) {
            printf("{\"status\":\"internal_error\",\"tick\":%u}\n", t);
            return 4;
          }
          radius_hits += c;
          max_hits = c > max_hits ? c : max_hits;
          radius_admitted += counting_grid_admitted_of(&grid, op->id, op->radius);
          ++radius_queries;
          break;
        }
        case GDS_SP_QUERY_KNN:
          collect_knn(&oracle, w.max_entity_id, op->v, op->k, &knn_buf);
          ++knn_queries;
          break;
      }
    }
    brute_force_end_tick(&oracle, t);
    if (cfg.history_ticks) {
      Snap snap;
      snap.tick = t;
      gds_vec_copy(snap.pos, pos);
      gds_vec_copy(snap.live, live);
      gds_vec_push(history, snap);
      while (history.size > (size_t)cfg.history_ticks + 1) {
        snap_free(&history.data[0]);
        memmove(history.data, history.data + 1, (history.size - 1) * sizeof *history.data);
        --history.size;
      }
    }

    /* Answers, timed: the digest of exactly what is in them, from packed
     * buffers. The best of `reps` repetitions is kept. */
    uint64_t t_answers = ~0ull;
    for (int rep = 0; rep < reps; ++rep) {
      const uint64_t t0 = gds_now_ns();
      uint64_t acc = 0;
      for (size_t i = 0; i < radius_buf.size; ++i) {
        const Hit* h = &radius_buf.data[i];
        acc += gds_digest_hit(h->id, h->p, h->salt);
      }
      KnnDigest d = gds_knn_digest();
      for (size_t i = 0; i < knn_buf.size; ++i) {
        const Hit* h = &knn_buf.data[i];
        if (h->salt) {
          acc ^= d.acc;
          d = gds_knn_digest();
        }
        gds_knn_push(&d, h->id, h->p);
      }
      acc ^= d.acc;
      gds_keep_u64(acc);
      sink ^= acc;
      const uint64_t t1 = gds_now_ns();
      const uint64_t ns = t1 - t0;
      t_answers = ns < t_answers ? ns : t_answers;
    }

    if (t == 0) continue; /* the load tick is a different question */
    gds_vec_push(state_ns, t_state);
    gds_vec_push(answer_ns, t_answers);
    gds_vec_push(floor_ns, t_state + t_answers);
  }
  gds_keep_u64(sink);

  const double hits_per_query = radius_queries ? (double)radius_hits / radius_queries : 0.0;
  const double admitted_per_query =
      radius_queries ? (double)radius_admitted / radius_queries : 0.0;
  printf("{\n");
  printf("  \"status\": \"ok\",\n");
  printf("  \"workload\": \"");
  gds_json_write_escaped(stdout, spec.id);
  printf("\",\n");
  printf("  \"visibility\": \"");
  gds_json_write_escaped(stdout, spec.visibility);
  printf("\",\n");
  printf("  \"ticks_measured\": %zu,\n", floor_ns.size);
  printf("  \"floor_ns_p50\": %.0f,\n", percentile(&floor_ns, 0.50));
  printf("  \"floor_ns_p99\": %.0f,\n", percentile(&floor_ns, 0.99));
  printf("  \"state_ns_p50\": %.0f,\n", percentile(&state_ns, 0.50));
  printf("  \"answer_ns_p50\": %.0f,\n", percentile(&answer_ns, 0.50));
  printf("  \"radius_queries\": %llu,\n", (unsigned long long)radius_queries);
  printf("  \"knn_queries\": %llu,\n", (unsigned long long)knn_queries);
  printf("  \"hits_per_radius_query\": %.2f,\n", hits_per_query);
  printf("  \"max_hits_in_one_query\": %llu,\n", (unsigned long long)max_hits);
  printf("  \"grid_admitted_per_radius_query\": %.2f,\n", admitted_per_query);
  printf("  \"grid_over_admission\": %.3f\n",
         hits_per_query > 0 ? admitted_per_query / hits_per_query : 0.0);
  printf("}\n");

  gds_vec_free(knn_buf);
  gds_vec_free(radius_buf);
  gds_vec_free(floor_ns);
  gds_vec_free(answer_ns);
  gds_vec_free(state_ns);
  for (size_t i = 0; i < history.size; ++i) snap_free(&history.data[i]);
  gds_vec_free(history);
  gds_vec_free(live);
  gds_vec_free(pos);
  counting_grid_free(&grid);
  brute_force_free(&oracle);
  gds_free_spatial_workload(&w);
  return 0;
}
