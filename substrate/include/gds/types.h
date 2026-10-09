/* Core value types shared by every candidate in the ECS track.
 *
 * These types describe *what* a structure must store and answer, never *how*.
 * A candidate is free to keep components in any layout, to reorder entities, to
 * defer work, or to reconstruct information on demand. */
#ifndef GDS_TYPES_H
#define GDS_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "gds/common.h"

typedef enum {
  GDS_POSITION = 0,
  GDS_VELOCITY = 1,
  GDS_HEALTH = 2,
  GDS_TAG = 3,
} ComponentId;

enum { GDS_COMPONENT_COUNT = 4 };

typedef uint8_t ComponentMask;

static inline ComponentMask gds_mask_of(ComponentId c) { return (ComponentMask)(1u << (unsigned)c); }

enum {
  GDS_K_POSITION = 1u << 0,
  GDS_K_VELOCITY = 1u << 1,
  GDS_K_HEALTH = 1u << 2,
  GDS_K_TAG = 1u << 3,
  GDS_K_ALL = 0xF,
};

typedef struct { float x, y, z; } Position;
typedef struct { float x, y, z; } Velocity;
typedef struct { int32_t hp, max_hp; } Health;
typedef struct { uint32_t bits; } Tag;

/* 12 bytes. Only the words belonging to the addressed component are meaningful. */
typedef union {
  uint32_t raw[3];
  Position position;
  Velocity velocity;
  Health health;
  Tag tag;
} ComponentValue;

static inline int gds_component_words(ComponentId c) {
  switch (c) {
    case GDS_POSITION:
    case GDS_VELOCITY: return 3;
    case GDS_HEALTH: return 2;
    case GDS_TAG: return 1;
  }
  return 3;
}

static inline bool gds_value_equal(ComponentId c, const ComponentValue* a, const ComponentValue* b) {
  const int n = gds_component_words(c);
  for (int i = 0; i < n; ++i)
    if (a->raw[i] != b->raw[i]) return false;
  return true;
}

/* Opaque handle. The candidate chooses the bit layout; the harness only stores
 * and returns handles. A handle of a destroyed entity must never be reported
 * alive again, even if the candidate recycles storage. */
typedef struct { uint64_t bits; } Entity;

static const Entity kGdsNullEntity = {~(uint64_t)0};

/* ------------------------------------------------------------------------
 * Observation digest.
 *
 * Query results must be independent of iteration order so that candidates are
 * free to reorder entities. Every candidate uses these exact functions, so the
 * digest is comparable across representations.
 * ------------------------------------------------------------------------ */

static inline uint64_t gds_digest_position(const Position* p) {
  uint64_t a = 0x1000193ull;
  a = gds_mix_word(a, gds_float_bits(p->x));
  a = gds_mix_word(a, gds_float_bits(p->y));
  a = gds_mix_word(a, gds_float_bits(p->z));
  return a;
}

static inline uint64_t gds_digest_velocity(const Velocity* v) {
  uint64_t a = 0x1000195ull;
  a = gds_mix_word(a, gds_float_bits(v->x));
  a = gds_mix_word(a, gds_float_bits(v->y));
  a = gds_mix_word(a, gds_float_bits(v->z));
  return a;
}

static inline uint64_t gds_digest_health(const Health* h) {
  uint64_t a = 0x1000197ull;
  a = gds_mix_word(a, (uint32_t)h->hp);
  a = gds_mix_word(a, (uint32_t)h->max_hp);
  return a;
}

static inline uint64_t gds_digest_tag(const Tag* t) { return gds_mix_word(0x100019Dull, t->bits); }

static inline uint64_t gds_digest_component(ComponentId c, const ComponentValue* v) {
  switch (c) {
    case GDS_POSITION: return gds_digest_position(&v->position);
    case GDS_VELOCITY: return gds_digest_velocity(&v->velocity);
    case GDS_HEALTH: return gds_digest_health(&v->health);
    case GDS_TAG: return gds_digest_tag(&v->tag);
  }
  return 0;
}

/* Digest of one entity for a query over `required`. Components are folded in
 * ComponentId order, so the result does not depend on storage order.
 *
 * `salt` is different on every query call and is folded in first, so the
 * digest of an entity under one call says nothing about its digest under the
 * next. Without it a query's answer was a sum a structure could keep as a
 * running total and never iterate anything to produce: candidates/ecs/query_memo
 * did exactly that and was 2.6x faster than any honest candidate on
 * w04_random_access. A candidate must take the salt from the call it is
 * answering and nowhere else. */
static inline uint64_t gds_digest_entity(ComponentMask required, const ComponentValue* values,
                                         uint64_t salt) {
  uint64_t a = gds_splitmix64(0xCBF29CE484222325ull ^ salt);
  for (int i = 0; i < GDS_COMPONENT_COUNT; ++i) {
    const ComponentMask bit = (ComponentMask)(1u << i);
    if (required & bit) a = gds_splitmix64(a ^ gds_digest_component((ComponentId)i, &values[i]));
  }
  return a;
}

#endif
