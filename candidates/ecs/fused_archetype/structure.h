#ifndef CANDIDATE_ECS_FUSED_ARCHETYPE_H
#define CANDIDATE_ECS_FUSED_ARCHETYPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gds/types.h"
#include "gds/vec.h"

/* archetype with integrate deferred into the next query and applied in the
 * walk that answers it.
 *
 * In every workload that integrates, a frame is point operations, integrate,
 * one to three queries and sync. archetype walks the Position and Velocity
 * columns once for integrate and again for every query that names them. Here
 * integrate(dt) records the step and nothing else. The next query resolves it
 * group by group: a group holding both of the pair that the query matches is
 * stepped and digested block by block, so the digest reads each block from L1
 * right after the step wrote it; a group holding the pair that the query does
 * not match is only stepped; a matching group without the pair is only
 * digested. The columns are then brought in from beyond L1 once per frame
 * instead of twice.
 *
 * Every position still receives exactly one p += v*dt per integrate call, with
 * the dt of that call, component by component in x, y, z order, before anything
 * observes it. That is the parent's arithmetic in the parent's order per
 * entity, so every observation is bit-identical to applying the step eagerly.
 *
 * The parent's code is reproduced rather than included by path. In the C++ it
 * had to be: every member of Archetype is private there, and the fused query
 * has to write a group's Position column and read all of its columns by row,
 * which neither a derived class nor a wrapper around the public interface can
 * reach. C hides nothing, but the port keeps the reproduction, so that the two
 * structures stay compiled from the same text, for the reason that follows.
 *
 * The reproduction keeps the parent's text wherever the mechanism does not need
 * to change it, because the predictions compare frame time against archetype
 * and a change in code shape moves frame time by as much as the thresholds. The
 * registry, the handle layout, the group table, append, swap-remove, the move
 * between groups and the query with no step pending are the parent's. Apart
 * from the prefix and the constants FUSED_ARCHETYPE_PAIR and
 * FUSED_ARCHETYPE_FUSE_ROWS, what differs is this and nothing else:
 * - integrate records the step. The walk that applies a step on its own is the
 *   parent's integrate loop, moved into resolve_pending.
 * - A query with a step pending is answered by query_and_step, the one new
 *   walk.
 * - create, add, remove, get, set and sync each test the pending flag and may
 *   call resolve_pending; the rest of each body is the parent's.
 * - In the C++ the group vector and the pending state are mutable, because
 *   query and get are const in its contract and may have to apply the step.
 *   In C every operation takes a non-const pointer, so they are ordinary
 *   members.
 * - reported_bytes also counts the group vector, which the parent leaves out.
 * query_and_step and resolve_pending are kept out of line (noinline). Inlined,
 * the new walk would be compiled into the same function as the parent's query
 * loop, and the step pass into every point operation, and the code around them
 * would no longer be compiled as the parent's is. */

#define FUSED_ARCHETYPE_INVALID (~0u)
#define FUSED_ARCHETYPE_TABLE_SIZE (1 << GDS_COMPONENT_COUNT)
#define FUSED_ARCHETYPE_PAIR ((ComponentMask)(GDS_K_POSITION | GDS_K_VELOCITY))

/* Rows per block of the fused walk. A block of all four columns is at most
 * 36 bytes a row, 9 KB at 256 rows: under a fifth of a 48 KB L1d and under a
 * third of a 32 KB one, so the block the step writes is still in L1 when the
 * digest reads it, beside the stack and the prefetcher's lines for the next
 * block. Per block the overhead is two loop entries, tens of cycles, against
 * 256 digests of tens of cycles each. 256 rows of each column is a whole
 * number of 64-byte lines (48, 48, 32 and 16), so a block boundary falls at
 * the same offset within a line in every block. */
#define FUSED_ARCHETYPE_FUSE_ROWS ((size_t)256)

typedef struct {
  uint32_t generation;
  uint32_t alive;
  uint32_t group;
  uint32_t row;
} fused_archetype_entry_;

