#ifndef CANDIDATE_ECS_BITSET_SOA_H
#define CANDIDATE_ECS_BITSET_SOA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gds/types.h"
#include "gds/vec.h"

/* soa with membership held a second time as bit planes.
 *
 * soa's query and integrate test every slot's liveness and mask byte, so a
 * query over a component few entities hold still visits every slot. Here one
 * plane per component and one for liveness hold a bit per slot, packed into
 * 64-bit words. Iteration ANDs one word from each plane it needs, skips a zero
 * word, and visits the set bits, so its work is O(slots/64 + matches) rather
 * than O(slots). Point access never reads the planes.
 *
 * soa's code is reproduced here rather than included by path. In the C++ it had
 * to be: every member of Soa is private there, and query and integrate need its
 * column arrays by slot index, which its public interface does not expose: a
 * wrapper would have to keep a handle per slot and resolve it on every match.
 * C hides nothing, but the port keeps the reproduction, as it keeps every
 * structure's C++ layout. The index space, the handle layout, the free list,
 * the seven parallel arrays and get, set, mask and alive are soa's, unchanged;
 * what differs is where the planes are written or read. */

/* One plane. A named type so that live_bits and every comp_bits entry can be
 * passed to put_bit_, which in the C++ takes a reference to the vector. */
typedef GDS_VEC(uint64_t) bitset_soa_plane_;

typedef struct {
  GDS_VEC(uint32_t) generation;
  GDS_VEC(ComponentMask) mask;
  GDS_VEC(uint8_t) alive;
  GDS_VEC(Position) position;
  GDS_VEC(Velocity) velocity;
  GDS_VEC(Health) health;
  GDS_VEC(Tag) tag;
  GDS_VEC(uint32_t) free_list;
  size_t live;

  /* Bit (i % 64) of word (i / 64) of a plane describes slot i. comp_bits is
   * indexed by ComponentId, so plane c mirrors bit c of mask for the four
   * component bits; bits 4 to 7 of mask have no plane, which query handles. */
  bitset_soa_plane_ live_bits;
  bitset_soa_plane_ comp_bits[GDS_COMPONENT_COUNT];
} bitset_soa;

#define BITSET_SOA_INVALID (~0u)

static inline void bitset_soa_init(bitset_soa* s) { memset(s, 0, sizeof *s); }

static inline void bitset_soa_free(bitset_soa* s) {
  gds_vec_free(s->generation);
  gds_vec_free(s->mask);
  gds_vec_free(s->alive);
  gds_vec_free(s->position);
  gds_vec_free(s->velocity);
  gds_vec_free(s->health);
  gds_vec_free(s->tag);
  gds_vec_free(s->free_list);
  gds_vec_free(s->live_bits);
  for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) gds_vec_free(s->comp_bits[c]);
}

static inline Entity bitset_soa_make_handle_(uint32_t index, uint32_t generation) {
  Entity e = {((uint64_t)generation << 32) | index};
  return e;
}

/* soa's query, unchanged, for masks the planes do not cover. */
static inline uint64_t bitset_soa_query_by_slot_(const bitset_soa* s, ComponentMask required,
                                                 uint64_t salt) {
  uint64_t acc = 0;
  const size_t n = s->mask.size;
  ComponentValue v[GDS_COMPONENT_COUNT];
  for (size_t i = 0; i < n; ++i) {
    if (!s->alive.data[i] || (s->mask.data[i] & required) != required) continue;
    if (required & GDS_K_POSITION) v[0].position = s->position.data[i];
    if (required & GDS_K_VELOCITY) v[1].velocity = s->velocity.data[i];
    if (required & GDS_K_HEALTH) v[2].health = s->health.data[i];
    if (required & GDS_K_TAG) v[3].tag = s->tag.data[i];
    acc += gds_digest_entity(required, v, salt);
  }
  return acc;
}

static inline void bitset_soa_put_bit_(bitset_soa_plane_* plane, uint32_t i, bool on) {
  const uint64_t bit = (uint64_t)1 << (i & 63u);
  uint64_t* word = &plane->data[i >> 6];
  *word = on ? (*word | bit) : (*word & ~bit);
}

static inline uint32_t bitset_soa_resolve_(const bitset_soa* s, Entity e) {
  const uint32_t i = (uint32_t)(e.bits & 0xFFFFFFFFu);
  if (i >= s->generation.size) return BITSET_SOA_INVALID;
  if (s->generation.data[i] != (uint32_t)(e.bits >> 32)) return BITSET_SOA_INVALID;
  if (!s->alive.data[i]) return BITSET_SOA_INVALID;
  return i;
}

