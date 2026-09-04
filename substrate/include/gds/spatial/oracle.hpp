// The spatial oracle.
//
// Linear scan, full-copy history, no cleverness anywhere. It is written to be
// checked by reading it. Deliberately shares no code with RebuildRewind: if the
// oracle's history and the wrapper's history were the same implementation, a
// bug in it would cancel out and neither would be verified.
#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "gds/spatial/types.hpp"

namespace gds::spatial {

class BruteForceOracle {
 public:
  static const char* name() { return "brute_force"; }
  static constexpr bool kNativeRewind = true;

  explicit BruteForceOracle(const WorldConfig& cfg) : cfg_(cfg) {
    pos_.assign(cfg.max_entity_id + 1, Vec3{0, 0, 0});
    live_.assign(cfg.max_entity_id + 1, 0);
  }

  void insert(EntityId id, Vec3 p) {
    if (!live_[id]) ++live_count_;
    live_[id] = 1;
    pos_[id] = p;
  }

  void remove(EntityId id) {
    if (live_[id]) --live_count_;
    live_[id] = 0;
  }

  void move_by(EntityId id, Vec3 delta) {
    if (!live_[id]) return;
    pos_[id] = wrap_into(add(pos_[id], delta), cfg_.bounds);
  }

  bool position_of(EntityId id, Vec3& out) const {
    if (id >= live_.size() || !live_[id]) return false;
    out = pos_[id];
    return true;
  }

  std::uint64_t query_radius(Vec3 c, float r) const {
    const float r2 = r * r;
    RadiusDigest d;
    for (std::size_t i = 0; i < live_.size(); ++i) {
      if (live_[i] && dist2(pos_[i], c) <= r2) d.hit(static_cast<EntityId>(i), pos_[i]);
    }
    return d.value();
  }

  std::uint64_t query_radius_of(EntityId self, float r) const {
    if (self >= live_.size() || !live_[self]) return 0;
    const Vec3 c = pos_[self];
    const float r2 = r * r;
    RadiusDigest d;
    for (std::size_t i = 0; i < live_.size(); ++i) {
      if (i == self) continue;
      if (live_[i] && dist2(pos_[i], c) <= r2) d.hit(static_cast<EntityId>(i), pos_[i]);
    }
    return d.value();
  }

  std::uint64_t query_knn(Vec3 c, std::uint32_t k) const {
    std::vector<Neighbour> all;
    all.reserve(live_count_);
    for (std::size_t i = 0; i < live_.size(); ++i) {
      if (live_[i]) all.push_back(Neighbour{dist2(pos_[i], c), static_cast<EntityId>(i)});
    }
    const std::size_t want = std::min<std::size_t>(k, all.size());
    std::partial_sort(all.begin(), all.begin() + static_cast<std::ptrdiff_t>(want), all.end(),
                      nearer);
    KnnDigest d;
    for (std::size_t i = 0; i < want; ++i) d.push(all[i].id, pos_[all[i].id]);
    return d.value();
  }

  void end_tick(std::uint64_t tick) {
    history_.push_back(Frame{tick, pos_, live_, live_count_});
    const std::size_t keep = static_cast<std::size_t>(cfg_.history_ticks) + 1;
    while (history_.size() > keep) history_.erase(history_.begin());
  }

  bool rewind_to(std::uint64_t tick) {
    for (std::size_t i = history_.size(); i-- > 0;) {
      if (history_[i].tick == tick) {
        pos_ = history_[i].pos;
        live_ = history_[i].live;
        live_count_ = history_[i].live_count;
        history_.resize(i + 1);
        return true;
      }
    }
    return false;
  }

  std::size_t entity_count() const { return live_count_; }

  std::size_t reported_bytes() const {
    std::size_t b = pos_.capacity() * sizeof(Vec3) + live_.capacity();
    for (const Frame& f : history_) {
      b += f.pos.capacity() * sizeof(Vec3) + f.live.capacity();
    }
    return b;
  }

 private:
  struct Frame {
    std::uint64_t tick;
    std::vector<Vec3> pos;
    std::vector<std::uint8_t> live;
    std::size_t live_count;
  };

  WorldConfig cfg_;
  std::vector<Vec3> pos_;
  std::vector<std::uint8_t> live_;
  std::size_t live_count_ = 0;
  std::vector<Frame> history_;
};

}  // namespace gds::spatial
