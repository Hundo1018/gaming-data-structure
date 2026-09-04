// Rewind for a structure that does not keep history: snapshot the world every
// tick, and on rewind clear the index and re-insert everything.
//
// This is not a strawman. It is what an engine does today: the game state is
// the authority, the spatial index is derived, and rollback restores the state
// and rebuilds the index. Any candidate declaring kNativeRewind = false is
// measured inside this wrapper on temporal workloads, so "keep history inside
// the index" has something real to beat.
//
// The wrapper's own memory is charged to the candidate, because it is part of
// what that approach costs.
#pragma once

#include <cstddef>
#include <vector>

#include "gds/spatial/types.hpp"

namespace gds::spatial {

template <class S>
class RebuildRewind {
 public:
  static const char* name() { return S::name(); }
  static constexpr bool kNativeRewind = true;
  static constexpr const char* kStrategy = "snapshot_rebuild";

  explicit RebuildRewind(const WorldConfig& cfg) : cfg_(cfg), inner_(cfg) {
    pos_.assign(cfg.max_entity_id + 1, Vec3{0, 0, 0});
    live_.assign(cfg.max_entity_id + 1, 0);
  }

  void insert(EntityId id, Vec3 p) {
    if (!live_[id]) ++live_count_;
    live_[id] = 1;
    pos_[id] = p;
    inner_.insert(id, p);
  }

  void remove(EntityId id) {
    if (live_[id]) --live_count_;
    live_[id] = 0;
    inner_.remove(id);
  }

  void move_by(EntityId id, Vec3 delta) {
    if (id < live_.size() && live_[id]) pos_[id] = wrap_into(add(pos_[id], delta), cfg_.bounds);
    inner_.move_by(id, delta);
  }

  bool position_of(EntityId id, Vec3& out) const { return inner_.position_of(id, out); }
  std::uint64_t query_radius(Vec3 c, float r) const { return inner_.query_radius(c, r); }
  std::uint64_t query_radius_of(EntityId id, float r) const {
    return inner_.query_radius_of(id, r);
  }
  std::uint64_t query_knn(Vec3 c, std::uint32_t k) const { return inner_.query_knn(c, k); }

  void end_tick(std::uint64_t tick) {
    inner_.end_tick(tick);
    if (cfg_.history_ticks == 0) return;
    frames_.push_back(Frame{tick, pos_, live_, live_count_});
    const std::size_t keep = static_cast<std::size_t>(cfg_.history_ticks) + 1;
    while (frames_.size() > keep) frames_.erase(frames_.begin());
  }

  bool rewind_to(std::uint64_t tick) {
    for (std::size_t i = frames_.size(); i-- > 0;) {
      if (frames_[i].tick != tick) continue;
      pos_ = frames_[i].pos;
      live_ = frames_[i].live;
      live_count_ = frames_[i].live_count;
      frames_.resize(i + 1);
      inner_ = S(cfg_);
      for (std::size_t id = 0; id < live_.size(); ++id) {
        if (live_[id]) inner_.insert(static_cast<EntityId>(id), pos_[id]);
      }
      return true;
    }
    return false;
  }

  std::size_t entity_count() const { return inner_.entity_count(); }

  std::size_t reported_bytes() const {
    std::size_t b = inner_.reported_bytes() + pos_.capacity() * sizeof(Vec3) + live_.capacity();
    for (const Frame& f : frames_) b += f.pos.capacity() * sizeof(Vec3) + f.live.capacity();
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
  S inner_;
  std::vector<Vec3> pos_;
  std::vector<std::uint8_t> live_;
  std::size_t live_count_ = 0;
  std::vector<Frame> frames_;
};

}  // namespace gds::spatial
