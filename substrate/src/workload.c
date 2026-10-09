#include "gds/workload.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gds/vec.h"
#include "parse_util.h"

const char* gds_access_name(GdsAccess a) {
  switch (a) {
    case GDS_ACCESS_UNIFORM: return "uniform";
    case GDS_ACCESS_ZIPF: return "zipf";
    case GDS_ACCESS_RECENT: return "recent";
  }
  return "unknown";
}

void gds_workload_spec_defaults(GdsWorkloadSpec* s) {
  memset(s, 0, sizeof *s);
  gds_copy_str(s->id, sizeof s->id, "unnamed");
  gds_copy_str(s->visibility, sizeof s->visibility, "public");
  gds_copy_str(s->track, sizeof s->track, "ecs");
  s->seed = 1;
  s->initial_entities = 10000;
  s->max_entities = 200000;
  s->frames = 300;
  s->ops_per_frame = 1000;
  s->w_create = 1.0f;
  s->w_destroy = 1.0f;
  s->w_add = 0.5f;
  s->w_remove = 0.5f;
  s->w_get = 4.0f;
  s->w_set = 3.0f;
  s->access = GDS_ACCESS_UNIFORM;
  s->zipf_exponent = 1.1;
  s->recency_window = 1024;
  s->burst_frame_ratio = 0.0f;
  s->burst_multiplier = 8;
  s->stale_access_ratio = 0.0f;
  s->access_width = 1;
  s->p_position = 1.0f;
  s->p_velocity = 0.8f;
  s->p_health = 0.5f;
  s->p_tag = 0.3f;
  s->integrate_per_frame = true;
  s->dt = 0.016666668f;
  s->query_masks[0] = GDS_K_POSITION | GDS_K_VELOCITY;
  s->n_query_masks = 1;
  s->verify_sweep_frames = 32;
}

static bool parse_bool(const char* v, bool* out) {
  if (!strcmp(v, "true") || !strcmp(v, "1") || !strcmp(v, "yes")) { *out = true; return true; }
  if (!strcmp(v, "false") || !strcmp(v, "0") || !strcmp(v, "no")) { *out = false; return true; }
  return false;
}

/* One `+`-joined component set, e.g. "position+velocity". */
static bool parse_mask_token(char* t, ComponentMask* out) {
  ComponentMask m = 0;
  char* save = NULL;
  for (char* part = strtok_r(t, "+", &save); part; part = strtok_r(NULL, "+", &save)) {
    part = gds_trim(part);
    if (!strcmp(part, "position")) m |= GDS_K_POSITION;
    else if (!strcmp(part, "velocity")) m |= GDS_K_VELOCITY;
    else if (!strcmp(part, "health")) m |= GDS_K_HEALTH;
    else if (!strcmp(part, "tag")) m |= GDS_K_TAG;
    else return false;
  }
  if (m == 0) return false;
  *out = m;
  return true;
}

