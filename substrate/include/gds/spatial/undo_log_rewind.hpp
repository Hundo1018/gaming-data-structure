// Rewind by remembering only what changed.
//
// The other strategy, RebuildRewind, copies the whole world every tick and
// rebuilds the index on rewind. This one records the previous position of each
// entity the first time it changes within a tick, and rewinds by replaying
// those records backwards.
//
// Which wins is not obvious and is the point of having both: the log is small
// when few entities move and large when they all do, and replaying it costs two
// index operations per record against the rebuild's one insert per entity. A
// workload where every entity moves every tick should favour the rebuild; one
// where a handful move should favour the log.
#pragma once

#include <cstddef>
#include <vector>

#include "gds/spatial/types.hpp"

namespace gds::spatial {

template <class S>
class UndoLogRewind {
 public:
  static const char* name() { return S::name(); }
  static constexpr bool kNativeRewind = true;
  static constexpr const char* kStrategy = "undo_log";

  explicit UndoLogRewind(const WorldConfig& cfg)
      : cfg_(cfg), inner_(cfg), keeping_(cfg.history_ticks > 0) {
    if (!keeping_) return;  // the mirror exists only to feed the log
    pos_.assign(cfg.max_entity_id + 1, Vec3{0, 0, 0});
    live_.assign(cfg.max_entity_id + 1, 0);
    // Epochs never repeat, not even after a rewind reissues a tick number, so a
    // stale stamp can never be mistaken for one from the current tick.
    stamp_.assign(cfg.max_entity_id + 1, 0);
  }

  void insert(EntityId id, Vec3 p) {
    if (keeping_) {
      note(id);
      if (!live_[id]) ++live_count_;
      live_[id] = 1;
      pos_[id] = p;
    }
    inner_.insert(id, p);
  }

  void remove(EntityId id) {
    if (keeping_) {
      note(id);
      if (live_[id]) --live_count_;
      live_[id] = 0;
    }
    inner_.remove(id);
  }

  void move_by(EntityId id, Vec3 delta) {
    if (keeping_ && id < live_.size() && live_[id]) {
      note(id);
      pos_[id] = wrap_into(add(pos_[id], delta), cfg_.bounds);
    }
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
    open_.tick = tick;
    frames_.push_back(std::move(open_));
    open_ = Frame{};
    ++epoch_;
    const std::size_t keep = static_cast<std::size_t>(cfg_.history_ticks) + 1;
    while (frames_.size() > keep) frames_.erase(frames_.begin());
  }

  bool rewind_to(std::uint64_t tick) {
    std::size_t target = frames_.size();
    for (std::size_t i = frames_.size(); i-- > 0;) {
      if (frames_[i].tick == tick) {
        target = i;
        break;
      }
    }
    if (target == frames_.size()) return false;
    // Anything changed since the checkpoint but not yet closed into a frame
    // must be undone first, newest change first.
    undo(open_);
    open_ = Frame{};
    for (std::size_t i = frames_.size(); i-- > target + 1;) {
      undo(frames_[i]);
    }
    frames_.resize(target + 1);
    ++epoch_;
    return true;
  }

  std::size_t entity_count() const { return inner_.entity_count(); }

  std::size_t reported_bytes() const {
    std::size_t b = inner_.reported_bytes() + pos_.capacity() * sizeof(Vec3) + live_.capacity() +
                    stamp_.capacity() * sizeof(std::uint64_t) +
                    open_.records.capacity() * sizeof(Record);
    for (const Frame& f : frames_) b += f.records.capacity() * sizeof(Record);
    return b;
  }

 private:
  struct Record {
    EntityId id;
    std::uint8_t was_live;
    Vec3 pos;
  };
  struct Frame {
    std::uint64_t tick = 0;
    std::vector<Record> records;
  };

  // Only the first change to an entity within a tick is worth recording: the
  // state being restored is the one at the tick boundary, not each step of it.
  void note(EntityId id) {
    if (cfg_.history_ticks == 0 || id >= stamp_.size()) return;
    if (stamp_[id] == epoch_ + 1) return;
    stamp_[id] = epoch_ + 1;
    open_.records.push_back(Record{id, live_[id], pos_[id]});
  }

  void undo(const Frame& f) {
    for (std::size_t i = f.records.size(); i-- > 0;) {
      const Record& rec = f.records[i];
      const bool now_live = live_[rec.id] != 0;
      if (now_live) inner_.remove(rec.id);
      if (rec.was_live) inner_.insert(rec.id, rec.pos);
      if (now_live && !rec.was_live) --live_count_;
      if (!now_live && rec.was_live) ++live_count_;
      live_[rec.id] = rec.was_live;
      pos_[rec.id] = rec.pos;
    }
  }

  WorldConfig cfg_;
  S inner_;
  bool keeping_ = false;
  std::vector<Vec3> pos_;
  std::vector<std::uint8_t> live_;
  std::vector<std::uint64_t> stamp_;
  std::size_t live_count_ = 0;
  std::uint64_t epoch_ = 0;
  Frame open_;
  std::vector<Frame> frames_;
};

}  // namespace gds::spatial
