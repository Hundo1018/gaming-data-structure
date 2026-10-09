/* The allocator every process here runs on.
 *
 * Defining malloc and its relatives in the executable replaces glibc's for every
 * caller, libc included; each one forwards to glibc's own implementation
 * through its __libc_ entry points and keeps a 16-byte header recording the
 * requested size, so that what is counted is what was asked for, not the
 * allocator's rounding.
 *
 * realloc never grows in place. It allocates the new block, copies, and frees
 * the old one, so for the length of the copy both are live, which is how the
 * peak was charged when every structure here was a std::vector that did the
 * same. A structure is measured on its representation, not on whether the
 * allocator happened to find room after a block. */
#include "gds/alloc.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gds/vec.h"

extern void* __libc_malloc(size_t);
extern void __libc_free(void*);
extern void* __libc_calloc(size_t, size_t);
extern void* __libc_memalign(size_t, size_t);

typedef struct {
  size_t size;
  size_t align; /* 0 for a plain block; otherwise the offset of the user pointer */
} Header;

enum { kHeader = 16 };
_Static_assert(sizeof(Header) == kHeader, "the header must be exactly 16 bytes");

/* A plain global: the allocator runs before and after main and must never
 * allocate or lock to reach its own counters. */
static GdsAllocStats g_counters;

static inline void note_alloc(size_t n) {
  g_counters.live_bytes += (int64_t)n;
  if (g_counters.live_bytes > g_counters.peak_bytes) g_counters.peak_bytes = g_counters.live_bytes;
  ++g_counters.alloc_count;
  g_counters.total_bytes += n;
}

static inline void note_free(size_t n) {
  g_counters.live_bytes -= (int64_t)n;
  ++g_counters.free_count;
}

static inline Header* header_of(void* p) { return (Header*)((char*)p - kHeader); }

void* malloc(size_t n) {
  char* base = __libc_malloc(n + kHeader);
  if (!base) return NULL;
  Header* h = (Header*)base;
  h->size = n;
  h->align = 0;
  note_alloc(n);
  return base + kHeader;
}

void* calloc(size_t count, size_t size) {
  if (size && count > SIZE_MAX / size) {
    errno = ENOMEM;
    return NULL;
  }
  const size_t n = count * size;
  char* base = __libc_calloc(1, n + kHeader);
  if (!base) return NULL;
  Header* h = (Header*)base;
  h->size = n;
  h->align = 0;
  note_alloc(n);
  return base + kHeader;
}

void free(void* p) {
  if (!p) return;
  Header* h = header_of(p);
  note_free(h->size);
  if (h->align == 0) __libc_free(h);
  else __libc_free((char*)p - h->align);
}

void* realloc(void* p, size_t n) {
  if (!p) return malloc(n);
  if (n == 0) {
    free(p);
    return NULL;
  }
  void* q = malloc(n);
  if (!q) return NULL;
  const size_t old = header_of(p)->size;
  memcpy(q, p, old < n ? old : n);
  free(p);
  return q;
}

static void* aligned(size_t align, size_t n) {
  if (align < kHeader) align = kHeader;
  if (align & (align - 1)) return NULL;
  char* base = __libc_memalign(align, n + align);
  if (!base) return NULL;
  char* user = base + align;
  Header* h = header_of(user);
  h->size = n;
  h->align = align;
  note_alloc(n);
  return user;
}

void* aligned_alloc(size_t align, size_t n) { return aligned(align, n); }
void* memalign(size_t align, size_t n) { return aligned(align, n); }
void* valloc(size_t n) { return aligned(4096, n); }
void* pvalloc(size_t n) { return aligned(4096, (n + 4095) & ~(size_t)4095); }

int posix_memalign(void** out, size_t align, size_t n) {
  if (align < sizeof(void*) || (align & (align - 1))) return EINVAL;
  void* p = aligned(align, n);
  if (!p) return ENOMEM;
  *out = p;
  return 0;
}

/* glibc's version would read a chunk header that is not where it expects. */
size_t malloc_usable_size(void* p) { return p ? header_of(p)->size : 0; }

void gds_alloc_reset(void) { memset(&g_counters, 0, sizeof g_counters); }

GdsAllocStats gds_alloc_snapshot(void) { return g_counters; }

void* gds_vec_alloc_copy_(const void* data, size_t keep, size_t new_cap, size_t elem) {
  void* fresh = new_cap ? malloc(new_cap * elem) : NULL;
  if (!fresh && new_cap) abort();
  if (keep) memcpy(fresh, data, keep * elem);
  return fresh;
}

void* gds_vec_regrow_(void* data, size_t keep, size_t new_cap, size_t elem) {
  /* An empty vector owns no block, as std::vector's does not. */
  void* fresh = new_cap ? malloc(new_cap * elem) : NULL;
  if (!fresh && new_cap) abort();
  if (keep) memcpy(fresh, data, keep * elem);
  free(data);
  return fresh;
}
