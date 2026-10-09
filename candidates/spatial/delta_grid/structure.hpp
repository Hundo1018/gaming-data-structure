#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "gds/spatial/types.hpp"
#include "spatial/uniform_grid/structure.hpp"

namespace gds::candidates {

using namespace gds::spatial;

// A grid that keeps its own history as data and keeps its index derived.
//
// Two parents, each contributing one half. From grid_undo_log, the record of
// what changed: the first change to an entity within a tick saves its previous
// liveness and position. From cell_sorted, the index: a counting sort into
// contiguous per-cell ranges with a directory. What is new is that neither
// choice those parents made for every tick is made here in advance.
//
// History. A closed frame is kept either as its records or, when the records
// pass an eighth of the id space, as a full pre-image of the arrays as they were
// when the tick began. Unwinding writes the positions and liveness arrays and
// nothing else: the index is never touched while history is replayed, which is
// the cost that made grid_undo_log lose when everything moves (a remove and an
// insert into the grid per record).
//
// Index. Every change, and every record unwound, lists its id as dirty. The
// first query after any change either rebuilds the whole index by counting sort
// or, when few ids are listed, moves just those into a delta layer: a held
// uniform_grid's intrusive per-cell lists, over the same cells, holding exactly
// the entities changed since the last rebuild. An entity in the delta is
// tombstoned in the base, so every live entity is in exactly one of the two and
// its stored position there is its current one.
//
// The held grid supplies the cell geometry, the lists and the authoritative
// positions and liveness; none of its mutators are used, and the lists hold
// only the delta.
class DeltaGrid {
 public:
  static const char* name() { return "delta_grid"; }
  static constexpr bool kNativeRewind = true;

  explicit DeltaGrid(const WorldConfig& cfg)
      : grid_(cfg),
        keeping_(cfg.history_ticks > 0),
        keep_(static_cast<std::size_t>(cfg.history_ticks) + 1) {
    const std::size_t n = grid_.pos_.size();
    start_.assign(grid_.head_.size() + 1, 0u);
    sorted_id_.assign(kPad, kNoEntity);
    sorted_x_.assign(kPad, 0.0f);
    sorted_y_.assign(kPad, 0.0f);
    sorted_z_.assign(kPad, 0.0f);
    slot_of_.assign(n, kNoSlot);
    listed_.assign(n, 0u);
    dirty_limit_ = static_cast<std::size_t>(kAlpha * static_cast<double>(n));
    dirty_.assign(dirty_limit_ + 1, kNoEntity);
    if (keeping_) {
      stamp_.assign(n, 0u);
      record_limit_ = static_cast<std::size_t>(kBeta * static_cast<double>(n));
    }
  }

  void insert(EntityId id, Vec3 p) {
    note(id);
    if (!grid_.live_[id]) ++grid_.live_count_;
    grid_.live_[id] = 1;
    grid_.pos_[id] = p;
    mark_dirty(id);
  }

  // Removing or moving a dead id changes nothing, so it is neither recorded
  // nor listed.
  void remove(EntityId id) {
    if (!grid_.live_[id]) return;
    note(id);
    grid_.live_[id] = 0;
    --grid_.live_count_;
    mark_dirty(id);
  }

  void move_by(EntityId id, Vec3 delta) {
    if (!grid_.live_[id]) return;
    note(id);
    grid_.pos_[id] = wrap_into(add(grid_.pos_[id], delta), grid_.bounds_);
    mark_dirty(id);
  }

  // These read the authoritative arrays and need no index.
  bool position_of(EntityId id, Vec3& out) const { return grid_.position_of(id, out); }
  std::size_t entity_count() const { return grid_.entity_count(); }

  // Whether a query walks the delta lists is decided once per query, as a
  // template argument, rather than tested in the per-cell loop: the k-nearest
  // gather visits many mostly empty cells, and a test there is reloaded after
  // every push into the gather vector, which measurably slowed it.
  std::uint64_t query_radius(Vec3 c, float r) const {
    sync();
    return delta_size_ > 0 ? radius_search<true>(c, r) : radius_search<false>(c, r);
  }

  std::uint64_t query_radius_of(EntityId self, float r) const {
    if (self >= grid_.live_.size() || !grid_.live_[self]) return 0;
    const Vec3 c = grid_.pos_[self];
    return query_radius(c, r) - RadiusDigest(c, r).term(self, c);
  }

