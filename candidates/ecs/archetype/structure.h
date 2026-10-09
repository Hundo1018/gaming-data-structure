#ifndef CANDIDATE_ECS_ARCHETYPE_H
#define CANDIDATE_ECS_ARCHETYPE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gds/types.h"
#include "gds/vec.h"

/* Entities are grouped by their exact component set. Within a group every
 * component is a densely packed typed column with no holes and no per-entity
 * mask test, so a query is a straight walk over the groups that match.
 * The cost lands on structural change: add or remove a component and the whole
 * entity is copied into a different group.
 *
 * Caveat recorded for later comparison: with four component types the group
 * lookup is a 16-entry direct table. A production archetype store hashes an
 * unbounded component set, so this implementation understates lookup cost. */

#define ARCHETYPE_INVALID (~0u)
#define ARCHETYPE_TABLE_SIZE (1 << GDS_COMPONENT_COUNT)

typedef struct {
  uint32_t generation;
  uint32_t alive;
  uint32_t group;
  uint32_t row;
} archetype_entry_;

typedef struct {
  ComponentMask mask;
  GDS_VEC(uint32_t) entity;
  GDS_VEC(Position) position;
  GDS_VEC(Velocity) velocity;
  GDS_VEC(Health) health;
  GDS_VEC(Tag) tag;
} archetype_group_;

typedef struct {
  GDS_VEC(archetype_entry_) entries;
  GDS_VEC(uint32_t) free_list;
  GDS_VEC(archetype_group_) groups;
  uint32_t table[ARCHETYPE_TABLE_SIZE];
  size_t live;
} archetype;

static inline void archetype_init(archetype* s) {
  memset(s, 0, sizeof *s);
  for (int i = 0; i < ARCHETYPE_TABLE_SIZE; ++i) s->table[i] = ARCHETYPE_INVALID;
}

