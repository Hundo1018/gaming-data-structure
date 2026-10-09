#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "gds/spatial/types.hpp"

namespace gds::candidates {

using namespace gds::spatial;

// morton_sorted with its sort key and its lookup replaced, and nothing else
// changed in kind. Positions are still written into flat arrays as they change,
// and the searchable form is still thrown away and rebuilt by the first query
// that follows any change.
//
// The key is uniform_grid's dense cell index instead of a Morton code. A dense
// integer key admits a counting sort, which orders the population with no
// comparison, and the running totals that sort computes are a directory: the
// entities of cell c are the range [start[c], start[c+1]). Finding a cell is
// one array read, as it is in the grid, instead of the binary search per cell
// that made the parent's k-nearest search slower than a linear scan.
//
// The cell geometry is uniform_grid's, reproduced here rather than included:
// its members are reachable only by inheriting its linked lists, which this
// structure does not have, and the parent's members are private.
class CellSorted {
 public:
  static const char* name() { return "cell_sorted"; }
  static constexpr bool kNativeRewind = false;

  explicit CellSorted(const WorldConfig& cfg) : bounds_(cfg.bounds) {
    const float floor_cell = std::max(bounds_.largest_extent() / 256.0f, 1e-4f);
    cell_ = std::max(cfg.typical_query_radius, floor_cell);
    inv_cell_ = 1.0f / cell_;
    nx_ = axis_cells(bounds_.extent_x());
    ny_ = axis_cells(bounds_.extent_y());
    nz_ = axis_cells(bounds_.extent_z());
    start_.assign(static_cast<std::size_t>(nx_) * ny_ * nz_ + 1, 0);
    const std::size_t n = cfg.max_entity_id + 1;
    pos_.assign(n, Vec3{0, 0, 0});
    live_.assign(n, 0);
    cell_of_.assign(n, 0);
  }

  void insert(EntityId id, Vec3 p) {
    if (!live_[id]) ++live_count_;
    live_[id] = 1;
    pos_[id] = p;
    dirty_ = true;
  }

  void remove(EntityId id) {
    if (!live_[id]) return;
    live_[id] = 0;
    --live_count_;
    dirty_ = true;
  }

  void move_by(EntityId id, Vec3 delta) {
    if (!live_[id]) return;
    pos_[id] = wrap_into(add(pos_[id], delta), bounds_);
    dirty_ = true;
  }

  bool position_of(EntityId id, Vec3& out) const {
    if (id >= live_.size() || !live_[id]) return false;
    out = pos_[id];
    return true;
  }

  std::uint64_t query_radius(Vec3 c, float r) const {
    rebuild_if_needed();
    RadiusDigest d(c, r);
    const float r2 = r * r;
    for_each_cell_in_box(c, r, [&](std::size_t b, std::size_t e) { scan_run(b, e, c, r2, d); });
    return d.value();
  }

  std::uint64_t query_radius_of(EntityId self, float r) const {
    if (self >= live_.size() || !live_[self]) return 0;
    return query_radius(pos_[self], r) - RadiusDigest(pos_[self], r).term(self, pos_[self]);
  }

  // uniform_grid's search, unaltered: widen a box from one cell edge, doubling,
  // until the kth gathered neighbour is inside its half-width. Only where the
  // candidates come from differs.
  std::uint64_t query_knn(Vec3 c, std::uint32_t k) const {
    KnnDigest d;
    if (k == 0 || live_count_ == 0) return d.value();
    rebuild_if_needed();
    const float limit = bounds_.largest_extent() * 2.0f;
    std::vector<Neighbour> found;
    for (float r = cell_; ; r *= 2.0f) {
      found.clear();
      for_each_cell_in_box(c, r, [&](std::size_t b, std::size_t e) {
        for (std::size_t s = b; s < e; ++s) {
          found.push_back(
              Neighbour{dist2(Vec3{sorted_x_[s], sorted_y_[s], sorted_z_[s]}, c), sorted_id_[s]});
        }
      });
      const std::size_t want = std::min<std::size_t>(k, found.size());
      if (want > 0) {
        std::partial_sort(found.begin(), found.begin() + static_cast<std::ptrdiff_t>(want),
                          found.end(), nearer);
      }
      if (found.size() >= k || r > limit) {
        if (want == 0 || found[want - 1].d2 <= r * r || r > limit) {
          for (std::size_t i = 0; i < want; ++i) d.push(found[i].id, pos_[found[i].id]);
          return d.value();
        }
      }
    }
  }

