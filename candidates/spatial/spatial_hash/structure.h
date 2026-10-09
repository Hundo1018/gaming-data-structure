#ifndef CANDIDATE_SPATIAL_SPATIAL_HASH_H
#define CANDIDATE_SPATIAL_SPATIAL_HASH_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gds/spatial/knn.h"
#include "gds/spatial/types.h"
#include "gds/vec.h"

#define SPATIAL_HASH_NO_KEY (~(uint64_t)0)

typedef struct {
  uint64_t key;
  EntityId head;
} spatial_hash_slot_;

/* The same cell decomposition as a uniform grid, but cells live in an open
 * addressed hash table instead of a dense array. Memory follows the number of
 * cells that actually hold something rather than the size of the world, and the
 * world needs no bounds at all: a coordinate anywhere hashes to a cell.
 *
 * The price is one hash lookup per cell a query visits, where the dense grid
 * does one array index. A query touching twenty-seven cells pays twenty-seven
 * probes into a table that no part of the walk has warmed. */
typedef struct {
  Bounds bounds;
  float cell;
  float inv_cell;
  GDS_VEC(spatial_hash_slot_) table;
  size_t mask;
  size_t used;
  GDS_VEC(EntityId) next;
  GDS_VEC(EntityId) prev;
  GDS_VEC(uint64_t) key_of;
  GDS_VEC(Vec3) pos;
  GDS_VEC(uint8_t) live;
  size_t live_count;
} spatial_hash;

enum { spatial_hash_native_rewind = 0 };

static inline int32_t spatial_hash_axis_cell_(const spatial_hash* s, float v, float lo) {
  return (int32_t)floorf((v - lo) * s->inv_cell);
}

/* Three signed cell coordinates biased into 21 unsigned bits each. The world
 * is finite, so 21 bits per axis cannot collide for any position the workload
 * can produce. */
static inline uint64_t spatial_hash_pack_(int32_t x, int32_t y, int32_t z) {
  const uint64_t ux = (uint64_t)(x + (1 << 20)) & 0x1FFFFF;
  const uint64_t uy = (uint64_t)(y + (1 << 20)) & 0x1FFFFF;
  const uint64_t uz = (uint64_t)(z + (1 << 20)) & 0x1FFFFF;
  return ux | (uy << 21) | (uz << 42);
}

static inline uint64_t spatial_hash_key_of_point_(const spatial_hash* s, Vec3 p) {
  return spatial_hash_pack_(spatial_hash_axis_cell_(s, p.x, s->bounds.min.x),
                            spatial_hash_axis_cell_(s, p.y, s->bounds.min.y),
                            spatial_hash_axis_cell_(s, p.z, s->bounds.min.z));
}

static inline size_t spatial_hash_find_slot_(const spatial_hash* s, uint64_t key) {
  size_t i = (size_t)gds_splitmix64(key) & s->mask;
  while (s->table.data[i].key != SPATIAL_HASH_NO_KEY && s->table.data[i].key != key)
    i = (i + 1) & s->mask;
  return i;
}

static inline void spatial_hash_rehash_(spatial_hash* s, size_t capacity) {
  /* The old table is swapped out (field by field: two GDS_VEC types never
   * match) and released only after every slot has moved into the new one. */
  GDS_VEC(spatial_hash_slot_) old;
  old.data = s->table.data;
  old.size = s->table.size;
  old.cap = s->table.cap;
  gds_vec_init(s->table);
  const spatial_hash_slot_ empty = {SPATIAL_HASH_NO_KEY, GDS_NO_ENTITY};
  gds_vec_assign(s->table, capacity, empty);
  s->mask = capacity - 1;
  s->used = 0;
  for (size_t j = 0; j < old.size; ++j) {
    const spatial_hash_slot_ slot = old.data[j];
    if (slot.key == SPATIAL_HASH_NO_KEY) continue;
    s->table.data[spatial_hash_find_slot_(s, slot.key)] = slot;
    ++s->used;
  }
  gds_vec_free(old);
}