static inline void bitset_soa_store_(bitset_soa* s, uint32_t i, ComponentId c,
                                     const ComponentValue* v) {
  switch (c) {
    case GDS_POSITION: s->position.data[i] = v->position; break;
    case GDS_VELOCITY: s->velocity.data[i] = v->velocity; break;
    case GDS_HEALTH: s->health.data[i] = v->health; break;
    case GDS_TAG: s->tag.data[i] = v->tag; break;
  }
}

static inline void bitset_soa_load_(const bitset_soa* s, uint32_t i, ComponentId c,
                                    ComponentValue* out) {
  switch (c) {
    case GDS_POSITION: out->position = s->position.data[i]; break;
    case GDS_VELOCITY: out->velocity = s->velocity.data[i]; break;
    case GDS_HEALTH: out->health = s->health.data[i]; break;
    case GDS_TAG: out->tag = s->tag.data[i]; break;
  }
}

static inline Entity bitset_soa_create(bitset_soa* s, ComponentMask mask,
                                       const ComponentValue* values) {
  uint32_t index;
  if (s->free_list.size) {
    index = gds_vec_pop(s->free_list);
  } else {
    index = (uint32_t)s->generation.size;
    const Position p0 = {0, 0, 0};
    const Velocity v0 = {0, 0, 0};
    const Health h0 = {0, 0};
    const Tag t0 = {0};
    gds_vec_push(s->generation, 1u);
    gds_vec_push(s->mask, 0);
    gds_vec_push(s->alive, 0);
    gds_vec_push(s->position, p0);
    gds_vec_push(s->velocity, v0);
    gds_vec_push(s->health, h0);
    gds_vec_push(s->tag, t0);
    /* The first slot of each run of 64 brings a zero word to every plane, so
     * the bits past the last slot are always zero and a scan can stop at the
     * last word without knowing the slot count. */
    if ((index & 63u) == 0) {
      gds_vec_push(s->live_bits, 0);
      for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) gds_vec_push(s->comp_bits[c], 0);
    }
  }
  s->mask.data[index] = mask;
  s->alive.data[index] = 1;
  /* All five bits are written, set or cleared, so a recycled slot's bits do
   * not depend on what its previous occupant left. */
  bitset_soa_put_bit_(&s->live_bits, index, true);
  for (int c = 0; c < GDS_COMPONENT_COUNT; ++c)
    bitset_soa_put_bit_(&s->comp_bits[c], index, (mask >> c) & 1u);
  if (mask & GDS_K_POSITION) s->position.data[index] = values[0].position;
  if (mask & GDS_K_VELOCITY) s->velocity.data[index] = values[1].velocity;
  if (mask & GDS_K_HEALTH) s->health.data[index] = values[2].health;
  if (mask & GDS_K_TAG) s->tag.data[index] = values[3].tag;
  ++s->live;
  return bitset_soa_make_handle_(index, s->generation.data[index]);
}

static inline void bitset_soa_destroy(bitset_soa* s, Entity e) {
  const uint32_t i = bitset_soa_resolve_(s, e);
  if (i == BITSET_SOA_INVALID) return;
  s->mask.data[i] = 0;
  s->alive.data[i] = 0;
  bitset_soa_put_bit_(&s->live_bits, i, false);
  for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) bitset_soa_put_bit_(&s->comp_bits[c], i, false);
  ++s->generation.data[i];
  gds_vec_push(s->free_list, i);
  --s->live;
}

static inline bool bitset_soa_alive(bitset_soa* s, Entity e) {
  return bitset_soa_resolve_(s, e) != BITSET_SOA_INVALID;
}

static inline void bitset_soa_add(bitset_soa* s, Entity e, ComponentId c, const ComponentValue* v) {
  const uint32_t i = bitset_soa_resolve_(s, e);
  if (i == BITSET_SOA_INVALID) return;
  s->mask.data[i] |= gds_mask_of(c);
  bitset_soa_put_bit_(&s->comp_bits[(int)c], i, true);
  bitset_soa_store_(s, i, c, v);
}

static inline void bitset_soa_remove(bitset_soa* s, Entity e, ComponentId c) {
  const uint32_t i = bitset_soa_resolve_(s, e);
  if (i == BITSET_SOA_INVALID) return;
  s->mask.data[i] &= (ComponentMask)~gds_mask_of(c);
  bitset_soa_put_bit_(&s->comp_bits[(int)c], i, false);
}