  void end_tick(std::uint64_t) {}
  bool rewind_to(std::uint64_t) { return false; }

  std::size_t entity_count() const { return live_count_; }

  std::size_t reported_bytes() const {
    return pos_.capacity() * sizeof(Vec3) + live_.capacity() +
           cell_of_.capacity() * sizeof(std::uint32_t) + start_.capacity() * sizeof(std::uint32_t) +
           sorted_id_.capacity() * sizeof(EntityId) + sorted_x_.capacity() * sizeof(float) +
           sorted_y_.capacity() * sizeof(float) + sorted_z_.capacity() * sizeof(float);
  }

 protected:
  // Protected rather than private so that a descendant can change one thing
  // and include the rest by path; cell_rows does, for the walk.
  //
  // One 256-bit vector of floats. GCC's -O2 cost model vectorises a loop only
  // when its trip count is a known multiple of the vector width, so the
  // distance test runs over blocks of exactly this many slots and masks the
  // lanes past the end of a cell's range instead of stopping at it.
  static constexpr std::size_t kLanes = 8;
  // A block starting at the last entity reads this far past it.
  static constexpr std::size_t kPad = kLanes - 1;

  std::uint32_t axis_cells(float extent) const {
    const int n = static_cast<int>(std::floor(extent * inv_cell_)) + 1;
    return static_cast<std::uint32_t>(std::max(1, n));
  }

  std::uint32_t axis_index(float v, float lo, std::uint32_t n) const {
    int i = static_cast<int>(std::floor((v - lo) * inv_cell_));
    if (i < 0) i = 0;
    if (i >= static_cast<int>(n)) i = static_cast<int>(n) - 1;
    return static_cast<std::uint32_t>(i);
  }

  std::uint32_t cell_index(Vec3 p) const {
    const std::uint32_t ix = axis_index(p.x, bounds_.min.x, nx_);
    const std::uint32_t iy = axis_index(p.y, bounds_.min.y, ny_);
    const std::uint32_t iz = axis_index(p.z, bounds_.min.z, nz_);
    return (iz * ny_ + iy) * nx_ + ix;
  }

  // A counting sort on the cell index. The running totals leave each entry of
  // the directory at the end of its cell's range; the scatter walks ids
  // downwards and pre-decrements, so each entry finishes at the start of its
  // range and no separate array of write cursors is needed. Within a cell,
  // entities end up in ascending id order, which nothing depends on.
  void rebuild_if_needed() const {
    if (!dirty_) return;
    const std::size_t cells = start_.size() - 1;
    std::fill(start_.begin(), start_.end(), 0u);
    const std::size_t n = live_.size();
    for (std::size_t id = 0; id < n; ++id) {
      if (!live_[id]) continue;
      const std::uint32_t c = cell_index(pos_[id]);
      cell_of_[id] = c;
      ++start_[c];
    }
    std::uint32_t end = 0;
    for (std::size_t c = 0; c < cells; ++c) {
      end += start_[c];
      start_[c] = end;
    }
    start_[cells] = end;
    const std::size_t padded = static_cast<std::size_t>(end) + kPad;
    sorted_id_.resize(padded);
    sorted_x_.resize(padded);
    sorted_y_.resize(padded);
    sorted_z_.resize(padded);
    for (std::size_t id = n; id-- > 0;) {
      if (!live_[id]) continue;
      const std::uint32_t slot = --start_[cell_of_[id]];
      sorted_id_[slot] = static_cast<EntityId>(id);
      sorted_x_[slot] = pos_[id].x;
      sorted_y_[slot] = pos_[id].y;
      sorted_z_[slot] = pos_[id].z;
    }
    dirty_ = false;
  }