static inline void spatial_hash_init(spatial_hash* s, const WorldConfig* cfg) {
  memset(s, 0, sizeof *s);
  s->bounds = cfg->bounds;
  float floor_cell = gds_bounds_largest_extent(&s->bounds) / 256.0f;
  if (floor_cell < 1e-4f) floor_cell = 1e-4f;
  s->cell = cfg->typical_query_radius < floor_cell ? floor_cell : cfg->typical_query_radius;
  s->inv_cell = 1.0f / s->cell;
  const size_t n = (size_t)cfg->max_entity_id + 1;
  const Vec3 zero = {0, 0, 0};
  gds_vec_assign(s->next, n, GDS_NO_ENTITY);
  gds_vec_assign(s->prev, n, GDS_NO_ENTITY);
  gds_vec_assign(s->key_of, n, SPATIAL_HASH_NO_KEY);
  gds_vec_assign(s->pos, n, zero);
  gds_vec_assign(s->live, n, 0);
  spatial_hash_rehash_(s, 1024);
}

static inline void spatial_hash_free(spatial_hash* s) {
  gds_vec_free(s->table);
  gds_vec_free(s->next);
  gds_vec_free(s->prev);
  gds_vec_free(s->key_of);
  gds_vec_free(s->pos);
  gds_vec_free(s->live);
}

static inline void spatial_hash_link_(spatial_hash* s, EntityId id, uint64_t key) {
  if ((s->used + 1) * 10 >= s->table.size * 7) spatial_hash_rehash_(s, s->table.size * 2);
  const size_t i = spatial_hash_find_slot_(s, key);
  spatial_hash_slot_* slot = &s->table.data[i];
  if (slot->key == SPATIAL_HASH_NO_KEY) {
    slot->key = key;
    slot->head = GDS_NO_ENTITY;
    ++s->used;
  }
  s->key_of.data[id] = key;
  const EntityId h = slot->head;
  s->next.data[id] = h;
  s->prev.data[id] = GDS_NO_ENTITY;
  if (h != GDS_NO_ENTITY) s->prev.data[h] = id;
  slot->head = id;
}

/* A cell that empties keeps its slot. Reclaiming it would need either
 * tombstones or a rehash on every removal, and the cell is almost always
 * reoccupied within a few ticks by something moving through it. */
static inline void spatial_hash_unlink_(spatial_hash* s, EntityId id) {
  const uint64_t key = s->key_of.data[id];
  if (key == SPATIAL_HASH_NO_KEY) return;
  const size_t i = spatial_hash_find_slot_(s, key);
  const EntityId p = s->prev.data[id];
  const EntityId n = s->next.data[id];
  if (p != GDS_NO_ENTITY) s->next.data[p] = n;
  else if (s->table.data[i].key == key) s->table.data[i].head = n;
  if (n != GDS_NO_ENTITY) s->prev.data[n] = p;
  s->key_of.data[id] = SPATIAL_HASH_NO_KEY;
  s->next.data[id] = GDS_NO_ENTITY;
  s->prev.data[id] = GDS_NO_ENTITY;
}

static inline void spatial_hash_insert(spatial_hash* s, EntityId id, Vec3 p) {
  if (s->live.data[id]) spatial_hash_unlink_(s, id);
  else ++s->live_count;
  s->live.data[id] = 1;
  s->pos.data[id] = p;
  spatial_hash_link_(s, id, spatial_hash_key_of_point_(s, p));
}

static inline void spatial_hash_remove(spatial_hash* s, EntityId id) {
  if (!s->live.data[id]) return;
  spatial_hash_unlink_(s, id);
  s->live.data[id] = 0;
  --s->live_count;
}

static inline void spatial_hash_move_by(spatial_hash* s, EntityId id, Vec3 delta) {
  if (!s->live.data[id]) return;
  const Vec3 np = gds_wrap_into(gds_add3(s->pos.data[id], delta), &s->bounds);
  s->pos.data[id] = np;
  const uint64_t nk = spatial_hash_key_of_point_(s, np);
  if (nk != s->key_of.data[id]) {
    spatial_hash_unlink_(s, id);
    spatial_hash_link_(s, id, nk);
  }
}

static inline bool spatial_hash_position_of(spatial_hash* s, EntityId id, Vec3* out) {
  if (id >= s->live.size || !s->live.data[id]) return false;
  *out = s->pos.data[id];
  return true;
}

/* The cell range of the axis-aligned box of half-width r around c, in signed
 * cell coordinates. Each cell of it is one probe into the table; a cell with
 * no slot holds nothing. */
typedef struct {
  int32_t x0, x1, y0, y1, z0, z1;
} spatial_hash_box_;