typedef struct {
  ComponentMask mask;
  GDS_VEC(uint32_t) entity;
  GDS_VEC(Position) position;
  GDS_VEC(Velocity) velocity;
  GDS_VEC(Health) health;
  GDS_VEC(Tag) tag;
} fused_archetype_group_;

typedef struct {
  GDS_VEC(fused_archetype_entry_) entries;
  GDS_VEC(uint32_t) free_list;
  /* In the C++ this was mutable because query and get are const in its
   * contract and may have to apply the pending step to the Position columns
   * before they answer. Here every operation takes a non-const pointer. */
  GDS_VEC(fused_archetype_group_) groups;
  uint32_t table[FUSED_ARCHETYPE_TABLE_SIZE];
  size_t live;
  bool pending;
  float pending_dt;
} fused_archetype;

static inline void fused_archetype_init(fused_archetype* s) {
  memset(s, 0, sizeof *s);
  for (int i = 0; i < FUSED_ARCHETYPE_TABLE_SIZE; ++i) s->table[i] = FUSED_ARCHETYPE_INVALID;
  s->pending = false;
  s->pending_dt = 0.0f;
}

static inline void fused_archetype_free(fused_archetype* s) {
  for (size_t i = 0; i < s->groups.size; ++i) {
    fused_archetype_group_* g = &s->groups.data[i];
    gds_vec_free(g->entity);
    gds_vec_free(g->position);
    gds_vec_free(g->velocity);
    gds_vec_free(g->health);
    gds_vec_free(g->tag);
  }
  gds_vec_free(s->entries);
  gds_vec_free(s->free_list);
  gds_vec_free(s->groups);
}

/* The one new walk. Every group holding both of the pair is stepped whether
 * or not the query matches it, so the step is resolved when this returns.
 * A group that is both stepped and matched is walked in blocks of
 * FUSED_ARCHETYPE_FUSE_ROWS, each stepped and then digested, so the digest's
 * loads hit lines the step has just written; a group that is only one of the
 * two is a single block. Per block the step is the parent's integrate loop and
 * the digest is the parent's query loop, rather than one loop doing both per
 * row: that keeps the step vectorised, so the measurement charges the fusion
 * with the second walk it removes and not also with the loss of a vectorised
 * step. */
static __attribute__((noinline)) uint64_t fused_archetype_query_and_step_(
    fused_archetype* s, ComponentMask required, uint64_t salt) {
  const float dt = s->pending_dt;
  s->pending = false;
  uint64_t acc = 0;
  ComponentValue v[GDS_COMPONENT_COUNT];
  for (size_t gi = 0; gi < s->groups.size; ++gi) {
    fused_archetype_group_* g = &s->groups.data[gi];
    const bool stepped = (g->mask & FUSED_ARCHETYPE_PAIR) == FUSED_ARCHETYPE_PAIR;
    const bool matched = (g->mask & required) == required;
    if (!stepped && !matched) continue;
    const size_t n = g->entity.size;
    const size_t block = stepped && matched ? FUSED_ARCHETYPE_FUSE_ROWS : n;
    for (size_t b = 0; b < n; b += block) {
      const size_t end = b + block < n ? b + block : n;
      if (stepped) {
        /* Scoped so that the restrict pointers cover the step only; the
         * digest below reads the same column through the vector. */
        Position* restrict p = g->position.data;
        const Velocity* restrict w = g->velocity.data;
        for (size_t r = b; r < end; ++r) {
          p[r].x += w[r].x * dt;
          p[r].y += w[r].y * dt;
          p[r].z += w[r].z * dt;
        }
      }
      if (matched) {
        for (size_t r = b; r < end; ++r) {
          if (required & GDS_K_POSITION) v[0].position = g->position.data[r];
          if (required & GDS_K_VELOCITY) v[1].velocity = g->velocity.data[r];
          if (required & GDS_K_HEALTH) v[2].health = g->health.data[r];
          if (required & GDS_K_TAG) v[3].tag = g->tag.data[r];
          acc += gds_digest_entity(required, v, salt);
        }
      }
    }
  }
  return acc;
}