bool gds_parse_workload_text(const char* text, GdsWorkloadSpec* out, char* error,
                             size_t error_len) {
  gds_workload_spec_defaults(out);
  char* buf = strdup(text);
  char* cursor = buf;
  char *key = NULL, *val = NULL;
  int line_no = 0;
  bool ok = true;
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
    bool b = false;
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

    if (!strcmp(key, "id")) gds_copy_str(out->id, sizeof out->id, val);
    else if (!strcmp(key, "visibility")) {
      if (strcmp(val, "public") && strcmp(val, "hidden")) {
        snprintf(error, error_len, "visibility must be public or hidden");
        ok = false;
      } else {
        gds_copy_str(out->visibility, sizeof out->visibility, val);
      }
    }
    else if (!strcmp(key, "track")) gds_copy_str(out->track, sizeof out->track, val);
    else if (!strcmp(key, "note")) gds_copy_str(out->note, sizeof out->note, val);
    else if (!strcmp(key, "seed")) {
      if (!gds_parse_u64(val, &u)) {
        snprintf(error, error_len, "seed: expected an integer");
        ok = false;
      } else {
        out->seed = u;
      }
    }
    else if (!strcmp(key, "initial_entities")) NEED_U(out->initial_entities);
    else if (!strcmp(key, "max_entities")) NEED_U(out->max_entities);
    else if (!strcmp(key, "frames")) NEED_U(out->frames);
    else if (!strcmp(key, "ops_per_frame")) NEED_U(out->ops_per_frame);
    else if (!strcmp(key, "w_create")) NEED_F(out->w_create);
    else if (!strcmp(key, "w_destroy")) NEED_F(out->w_destroy);
    else if (!strcmp(key, "w_add")) NEED_F(out->w_add);
    else if (!strcmp(key, "w_remove")) NEED_F(out->w_remove);
    else if (!strcmp(key, "w_get")) NEED_F(out->w_get);
    else if (!strcmp(key, "w_set")) NEED_F(out->w_set);
    else if (!strcmp(key, "access")) {
      if (!strcmp(val, "uniform")) out->access = GDS_ACCESS_UNIFORM;
      else if (!strcmp(val, "zipf")) out->access = GDS_ACCESS_ZIPF;
      else if (!strcmp(val, "recent")) out->access = GDS_ACCESS_RECENT;
      else {
        snprintf(error, error_len, "access must be uniform, zipf or recent");
        ok = false;
      }
    }
    else if (!strcmp(key, "zipf_exponent")) {
      if (!gds_parse_double(val, &d)) {
        snprintf(error, error_len, "zipf_exponent: expected a number");
        ok = false;
      } else {
        out->zipf_exponent = d;
      }
    }
    else if (!strcmp(key, "recency_window")) NEED_U(out->recency_window);
    else if (!strcmp(key, "burst_frame_ratio")) NEED_F(out->burst_frame_ratio);
    else if (!strcmp(key, "burst_multiplier")) NEED_U(out->burst_multiplier);
    else if (!strcmp(key, "stale_access_ratio")) NEED_F(out->stale_access_ratio);
    else if (!strcmp(key, "access_width")) NEED_U(out->access_width);
    else if (!strcmp(key, "p_position")) NEED_F(out->p_position);
    else if (!strcmp(key, "p_velocity")) NEED_F(out->p_velocity);
    else if (!strcmp(key, "p_health")) NEED_F(out->p_health);
    else if (!strcmp(key, "p_tag")) NEED_F(out->p_tag);
    else if (!strcmp(key, "integrate_per_frame")) {
      if (!parse_bool(val, &b)) {
        snprintf(error, error_len, "integrate_per_frame: expected a boolean");
        ok = false;
      } else {
        out->integrate_per_frame = b;
      }
    }
    else if (!strcmp(key, "dt")) NEED_F(out->dt);
    else if (!strcmp(key, "verify_sweep_frames")) NEED_U(out->verify_sweep_frames);
    else if (!strcmp(key, "query_masks")) {
      out->n_query_masks = 0;
      char* save = NULL;
      for (char* tok = strtok_r(val, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        tok = gds_trim(tok);
        if (!*tok) continue;
        ComponentMask m = 0;
        char copy[128];
        gds_copy_str(copy, sizeof copy, tok);
        if (!parse_mask_token(copy, &m)) {
          snprintf(error, error_len, "query_masks: unknown component set '%s'", tok);
          ok = false;
          break;
        }
        if (out->n_query_masks == GDS_MAX_QUERY_MASKS) {
          snprintf(error, error_len, "query_masks: more than %d sets", GDS_MAX_QUERY_MASKS);
          ok = false;
          break;
        }
        out->query_masks[out->n_query_masks++] = m;
      }
    }
    else {
      snprintf(error, error_len, "line %d: unknown key '%s'", line_no, key);
      ok = false;
    }
#undef NEED_U
#undef NEED_F
    if (!ok) break;
  }
  free(buf);
  if (ok && out->max_entities < out->initial_entities) out->max_entities = out->initial_entities;
  return ok;
}

bool gds_parse_workload_file(const char* path, GdsWorkloadSpec* out, char* error,
                             size_t error_len) {
  char* text = gds_read_file(path);
  if (!text) {
    snprintf(error, error_len, "cannot open workload file: %s", path);
    return false;
  }
  const bool ok = gds_parse_workload_text(text, out, error, error_len);
  free(text);
  return ok;
}

/* ------------------------------------------------------------------------
 * Generation
 * ------------------------------------------------------------------------ */

/* Rejection-inversion sampling for Zipf, after Hörmann & Derflinger (1996).
 * Exact for the given (n, exponent) and needs no precomputed table, so the
 * distribution stays correct while the live-entity count moves every frame. */
typedef struct { double s; } Zipf;

static double zipf_h(const Zipf* z, double x) { return (pow(x, 1.0 - z->s) - 1.0) / (1.0 - z->s); }
static double zipf_h_inv(const Zipf* z, double y) {
  return pow((1.0 - z->s) * y + 1.0, 1.0 / (1.0 - z->s));
}
static double zipf_next_unit(uint64_t* rng) {
  *rng = gds_splitmix64(*rng);
  return (double)(*rng >> 11) * (1.0 / 9007199254740992.0);
}

