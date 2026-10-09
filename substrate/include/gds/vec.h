/* A growable array, the one container every structure here needs.
 *
 * Growth follows libstdc++'s std::vector on purpose: doubling on push, and on
 * resize growing to size + max(size, added). The C++ version of this suite
 * used std::vector throughout, and keeping its growth policy keeps memory
 * figures comparable with the measurements taken before the port. reserve and
 * assign allocate exactly what they are asked for, as std::vector does.
 *
 * Resized-in elements are zero-filled, which is what value-initialisation gave
 * every element type used here (integers, floats, plain structs of them).
 *
 * Capacity, not size, is what a structure owns: gds_vec_bytes is what its
 * reported_bytes should count. */
#ifndef GDS_VEC_H
#define GDS_VEC_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "gds/common.h"

#define GDS_VEC(T) \
  struct {         \
    T* data;       \
    size_t size;   \
    size_t cap;    \
  }

/* Moves `data` into a block of `new_cap` elements of `elem` bytes, keeping the
 * first `keep` elements. Aborts on exhaustion: no structure here has a way to
 * report it and every measurement would be meaningless after it. */
void* gds_vec_regrow_(void* data, size_t keep, size_t new_cap, size_t elem);

/* The same without freeing `data`: the caller frees it once it is done reading
 * from it. */
void* gds_vec_alloc_copy_(const void* data, size_t keep, size_t new_cap, size_t elem);

#define gds_vec_init(v) ((v).data = NULL, (v).size = 0, (v).cap = 0)

#define gds_vec_reserve(v, n)                                                         \
  do {                                                                                \
    size_t gds_n_ = (n);                                                              \
    if (gds_n_ > (v).cap) {                                                           \
      (v).data = gds_vec_regrow_((v).data, (v).size, gds_n_, sizeof(*(v).data));      \
      (v).cap = gds_n_;                                                               \
    }                                                                                 \
  } while (0)

/* When the push grows the block, the old one is freed only after x has been
 * read, as libstdc++'s push_back does it: x may refer to an element of v
 * itself, e.g. gds_vec_push(v, v.data[0]) or a pointer taken into v. */
#define gds_vec_push(v, x)                                                            \
  do {                                                                                \
    if ((v).size == (v).cap) {                                                        \
      size_t gds_c_ = (v).cap ? 2 * (v).cap : 1;                                      \
      void* gds_old_ = (v).data;                                                      \
      (v).data = gds_vec_alloc_copy_((v).data, (v).size, gds_c_, sizeof(*(v).data));  \
      (v).cap = gds_c_;                                                               \
      (v).data[(v).size++] = (x);                                                     \
      free(gds_old_);                                                                 \
    } else {                                                                          \
      (v).data[(v).size++] = (x);                                                     \
    }                                                                                 \
  } while (0)

#define gds_vec_resize(v, n)                                                          \
  do {                                                                                \
    size_t gds_n_ = (n);                                                              \
    if (gds_n_ > (v).cap) {                                                           \
      size_t gds_c_ = (v).size + gds_max_size((v).size, gds_n_ - (v).size);           \
      (v).data = gds_vec_regrow_((v).data, (v).size, gds_c_, sizeof(*(v).data));      \
      (v).cap = gds_c_;                                                               \
    }                                                                                 \
    if (gds_n_ > (v).size)                                                            \
      memset((v).data + (v).size, 0, (gds_n_ - (v).size) * sizeof(*(v).data));        \
    (v).size = gds_n_;                                                                \
  } while (0)

/* n copies of x, as std::vector::assign(n, x): a fresh exact block if the
 * current one is too small. */
#define gds_vec_assign(v, n, x)                                                       \
  do {                                                                                \
    size_t gds_n_ = (n);                                                              \
    if (gds_n_ > (v).cap) {                                                           \
      void* gds_fresh_ = gds_vec_regrow_(NULL, 0, gds_n_, sizeof(*(v).data));         \
      free((v).data);                                                                 \
      (v).data = gds_fresh_;                                                          \
      (v).cap = gds_n_;                                                               \
    }                                                                                 \
    for (size_t gds_i_ = 0; gds_i_ < gds_n_; ++gds_i_) (v).data[gds_i_] = (x);        \
    (v).size = gds_n_;                                                                \
  } while (0)

/* dst = src, as std::vector's copy assignment: dst keeps its block when it is
 * large enough, and otherwise gets a fresh exact one. */
#define gds_vec_assign_from(dst, src)                                                 \
  do {                                                                                \
    if ((src).size > (dst).cap) {                                                     \
      void* gds_fresh_ = gds_vec_regrow_(NULL, 0, (src).size, sizeof(*(dst).data));   \
      free((dst).data);                                                               \
      (dst).data = gds_fresh_;                                                        \
      (dst).cap = (src).size;                                                         \
    }                                                                                 \
    if ((src).size) memcpy((dst).data, (src).data, (src).size * sizeof(*(dst).data)); \
    (dst).size = (src).size;                                                          \
  } while (0)

#define gds_vec_clear(v) ((v).size = 0)
#define gds_vec_back(v) ((v).data[(v).size - 1])
#define gds_vec_pop(v) ((v).data[--(v).size])
#define gds_vec_bytes(v) ((v).cap * sizeof(*(v).data))

#define gds_vec_free(v)  \
  do {                   \
    free((v).data);      \
    (v).data = NULL;     \
    (v).size = 0;        \
    (v).cap = 0;         \
  } while (0)

/* A copy with exactly `size` capacity, as std::vector's copy constructor. */
#define gds_vec_copy(dst, src)                                                        \
  do {                                                                                \
    (dst).data = gds_vec_regrow_(NULL, 0, (src).size, sizeof(*(src).data));           \
    if ((src).size) memcpy((dst).data, (src).data, (src).size * sizeof(*(src).data)); \
    (dst).size = (src).size;                                                          \
    (dst).cap = (src).size;                                                           \
  } while (0)

#endif