/* The parent's integrate loop: the plain pass every operation other than
 * query uses to catch up. In the C++ it was const because get and query are
 * const in its contract; applying the step changes no answer the structure
 * gives, only how far its columns lag behind those answers, which is what the
 * mutable members there were for. Here it takes the structure as every
 * operation does. */
static __attribute__((noinline)) void fused_archetype_resolve_pending_(
    fused_archetype* s) {
  const float dt = s->pending_dt;
  s->pending = false;
  const ComponentMask need = GDS_K_POSITION | GDS_K_VELOCITY;
  for (size_t gi = 0; gi < s->groups.size; ++gi) {
    fused_archetype_group_* g = &s->groups.data[gi];
    if ((g->mask & need) != need) continue;
    const size_t n = g->position.size;
    Position* restrict p = g->position.data;
    const Velocity* restrict v = g->velocity.data;
    for (size_t r = 0; r < n; ++r) {
      p[r].x += v[r].x * dt;
      p[r].y += v[r].y * dt;
      p[r].z += v[r].z * dt;
    }
  }
}

static inline uint32_t fused_archetype_resolve_(const fused_archetype* s, Entity e) {
  const uint32_t i = (uint32_t)(e.bits & 0xFFFFFFFFu);
  if (i >= s->entries.size) return FUSED_ARCHETYPE_INVALID;
  const fused_archetype_entry_* en = &s->entries.data[i];
  if (en->generation != (uint32_t)(e.bits >> 32) || !en->alive) return FUSED_ARCHETYPE_INVALID;
  return i;
}

static inline uint32_t fused_archetype_group_for_(fused_archetype* s, ComponentMask mask) {
  if (s->table[mask] != FUSED_ARCHETYPE_INVALID) return s->table[mask];
  const uint32_t id = (uint32_t)s->groups.size;
  const fused_archetype_group_ empty = {0};
  gds_vec_push(s->groups, empty);
  gds_vec_back(s->groups).mask = mask;
  s->table[mask] = id;
  return id;
}

static inline uint32_t fused_archetype_append_row_(fused_archetype* s, uint32_t g,
                                                   uint32_t entity, ComponentMask mask,
                                                   const ComponentValue* values) {
  fused_archetype_group_* gr = &s->groups.data[g];
  const uint32_t row = (uint32_t)gr->entity.size;
  gds_vec_push(gr->entity, entity);
  if (mask & GDS_K_POSITION) gds_vec_push(gr->position, values[0].position);
  if (mask & GDS_K_VELOCITY) gds_vec_push(gr->velocity, values[1].velocity);
  if (mask & GDS_K_HEALTH) gds_vec_push(gr->health, values[2].health);
  if (mask & GDS_K_TAG) gds_vec_push(gr->tag, values[3].tag);
  return row;
}

static inline void fused_archetype_remove_row_(fused_archetype* s, uint32_t g, uint32_t row) {
  fused_archetype_group_* gr = &s->groups.data[g];
  const uint32_t moved = gds_vec_back(gr->entity);
  gr->entity.data[row] = moved;
  (void)gds_vec_pop(gr->entity);
  if (gr->mask & GDS_K_POSITION) {
    gr->position.data[row] = gds_vec_back(gr->position);
    (void)gds_vec_pop(gr->position);
  }
  if (gr->mask & GDS_K_VELOCITY) {
    gr->velocity.data[row] = gds_vec_back(gr->velocity);
    (void)gds_vec_pop(gr->velocity);
  }
  if (gr->mask & GDS_K_HEALTH) {
    gr->health.data[row] = gds_vec_back(gr->health);
    (void)gds_vec_pop(gr->health);
  }
  if (gr->mask & GDS_K_TAG) {
    gr->tag.data[row] = gds_vec_back(gr->tag);
    (void)gds_vec_pop(gr->tag);
  }
  if (row < gr->entity.size) s->entries.data[moved].row = row;
}

