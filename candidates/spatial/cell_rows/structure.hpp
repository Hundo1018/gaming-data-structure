#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "spatial/cell_sorted/structure.hpp"

namespace gds::candidates {

// cell_sorted with one thing changed: how a query reads the box.
//
// The cell index runs x fastest, so the cells of one row of the box are
// consecutive in the cell order, and in a counting-sorted array their ranges
// are adjacent. The whole row is then one run, [start[row + x0],
// start[row + x1 + 1]), found with two directory reads however many cells it
// spans, and scanned as one sequence of vector blocks. cell_sorted reads each
// cell on its own, as uniform_grid reads its list heads, so that its
// comparison with the grid is only about the key and the directory; this pair
// isolates the run length. A linked grid cannot make this change: its cells are
// not stored next to each other.
//
// Everything else, the rebuild, the arrays and the k-nearest search strategy,
// is cell_sorted's code, included by path.
class CellRows : public CellSorted {
 public:
  using CellSorted::CellSorted;
  static const char* name() { return "cell_rows"; }

  std::uint64_t query_radius(Vec3 c, float r) const {
    rebuild_if_needed();
    RadiusDigest d(c, r);
    const float r2 = r * r;
    for_each_row_in_box(c, r, [&](std::size_t b, std::size_t e) { scan_run(b, e, c, r2, d); });
    return d.value();
  }

  std::uint64_t query_radius_of(EntityId self, float r) const {
    if (self >= live_.size() || !live_[self]) return 0;
    return query_radius(pos_[self], r) - RadiusDigest(pos_[self], r).term(self, pos_[self]);
  }

  // cell_sorted's search, gathering from row runs instead of cell ranges.
  std::uint64_t query_knn(Vec3 c, std::uint32_t k) const {
    KnnDigest d;
    if (k == 0 || live_count_ == 0) return d.value();
    rebuild_if_needed();
    const float limit = bounds_.largest_extent() * 2.0f;
    std::vector<Neighbour> found;
    for (float r = cell_; ; r *= 2.0f) {
      found.clear();
      for_each_row_in_box(c, r, [&](std::size_t b, std::size_t e) {
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

 private:
  // The same box as cell_sorted's walk, every row of it as one run.
  template <class F>
  void for_each_row_in_box(Vec3 c, float r, F&& f) const {
    const std::uint32_t x0 = axis_index(c.x - r, bounds_.min.x, nx_);
    const std::uint32_t x1 = axis_index(c.x + r, bounds_.min.x, nx_);
    const std::uint32_t y0 = axis_index(c.y - r, bounds_.min.y, ny_);
    const std::uint32_t y1 = axis_index(c.y + r, bounds_.min.y, ny_);
    const std::uint32_t z0 = axis_index(c.z - r, bounds_.min.z, nz_);
    const std::uint32_t z1 = axis_index(c.z + r, bounds_.min.z, nz_);
    for (std::uint32_t z = z0; z <= z1; ++z) {
      for (std::uint32_t y = y0; y <= y1; ++y) {
        const std::size_t row = (static_cast<std::size_t>(z) * ny_ + y) * nx_;
        const std::size_t b = start_[row + x0];
        const std::size_t e = start_[row + x1 + 1];
        if (b < e) f(b, e);
      }
    }
  }
};

}  // namespace gds::candidates
