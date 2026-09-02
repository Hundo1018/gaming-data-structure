// Objective memory accounting.
//
// A candidate reports its own footprint via reported_bytes(), which is a claim.
// This tracker replaces the global allocation operators and measures what the
// process actually took, which is evidence. Both are recorded; they are
// expected to disagree and the disagreement is informative.
#pragma once

#include <cstddef>
#include <cstdint>

namespace gds {

struct AllocStats {
  std::int64_t live_bytes;   // bytes outstanding, relative to the last reset
  std::int64_t peak_bytes;   // high-water mark of live_bytes since the reset
  std::uint64_t alloc_count;
  std::uint64_t free_count;
  std::uint64_t total_bytes; // cumulative bytes requested since the reset
};

// Zeroes the counters. Call immediately before constructing the structure so
// that harness-owned memory (the op stream) is not charged to the candidate.
void alloc_reset();
AllocStats alloc_snapshot();

}  // namespace gds