  std::uint64_t query_knn(Vec3 c, std::uint32_t k) const {
    if (k == 0 || grid_.live_count_ == 0) return KnnDigest{}.value();
    sync();
    return delta_size_ > 0 ? knn_search<true>(c, k) : knn_search<false>(c, k);
  }

  // Closes the open frame into the ring. The ring grows by one slot per tick
  // closed until it holds history_ticks + 1, as the oracle's history does, so
  // a run shorter than the declared history never pays for frames it cannot
  // have; from then on it is circular. Slots keep their buffers, so once the
  // ring is full a tick allocates nothing: a pre-image swaps its arrays with
  // the slot's, and a records frame swaps its record vector.
  //
  // With no history kept, only the tick just closed and whether anything has
  // changed since are remembered, for rewind_to.
  void end_tick(std::uint64_t tick) {
    if (!keeping_) {
      closed_ = tick;
      has_closed_ = true;
      changed_ = false;
      return;
    }
    if (count_ == keep_) {
      oldest_ = (oldest_ + 1) % keep_;
      --count_;
    } else if (count_ == ring_.size()) {
      grow_ring();
    }
    Frame& f = frame(count_);
    f.tick = tick;
    f.full = open_.full;
    f.live_count = open_.live_count;
    if (open_.full) {
      std::swap(f.pos, open_.pos);
      std::swap(f.live, open_.live);
    } else {
      std::swap(f.rec, open_.rec);
    }
    ++count_;
    open_.rec.clear();
    open_.full = false;
    ++epoch_;
  }

  // Undoing a frame restores the state at the end of the frame before it, so
  // reaching tick T means undoing the open tick and then every frame newer than
  // T, newest first. A pre-image among them makes everything newer than it
  // irrelevant: the oldest one is copied and only the record frames older than
  // it are undone. The frame of T itself is never undone; it is kept so that T
  // is known to be inside the window, as the oracle's snapshot of T is.
  //
  // With no history kept, the one rewind the contract allows is to the tick
  // just closed, and it is honoured exactly when nothing has changed since,
  // because the state is then already the one asked for. That is the rewind
  // a workload of depth 0 issues, as the first operation of its tick.
  bool rewind_to(std::uint64_t tick) {
    if (!keeping_) return has_closed_ && tick == closed_ && !changed_;
    std::size_t target = count_;
    for (std::size_t j = count_; j-- > 0;) {
      if (frame(j).tick == tick) {
        target = j;
        break;
      }
    }
    if (target == count_) return false;
    std::size_t jump = count_;
    for (std::size_t j = target + 1; j < count_; ++j) {
      if (frame(j).full) {
        jump = j;
        break;
      }
    }
    if (jump < count_) {
      restore_image(frame(jump));
    } else if (open_.full) {
      restore_image(open_);
    } else {
      undo_records(open_);
    }
    for (std::size_t j = jump; j-- > target + 1;) undo_records(frame(j));
    count_ = target + 1;
    open_.rec.clear();
    open_.full = false;
    // A fresh epoch, so the stamps of the discarded open tick cannot stop the
    // continuing tick from recording its own first changes.
    ++epoch_;
    return true;
  }

  std::size_t reported_bytes() const {
    std::size_t b = grid_.reported_bytes() + start_.capacity() * sizeof(std::uint32_t) +
                    sorted_id_.capacity() * sizeof(EntityId) +
                    sorted_x_.capacity() * sizeof(float) + sorted_y_.capacity() * sizeof(float) +
                    sorted_z_.capacity() * sizeof(float) +
                    slot_of_.capacity() * sizeof(std::uint32_t) +
                    listed_.capacity() * sizeof(std::uint32_t) +
                    dirty_.capacity() * sizeof(EntityId) +
                    stamp_.capacity() * sizeof(std::uint32_t) + ring_.capacity() * sizeof(Frame) +
                    frame_bytes(open_);
    for (const Frame& f : ring_) b += frame_bytes(f);
    return b;
  }

 private:
  // See hypothesis.md for the cost argument behind both fractions.
  //
  // kAlpha: the first query after changes rebuilds when the listed ids exceed
  // this share of the live population, and otherwise moves them into the
  // delta.
  static constexpr double kAlpha = 0.5;
  // kBeta: a tick whose records exceed this share of the id space is kept as a
  // pre-image instead.
  static constexpr double kBeta = 0.125;

