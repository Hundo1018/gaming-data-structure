/* Objective memory accounting.
 *
 * A candidate reports its own footprint via <prefix>_reported_bytes, which is a
 * claim. This tracker replaces malloc, calloc, realloc, free and the aligned
 * allocators for the whole process and measures what was actually taken, which
 * is evidence. Both are recorded; they are expected to disagree and the
 * disagreement is informative.
 *
 * A structure cannot opt out: libc's own calls to malloc are interposed too,
 * so there is no allocation path the tracker does not see. */
#ifndef GDS_ALLOC_H
#define GDS_ALLOC_H

#include <stdint.h>

typedef struct {
  int64_t live_bytes;   /* bytes outstanding, relative to the last reset */
  int64_t peak_bytes;   /* high-water mark of live_bytes since the reset */
  uint64_t alloc_count;
  uint64_t free_count;
  uint64_t total_bytes; /* cumulative bytes requested since the reset */
} GdsAllocStats;

/* Zeroes the counters. Call immediately before constructing the structure so
 * that harness-owned memory (the op stream) is not charged to the candidate. */
void gds_alloc_reset(void);
GdsAllocStats gds_alloc_snapshot(void);

#endif
