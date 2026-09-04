#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "gds/spatial/types.hpp"

namespace gds::candidates {

using namespace gds::spatial;

// A fixed grid of cells over the world box. Each cell holds an intrusive
// doubly-linked list threaded through per-entity arrays, so a cell costs four
// bytes whether or not anything is in it, and moving between cells is two
// unlinks and two links with no allocation and no search.
//
// Cell size is set to the workload's typical query radius, floored so the grid
// cannot exceed a few million cells. At that size a radius query touches at
// most twenty-seven cells regardless of how many entities exist.
class UniformGrid {
 public:
  static const char* name() { return "uniform_grid"; }
  static constexpr bool kNativeRewind = false;

  explicit UniformGrid(const WorldConfig& cfg) : bounds_(cfg.bounds) {
    const float floor_cell = std::max(bounds_.largest_extent() / 256.0f, 1e-4f);
    cell_ = std::max(cfg.typical_query_radius, floor_cell);
    inv_cell_ = 1.0f / cell_;
    nx_ = axis_cells(bounds_.extent_x());
    ny_ = axis_cells(bounds_.extent_y());
    nz_ = axis_cells(bounds_.extent_z());
    head_.assign(static_cast<std::size_t>(nx_) * ny_ * nz_, kNoEntity);
    const std::size_t n = cfg.max_entity_id + 1;
    next_.assign(n, kNoEntity);
    prev_.assign(n, kNoEntity);
    cell_of_.assign(n, kNoCell);
    pos_.assign(n, Vec3{0, 0, 0});
    live_.assign(n, 0);
  }

  void insert(EntityId id, Vec3 p) {
    if (live_[id]) unlink(id);
    else ++live_count_;
    live_[id] = 1;
    pos_[id] = p;
    link(id, cell_index(p));
  }

  void remove(EntityId id) {
    if (!live_[id]) return;
    unlink(id);
    live_[id] = 0;
    --live_count_;
  }

  void move_by(EntityId id, Vec3 delta) {
    if (!live_[id]) return;
    const Vec3 np = wrap_into(add(pos_[id], delta), bounds_);
    pos_[id] = np;
    const std::uint32_t nc = cell_index(np);
    if (nc != cell_of_[id]) {
      unlink(id);
      link(id, nc);
    }
  }

  bool position_of(EntityId id, Vec3& out) const {
    if (id >= live_.size() || !live_[id]) return false;
    out = pos_[id];
    return true;
  }

  std::uint64_t query_radius(Vec3 c, float r) const {
    RadiusDigest d;
    const float r2 = r * r;
    for_each_in_box(c, r, [&](EntityId id) {
      if (dist2(pos_[id], c) <= r2) d.hit(id, pos_[id]);
    });
    return d.value();
  }

  std::uint64_t query_radius_of(EntityId self, float r) const {
    if (self >= live_.size() || !live_[self]) return 0;
    // The digest is a sum, so removing the centre from its own result is a
    // subtraction rather than a branch inside the inner loop.
    return query_radius(pos_[self], r) - digest_hit(self, pos_[self]);
  }

  std::uint64_t query_knn(Vec3 c, std::uint32_t k) const {
    KnnDigest d;
    if (k == 0 || live_count_ == 0) return d.value();
    // Everything outside the box of half-width r is farther than r from c, so
    // once the kth gathered neighbour is within r the answer cannot change.
    const float limit = bounds_.largest_extent() * 2.0f;
    std::vector<Neighbour> found;
    for (float r = cell_; ; r *= 2.0f) {
      found.clear();
      for_each_in_box(c, r, [&](EntityId id) {
        found.push_back(Neighbour{dist2(pos_[id], c), id});
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
    return head_.capacity() * sizeof(EntityId) + next_.capacity() * sizeof(EntityId) +
           prev_.capacity() * sizeof(EntityId) + cell_of_.capacity() * sizeof(std::uint32_t) +
           pos_.capacity() * sizeof(Vec3) + live_.capacity();
  }

 protected:
  static constexpr std::uint32_t kNoCell = ~0u;

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

  void link(EntityId id, std::uint32_t cell) {
    cell_of_[id] = cell;
    const EntityId h = head_[cell];
    next_[id] = h;
    prev_[id] = kNoEntity;
    if (h != kNoEntity) prev_[h] = id;
    head_[cell] = id;
  }

  void unlink(EntityId id) {
    const std::uint32_t cell = cell_of_[id];
    if (cell == kNoCell) return;
    const EntityId p = prev_[id];
    const EntityId n = next_[id];
    if (p != kNoEntity) next_[p] = n;
    else head_[cell] = n;
    if (n != kNoEntity) prev_[n] = p;
    cell_of_[id] = kNoCell;
    next_[id] = kNoEntity;
    prev_[id] = kNoEntity;
  }

  // Walks every cell overlapping the axis-aligned box of half-width r. The box
  // is not clipped to a sphere, so the callback sees points outside the radius
  // and does the exact test itself.
  template <class F>
  void for_each_in_box(Vec3 c, float r, F&& f) const {
    const std::uint32_t x0 = axis_index(c.x - r, bounds_.min.x, nx_);
    const std::uint32_t x1 = axis_index(c.x + r, bounds_.min.x, nx_);
    const std::uint32_t y0 = axis_index(c.y - r, bounds_.min.y, ny_);
    const std::uint32_t y1 = axis_index(c.y + r, bounds_.min.y, ny_);
    const std::uint32_t z0 = axis_index(c.z - r, bounds_.min.z, nz_);
    const std::uint32_t z1 = axis_index(c.z + r, bounds_.min.z, nz_);
    for (std::uint32_t z = z0; z <= z1; ++z) {
      for (std::uint32_t y = y0; y <= y1; ++y) {
        const std::uint32_t row = (z * ny_ + y) * nx_;
        for (std::uint32_t x = x0; x <= x1; ++x) {
          for (EntityId id = head_[row + x]; id != kNoEntity; id = next_[id]) f(id);
        }
      }
    }
  }

  Bounds bounds_;
  float cell_ = 1.0f;
  float inv_cell_ = 1.0f;
  std::uint32_t nx_ = 1, ny_ = 1, nz_ = 1;
  std::vector<EntityId> head_;
  std::vector<EntityId> next_;
  std::vector<EntityId> prev_;
  std::vector<std::uint32_t> cell_of_;
  std::vector<Vec3> pos_;
  std::vector<std::uint8_t> live_;
  std::size_t live_count_ = 0;
};

}  // namespace gds::candidates