  // cell_sorted's block width and padding, for the same reason: GCC's -O2 cost
  // model vectorises the distance test only over a trip count that is a known
  // multiple of the vector width, and a block starting at the last entity reads
  // this far past it.
  static constexpr std::size_t kLanes = 8;
  static constexpr std::size_t kPad = kLanes - 1;
  static constexpr std::uint32_t kNoSlot = ~0u;

  // The query box is widened on each side by this share of |c| + r, and by
  // kBoxFloor, so that it holds every point the shared test accepts. Computing
  // c - r and c + r rounds them by up to one ulp of |c| + r, which can move a
  // bound onto a cell boundary past a point dist2 accepts at exactly r*r, and
  // dist2 itself, rounded, accepts points up to about three roundings of r
  // beyond r. A millionth is about seventeen float roundings, so it covers
  // both with margin, and it moves a bound across a cell boundary only when
  // the bound lies within a millionth of |c| + r of that boundary. The floor
  // covers a squared difference that underflows to zero, which happens below
  // 2^-75.
  static constexpr float kBoxSlack = 1e-6f;
  static constexpr float kBoxFloor = 1e-20f;

  // A record packs the previous liveness into the top bit of the id. Ids are
  // dense array indices, and an id space of 2^31 would need tens of gigabytes
  // of per-id arrays before this mattered.
  static constexpr std::uint32_t kLiveBit = 1u << 31;
  struct Record {
    std::uint32_t key;
    Vec3 pos;
  };

  // A frame is the history of one tick: its records, or, when `full`, the
  // positions and liveness as the tick began and the live count then. A slot
  // keeps the buffers of both kinds once it has used them.
  struct Frame {
    std::uint64_t tick = 0;
    bool full = false;
    std::size_t live_count = 0;
    std::vector<Record> rec;
    std::vector<Vec3> pos;
    std::vector<std::uint8_t> live;
  };

  // uniform_grid, held rather than inherited. Its lists are derived state
  // here, which the first query after a change repairs like the rest of the
  // index, and queries are const; holding the grid as a mutable member lets
  // them do that, where an inherited grid's members could not be. Friendship
  // opens its protected members to this class.
  struct Grid : UniformGrid {
    friend class DeltaGrid;
    using UniformGrid::UniformGrid;
  };

  static std::size_t frame_bytes(const Frame& f) {
    return f.rec.capacity() * sizeof(Record) + f.pos.capacity() * sizeof(Vec3) + f.live.capacity();
  }

  Frame& frame(std::size_t j) { return ring_[(oldest_ + j) % ring_.size()]; }

  // Called only while the ring is short of keep_ slots and every slot holds a
  // retained frame. The oldest frame is then still in slot 0, because the ring
  // turns only once it is full, so appending a slot keeps the frames in order.
  // Capacity is grown towards keep_ but never past it.
  void grow_ring() {
    if (ring_.size() == ring_.capacity()) {
      ring_.reserve(std::min(keep_, std::max<std::size_t>(4, 2 * ring_.size())));
    }
    ring_.emplace_back();
  }

  // Only the first change to an entity within a tick is recorded: the state to
  // restore is the one at the tick boundary. Once the open tick has become a
  // pre-image nothing further is recorded, because the pre-image already holds
  // every entity as the tick began. With no history kept, a change only
  // clears the way back to the tick just closed.
  void note(EntityId id) {
    if (!keeping_) {
      changed_ = true;
      return;
    }
    if (open_.full || stamp_[id] == epoch_) return;
    stamp_[id] = epoch_;
    open_.rec.push_back(Record{id | (grid_.live_[id] ? kLiveBit : 0u), grid_.pos_[id]});
    if (open_.rec.size() > record_limit_) take_pre_image();
  }

  // Records only grow within a tick, so a tick that will end with more than
  // the limit can be turned into a pre-image the moment it crosses it, and the
  // frame is the same one end_tick would have built. Copy the arrays as they
  // are now and apply the records backwards onto the copy.
  void take_pre_image() {
    open_.pos.assign(grid_.pos_.begin(), grid_.pos_.end());
    open_.live.assign(grid_.live_.begin(), grid_.live_.end());
    std::size_t lc = grid_.live_count_;
    for (std::size_t i = open_.rec.size(); i-- > 0;) {
      const Record& rr = open_.rec[i];
      const EntityId id = rr.key & ~kLiveBit;
      const std::uint8_t was = (rr.key & kLiveBit) ? 1 : 0;
      if (open_.live[id] && !was) --lc;
      if (!open_.live[id] && was) ++lc;
      open_.live[id] = was;
      open_.pos[id] = rr.pos;
    }
    open_.live_count = lc;
    open_.rec.clear();
    open_.full = true;
  }