static inline void fused_archetype_read_(const fused_archetype* s, uint32_t g, uint32_t row,
                                         ComponentId c, ComponentValue* out) {
  const fused_archetype_group_* gr = &s->groups.data[g];
  switch (c) {
    case GDS_POSITION: out->position = gr->position.data[row]; break;
    case GDS_VELOCITY: out->velocity = gr->velocity.data[row]; break;
    case GDS_HEALTH: out->health = gr->health.data[row]; break;
    case GDS_TAG: out->tag = gr->tag.data[row]; break;
  }
}

static inline void fused_archetype_write_(fused_archetype* s, uint32_t g, uint32_t row,
                                          ComponentId c, const ComponentValue* v) {
  fused_archetype_group_* gr = &s->groups.data[g];
  switch (c) {
    case GDS_POSITION: gr->position.data[row] = v->position; break;
    case GDS_VELOCITY: gr->velocity.data[row] = v->velocity; break;
    case GDS_HEALTH: gr->health.data[row] = v->health; break;
    case GDS_TAG: gr->tag.data[row] = v->tag; break;
  }
}

static inline void fused_archetype_read_all_(const fused_archetype* s, uint32_t g, uint32_t row,
                                             ComponentMask mask, ComponentValue* out) {
  const fused_archetype_group_* gr = &s->groups.data[g];
  if (mask & GDS_K_POSITION) out[0].position = gr->position.data[row];
  if (mask & GDS_K_VELOCITY) out[1].velocity = gr->velocity.data[row];
  if (mask & GDS_K_HEALTH) out[2].health = gr->health.data[row];
  if (mask & GDS_K_TAG) out[3].tag = gr->tag.data[row];
}

static inline void fused_archetype_move_entity_(fused_archetype* s, uint32_t i,
                                                ComponentMask new_mask,
                                                const ComponentValue* vals) {
  fused_archetype_entry_* en = &s->entries.data[i];
  const uint32_t old_group = en->group;
  const uint32_t old_row = en->row;
  const uint32_t new_group = fused_archetype_group_for_(s, new_mask);
  const uint32_t new_row = fused_archetype_append_row_(s, new_group, i, new_mask, vals);
  fused_archetype_remove_row_(s, old_group, old_row);
  en->group = new_group;
  en->row = new_row;
}

/* Resolves when the new entity holds both of the pair: it is appended to a
 * group the pending step covers, and would otherwise receive a step recorded
 * before it existed. Any other mask lands in a group the step never touches. */
static inline Entity fused_archetype_create(fused_archetype* s, ComponentMask mask,
                                            const ComponentValue* values) {
  if (s->pending && (mask & FUSED_ARCHETYPE_PAIR) == FUSED_ARCHETYPE_PAIR) {
    fused_archetype_resolve_pending_(s);
  }
  uint32_t index;
  if (s->free_list.size) {
    index = gds_vec_pop(s->free_list);
  } else {
    index = (uint32_t)s->entries.size;
    const fused_archetype_entry_ fresh = {1, 0, FUSED_ARCHETYPE_INVALID, 0};
    gds_vec_push(s->entries, fresh);
  }
  fused_archetype_entry_* en = &s->entries.data[index];
  en->alive = 1;
  const uint32_t g = fused_archetype_group_for_(s, mask);
  en->group = g;
  en->row = fused_archetype_append_row_(s, g, index, mask, values);
  ++s->live;
  const Entity e = {((uint64_t)en->generation << 32) | index};
  return e;
}

/* No resolve. The swap-remove moves the last row's Position and Velocity
 * together into the hole, so the moved entity still meets the step with its
 * own two values, and the destroyed entity's step can no longer be observed. */
static inline void fused_archetype_destroy(fused_archetype* s, Entity e) {
  const uint32_t i = fused_archetype_resolve_(s, e);
  if (i == FUSED_ARCHETYPE_INVALID) return;
  fused_archetype_remove_row_(s, s->entries.data[i].group, s->entries.data[i].row);
  s->entries.data[i].alive = 0;
  s->entries.data[i].group = FUSED_ARCHETYPE_INVALID;
  ++s->entries.data[i].generation;
  gds_vec_push(s->free_list, i);
  --s->live;
}