static uint32_t zipf_sample(const Zipf* z, uint32_t n, uint64_t* rng) {
  if (n <= 1) return 0;
  const double h_x1 = zipf_h(z, 1.5) - 1.0;
  const double h_n = zipf_h(z, (double)n + 0.5);
  const double s_const = 2.0 - zipf_h_inv(z, zipf_h(z, 2.5) - pow(2.0, -z->s));
  for (int guard = 0; guard < 64; ++guard) {
    const double u = h_n + zipf_next_unit(rng) * (h_x1 - h_n);
    const double x = zipf_h_inv(z, u);
    double k = floor(x + 0.5);
    if (k < 1.0) k = 1.0;
    if (k > (double)n) k = (double)n;
    if (k - x <= s_const || u >= zipf_h(z, k + 0.5) - pow(k, -z->s)) {
      return (uint32_t)k - 1; /* 0-based rank */
    }
  }
  return 0;
}

typedef struct { uint64_t state; } Rng;

static inline uint64_t rng_next(Rng* r) {
  r->state = gds_splitmix64(r->state + 0x9E3779B97F4A7C15ull);
  return r->state;
}
static inline uint32_t rng_below(Rng* r, uint32_t n) {
  if (n == 0) return 0;
  return (uint32_t)(rng_next(r) % n);
}
static inline float rng_unit(Rng* r) {
  return (float)((double)(rng_next(r) >> 40) * (1.0 / 16777216.0));
}
static inline float rng_range(Rng* r, float lo, float hi) { return lo + rng_unit(r) * (hi - lo); }

typedef GDS_VEC(uint32_t) U32Vec;
typedef GDS_VEC(GdsOp) OpVec;
typedef GDS_VEC(GdsFrame) FrameVec;

typedef struct {
  const GdsWorkloadSpec* spec;
  Rng rng;
  Zipf zipf;
  uint64_t zipf_rng;
  /* live holds slot indices of entities the generator believes are alive; dead
   * holds slots whose handles are stale. The generator mirrors only liveness,
   * never component values, so it stays cheap. */
  U32Vec live, dead, live_pos; /* live_pos: slot -> index in live, or UINT32_MAX */
  uint32_t next_slot;
  OpVec ops;
} Gen;

static ComponentMask random_mask(Gen* g) {
  ComponentMask m = 0;
  if (rng_unit(&g->rng) < g->spec->p_position) m |= GDS_K_POSITION;
  if (rng_unit(&g->rng) < g->spec->p_velocity) m |= GDS_K_VELOCITY;
  if (rng_unit(&g->rng) < g->spec->p_health) m |= GDS_K_HEALTH;
  if (rng_unit(&g->rng) < g->spec->p_tag) m |= GDS_K_TAG;
  if (m == 0) m = GDS_K_POSITION;
  return m;
}

/* Every draw in its own statement, in the order the generator has always
 * made them: C leaves the order of evaluation inside an initialiser open. */
static void fill_values(Gen* g, ComponentValue* v) {
  memset(v, 0, sizeof(ComponentValue) * GDS_COMPONENT_COUNT);
  v[0].position.x = rng_range(&g->rng, -512.f, 512.f);
  v[0].position.y = rng_range(&g->rng, -64.f, 64.f);
  v[0].position.z = rng_range(&g->rng, -512.f, 512.f);
  v[1].velocity.x = rng_range(&g->rng, -8.f, 8.f);
  v[1].velocity.y = rng_range(&g->rng, -2.f, 2.f);
  v[1].velocity.z = rng_range(&g->rng, -8.f, 8.f);
  v[2].health.hp = (int32_t)(1 + rng_below(&g->rng, 100));
  v[2].health.max_hp = 100;
  v[3].tag.bits = (uint32_t)(rng_next(&g->rng) >> 32);
}

/* Create carries no payload: the four initial component values are a pure
 * function of the slot index (gds_create_values), so the oracle and every
 * candidate construct identical entities without widening the op record. */
static void emit_create(Gen* g) {
  GdsOp op;
  memset(&op, 0, sizeof op);
  op.kind = GDS_OP_CREATE;
  op.mask = random_mask(g);
  op.slot = g->next_slot;
  gds_vec_push(g->ops, op);
  gds_vec_push(g->live_pos, (uint32_t)g->live.size);
  gds_vec_push(g->live, g->next_slot);
  ++g->next_slot;
}

static uint32_t pick_live(Gen* g) {
  const uint32_t n = (uint32_t)g->live.size;
  if (n == 0) return UINT32_MAX;
  switch (g->spec->access) {
    case GDS_ACCESS_UNIFORM: return g->live.data[rng_below(&g->rng, n)];
    case GDS_ACCESS_ZIPF: {
      const uint32_t rank = zipf_sample(&g->zipf, n, &g->zipf_rng);
      return g->live.data[rank < n ? rank : n - 1];
    }
    case GDS_ACCESS_RECENT: {
      const uint32_t win = g->spec->recency_window < n ? g->spec->recency_window : n;
      return g->live.data[n - 1 - rng_below(&g->rng, win)];
    }
  }
  return g->live.data[0];
}

