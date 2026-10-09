/* Shared by every track: hashing, bit casts, token pasting, and the one
 * barrier that stops the optimiser from deleting work nobody reads.
 *
 * C has no templates, so the substrate's generic parts (a replay of an op
 * stream, a history wrapper around an index) are written as template headers:
 * files ending in .inc.h that are included with a macro naming the structure
 * they are instantiated for. A structure is a prefix, a type of that name, and
 * functions named <prefix>_<operation>; GDS_CAT builds those names. */
#ifndef GDS_COMMON_H
#define GDS_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define GDS_CAT_(a, b) a##b
#define GDS_CAT(a, b) GDS_CAT_(a, b)
#define GDS_STR_(a) #a
#define GDS_STR(a) GDS_STR_(a)

/* <prefix>_<name>, e.g. GDS_FN(uniform_grid, insert) is uniform_grid_insert. */
#define GDS_FN(prefix, name) GDS_CAT(prefix, GDS_CAT(_, name))

static inline uint64_t gds_splitmix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ull;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
  return x ^ (x >> 31);
}

static inline uint64_t gds_mix_word(uint64_t acc, uint32_t w) {
  return gds_splitmix64(acc ^ ((uint64_t)w + 0x9E3779B97F4A7C15ull + (acc << 6) + (acc >> 2)));
}

static inline uint32_t gds_float_bits(float f) {
  uint32_t u;
  memcpy(&u, &f, sizeof u);
  return u;
}

/* Stops the optimiser from deleting work whose result is never read. */
static inline void gds_keep_u64(uint64_t v) { __asm__ volatile("" : : "r,m"(v) : "memory"); }

static inline size_t gds_min_size(size_t a, size_t b) { return a < b ? a : b; }
static inline size_t gds_max_size(size_t a, size_t b) { return a > b ? a : b; }

#endif