  void undo_records(const Frame& f) {
    for (std::size_t i = f.rec.size(); i-- > 0;) {
      const Record& rr = f.rec[i];
      const EntityId id = rr.key & ~kLiveBit;
      const std::uint8_t was = (rr.key & kLiveBit) ? 1 : 0;
      if (grid_.live_[id] && !was) --grid_.live_count_;
      if (!grid_.live_[id] && was) ++grid_.live_count_;
      grid_.live_[id] = was;
      grid_.pos_[id] = rr.pos;
      mark_dirty(id);
    }
  }

  // Copying a pre-image changes an unknown share of the world, so it lists
  // nothing and asks for a rebuild.
  void restore_image(const Frame& f) {
    std::copy(f.pos.begin(), f.pos.end(), grid_.pos_.begin());
    std::copy(f.live.begin(), f.live.end(), grid_.live_.begin());
    grid_.live_count_ = f.live_count;
    full_ = true;
  }

  // An id is listed once per pass between two queries; `listed_` holds the
  // pass it was last listed in, so starting a new pass clears every mark at
  // once. Past the limit, the list exceeds kAlpha of every id slot, and so of
  // the live population whatever happens before the next query: that query
  // will rebuild, and listing more ids would be wasted. The list therefore
  // never holds more than limit + 1 ids, and is a fixed array of that many with
  // a count, which keeps this function small enough to be inlined into every
  // mutation.
  void mark_dirty(EntityId id) {
    if (full_ || listed_[id] == pass_) return;
    listed_[id] = pass_;
    dirty_[dirty_count_++] = id;
    if (dirty_count_ > dirty_limit_) full_ = true;
  }

  // The index is derived state, repaired by the first query after a change.
  // The test is kept apart from the repair, so that a query whose index is
  // current pays only the test.
  void sync() const {
    if (full_ || dirty_count_ != 0) repair();
  }

  void repair() const {
    if (full_ ||
        static_cast<double>(dirty_count_) > kAlpha * static_cast<double>(grid_.live_count_)) {
      rebuild();
    } else {
      apply_dirty();
    }
    dirty_count_ = 0;
    full_ = false;
    ++pass_;
  }

  // Each listed id leaves the base, if it was there, by tombstoning its slot,
  // and is then linked into, moved within, or unlinked from the delta lists
  // according to its current state.
  void apply_dirty() const {
    for (std::size_t q = 0; q < dirty_count_; ++q) {
      const EntityId id = dirty_[q];
      const std::uint32_t s = slot_of_[id];
      if (s != kNoSlot) {
        sorted_x_[s] = std::numeric_limits<float>::quiet_NaN();
        sorted_id_[s] = kNoEntity;
        slot_of_[id] = kNoSlot;
      }
      const std::uint32_t was = grid_.cell_of_[id];
      if (grid_.live_[id]) {
        const std::uint32_t c = grid_.cell_index(grid_.pos_[id]);
        if (was == c) continue;
        if (was == Grid::kNoCell) ++delta_size_;
        else grid_.unlink(id);
        grid_.link(id, c);
      } else if (was != Grid::kNoCell) {
        grid_.unlink(id);
        --delta_size_;
      }
    }
  }

  // cell_sorted's counting sort, with `slot_of_` as both the per-id cell
  // scratch of the counting pass and, after the scatter, the slot each id
  // landed in, which is what tombstoning needs. A rebuild empties the delta
  // first; each non-empty list has a member whose cell names its head.
  void rebuild() const {
    const std::vector<std::uint8_t>& live = grid_.live_;
    const std::vector<Vec3>& pos = grid_.pos_;
    const std::size_t n = live.size();
    if (delta_size_ > 0) {
      for (std::size_t id = 0; id < n; ++id) {
        const std::uint32_t c = grid_.cell_of_[id];
        if (c == Grid::kNoCell) continue;
        grid_.head_[c] = kNoEntity;
        grid_.cell_of_[id] = Grid::kNoCell;
      }
      delta_size_ = 0;
    }
    const std::size_t cells = start_.size() - 1;
    std::fill(start_.begin(), start_.end(), 0u);
    for (std::size_t id = 0; id < n; ++id) {
      if (!live[id]) {
        slot_of_[id] = kNoSlot;
        continue;
      }
      const std::uint32_t c = grid_.cell_index(pos[id]);
      slot_of_[id] = c;
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
      if (!live[id]) continue;
      const std::uint32_t slot = --start_[slot_of_[id]];
      sorted_id_[slot] = static_cast<EntityId>(id);
      sorted_x_[slot] = pos[id].x;
      sorted_y_[slot] = pos[id].y;
      sorted_z_[slot] = pos[id].z;
      slot_of_[id] = slot;
    }
  }