/* No resolve: the step changes no liveness. */
static inline bool fused_archetype_alive(fused_archetype* s, Entity e) {
  return fused_archetype_resolve_(s, e) != FUSED_ARCHETYPE_INVALID;
}

/* Resolves when the component is Position or Velocity and the entity holds
 * both after the add. Overwriting Position would have the step added to the
 * new value; overwriting Velocity would have the step computed from the new
 * velocity; gaining the second of the pair moves the entity into a group the
 * step covers although it did not hold both when integrate was called. An
 * entity that does not hold both after the add did not hold both before, and
 * the step never touches it. Adding Health or Tag changes neither value, and
 * the move it causes carries Position and Velocity together into a group that
 * is stepped exactly when the old one was. */
static inline void fused_archetype_add(fused_archetype* s, Entity e, ComponentId c,
                                       const ComponentValue* v) {
  const uint32_t i = fused_archetype_resolve_(s, e);
  if (i == FUSED_ARCHETYPE_INVALID) return;
  fused_archetype_entry_* en = &s->entries.data[i];
  if (s->pending && (gds_mask_of(c) & FUSED_ARCHETYPE_PAIR) &&
      ((s->groups.data[en->group].mask | gds_mask_of(c)) & FUSED_ARCHETYPE_PAIR) ==
          FUSED_ARCHETYPE_PAIR) {
    fused_archetype_resolve_pending_(s);
  }
  if (s->groups.data[en->group].mask & gds_mask_of(c)) {
    fused_archetype_write_(s, en->group, en->row, c, v);
    return;
  }
  ComponentValue vals[GDS_COMPONENT_COUNT];
  const ComponentMask old_mask = s->groups.data[en->group].mask;
  fused_archetype_read_all_(s, en->group, en->row, old_mask, vals);
  vals[(int)c] = *v;
  fused_archetype_move_entity_(s, i, (ComponentMask)(old_mask | gds_mask_of(c)), vals);
}

/* Resolves only when Velocity is removed from an entity that holds Position:
 * the position stays and must receive the step computed from the velocity
 * being discarded. Removing Position from an entity holding both discards the
 * one value the step would change, and no later operation can read it back,
 * since an add writes a fresh value. Removing Health or Tag keeps the pair
 * together, as in add. */
static inline void fused_archetype_remove(fused_archetype* s, Entity e, ComponentId c) {
  const uint32_t i = fused_archetype_resolve_(s, e);
  if (i == FUSED_ARCHETYPE_INVALID) return;
  fused_archetype_entry_* en = &s->entries.data[i];
  const ComponentMask old_mask = s->groups.data[en->group].mask;
  if (!(old_mask & gds_mask_of(c))) return;
  if (s->pending && c == GDS_VELOCITY && (old_mask & GDS_K_POSITION)) {
    fused_archetype_resolve_pending_(s);
  }
  ComponentValue vals[GDS_COMPONENT_COUNT];
  fused_archetype_read_all_(s, en->group, en->row, old_mask, vals);
  fused_archetype_move_entity_(s, i, (ComponentMask)(old_mask & ~gds_mask_of(c)), vals);
}

/* Resolves only for the Position of an entity holding both, the one value
 * the step changes. */
static inline bool fused_archetype_get(fused_archetype* s, Entity e, ComponentId c,
                                       ComponentValue* out) {
  const uint32_t i = fused_archetype_resolve_(s, e);
  if (i == FUSED_ARCHETYPE_INVALID) return false;
  const fused_archetype_entry_* en = &s->entries.data[i];
  if (!(s->groups.data[en->group].mask & gds_mask_of(c))) return false;
  if (s->pending && c == GDS_POSITION &&
      (s->groups.data[en->group].mask & FUSED_ARCHETYPE_PAIR) == FUSED_ARCHETYPE_PAIR) {
    fused_archetype_resolve_pending_(s);
  }
  fused_archetype_read_(s, en->group, en->row, c, out);
  return true;
}

