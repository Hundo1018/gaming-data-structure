#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "gds/spatial/types.hpp"

namespace gds::candidates {

using namespace gds::spatial;

// The same cell decomposition as a uniform grid, but cells live in an open
// addressed hash table instead of a dense array. Memory follows the number of
// cells that actually hold something rather than the size of the world, and the
// world needs no bounds at all: a coordinate anywhere hashes to a cell.
//
// The price is one hash lookup per cell a query visits, where the dense grid
// does one array index. A query touching twenty-seven cells pays twenty-seven
// probes into a table that no part of the walk has warmed.
class SpatialHash {
 public:
  static const char* name() { return "spatial_hash"; }
  static constexpr bool kNativeRewind = false;

  explicit SpatialHash(const WorldConfig& cfg) : bounds_(cfg.bounds) {
    const float floor_cell = std::max(bounds_.largest_extent() / 256.0f, 1e-4f);
    cell_ = std::max(cfg.typical_query_radius, floor_cell);
    inv_cell_ = 1.0f / cell_;
    const std::size_t n = cfg.max_entity_id + 1;
    next_.assign(n, kNoEntity);
    prev_.assign(n, kNoEntity);
    key_of_.assign(n, kNoKey);
    pos_.assign(n, Vec3{0, 0, 0});
    live_.assign(n, 0);
    rehash(1024);
  }

  void insert(EntityId id, Vec3 p) {
    if (live_[id]) unlink(id);
    else ++live_count_;
    live_[id] = 1;
    pos_[id] = p;
    link(id, key_of_point(p));
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
    const std::uint64_t nk = key_of_point(np);
    if (nk != key_of_[id]) {
      unlink(id);
      link(id, nk);
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
    return query_radius(pos_[self], r) - digest_hit(self, pos_[self]);
  }

  std::uint64_t query_knn(Vec3 c, std::uint32_t k) const {
    KnnDigest d;
    if (k == 0 || live_count_ == 0) return d.value();
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
    return table_.capacity() * sizeof(Slot) + next_.capacity() * sizeof(EntityId) +
           prev_.capacity() * sizeof(EntityId) + key_of_.capacity() * sizeof(std::uint64_t) +
           pos_.capacity() * sizeof(Vec3) + live_.capacity();
  }

 private:
  static constexpr std::uint64_t kNoKey = ~std::uint64_t(0);

  struct Slot {
    std::uint64_t key = kNoKey;
    EntityId head = kNoEntity;
  };

  std::int32_t axis_cell(float v, float lo) const {
    return static_cast<std::int32_t>(std::floor((v - lo) * inv_cell_));
  }

  // Three signed cell coordinates biased into 21 unsigned bits each. The world
  // is finite, so 21 bits per axis cannot collide for any position the workload
  // can produce.
  static std::uint64_t pack(std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::uint64_t ux = static_cast<std::uint64_t>(x + (1 << 20)) & 0x1FFFFF;
    const std::uint64_t uy = static_cast<std::uint64_t>(y + (1 << 20)) & 0x1FFFFF;
    const std::uint64_t uz = static_cast<std::uint64_t>(z + (1 << 20)) & 0x1FFFFF;
    return ux | (uy << 21) | (uz << 42);
  }

  std::uint64_t key_of_point(Vec3 p) const {
    return pack(axis_cell(p.x, bounds_.min.x), axis_cell(p.y, bounds_.min.y),
                axis_cell(p.z, bounds_.min.z));
  }

  std::size_t find_slot(std::uint64_t key) const {
    std::size_t i = static_cast<std::size_t>(splitmix64(key)) & mask_;
    while (table_[i].key != kNoKey && table_[i].key != key) i = (i + 1) & mask_;
    return i;
  }

  void rehash(std::size_t capacity) {
    std::vector<Slot> old;
    old.swap(table_);
    table_.assign(capacity, Slot{});
    mask_ = capacity - 1;
    used_ = 0;
    for (const Slot& s : old) {
      if (s.key == kNoKey) continue;
      table_[find_slot(s.key)] = s;
      ++used_;
    }
  }

  void link(EntityId id, std::uint64_t key) {
    if ((used_ + 1) * 10 >= table_.size() * 7) rehash(table_.size() * 2);
    const std::size_t i = find_slot(key);
    if (table_[i].key == kNoKey) {
      table_[i].key = key;
      table_[i].head = kNoEntity;
      ++used_;
    }
    key_of_[id] = key;
    const EntityId h = table_[i].head;
    next_[id] = h;
    prev_[id] = kNoEntity;
    if (h != kNoEntity) prev_[h] = id;
    table_[i].head = id;
  }

  // A cell that empties keeps its slot. Reclaiming it would need either
  // tombstones or a rehash on every removal, and the cell is almost always
  // reoccupied within a few ticks by something moving through it.
  void unlink(EntityId id) {
    const std::uint64_t key = key_of_[id];
    if (key == kNoKey) return;
    const std::size_t i = find_slot(key);
    const EntityId p = prev_[id];
    const EntityId n = next_[id];
    if (p != kNoEntity) next_[p] = n;
    else if (table_[i].key == key) table_[i].head = n;
    if (n != kNoEntity) prev_[n] = p;
    key_of_[id] = kNoKey;
    next_[id] = kNoEntity;
    prev_[id] = kNoEntity;
  }

  template <class F>
  void for_each_in_box(Vec3 c, float r, F&& f) const {
    const std::int32_t x0 = axis_cell(c.x - r, bounds_.min.x);
    const std::int32_t x1 = axis_cell(c.x + r, bounds_.min.x);
    const std::int32_t y0 = axis_cell(c.y - r, bounds_.min.y);
    const std::int32_t y1 = axis_cell(c.y + r, bounds_.min.y);
    const std::int32_t z0 = axis_cell(c.z - r, bounds_.min.z);
    const std::int32_t z1 = axis_cell(c.z + r, bounds_.min.z);
    for (std::int32_t z = z0; z <= z1; ++z) {
      for (std::int32_t y = y0; y <= y1; ++y) {
        for (std::int32_t x = x0; x <= x1; ++x) {
          const std::size_t i = find_slot(pack(x, y, z));
          if (table_[i].key == kNoKey) continue;
          for (EntityId id = table_[i].head; id != kNoEntity; id = next_[id]) f(id);
        }
      }
    }
  }

  Bounds bounds_;
  float cell_ = 1.0f;
  float inv_cell_ = 1.0f;
  std::vector<Slot> table_;
  std::size_t mask_ = 0;
  std::size_t used_ = 0;
  std::vector<EntityId> next_;
  std::vector<EntityId> prev_;
  std::vector<std::uint64_t> key_of_;
  std::vector<Vec3> pos_;
  std::vector<std::uint8_t> live_;
  std::size_t live_count_ = 0;
};

}  // namespace gds::candidates