static inline bool bitset_soa_get(bitset_soa* s, Entity e, ComponentId c, ComponentValue* out) {
  const uint32_t i = bitset_soa_resolve_(s, e);
  if (i == BITSET_SOA_INVALID || !(s->mask.data[i] & gds_mask_of(c))) return false;
  bitset_soa_load_(s, i, c, out);
  return true;
}

static inline bool bitset_soa_set(bitset_soa* s, Entity e, ComponentId c, const ComponentValue* v) {
  const uint32_t i = bitset_soa_resolve_(s, e);
  if (i == BITSET_SOA_INVALID || !(s->mask.data[i] & gds_mask_of(c))) return false;
  bitset_soa_store_(s, i, c, v);
  return true;
}

static inline ComponentMask bitset_soa_mask(bitset_soa* s, Entity e) {
  const uint32_t i = bitset_soa_resolve_(s, e);
  return i == BITSET_SOA_INVALID ? (ComponentMask)0 : s->mask.data[i];
}

/* The liveness word is ANDed in even though destroy clears a dead slot's
 * component bits, which already keeps it out of any non-empty mask: for the
 * empty mask, which matches every live entity, it is the only plane. Once a
 * slot is found, the gather and the digest are soa's. */
static inline uint64_t bitset_soa_query(bitset_soa* s, ComponentMask required, uint64_t salt) {
  /* Bits 4 to 7 of a mask name no component and have no plane, but soa and
   * the oracle keep them in an entity's mask and match on them. A query that
   * names one takes soa's per-slot test, so the answer is soa's for every
   * mask. No workload can build such a mask; the planes serve every one that
   * a workload can. */
  if (required & (ComponentMask)~GDS_K_ALL) {
    return bitset_soa_query_by_slot_(s, required, salt);
  }
  const uint64_t* planes[GDS_COMPONENT_COUNT];
  int np = 0;
  for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) {
    if (required & (1u << c)) planes[np++] = s->comp_bits[c].data;
  }
  const uint64_t* live = s->live_bits.data;
  const size_t words = s->live_bits.size;
  uint64_t acc = 0;
  ComponentValue v[GDS_COMPONENT_COUNT];
  for (size_t w = 0; w < words; ++w) {
    uint64_t bits = live[w];
    for (int k = 0; k < np; ++k) bits &= planes[k][w];
    while (bits != 0) {
      const size_t i = (w << 6) | (size_t)__builtin_ctzll(bits);
      bits &= bits - 1;
      if (required & GDS_K_POSITION) v[0].position = s->position.data[i];
      if (required & GDS_K_VELOCITY) v[1].velocity = s->velocity.data[i];
      if (required & GDS_K_HEALTH) v[2].health = s->health.data[i];
      if (required & GDS_K_TAG) v[3].tag = s->tag.data[i];
      acc += gds_digest_entity(required, v, salt);
    }
  }
  return acc;
}

/* The same walk over liveness, Position and Velocity; the update is soa's. */
static inline void bitset_soa_integrate(bitset_soa* s, float dt) {
  const uint64_t* live = s->live_bits.data;
  const uint64_t* pos = s->comp_bits[0].data;
  const uint64_t* vel = s->comp_bits[1].data;
  const size_t words = s->live_bits.size;
  for (size_t w = 0; w < words; ++w) {
    uint64_t bits = live[w] & pos[w] & vel[w];
    while (bits != 0) {
      const size_t i = (w << 6) | (size_t)__builtin_ctzll(bits);
      bits &= bits - 1;
      Position* p = &s->position.data[i];
      const Velocity* v = &s->velocity.data[i];
      p->x += v->x * dt;
      p->y += v->y * dt;
      p->z += v->z * dt;
    }
  }
}

static inline void bitset_soa_sync(bitset_soa* s) { (void)s; }

static inline size_t bitset_soa_entity_count(bitset_soa* s) { return s->live; }

static inline size_t bitset_soa_reported_bytes(bitset_soa* s) {
  size_t plane_words = s->live_bits.cap;
  for (int c = 0; c < GDS_COMPONENT_COUNT; ++c) plane_words += s->comp_bits[c].cap;
  return gds_vec_bytes(s->generation) + gds_vec_bytes(s->mask) + gds_vec_bytes(s->alive) +
         gds_vec_bytes(s->position) + gds_vec_bytes(s->velocity) + gds_vec_bytes(s->health) +
         gds_vec_bytes(s->tag) + gds_vec_bytes(s->free_list) + plane_words * sizeof(uint64_t);
}

#endif