  template <bool kWalkDelta>
  std::uint64_t radius_search(Vec3 c, float r) const {
    RadiusDigest d(c, r);
    const float r2 = r * r;
    const std::vector<Vec3>& pos = grid_.pos_;
    for_each_cell_in_box(c, r, [&](std::size_t cell) {
      const std::size_t b = start_[cell];
      const std::size_t e = start_[cell + 1];
      if (b < e) scan_run(b, e, c, r2, d);
      if constexpr (kWalkDelta) {
        for (EntityId id = grid_.head_[cell]; id != kNoEntity; id = grid_.next_[id]) {
          if (dist2(pos[id], c) <= r2) d.hit(id, pos[id]);
        }
      }
    });
    return d.value();
  }

  // uniform_grid's box doubling; candidates are gathered from each cell's base
  // range and its delta list. A tombstone is skipped by its id rather than left
  // to the distance: its NaN would compare equal to every distance under
  // `nearer` and break the ordering the partial sort needs.
  //
  // The search stops early, before the kth neighbour is inside the box, only
  // once the box has passed twice the largest extent and spans every cell, so
  // that everything has been gathered. For a centre inside the world the first
  // implies the second and this is uniform_grid's stop exactly; for a centre
  // outside it, a box that wide can still miss the far side of the world. A
  // half-width that has overflowed to infinity ends the search whatever the
  // box, so the doubling always ends.
  template <bool kWalkDelta>
  std::uint64_t knn_search(Vec3 c, std::uint32_t k) const {
    KnnDigest d;
    const float limit = grid_.bounds_.largest_extent() * 2.0f;
    const std::vector<Vec3>& pos = grid_.pos_;
    std::vector<Neighbour> found;
    for (float r = grid_.cell_; ; r *= 2.0f) {
      found.clear();
      const bool whole = for_each_cell_in_box(c, r, [&](std::size_t cell) {
        for (std::size_t s = start_[cell], e = start_[cell + 1]; s < e; ++s) {
          const EntityId id = sorted_id_[s];
          if (id == kNoEntity) continue;
          found.push_back(Neighbour{dist2(Vec3{sorted_x_[s], sorted_y_[s], sorted_z_[s]}, c), id});
        }
        if constexpr (kWalkDelta) {
          for (EntityId id = grid_.head_[cell]; id != kNoEntity; id = grid_.next_[id]) {
            found.push_back(Neighbour{dist2(pos[id], c), id});
          }
        }
      });
      const bool done = (r > limit && whole) || !(r < std::numeric_limits<float>::infinity());
      const std::size_t want = std::min<std::size_t>(k, found.size());
      if (want > 0) {
        std::partial_sort(found.begin(), found.begin() + static_cast<std::ptrdiff_t>(want),
                          found.end(), nearer);
      }
      if (found.size() >= k || done) {
        if (want == 0 || found[want - 1].d2 <= r * r || done) {
          for (std::size_t i = 0; i < want; ++i) d.push(found[i].id, pos[found[i].id]);
          return d.value();
        }
      }
    }
  }

  // One bound of the query box on one axis: the cell holding v, clamped to the
  // axis. The clamp is made on the float before it is converted, so an
  // infinite bound lands on an end of the axis rather than converting to an
  // undefined integer. A comparison with NaN is false, so the order of the two
  // clamps decides where a NaN bound lands: the lower bound clamps from below
  // first and lands on the first cell, the upper from above first and lands on
  // the last, and the box widens either way. For a finite bound this is
  // uniform_grid's axis_index, which is monotone in v, so a lower bound below a
  // point's coordinate never names a cell above the point's, nor an upper bound
  // above it one below.
  std::uint32_t box_lower(float v, float lo, std::uint32_t n) const {
    float f = std::floor((v - lo) * grid_.inv_cell_);
    const float last = static_cast<float>(n - 1);
    f = f > 0.0f ? f : 0.0f;
    f = f < last ? f : last;
    return static_cast<std::uint32_t>(f);
  }

