/* Template header: sorting, partial sorting, heaps and binary search over an
 * array of one element type, with the comparison inlined.
 *
 * Include with
 *   GDS_SORT_NAME       prefix of the generated functions
 *   GDS_SORT_T          element type
 *   GDS_SORT_LESS(a, b) a strict weak order on two values of GDS_SORT_T
 * defined; all three are undefined again at the end.
 *
 * Generated, for T* a of n elements:
 *   NAME_sort(a, n)                 introsort, as std::sort
 *   NAME_partial_sort(a, mid, n)    the mid smallest, sorted, in a[0..mid)
 *   NAME_make_heap(a, n)            max-heap under LESS
 *   NAME_push_heap(a, n)            a[n-1] joins the heap a[0..n-1)
 *   NAME_pop_heap(a, n)             the largest moves to a[n-1]
 *   NAME_sort_heap(a, n)            a heap into ascending order
 *   NAME_lower_bound(a, n, key)     first index with !(a[i] < key)
 *   NAME_upper_bound(a, n, key)     first index with key < a[i]
 *
 * Each algorithm follows libstdc++'s step for step: the C++ version of this
 * suite called std::sort and its relatives, and following them exactly keeps
 * the order of equal elements, and with it each structure's memory layout, the
 * same as in the measurements taken before the port. qsort is not used
 * because its comparison is a call through a pointer on every step. */
#ifndef GDS_SORT_NAME
#error "define GDS_SORT_NAME, GDS_SORT_T and GDS_SORT_LESS before including gds/sort.inc.h"
#endif

#include <stddef.h>

#include "gds/common.h"

#define GDS_SO_(n) GDS_CAT(GDS_SORT_NAME, GDS_CAT(_, n))
#define GDS_SO_T GDS_SORT_T

static inline void GDS_SO_(push_heap_)(GDS_SO_T* a, ptrdiff_t hole, ptrdiff_t top, GDS_SO_T value) {
  ptrdiff_t parent = (hole - 1) / 2;
  while (hole > top && GDS_SORT_LESS(a[parent], value)) {
    a[hole] = a[parent];
    hole = parent;
    parent = (hole - 1) / 2;
  }
  a[hole] = value;
}

static inline void GDS_SO_(adjust_heap_)(GDS_SO_T* a, ptrdiff_t hole, ptrdiff_t len,
                                         GDS_SO_T value) {
  const ptrdiff_t top = hole;
  ptrdiff_t child = hole;
  while (child < (len - 1) / 2) {
    child = 2 * (child + 1);
    if (GDS_SORT_LESS(a[child], a[child - 1])) --child;
    a[hole] = a[child];
    hole = child;
  }
  if ((len & 1) == 0 && child == (len - 2) / 2) {
    child = 2 * (child + 1);
    a[hole] = a[child - 1];
    hole = child - 1;
  }
  GDS_SO_(push_heap_)(a, hole, top, value);
}

/* Moves a[0] to *result and re-heaps a[0..len) with the value that was in
 * *result. */
static inline void GDS_SO_(pop_heap_)(GDS_SO_T* a, ptrdiff_t len, GDS_SO_T* result) {
  GDS_SO_T value = *result;
  *result = a[0];
  GDS_SO_(adjust_heap_)(a, 0, len, value);
}

static inline void GDS_SO_(make_heap)(GDS_SO_T* a, size_t n) {
  const ptrdiff_t len = (ptrdiff_t)n;
  if (len < 2) return;
  ptrdiff_t parent = (len - 2) / 2;
  for (;;) {
    GDS_SO_T value = a[parent];
    GDS_SO_(adjust_heap_)(a, parent, len, value);
    if (parent == 0) return;
    --parent;
  }
}

static inline void GDS_SO_(push_heap)(GDS_SO_T* a, size_t n) {
  const ptrdiff_t len = (ptrdiff_t)n;
  GDS_SO_T value = a[len - 1];
  GDS_SO_(push_heap_)(a, len - 1, 0, value);
}

static inline void GDS_SO_(pop_heap)(GDS_SO_T* a, size_t n) {
  if (n > 1) GDS_SO_(pop_heap_)(a, (ptrdiff_t)n - 1, &a[n - 1]);
}

static inline void GDS_SO_(sort_heap)(GDS_SO_T* a, size_t n) {
  ptrdiff_t last = (ptrdiff_t)n;
  while (last > 1) {
    --last;
    GDS_SO_(pop_heap_)(a, last, &a[last]);
  }
}

static inline void GDS_SO_(heap_select_)(GDS_SO_T* a, size_t mid, size_t n) {
  GDS_SO_(make_heap)(a, mid);
  for (size_t i = mid; i < n; ++i)
    if (GDS_SORT_LESS(a[i], a[0])) GDS_SO_(pop_heap_)(a, (ptrdiff_t)mid, &a[i]);
}

