#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "gds/spatial/types.hpp"

namespace gds::candidates {

using namespace gds::spatial;

// One axis only. Entities are kept sorted by x, and a radius query binary
// searches the slab [c.x - r, c.x + r] and tests everything inside it.
//
// There is no cell size to get wrong and no world bounds to configure, and a
// query is one binary search followed by a single sequential run with no
// pointer chasing anywhere. The cost is that the slab is thin in one axis and
// spans the whole world in the other two, so the number of entities examined
// grows with the world's cross-section rather than with the query volume.
//
// Present as the middle of the range: it should be far better than scanning
// everything and clearly worse than a structure that partitions all three axes,
// and the size of both gaps is worth having on the record.
class AxisSorted {
 public:
  static const char* name() { return "axis_sorted"; }
  static constexpr bool kNativeRewind = false;

  explicit AxisSorted(const WorldConfig& cfg) : bounds_(cfg.bounds) {
    const std::size_t n = cfg.max_entity_id + 1;
    pos_.assign(n, Vec3{0, 0, 0});
    live_.assign(n, 0);
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
    RadiusDigest d;
    const float r2 = r * r;
    for_each_in_slab(c.x, r, [&](std::size_t i) {
      if (dist2(sorted_pos_[i], c) <= r2) d.hit(sorted_id_[i], sorted_pos_[i]);
    });
    return d.value();
  }

  std::uint64_t query_radius_of(EntityId self, float r) const {
    if (self >= live_.size() || !live_[self]) return 0;
    return query_radius(pos_[self], r) - digest_hit(self, pos_[self]);
  }

  std::uint64_t query_knn(Vec3 c, std::uint32_t k) const {
    rebuild_if_needed();
    KnnDigest d;
    if (k == 0 || live_count_ == 0) return d.value();
    const float limit = bounds_.largest_extent() * 2.0f;
    std::vector<Neighbour> found;
    for (float r = std::max(1e-3f, bounds_.largest_extent() / 256.0f); ; r *= 2.0f) {
      found.clear();
      for_each_in_slab(c.x, r, [&](std::size_t i) {
        found.push_back(Neighbour{dist2(sorted_pos_[i], c), sorted_id_[i]});
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
           sorted_x_.capacity() * sizeof(float) + sorted_id_.capacity() * sizeof(EntityId) +
           sorted_pos_.capacity() * sizeof(Vec3);
  }

 private:
  void rebuild_if_needed() const {
    if (!dirty_) return;
    order_.clear();
    order_.reserve(live_count_);
    for (std::size_t i = 0; i < live_.size(); ++i) {
      if (live_[i]) order_.push_back(static_cast<EntityId>(i));
    }
    std::sort(order_.begin(), order_.end(), [this](EntityId a, EntityId b) {
      if (pos_[a].x != pos_[b].x) return pos_[a].x < pos_[b].x;
      return a < b;
    });
    sorted_x_.resize(order_.size());
    sorted_id_.resize(order_.size());
    sorted_pos_.resize(order_.size());
    for (std::size_t i = 0; i < order_.size(); ++i) {
      sorted_id_[i] = order_[i];
      sorted_pos_[i] = pos_[order_[i]];
      sorted_x_[i] = sorted_pos_[i].x;
    }
    dirty_ = false;
  }

  template <class F>
  void for_each_in_slab(float cx, float r, F&& f) const {
    const auto lo = std::lower_bound(sorted_x_.begin(), sorted_x_.end(), cx - r);
    const auto hi = std::upper_bound(sorted_x_.begin(), sorted_x_.end(), cx + r);
    for (auto it = lo; it != hi; ++it) f(static_cast<std::size_t>(it - sorted_x_.begin()));
  }

  Bounds bounds_;
  std::vector<Vec3> pos_;
  std::vector<std::uint8_t> live_;
  std::size_t live_count_ = 0;
  mutable bool dirty_ = true;
  mutable std::vector<EntityId> order_;
  mutable std::vector<float> sorted_x_;
  mutable std::vector<EntityId> sorted_id_;
  mutable std::vector<Vec3> sorted_pos_;
};

}  // namespace gds::candidates