static uint32_t pick_target(Gen* g) {
  if (g->dead.size && rng_unit(&g->rng) < g->spec->stale_access_ratio) {
    return g->dead.data[rng_below(&g->rng, (uint32_t)g->dead.size)];
  }
  return pick_live(g);
}

static void kill_slot(Gen* g, uint32_t slot) {
  const uint32_t idx = g->live_pos.data[slot];
  if (idx == UINT32_MAX) return;
  const uint32_t last = gds_vec_back(g->live);
  g->live.data[idx] = last;
  g->live_pos.data[last] = idx;
  --g->live.size;
  g->live_pos.data[slot] = UINT32_MAX;
  gds_vec_push(g->dead, slot);
}

void gds_generate_workload(const GdsWorkloadSpec* spec, GdsWorkload* w) {
  memset(w, 0, sizeof *w);
  w->spec = *spec;
  Gen g;
  memset(&g, 0, sizeof g);
  g.spec = spec;
  g.rng.state = gds_splitmix64(spec->seed | 1);
  g.zipf.s = spec->zipf_exponent == 1.0 ? 1.000001 : spec->zipf_exponent;
  g.zipf_rng = gds_splitmix64(spec->seed ^ 0xA5A5A5A5A5A5A5A5ull);
  FrameVec frames;
  gds_vec_init(frames);

  float total_w = spec->w_create + spec->w_destroy + spec->w_add + spec->w_remove +
                  spec->w_get + spec->w_set;
  if (total_w < 0.0001f) total_w = 0.0001f;

  for (uint32_t i = 0; i < spec->initial_entities; ++i) emit_create(&g);
  GdsFrame f0 = {0, (uint32_t)g.ops.size};
  gds_vec_push(frames, f0);

  for (uint32_t f = 1; f < spec->frames; ++f) {
    const uint32_t begin = (uint32_t)g.ops.size;
    uint32_t count = spec->ops_per_frame;
    if (spec->burst_frame_ratio > 0.f && rng_unit(&g.rng) < spec->burst_frame_ratio) {
      count *= spec->burst_multiplier > 1 ? spec->burst_multiplier : 1;
    }
    for (uint32_t i = 0; i < count; ++i) {
      const float r = rng_unit(&g.rng) * total_w;
      float acc = spec->w_create;
      if (r < acc && g.live.size < spec->max_entities) {
        emit_create(&g);
        continue;
      }
      acc += spec->w_destroy;
      if (r < acc) {
        const uint32_t slot = pick_live(&g);
        if (slot == UINT32_MAX) continue;
        GdsOp op;
        memset(&op, 0, sizeof op);
        op.kind = GDS_OP_DESTROY;
        op.slot = slot;
        gds_vec_push(g.ops, op);
        kill_slot(&g, slot);
        continue;
      }
      const uint32_t slot = pick_target(&g);
      if (slot == UINT32_MAX) continue;
      GdsOp op;
      memset(&op, 0, sizeof op);
      op.slot = slot;
      op.comp = (uint8_t)rng_below(&g.rng, GDS_COMPONENT_COUNT);
      ComponentValue vals[GDS_COMPONENT_COUNT];
      fill_values(&g, vals);
      op.value = vals[op.comp];
      acc += spec->w_add;
      if (r < acc) { op.kind = GDS_OP_ADD; gds_vec_push(g.ops, op); continue; }
      acc += spec->w_remove;
      if (r < acc) { op.kind = GDS_OP_REMOVE; gds_vec_push(g.ops, op); continue; }
      acc += spec->w_get;
      op.kind = r < acc ? GDS_OP_GET : GDS_OP_SET;
      gds_vec_push(g.ops, op);
      /* Wider access repeats the same kind on the same entity for the next
       * components in id order, from values already drawn, so a width of 1
       * consumes exactly the random numbers it always did. */
      uint32_t width = spec->access_width > 1 ? spec->access_width : 1;
      if (width > GDS_COMPONENT_COUNT) width = GDS_COMPONENT_COUNT;
      for (uint32_t k = 1; k < width && i + 1 < count; ++k) {
        GdsOp more = op;
        more.comp = (uint8_t)((op.comp + k) % GDS_COMPONENT_COUNT);
        more.value = vals[more.comp];
        gds_vec_push(g.ops, more);
        ++i;
      }
    }
    GdsFrame fr = {begin, (uint32_t)g.ops.size};
    gds_vec_push(frames, fr);
  }

  w->ops = g.ops.data;
  w->n_ops = g.ops.size;
  w->frames = frames.data;
  w->n_frames = frames.size;
  w->slot_count = g.next_slot;
  gds_vec_free(g.live);
  gds_vec_free(g.dead);
  gds_vec_free(g.live_pos);
}

void gds_free_workload(GdsWorkload* w) {
  free(w->ops);
  free(w->frames);
  memset(w, 0, sizeof *w);
}