/* Resolves when the component is Position or Velocity and the entity holds
 * both, for the reasons an overwriting add does. */
static inline bool fused_archetype_set(fused_archetype* s, Entity e, ComponentId c,
                                       const ComponentValue* v) {
  const uint32_t i = fused_archetype_resolve_(s, e);
  if (i == FUSED_ARCHETYPE_INVALID) return false;
  const fused_archetype_entry_* en = &s->entries.data[i];
  if (!(s->groups.data[en->group].mask & gds_mask_of(c))) return false;
  if (s->pending && (gds_mask_of(c) & FUSED_ARCHETYPE_PAIR) &&
      (s->groups.data[en->group].mask & FUSED_ARCHETYPE_PAIR) == FUSED_ARCHETYPE_PAIR) {
    fused_archetype_resolve_pending_(s);
  }
  fused_archetype_write_(s, en->group, en->row, c, v);
  return true;
}

/* No resolve: the step changes no mask. */
static inline ComponentMask fused_archetype_mask(fused_archetype* s, Entity e) {
  const uint32_t i = fused_archetype_resolve_(s, e);
  return i == FUSED_ARCHETYPE_INVALID ? (ComponentMask)0
                                      : s->groups.data[s->entries.data[i].group].mask;
}

/* With no step pending, the parent's query. With one pending, query_and_step
 * answers and resolves it in one walk and clears the flag, so a second query
 * in the same frame comes here and takes the parent's loop. */
static inline uint64_t fused_archetype_query(fused_archetype* s, ComponentMask required,
                                             uint64_t salt) {
  if (s->pending) return fused_archetype_query_and_step_(s, required, salt);
  uint64_t acc = 0;
  ComponentValue v[GDS_COMPONENT_COUNT];
  for (size_t gi = 0; gi < s->groups.size; ++gi) {
    const fused_archetype_group_* g = &s->groups.data[gi];
    if ((g->mask & required) != required) continue;
    const size_t n = g->entity.size;
    for (size_t r = 0; r < n; ++r) {
      if (required & GDS_K_POSITION) v[0].position = g->position.data[r];
      if (required & GDS_K_VELOCITY) v[1].velocity = g->velocity.data[r];
      if (required & GDS_K_HEALTH) v[2].health = g->health.data[r];
      if (required & GDS_K_TAG) v[3].tag = g->tag.data[r];
      acc += gds_digest_entity(required, v, salt);
    }
  }
  return acc;
}

/* A second step cannot be folded into the first: (p + v*a) + v*b is not
 * p + v*(a + b) in float arithmetic. So a pending step is applied before the
 * next is recorded, and at most one is ever pending. */
static inline void fused_archetype_integrate(fused_archetype* s, float dt) {
  if (s->pending) fused_archetype_resolve_pending_(s);
  s->pending = true;
  s->pending_dt = dt;
}

/* A frame whose queries name nothing, or a frame with no query at all, still
 * has its step applied here, so no step outlives the frame it belongs to. */
static inline void fused_archetype_sync(fused_archetype* s) {
  if (s->pending) fused_archetype_resolve_pending_(s);
}

/* No resolve: the step creates and destroys nothing. */
static inline size_t fused_archetype_entity_count(fused_archetype* s) { return s->live; }

/* The parent's count plus the group vector itself, which is a heap
 * allocation the structure owns and the parent leaves out. The pending step
 * allocates nothing. */
static inline size_t fused_archetype_reported_bytes(fused_archetype* s) {
  size_t b = gds_vec_bytes(s->entries) + gds_vec_bytes(s->free_list) + sizeof(s->table) +
             gds_vec_bytes(s->groups);
  for (size_t gi = 0; gi < s->groups.size; ++gi) {
    const fused_archetype_group_* g = &s->groups.data[gi];
    b += gds_vec_bytes(g->entity) + gds_vec_bytes(g->position) + gds_vec_bytes(g->velocity) +
         gds_vec_bytes(g->health) + gds_vec_bytes(g->tag);
  }
  return b;
}

#endif
