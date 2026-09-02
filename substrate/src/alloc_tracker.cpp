#include "gds/alloc_tracker.hpp"

#include <cstdlib>
#include <new>

namespace gds {
namespace {

struct Counters {
  std::int64_t live = 0;
  std::int64_t peak = 0;
  std::uint64_t allocs = 0;
  std::uint64_t frees = 0;
  std::uint64_t total = 0;
};

// A plain global, not a function-local static: the allocation operators run
// before and after main, and must never themselves allocate or lock.
Counters g_counters;

constexpr std::size_t kHeader = 16;

struct Header {
  std::size_t size;
  std::size_t align;
};

inline void note_alloc(std::size_t n) {
  g_counters.live += static_cast<std::int64_t>(n);
  if (g_counters.live > g_counters.peak) g_counters.peak = g_counters.live;
  ++g_counters.allocs;
  g_counters.total += n;
}

inline void note_free(std::size_t n) {
  g_counters.live -= static_cast<std::int64_t>(n);
  ++g_counters.frees;
}

void* raw_alloc(std::size_t n) {
  void* base = std::malloc(n + kHeader);
  if (!base) return nullptr;
  Header* h = static_cast<Header*>(base);
  h->size = n;
  h->align = 0;
  note_alloc(n);
  return static_cast<char*>(base) + kHeader;
}

void raw_free(void* p) {
  if (!p) return;
  char* base = static_cast<char*>(p) - kHeader;
  Header* h = reinterpret_cast<Header*>(base);
  const std::size_t n = h->size;
  const std::size_t a = h->align;
  note_free(n);
  if (a == 0) {
    std::free(base);
  } else {
    std::free(static_cast<char*>(p) - a);
  }
}

void* raw_alloc_aligned(std::size_t n, std::size_t align) {
  if (align < kHeader) align = kHeader;
  const std::size_t padded = ((n + align + align - 1) / align) * align;
  void* base = std::aligned_alloc(align, padded);
  if (!base) return nullptr;
  char* user = static_cast<char*>(base) + align;
  Header* h = reinterpret_cast<Header*>(user - kHeader);
  h->size = n;
  h->align = align;
  note_alloc(n);
  return user;
}

}  // namespace

void alloc_reset() {
  g_counters.live = 0;
  g_counters.peak = 0;
  g_counters.allocs = 0;
  g_counters.frees = 0;
  g_counters.total = 0;
}

AllocStats alloc_snapshot() {
  AllocStats s;
  s.live_bytes = g_counters.live;
  s.peak_bytes = g_counters.peak;
  s.alloc_count = g_counters.allocs;
  s.free_count = g_counters.frees;
  s.total_bytes = g_counters.total;
  return s;
}

}  // namespace gds

void* operator new(std::size_t n) {
  void* p = gds::raw_alloc(n);
  if (!p) throw std::bad_alloc();
  return p;
}
void* operator new[](std::size_t n) { return operator new(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return gds::raw_alloc(n); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return gds::raw_alloc(n); }

void* operator new(std::size_t n, std::align_val_t a) {
  void* p = gds::raw_alloc_aligned(n, static_cast<std::size_t>(a));
  if (!p) throw std::bad_alloc();
  return p;
}
void* operator new[](std::size_t n, std::align_val_t a) { return operator new(n, a); }
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
  return gds::raw_alloc_aligned(n, static_cast<std::size_t>(a));
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
  return gds::raw_alloc_aligned(n, static_cast<std::size_t>(a));
}

void operator delete(void* p) noexcept { gds::raw_free(p); }
void operator delete[](void* p) noexcept { gds::raw_free(p); }
void operator delete(void* p, std::size_t) noexcept { gds::raw_free(p); }
void operator delete[](void* p, std::size_t) noexcept { gds::raw_free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { gds::raw_free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { gds::raw_free(p); }
void operator delete(void* p, std::align_val_t) noexcept { gds::raw_free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { gds::raw_free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { gds::raw_free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { gds::raw_free(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { gds::raw_free(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept {
  gds::raw_free(p);
}