  std::uint32_t box_upper(float v, float lo, std::uint32_t n) const {
    float f = std::floor((v - lo) * grid_.inv_cell_);
    const float last = static_cast<float>(n - 1);
    f = f < last ? f : last;
    f = f > 0.0f ? f : 0.0f;
    return static_cast<std::uint32_t>(f);
  }

  // Every cell overlapping the box of half-width r around c, in uniform_grid's
  // order, with the box widened as kBoxSlack describes so that it is
  // conservative for the shared test. The box is not clipped to a sphere, so
  // the caller does the exact test. Returns whether the box spans every cell.
  //
  // Kept out of line. Inlined, the walk shares its registers with the digest
  // and the k-nearest bookkeeping of the query around it, and the queries of a
  // tick of s01 took about 8 us longer; the version before the review, whose
  // box was computed inside the walk, was compiled out of line by the
  // compiler's own choice.
  template <class F>
  [[gnu::noinline]] bool for_each_cell_in_box(Vec3 c, float r, F&& f) const {
    const Bounds& b = grid_.bounds_;
    const std::uint32_t nx = grid_.nx_, ny = grid_.ny_, nz = grid_.nz_;
    const float mx = (std::fabs(c.x) + r) * kBoxSlack + kBoxFloor;
    const float my = (std::fabs(c.y) + r) * kBoxSlack + kBoxFloor;
    const float mz = (std::fabs(c.z) + r) * kBoxSlack + kBoxFloor;
    const std::uint32_t x0 = box_lower((c.x - r) - mx, b.min.x, nx);
    const std::uint32_t x1 = box_upper((c.x + r) + mx, b.min.x, nx);
    const std::uint32_t y0 = box_lower((c.y - r) - my, b.min.y, ny);
    const std::uint32_t y1 = box_upper((c.y + r) + my, b.min.y, ny);
    const std::uint32_t z0 = box_lower((c.z - r) - mz, b.min.z, nz);
    const std::uint32_t z1 = box_upper((c.z + r) + mz, b.min.z, nz);
    for (std::uint32_t z = z0; z <= z1; ++z) {
      for (std::uint32_t y = y0; y <= y1; ++y) {
        const std::size_t row = (static_cast<std::size_t>(z) * ny + y) * nx;
        for (std::uint32_t x = x0; x <= x1; ++x) f(row + x);
      }
    }
    return x0 == 0 && y0 == 0 && z0 == 0 && x1 + 1 == nx && y1 + 1 == ny && z1 + 1 == nz;
  }

  // cell_sorted's exact test over one base range, kLanes slots at a time. A
  // tombstone holds NaN in x, so its distance is NaN and `<=` rejects it in
  // its lane like any point outside the radius; lanes past the range are
  // masked by index.
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

  // The geometry, the authoritative arrays and the delta lists.
  mutable Grid grid_;

  // Base: cell_sorted's directory and cell-ordered arrays, plus where each id
  // sits in them.
  mutable std::vector<std::uint32_t> start_;
  mutable std::vector<EntityId> sorted_id_;
  mutable std::vector<float> sorted_x_;
  mutable std::vector<float> sorted_y_;
  mutable std::vector<float> sorted_z_;
  mutable std::vector<std::uint32_t> slot_of_;
  // Delta: the held grid's lists; this counts their members.
  mutable std::size_t delta_size_ = 0;

  // Ids changed since the last query, the first dirty_count_ entries of
  // dirty_, and whether the next query must rebuild regardless.
  std::vector<EntityId> dirty_;
  mutable std::size_t dirty_count_ = 0;
  std::vector<std::uint32_t> listed_;
  mutable std::uint32_t pass_ = 1;
  std::size_t dirty_limit_ = 0;
  mutable bool full_ = false;

  // History: up to keep_ closed frames in a ring starting at oldest_, and the
  // open one. Without history, the tick last closed and whether anything has
  // changed since.
  bool keeping_ = false;
  std::size_t keep_ = 1;
  std::vector<std::uint32_t> stamp_;
  std::uint32_t epoch_ = 1;
  std::size_t record_limit_ = 0;
  Frame open_;
  std::vector<Frame> ring_;
  std::size_t oldest_ = 0;
  std::size_t count_ = 0;
  std::uint64_t closed_ = 0;
  bool has_closed_ = false;
  bool changed_ = false;
};

}  // namespace gds::candidates