static inline void GDS_SO_(partial_sort)(GDS_SO_T* a, size_t mid, size_t n) {
  GDS_SO_(heap_select_)(a, mid, n);
  GDS_SO_(sort_heap)(a, mid);
}

static inline void GDS_SO_(swap_)(GDS_SO_T* x, GDS_SO_T* y) {
  GDS_SO_T t = *x;
  *x = *y;
  *y = t;
}

static inline void GDS_SO_(move_median_to_first_)(GDS_SO_T* result, GDS_SO_T* a, GDS_SO_T* b,
                                                  GDS_SO_T* c) {
  if (GDS_SORT_LESS(*a, *b)) {
    if (GDS_SORT_LESS(*b, *c)) GDS_SO_(swap_)(result, b);
    else if (GDS_SORT_LESS(*a, *c)) GDS_SO_(swap_)(result, c);
    else GDS_SO_(swap_)(result, a);
  } else if (GDS_SORT_LESS(*a, *c)) {
    GDS_SO_(swap_)(result, a);
  } else if (GDS_SORT_LESS(*b, *c)) {
    GDS_SO_(swap_)(result, c);
  } else {
    GDS_SO_(swap_)(result, b);
  }
}

static inline GDS_SO_T* GDS_SO_(unguarded_partition_)(GDS_SO_T* first, GDS_SO_T* last,
                                                      GDS_SO_T* pivot) {
  for (;;) {
    while (GDS_SORT_LESS(*first, *pivot)) ++first;
    --last;
    while (GDS_SORT_LESS(*pivot, *last)) --last;
    if (!(first < last)) return first;
    GDS_SO_(swap_)(first, last);
    ++first;
  }
}

static inline void GDS_SO_(introsort_loop_)(GDS_SO_T* first, GDS_SO_T* last, int depth) {
  while (last - first > 16) {
    if (depth == 0) {
      GDS_SO_(partial_sort)(first, (size_t)(last - first), (size_t)(last - first));
      return;
    }
    --depth;
    GDS_SO_T* mid = first + (last - first) / 2;
    GDS_SO_(move_median_to_first_)(first, first + 1, mid, last - 1);
    GDS_SO_T* cut = GDS_SO_(unguarded_partition_)(first + 1, last, first);
    GDS_SO_(introsort_loop_)(cut, last, depth);
    last = cut;
  }
}

static inline void GDS_SO_(unguarded_linear_insert_)(GDS_SO_T* last) {
  GDS_SO_T value = *last;
  GDS_SO_T* next = last - 1;
  while (GDS_SORT_LESS(value, *next)) {
    *last = *next;
    last = next;
    --next;
  }
  *last = value;
}

static inline void GDS_SO_(insertion_sort_)(GDS_SO_T* first, GDS_SO_T* last) {
  if (first == last) return;
  for (GDS_SO_T* i = first + 1; i != last; ++i) {
    if (GDS_SORT_LESS(*i, *first)) {
      GDS_SO_T value = *i;
      memmove(first + 1, first, (size_t)(i - first) * sizeof *first);
      *first = value;
    } else {
      GDS_SO_(unguarded_linear_insert_)(i);
    }
  }
}

static inline void GDS_SO_(sort)(GDS_SO_T* a, size_t n) {
  if (n < 2) return;
  GDS_SO_T* first = a;
  GDS_SO_T* last = a + n;
  const int lg = 63 - __builtin_clzll((unsigned long long)n);
  GDS_SO_(introsort_loop_)(first, last, lg * 2);
  if (last - first > 16) {
    GDS_SO_(insertion_sort_)(first, first + 16);
    for (GDS_SO_T* i = first + 16; i != last; ++i) GDS_SO_(unguarded_linear_insert_)(i);
  } else {
    GDS_SO_(insertion_sort_)(first, last);
  }
}

static inline size_t GDS_SO_(lower_bound)(const GDS_SO_T* a, size_t n, GDS_SO_T key) {
  size_t first = 0;
  ptrdiff_t len = (ptrdiff_t)n;
  while (len > 0) {
    const ptrdiff_t half = len >> 1;
    const size_t middle = first + (size_t)half;
    if (GDS_SORT_LESS(a[middle], key)) {
      first = middle + 1;
      len = len - half - 1;
    } else {
      len = half;
    }
  }
  return first;
}

static inline size_t GDS_SO_(upper_bound)(const GDS_SO_T* a, size_t n, GDS_SO_T key) {
  size_t first = 0;
  ptrdiff_t len = (ptrdiff_t)n;
  while (len > 0) {
    const ptrdiff_t half = len >> 1;
    const size_t middle = first + (size_t)half;
    if (GDS_SORT_LESS(key, a[middle])) {
      len = half;
    } else {
      first = middle + 1;
      len = len - half - 1;
    }
  }
  return first;
}

#undef GDS_SO_
#undef GDS_SO_T
#undef GDS_SORT_NAME
#undef GDS_SORT_T
#undef GDS_SORT_LESS