static inline void archetype_free(archetype* s) {
  for (size_t i = 0; i < s->groups.size; ++i) {
    archetype_group_* g = &s->groups.data[i];
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

static inline uint32_t archetype_resolve_(const archetype* s, Entity e) {
  const uint32_t i = (uint32_t)(e.bits & 0xFFFFFFFFu);
  if (i >= s->entries.size) return ARCHETYPE_INVALID;
  const archetype_entry_* en = &s->entries.data[i];
  if (en->generation != (uint32_t)(e.bits >> 32) || !en->alive) return ARCHETYPE_INVALID;
  return i;
}

static inline uint32_t archetype_group_for_(archetype* s, ComponentMask mask) {
  if (s->table[mask] != ARCHETYPE_INVALID) return s->table[mask];
  const uint32_t id = (uint32_t)s->groups.size;
  const archetype_group_ empty = {0};
  gds_vec_push(s->groups, empty);
  gds_vec_back(s->groups).mask = mask;
  s->table[mask] = id;
  return id;
}

static inline uint32_t archetype_append_row_(archetype* s, uint32_t g, uint32_t entity,
                                             ComponentMask mask, const ComponentValue* values) {
  archetype_group_* gr = &s->groups.data[g];
  const uint32_t row = (uint32_t)gr->entity.size;
  gds_vec_push(gr->entity, entity);
  if (mask & GDS_K_POSITION) gds_vec_push(gr->position, values[0].position);
  if (mask & GDS_K_VELOCITY) gds_vec_push(gr->velocity, values[1].velocity);
  if (mask & GDS_K_HEALTH) gds_vec_push(gr->health, values[2].health);
  if (mask & GDS_K_TAG) gds_vec_push(gr->tag, values[3].tag);
  return row;
}

static inline void archetype_remove_row_(archetype* s, uint32_t g, uint32_t row) {
  archetype_group_* gr = &s->groups.data[g];
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

static inline void archetype_read_(const archetype* s, uint32_t g, uint32_t row, ComponentId c,
                                   ComponentValue* out) {
  const archetype_group_* gr = &s->groups.data[g];
  switch (c) {
    case GDS_POSITION: out->position = gr->position.data[row]; break;
    case GDS_VELOCITY: out->velocity = gr->velocity.data[row]; break;
    case GDS_HEALTH: out->health = gr->health.data[row]; break;
    case GDS_TAG: out->tag = gr->tag.data[row]; break;
  }
}

static inline void archetype_write_(archetype* s, uint32_t g, uint32_t row, ComponentId c,
                                    const ComponentValue* v) {
  archetype_group_* gr = &s->groups.data[g];
  switch (c) {
    case GDS_POSITION: gr->position.data[row] = v->position; break;
    case GDS_VELOCITY: gr->velocity.data[row] = v->velocity; break;
    case GDS_HEALTH: gr->health.data[row] = v->health; break;
    case GDS_TAG: gr->tag.data[row] = v->tag; break;
  }
}

static inline void archetype_read_all_(const archetype* s, uint32_t g, uint32_t row,
                                       ComponentMask mask, ComponentValue* out) {
  const archetype_group_* gr = &s->groups.data[g];
  if (mask & GDS_K_POSITION) out[0].position = gr->position.data[row];
  if (mask & GDS_K_VELOCITY) out[1].velocity = gr->velocity.data[row];
  if (mask & GDS_K_HEALTH) out[2].health = gr->health.data[row];
  if (mask & GDS_K_TAG) out[3].tag = gr->tag.data[row];
}

static inline void archetype_move_entity_(archetype* s, uint32_t i, ComponentMask new_mask,
                                          const ComponentValue* vals) {
  archetype_entry_* en = &s->entries.data[i];
  const uint32_t old_group = en->group;
  const uint32_t old_row = en->row;
  const uint32_t new_group = archetype_group_for_(s, new_mask);
  const uint32_t new_row = archetype_append_row_(s, new_group, i, new_mask, vals);
  archetype_remove_row_(s, old_group, old_row);
  en->group = new_group;
  en->row = new_row;
}

static inline Entity archetype_create(archetype* s, ComponentMask mask,
                                      const ComponentValue* values) {
  uint32_t index;
  if (s->free_list.size) {
    index = gds_vec_pop(s->free_list);
  } else {
    index = (uint32_t)s->entries.size;
    const archetype_entry_ fresh = {1, 0, ARCHETYPE_INVALID, 0};
    gds_vec_push(s->entries, fresh);
  }
  archetype_entry_* en = &s->entries.data[index];
  en->alive = 1;
  const uint32_t g = archetype_group_for_(s, mask);
  en->group = g;
  en->row = archetype_append_row_(s, g, index, mask, values);
  ++s->live;
  const Entity e = {((uint64_t)en->generation << 32) | index};
  return e;
}

static inline void archetype_destroy(archetype* s, Entity e) {
  const uint32_t i = archetype_resolve_(s, e);
  if (i == ARCHETYPE_INVALID) return;
  archetype_remove_row_(s, s->entries.data[i].group, s->entries.data[i].row);
  s->entries.data[i].alive = 0;
  s->entries.data[i].group = ARCHETYPE_INVALID;
  ++s->entries.data[i].generation;
  gds_vec_push(s->free_list, i);
  --s->live;
}

static inline bool archetype_alive(archetype* s, Entity e) {
  return archetype_resolve_(s, e) != ARCHETYPE_INVALID;
}

static inline void archetype_add(archetype* s, Entity e, ComponentId c, const ComponentValue* v) {
  const uint32_t i = archetype_resolve_(s, e);
  if (i == ARCHETYPE_INVALID) return;
  archetype_entry_* en = &s->entries.data[i];
  if (s->groups.data[en->group].mask & gds_mask_of(c)) {
    archetype_write_(s, en->group, en->row, c, v);
    return;
  }
  ComponentValue vals[GDS_COMPONENT_COUNT];
  const ComponentMask old_mask = s->groups.data[en->group].mask;
  archetype_read_all_(s, en->group, en->row, old_mask, vals);
  vals[(int)c] = *v;
  archetype_move_entity_(s, i, (ComponentMask)(old_mask | gds_mask_of(c)), vals);
}

static inline void archetype_remove(archetype* s, Entity e, ComponentId c) {
  const uint32_t i = archetype_resolve_(s, e);
  if (i == ARCHETYPE_INVALID) return;
  archetype_entry_* en = &s->entries.data[i];
  const ComponentMask old_mask = s->groups.data[en->group].mask;
  if (!(old_mask & gds_mask_of(c))) return;
  ComponentValue vals[GDS_COMPONENT_COUNT];
  archetype_read_all_(s, en->group, en->row, old_mask, vals);
  archetype_move_entity_(s, i, (ComponentMask)(old_mask & ~gds_mask_of(c)), vals);
}

static inline bool archetype_get(archetype* s, Entity e, ComponentId c, ComponentValue* out) {
  const uint32_t i = archetype_resolve_(s, e);
  if (i == ARCHETYPE_INVALID) return false;
  const archetype_entry_* en = &s->entries.data[i];
  if (!(s->groups.data[en->group].mask & gds_mask_of(c))) return false;
  archetype_read_(s, en->group, en->row, c, out);
  return true;
}

static inline bool archetype_set(archetype* s, Entity e, ComponentId c, const ComponentValue* v) {
  const uint32_t i = archetype_resolve_(s, e);
  if (i == ARCHETYPE_INVALID) return false;
  const archetype_entry_* en = &s->entries.data[i];
  if (!(s->groups.data[en->group].mask & gds_mask_of(c))) return false;
  archetype_write_(s, en->group, en->row, c, v);
  return true;
}

static inline ComponentMask archetype_mask(archetype* s, Entity e) {
  const uint32_t i = archetype_resolve_(s, e);
  return i == ARCHETYPE_INVALID ? (ComponentMask)0 : s->groups.data[s->entries.data[i].group].mask;
}

static inline uint64_t archetype_query(archetype* s, ComponentMask required, uint64_t salt) {
  uint64_t acc = 0;
  ComponentValue v[GDS_COMPONENT_COUNT];
  for (size_t gi = 0; gi < s->groups.size; ++gi) {
    const archetype_group_* g = &s->groups.data[gi];
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

static inline void archetype_integrate(archetype* s, float dt) {
  const ComponentMask need = GDS_K_POSITION | GDS_K_VELOCITY;
  for (size_t gi = 0; gi < s->groups.size; ++gi) {
    archetype_group_* g = &s->groups.data[gi];
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

static inline void archetype_sync(archetype* s) { (void)s; }

static inline size_t archetype_entity_count(archetype* s) { return s->live; }

static inline size_t archetype_reported_bytes(archetype* s) {
  size_t b = gds_vec_bytes(s->entries) + gds_vec_bytes(s->free_list) + sizeof(s->table);
  for (size_t gi = 0; gi < s->groups.size; ++gi) {
    const archetype_group_* g = &s->groups.data[gi];
    b += gds_vec_bytes(g->entity) + gds_vec_bytes(g->position) + gds_vec_bytes(g->velocity) +
         gds_vec_bytes(g->health) + gds_vec_bytes(g->tag);
  }
  return b;
}

#endif