  // Every cell overlapping the axis-aligned box of half-width r, in
  // uniform_grid's order, each as its range of the sorted arrays. The box is
  // not clipped to a sphere, so the caller does the exact test.
  //
  // The cells of one row are consecutive in the cell order, so their ranges
  // are adjacent and a row could be read as one run from two directory reads.
  // That is a second change to the lookup, which uniform_grid's layout cannot
  // make, and it is left out so that the pair differs only in the key and the
  // directory; each cell is read on its own, as the grid reads its list heads.
  template <class F>
  void for_each_cell_in_box(Vec3 c, float r, F&& f) const {
    const std::uint32_t x0 = axis_index(c.x - r, bounds_.min.x, nx_);
    const std::uint32_t x1 = axis_index(c.x + r, bounds_.min.x, nx_);
    const std::uint32_t y0 = axis_index(c.y - r, bounds_.min.y, ny_);
    const std::uint32_t y1 = axis_index(c.y + r, bounds_.min.y, ny_);
    const std::uint32_t z0 = axis_index(c.z - r, bounds_.min.z, nz_);
    const std::uint32_t z1 = axis_index(c.z + r, bounds_.min.z, nz_);
    for (std::uint32_t z = z0; z <= z1; ++z) {
      for (std::uint32_t y = y0; y <= y1; ++y) {
        const std::size_t row = (static_cast<std::size_t>(z) * ny_ + y) * nx_;
        for (std::uint32_t x = x0; x <= x1; ++x) {
          const std::size_t b = start_[row + x];
          const std::size_t e = start_[row + x + 1];
          if (b < e) f(b, e);
        }
      }
    }
  }

  // The exact test over one cell's range, a block of kLanes slots at a time.
  // The block is indexed from its own base pointer so the compiler sees
  // contiguous loads; each lane's accept bit is the shared dist2 test and its
  // index being inside the range, and the accepted lanes are then folded one by
  // one. A cell with the cell edge at the query radius usually holds fewer
  // entities than a block, so its range is one block with the tail masked.
  void scan_run(std::size_t b, std::size_t e, Vec3 c, float r2, RadiusDigest& d) const {
    for (std::size_t i = b; i < e; i += kLanes) {
      const float* px = sorted_x_.data() + i;
      const float* py = sorted_y_.data() + i;
      const float* pz = sorted_z_.data() + i;
      const std::size_t left = e - i;
      std::uint32_t hits = 0;
      for (std::size_t j = 0; j < kLanes; ++j) {
        const bool in = dist2(Vec3{px[j], py[j], pz[j]}, c) <= r2;
        hits |= static_cast<std::uint32_t>(in & (j < left)) << j;
      }
      while (hits != 0) {
        const std::size_t j = static_cast<std::size_t>(std::countr_zero(hits));
        hits &= hits - 1;
        d.hit(sorted_id_[i + j], Vec3{px[j], py[j], pz[j]});
      }
    }
  }

  Bounds bounds_;
  float cell_ = 1.0f;
  float inv_cell_ = 1.0f;
  std::uint32_t nx_ = 1, ny_ = 1, nz_ = 1;
  std::vector<Vec3> pos_;
  std::vector<std::uint8_t> live_;
  std::size_t live_count_ = 0;
  mutable bool dirty_ = true;
  mutable std::vector<std::uint32_t> cell_of_;
  mutable std::vector<std::uint32_t> start_;
  mutable std::vector<EntityId> sorted_id_;
  mutable std::vector<float> sorted_x_;
  mutable std::vector<float> sorted_y_;
  mutable std::vector<float> sorted_z_;
};

}  // namespace gds::candidates
