#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "gds/spatial/types.hpp"

namespace gds::candidates {

using namespace gds::spatial;

// No incremental update at all. Positions are written into a flat array as they
// change; the searchable form is thrown away and rebuilt from scratch the next
// time anything is asked.
//
// The rebuilt form sorts entities by the Morton code of their cell, so entities
// that are near each other in the world are near each other in memory, and the
// scan for a cell is a contiguous run rather than a pointer chase. The bet is
// that a query pays so much less that it covers rebuilding the whole array once
// per tick.
class MortonSorted {
 public:
  static const char* name() { return "morton_sorted"; }
  static constexpr bool kNativeRewind = false;

  explicit MortonSorted(const WorldConfig& cfg) : bounds_(cfg.bounds) {
    const float floor_cell = std::max(bounds_.largest_extent() / 256.0f, 1e-4f);
    cell_ = std::max(cfg.typical_query_radius, floor_cell);
    inv_cell_ = 1.0f / cell_;
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
    for_each_in_box(c, r, [&](std::size_t slot) {
      if (dist2(sorted_pos_[slot], c) <= r2) d.hit(sorted_id_[slot], sorted_pos_[slot]);
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
    for (float r = cell_; ; r *= 2.0f) {
      found.clear();
      for_each_in_box(c, r, [&](std::size_t slot) {
        found.push_back(Neighbour{dist2(sorted_pos_[slot], c), sorted_id_[slot]});
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
           sorted_.capacity() * sizeof(Entry) + sorted_id_.capacity() * sizeof(EntityId) +
           sorted_pos_.capacity() * sizeof(Vec3) + sorted_code_.capacity() * sizeof(std::uint64_t);
  }

 private:
  struct Entry {
    std::uint64_t code;
    EntityId id;
  };

  static std::uint64_t spread(std::uint64_t x) {
    x &= 0x1FFFFFull;
    x = (x | (x << 32)) & 0x1F00000000FFFFull;
    x = (x | (x << 16)) & 0x1F0000FF0000FFull;
    x = (x | (x << 8)) & 0x100F00F00F00F00Full;
    x = (x | (x << 4)) & 0x10C30C30C30C30C3ull;
    x = (x | (x << 2)) & 0x1249249249249249ull;
    return x;
  }

  static std::uint64_t morton(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return spread(x) | (spread(y) << 1) | (spread(z) << 2);
  }

  std::uint32_t axis_cell(float v, float lo) const {
    const long long i = static_cast<long long>(std::floor((v - lo) * inv_cell_));
    if (i < 0) return 0;
    if (i > 0x1FFFFF) return 0x1FFFFF;
    return static_cast<std::uint32_t>(i);
  }

  std::uint64_t code_of(Vec3 p) const {
    return morton(axis_cell(p.x, bounds_.min.x), axis_cell(p.y, bounds_.min.y),
                  axis_cell(p.z, bounds_.min.z));
  }

  void rebuild_if_needed() const {
    if (!dirty_) return;
    sorted_.clear();
    sorted_.reserve(live_count_);
    for (std::size_t i = 0; i < live_.size(); ++i) {
      if (live_[i]) sorted_.push_back(Entry{code_of(pos_[i]), static_cast<EntityId>(i)});
    }
    std::sort(sorted_.begin(), sorted_.end(), [](const Entry& a, const Entry& b) {
      if (a.code != b.code) return a.code < b.code;
      return a.id < b.id;
    });
    sorted_code_.resize(sorted_.size());
    sorted_id_.resize(sorted_.size());
    sorted_pos_.resize(sorted_.size());
    for (std::size_t i = 0; i < sorted_.size(); ++i) {
      sorted_code_[i] = sorted_[i].code;
      sorted_id_[i] = sorted_[i].id;
      sorted_pos_[i] = pos_[sorted_[i].id];
    }
    dirty_ = false;
  }

  // One binary search per cell in the query box. The cells of a box are not
  // contiguous in Morton order, so this is the cost of the ordering: locality
  // inside a cell, a search to find each one.
  template <class F>
  void for_each_in_box(Vec3 c, float r, F&& f) const {
    const std::uint32_t x0 = axis_cell(c.x - r, bounds_.min.x);
    const std::uint32_t x1 = axis_cell(c.x + r, bounds_.min.x);
    const std::uint32_t y0 = axis_cell(c.y - r, bounds_.min.y);
    const std::uint32_t y1 = axis_cell(c.y + r, bounds_.min.y);
    const std::uint32_t z0 = axis_cell(c.z - r, bounds_.min.z);
    const std::uint32_t z1 = axis_cell(c.z + r, bounds_.min.z);
    for (std::uint32_t z = z0; z <= z1; ++z) {
      for (std::uint32_t y = y0; y <= y1; ++y) {
        for (std::uint32_t x = x0; x <= x1; ++x) {
          const std::uint64_t code = morton(x, y, z);
          const auto begin = std::lower_bound(sorted_code_.begin(), sorted_code_.end(), code);
          for (auto it = begin; it != sorted_code_.end() && *it == code; ++it) {
            f(static_cast<std::size_t>(it - sorted_code_.begin()));
          }
        }
      }
    }
  }

  Bounds bounds_;
  float cell_ = 1.0f;
  float inv_cell_ = 1.0f;
  std::vector<Vec3> pos_;
  std::vector<std::uint8_t> live_;
  std::size_t live_count_ = 0;
  mutable bool dirty_ = true;
  mutable std::vector<Entry> sorted_;
  mutable std::vector<std::uint64_t> sorted_code_;
  mutable std::vector<EntityId> sorted_id_;
  mutable std::vector<Vec3> sorted_pos_;
};

}  // namespace gds::candidates