static inline spatial_hash_box_ spatial_hash_box_of_(const spatial_hash* s, Vec3 c, float r) {
  spatial_hash_box_ b;
  b.x0 = spatial_hash_axis_cell_(s, c.x - r, s->bounds.min.x);
  b.x1 = spatial_hash_axis_cell_(s, c.x + r, s->bounds.min.x);
  b.y0 = spatial_hash_axis_cell_(s, c.y - r, s->bounds.min.y);
  b.y1 = spatial_hash_axis_cell_(s, c.y + r, s->bounds.min.y);
  b.z0 = spatial_hash_axis_cell_(s, c.z - r, s->bounds.min.z);
  b.z1 = spatial_hash_axis_cell_(s, c.z + r, s->bounds.min.z);
  return b;
}

static inline uint64_t spatial_hash_query_radius(spatial_hash* s, Vec3 c, float r) {
  RadiusDigest d = gds_radius_digest(c, r);
  const float r2 = r * r;
  const spatial_hash_box_ b = spatial_hash_box_of_(s, c, r);
  const spatial_hash_slot_* table = s->table.data;
  const EntityId* next = s->next.data;
  const Vec3* pos = s->pos.data;
  for (int32_t z = b.z0; z <= b.z1; ++z) {
    for (int32_t y = b.y0; y <= b.y1; ++y) {
      for (int32_t x = b.x0; x <= b.x1; ++x) {
        const size_t i = spatial_hash_find_slot_(s, spatial_hash_pack_(x, y, z));
        if (table[i].key == SPATIAL_HASH_NO_KEY) continue;
        for (EntityId id = table[i].head; id != GDS_NO_ENTITY; id = next[id])
          if (gds_dist2(pos[id], c) <= r2) gds_radius_hit(&d, id, pos[id]);
      }
    }
  }
  return d.acc;
}

static inline uint64_t spatial_hash_query_radius_of(spatial_hash* s, EntityId self, float r) {
  if (self >= s->live.size || !s->live.data[self]) return 0;
  const Vec3 c = s->pos.data[self];
  const RadiusDigest own = gds_radius_digest(c, r);
  return spatial_hash_query_radius(s, c, r) - gds_radius_term(&own, self, c);
}

static inline uint64_t spatial_hash_query_knn(spatial_hash* s, Vec3 c, uint32_t k) {
  KnnDigest d = gds_knn_digest();
  if (k == 0 || s->live_count == 0) return d.acc;
  const float limit = gds_bounds_largest_extent(&s->bounds) * 2.0f;
  const spatial_hash_slot_* table = s->table.data;
  const EntityId* next = s->next.data;
  const Vec3* pos = s->pos.data;
  GDS_VEC(Neighbour) found;
  gds_vec_init(found);
  for (float r = s->cell;; r *= 2.0f) {
    gds_vec_clear(found);
    const spatial_hash_box_ b = spatial_hash_box_of_(s, c, r);
    for (int32_t z = b.z0; z <= b.z1; ++z) {
      for (int32_t y = b.y0; y <= b.y1; ++y) {
        for (int32_t x = b.x0; x <= b.x1; ++x) {
          const size_t i = spatial_hash_find_slot_(s, spatial_hash_pack_(x, y, z));
          if (table[i].key == SPATIAL_HASH_NO_KEY) continue;
          for (EntityId id = table[i].head; id != GDS_NO_ENTITY; id = next[id]) {
            const Neighbour nb = {gds_dist2(pos[id], c), id};
            gds_vec_push(found, nb);
          }
        }
      }
    }
    const size_t n = found.size;
    const size_t want = k < n ? k : n;
    if (want > 0) gds_neighbour_partial_sort(found.data, want, n);
    if (n >= k || r > limit) {
      if (want == 0 || found.data[want - 1].d2 <= r * r || r > limit) {
        for (size_t i = 0; i < want; ++i)
          gds_knn_push(&d, found.data[i].id, pos[found.data[i].id]);
        gds_vec_free(found);
        return d.acc;
      }
    }
  }
}

static inline void spatial_hash_end_tick(spatial_hash* s, uint64_t tick) {
  (void)s;
  (void)tick;
}

static inline bool spatial_hash_rewind_to(spatial_hash* s, uint64_t tick) {
  (void)s;
  (void)tick;
  return false;
}

static inline size_t spatial_hash_entity_count(spatial_hash* s) { return s->live_count; }

static inline size_t spatial_hash_reported_bytes(spatial_hash* s) {
  return gds_vec_bytes(s->table) + gds_vec_bytes(s->next) + gds_vec_bytes(s->prev) +
         gds_vec_bytes(s->key_of) + gds_vec_bytes(s->pos) + gds_vec_bytes(s->live);
}

#endif
