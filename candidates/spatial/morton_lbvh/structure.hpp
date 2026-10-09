#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "gds/spatial/types.hpp"

namespace gds::candidates {

using namespace gds::spatial;

// morton_sorted's order, with its lookup replaced by a hierarchy of bounding
// boxes built over that order. Positions are written into flat arrays as they
// change, and the searchable form is thrown away and rebuilt by the first query
// that follows any change, as in the parent.
//
// The rebuild sorts live entities by a 30-bit Morton code with a radix sort,
// gathers ids and positions into that order as four parallel arrays, and
// groups the result bottom-up: a leaf is a run of kLeaf consecutive entities,
// an internal node is a run of kFan consecutive nodes of the level below, and
// every node keeps the box of what lies beneath it. Morton order is spatially
// coherent, so a consecutive run is a compact region, and a run of sixteen
// entities is small where the world is crowded and large where it is empty.
// The hierarchy adapts to density with nothing allocated per unit of world
// volume, where a grid's cell array is sized by the world.
//
// The code quantises each axis to 1024 steps of its own extent, so its cells,
// and on average the regions a run covers, have the proportions of the world:
// in a world four times wider than it is tall they are four times longer in x
// and y than in z.
//
// Nothing about a node is stored except its box. Its entity range is implied
// by its position: node j of level l covers entities [j*kLeaf*kFan^l,
// (j+1)*kLeaf*kFan^l), clipped to the population, and its children are nodes
// [j*kFan, (j+1)*kFan) of level l-1.
//
// The parent's members are private, so nothing of it could be reused by
// including it; its code is also a different one, 21 bits of cell index per
// axis where this one is 10 bits of position per axis.
class MortonLbvh {
 public:
  static const char* name() { return "morton_lbvh"; }
  static constexpr bool kNativeRewind = false;

  // A leaf is sixteen entities because sixteen floats are one 64-byte cache
  // line: a leaf is one whole line of each of the x, y, z and id arrays, and
  // the distance loop over it is two 256-bit vectors per axis with no
  // remainder.
  static constexpr std::uint32_t kLeaf = 16;
  // A node has eight children because eight floats are one 256-bit vector, so
  // the boxes of all of a node's children are tested by one pass of vector
  // arithmetic over six rows of eight floats, one 192-byte group of three
  // cache lines.
  static constexpr std::uint32_t kFan = 8;

  explicit MortonLbvh(const WorldConfig& cfg) : bounds_(cfg.bounds) {
    // 1024 steps over each axis's own extent of the world bounds, so every
    // axis contributes all ten of its bits to the code. A code cell therefore
    // has the world's proportions rather than a cube's. An axis with no extent
    // gets no scale and every entity lands on its step 0.
    scale_x_ = axis_scale(bounds_.extent_x());
    scale_y_ = axis_scale(bounds_.extent_y());
    scale_z_ = axis_scale(bounds_.extent_z());
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
    RadiusDigest d(c, r);
    collect_radius(c, r, d);
    return d.value();
  }

  std::uint64_t query_radius_of(EntityId self, float r) const {
    if (self >= live_.size() || !live_[self]) return 0;
    const Vec3 p = pos_[self];
    RadiusDigest d(p, r);
    collect_radius(p, r, d);
    // The scan accepted the centre exactly when this same test passes, and the
    // digest is a sum, so removing it is a subtraction rather than a branch on
    // every hit.
    if (dist2(p, p) <= r * r) return d.value() - d.term(self, p);
    return d.value();
  }

  // Best-first: nodes are expanded in order of their distance from c, and the
  // search stops when the nearest unexpanded node is strictly farther than the
  // kth best found. A node at exactly that distance can still hold an entity
  // that ties on distance and wins on id, so it is expanded.
  std::uint64_t query_knn(Vec3 c, std::uint32_t k) const {
    rebuild_if_needed();
    KnnDigest d;
    if (k == 0 || live_count_ == 0) return d.value();
    const std::size_t want = std::min<std::size_t>(k, live_count_);
    best_.clear();
    frontier_.clear();
    frontier_.push_back(Pending{0.0f, levels_ - 1, 0});
    while (!frontier_.empty()) {
      std::pop_heap(frontier_.begin(), frontier_.end(), farther);
      const Pending at = frontier_.back();
      frontier_.pop_back();
      if (best_.size() == want && at.d2 > best_.front().d2) break;
      if (at.level == 0) {
        scan_leaf_knn(at.index, c, want);
        continue;
      }
      const std::uint32_t below = at.level - 1;
      const std::uint32_t first = at.index * kFan;
      const std::uint32_t valid = std::min(kFan, level_count_[below] - first);
      float bd[kFan];
      box_dist2(boxes_[level_group_[below] + at.index], c, bd);
      for (std::uint32_t j = 0; j < valid; ++j) {
        if (best_.size() == want && bd[j] > best_.front().d2) continue;
        frontier_.push_back(Pending{bd[j], below, first + j});
        std::push_heap(frontier_.begin(), frontier_.end(), farther);
      }
    }
    std::sort_heap(best_.begin(), best_.end(), nearer);
    for (const Neighbour& n : best_) d.push(n.id, pos_[n.id]);
    return d.value();
  }

  void end_tick(std::uint64_t) {}
  bool rewind_to(std::uint64_t) { return false; }

  std::size_t entity_count() const { return live_count_; }

  std::size_t reported_bytes() const {
    return pos_.capacity() * sizeof(Vec3) + live_.capacity() +
           keys_.capacity() * sizeof(std::uint64_t) + spare_.capacity() * sizeof(std::uint64_t) +
           (sx_.capacity() + sy_.capacity() + sz_.capacity()) * sizeof(FloatLine) +
           sid_.capacity() * sizeof(IdLine) + boxes_.capacity() * sizeof(BoxGroup) +
           frontier_.capacity() * sizeof(Pending) + best_.capacity() * sizeof(Neighbour);
  }

 private:
  // Sixteen consecutive values of one sorted array: one 64-byte line, with no
  // padding, so a vector of lines is a plain array of floats (or ids) in sorted
  // order whose every sixteenth element starts a cache line. Slot i of the
  // sorted order is element i % kLeaf of line i / kLeaf, and leaf j is line j
  // of each of the four arrays. A slot past the end of the population holds
  // NaN coordinates, which fail every <= test, so the distance loop never
  // needs to know how full the last leaf is.
  struct alignas(64) FloatLine {
    float v[kLeaf];
  };
  struct alignas(64) IdLine {
    EntityId v[kLeaf];
  };
  static_assert(sizeof(FloatLine) == kLeaf * sizeof(float), "a line is its sixteen values");
  static_assert(sizeof(IdLine) == kLeaf * sizeof(EntityId), "a line is its sixteen values");

  // The boxes of kFan sibling nodes, one field per 32-byte row. A lane past
  // the end of its level holds an empty box, +inf to -inf, so a parent's box is
  // the union of all eight lanes without asking which are real.
  struct alignas(64) BoxGroup {
    float lo_x[kFan];
    float lo_y[kFan];
    float lo_z[kFan];
    float hi_x[kFan];
    float hi_y[kFan];
    float hi_z[kFan];
  };

  struct Pending {
    float d2;
    std::uint32_t level;
    std::uint32_t index;
  };

  static bool farther(const Pending& a, const Pending& b) { return a.d2 > b.d2; }

  // A node count fits in 28 bits (2^32 ids over leaves of 16), and each level
  // divides it by eight, so no tree has more than eleven levels.
  static constexpr std::uint32_t kMaxLevels = 16;
  static constexpr float kInf = std::numeric_limits<float>::infinity();
  static constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

  // The ten low bits of x moved to every third bit.
  static constexpr std::uint32_t spread_bits(std::uint32_t x) {
    x &= 0x000003FFu;
    x = (x ^ (x << 16)) & 0xFF0000FFu;
    x = (x ^ (x << 8)) & 0x0300F00Fu;
    x = (x ^ (x << 4)) & 0x030C30C3u;
    x = (x ^ (x << 2)) & 0x09249249u;
    return x;
  }

  struct SpreadTable {
    std::uint32_t v[1024];
    constexpr SpreadTable() : v{} {
      for (std::uint32_t i = 0; i < 1024; ++i) v[i] = spread_bits(i);
    }
  };

  // The rebuild computes three spreads per entity, and as shifts and masks
  // they were most of the cost of computing the codes. As a 4 KB table built
  // at compile time they are three loads that stay in L1. The table is static
  // data, not a heap allocation, and is the same for every instance.
  static std::uint32_t spread(std::uint32_t x) {
    static constexpr SpreadTable table{};
    return table.v[x];
  }

  // A negative or NaN extent is treated as none. An extent so small that the
  // scale overflows to infinity is harmless: step() maps the product to 0 or
  // 1023, and the code only orders.
  static float axis_scale(float extent) { return extent > 0.0f ? 1024.0f / extent : 0.0f; }

  // The code decides only the order, never an answer: boxes are built from the
  // positions themselves. So clamping a value off either end of the world, or
  // a NaN, to a valid step changes nothing but where in the order it lands.
  static std::uint32_t step(float v, float lo, float scale) {
    float t = (v - lo) * scale;
    t = t > 0.0f ? t : 0.0f;
    t = t < 1023.0f ? t : 1023.0f;
    return static_cast<std::uint32_t>(t);
  }

  std::uint32_t code_of(Vec3 p) const {
    return spread(step(p.x, bounds_.min.x, scale_x_)) |
           (spread(step(p.y, bounds_.min.y, scale_y_)) << 1) |
           (spread(step(p.z, bounds_.min.z, scale_z_)) << 2);
  }

  // Lower bounds on the squared distance from c to each of the eight boxes.
  //
  // Per axis the gap is max(lo - c, c - hi, 0), and the three squared gaps are
  // summed in the order dist2 sums its terms. For an entity p in the box,
  // lo <= p <= hi, and rounding is monotonic, so each rounded gap is no larger
  // in magnitude than the rounded p - c that dist2 computes, each square is no
  // larger, and the sum is no larger. The bound can therefore only
  // under-estimate dist2(p, c) as the substrate computes it, never exceed it,
  // and pruning on it cannot drop an entity the accept test would keep. The
  // order is an association: commuting two terms gives the same float, but
  // grouping the last two first can round above dist2 for a point on a face.
  static void box_dist2(const BoxGroup& g, Vec3 c, float* out) {
    for (std::uint32_t j = 0; j < kFan; ++j) {
      const float ex = std::max(std::max(g.lo_x[j] - c.x, c.x - g.hi_x[j]), 0.0f);
      const float ey = std::max(std::max(g.lo_y[j] - c.y, c.y - g.hi_y[j]), 0.0f);
      const float ez = std::max(std::max(g.lo_z[j] - c.z, c.z - g.hi_z[j]), 0.0f);
      out[j] = ex * ex + ey * ey + ez * ez;
    }
  }

  // The accept test is dist2(p, c) <= r*r for every entity, including those of
  // a leaf whose box lies wholly inside the sphere. The distances are computed
  // for the whole leaf first, a fixed-length loop the compiler vectorises,
  // and only then tested.
  void leaf_dist2(std::uint32_t leaf, Vec3 c, float* out) const {
    const FloatLine& x = sx_[leaf];
    const FloatLine& y = sy_[leaf];
    const FloatLine& z = sz_[leaf];
    for (std::uint32_t j = 0; j < kLeaf; ++j) out[j] = dist2(Vec3{x.v[j], y.v[j], z.v[j]}, c);
  }

  void collect_radius(Vec3 c, float r, RadiusDigest& d) const {
    rebuild_if_needed();
    if (live_count_ == 0) return;
    const float r2 = r * r;
    struct Visit {
      std::uint32_t level;
      std::uint32_t index;
    };
    // Depth first. Each expanded node leaves at most kFan - 1 siblings waiting
    // per level, so the stack never exceeds kFan entries per level.
    std::array<Visit, kFan * kMaxLevels> stack;
    std::size_t sp = 0;
    if (levels_ == 1) {
      scan_leaf_radius(0, c, r2, d);
      return;
    }
    stack[sp++] = Visit{levels_ - 1, 0};
    while (sp > 0) {
      const Visit at = stack[--sp];
      const std::uint32_t below = at.level - 1;
      const std::uint32_t first = at.index * kFan;
      const std::uint32_t valid = std::min(kFan, level_count_[below] - first);
      float bd[kFan];
      box_dist2(boxes_[level_group_[below] + at.index], c, bd);
      for (std::uint32_t j = 0; j < valid; ++j) {
        if (bd[j] > r2) continue;
        if (below == 0) scan_leaf_radius(first + j, c, r2, d);
        else stack[sp++] = Visit{below, first + j};
      }
    }
  }

  void scan_leaf_radius(std::uint32_t leaf, Vec3 c, float r2, RadiusDigest& d) const {
    const FloatLine& x = sx_[leaf];
    const FloatLine& y = sy_[leaf];
    const FloatLine& z = sz_[leaf];
    const IdLine& ids = sid_[leaf];
    float d2[kLeaf];
    leaf_dist2(leaf, c, d2);
    for (std::uint32_t j = 0; j < kLeaf; ++j) {
      if (d2[j] <= r2) d.hit(ids.v[j], Vec3{x.v[j], y.v[j], z.v[j]});
    }
  }

  // best_ is a max-heap under nearer, so its front is the kth best so far.
  void scan_leaf_knn(std::uint32_t leaf, Vec3 c, std::size_t want) const {
    const IdLine& ids = sid_[leaf];
    float d2[kLeaf];
    leaf_dist2(leaf, c, d2);
    float limit = best_.size() == want ? best_.front().d2 : kInf;
    for (std::uint32_t j = 0; j < kLeaf; ++j) {
      // Equal distance still competes on id; NaN padding fails here.
      if (!(d2[j] <= limit)) continue;
      const Neighbour n{d2[j], ids.v[j]};
      if (best_.size() < want) {
        best_.push_back(n);
        std::push_heap(best_.begin(), best_.end(), nearer);
      } else if (nearer(n, best_.front())) {
        std::pop_heap(best_.begin(), best_.end(), nearer);
        best_.back() = n;
        std::push_heap(best_.begin(), best_.end(), nearer);
      } else {
        continue;
      }
      if (best_.size() == want) limit = best_.front().d2;
    }
  }

  void rebuild_if_needed() const {
    if (!dirty_) return;
    dirty_ = false;
    const std::uint32_t n = static_cast<std::uint32_t>(live_count_);
    levels_ = 0;
    if (n == 0) return;
    sort_live(n);
    gather(n);
    build_boxes(n);
  }

  // Keys are (code << 32 | id), so one 8-byte word carries both through the
  // sort. Ids are visited in ascending order and every pass is stable, so
  // equal codes stay in id order and the result is the same every time.
  //
  // Least significant digit first, three digits of ten bits: a 1024-bucket
  // histogram is 4 KB, so all three fit in L1 together and are filled in the
  // same pass that computes the codes. A digit every key shares is skipped,
  // which happens when the whole population lies inside one cell of the
  // code's coarser levels.
  void sort_live(std::uint32_t n) const {
    keys_.resize(n);
    spare_.resize(n);
    std::array<std::array<std::uint32_t, 1024>, 3> count{};
    std::uint32_t k = 0;
    for (std::size_t id = 0; id < live_.size(); ++id) {
      if (!live_[id]) continue;
      const std::uint32_t code = code_of(pos_[id]);
      ++count[0][code & 1023u];
      ++count[1][(code >> 10) & 1023u];
      ++count[2][code >> 20];
      keys_[k++] = (static_cast<std::uint64_t>(code) << 32) | static_cast<std::uint64_t>(id);
    }
    for (std::uint32_t pass = 0; pass < 3; ++pass) {
      const std::uint32_t shift = 32 + 10 * pass;
      std::array<std::uint32_t, 1024>& c = count[pass];
      if (c[(keys_[0] >> shift) & 1023u] == n) continue;
      std::uint32_t total = 0;
      for (std::uint32_t& slot : c) {
        const std::uint32_t here = slot;
        slot = total;
        total += here;
      }
      for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint64_t key = keys_[i];
        spare_[c[(key >> shift) & 1023u]++] = key;
      }
      keys_.swap(spare_);
    }
  }

  void gather(std::uint32_t n) const {
    const std::uint32_t nleaves = (n + kLeaf - 1) / kLeaf;
    sx_.resize(nleaves);
    sy_.resize(nleaves);
    sz_.resize(nleaves);
    sid_.resize(nleaves);
    for (std::uint32_t i = 0; i < n; ++i) {
      const EntityId id = static_cast<EntityId>(keys_[i]);
      const Vec3 p = pos_[id];
      const std::uint32_t line = i / kLeaf;
      const std::uint32_t j = i % kLeaf;
      sx_[line].v[j] = p.x;
      sy_[line].v[j] = p.y;
      sz_[line].v[j] = p.z;
      sid_[line].v[j] = id;
    }
    const std::uint32_t last = nleaves - 1;
    for (std::uint32_t j = n - last * kLeaf; j < kLeaf; ++j) {
      sx_[last].v[j] = kNaN;
      sy_[last].v[j] = kNaN;
      sz_[last].v[j] = kNaN;
      sid_[last].v[j] = kNoEntity;
    }
  }

  static void set_empty(BoxGroup& g, std::uint32_t lane) {
    g.lo_x[lane] = g.lo_y[lane] = g.lo_z[lane] = kInf;
    g.hi_x[lane] = g.hi_y[lane] = g.hi_z[lane] = -kInf;
  }

  void build_boxes(std::uint32_t n) const {
    // Level 0 holds one box per leaf; each level above holds one per kFan
    // boxes of the level below, until a single root.
    std::uint32_t count = (n + kLeaf - 1) / kLeaf;
    std::uint32_t groups = 0;
    levels_ = 0;
    while (true) {
      level_count_[levels_] = count;
      level_group_[levels_] = groups;
      ++levels_;
      groups += (count + kFan - 1) / kFan;
      if (count == 1) break;
      count = (count + kFan - 1) / kFan;
    }
    boxes_.resize(groups);
    for (std::uint32_t l = 0; l < levels_; ++l) {
      BoxGroup& tail = boxes_[level_group_[l] + (level_count_[l] - 1) / kFan];
      for (std::uint32_t lane = (level_count_[l] - 1) % kFan + 1; lane < kFan; ++lane) {
        set_empty(tail, lane);
      }
    }

    // Leaf boxes, over the real slots only: the padding is NaN.
    for (std::uint32_t leaf = 0; leaf < level_count_[0]; ++leaf) {
      const FloatLine& x = sx_[leaf];
      const FloatLine& y = sy_[leaf];
      const FloatLine& z = sz_[leaf];
      const std::uint32_t used = std::min(kLeaf, n - leaf * kLeaf);
      float lx = x.v[0], ly = y.v[0], lz = z.v[0];
      float hx = lx, hy = ly, hz = lz;
      for (std::uint32_t j = 1; j < used; ++j) {
        lx = std::min(lx, x.v[j]);
        ly = std::min(ly, y.v[j]);
        lz = std::min(lz, z.v[j]);
        hx = std::max(hx, x.v[j]);
        hy = std::max(hy, y.v[j]);
        hz = std::max(hz, z.v[j]);
      }
      BoxGroup& g = boxes_[level_group_[0] + leaf / kFan];
      const std::uint32_t lane = leaf % kFan;
      g.lo_x[lane] = lx;
      g.lo_y[lane] = ly;
      g.lo_z[lane] = lz;
      g.hi_x[lane] = hx;
      g.hi_y[lane] = hy;
      g.hi_z[lane] = hz;
    }

    // A node's box is the union of its children's group, all eight lanes.
    for (std::uint32_t l = 1; l < levels_; ++l) {
      for (std::uint32_t node = 0; node < level_count_[l]; ++node) {
        const BoxGroup& kids = boxes_[level_group_[l - 1] + node];
        float lx = kids.lo_x[0], ly = kids.lo_y[0], lz = kids.lo_z[0];
        float hx = kids.hi_x[0], hy = kids.hi_y[0], hz = kids.hi_z[0];
        for (std::uint32_t j = 1; j < kFan; ++j) {
          lx = std::min(lx, kids.lo_x[j]);
          ly = std::min(ly, kids.lo_y[j]);
          lz = std::min(lz, kids.lo_z[j]);
          hx = std::max(hx, kids.hi_x[j]);
          hy = std::max(hy, kids.hi_y[j]);
          hz = std::max(hz, kids.hi_z[j]);
        }
        BoxGroup& g = boxes_[level_group_[l] + node / kFan];
        const std::uint32_t lane = node % kFan;
        g.lo_x[lane] = lx;
        g.lo_y[lane] = ly;
        g.lo_z[lane] = lz;
        g.hi_x[lane] = hx;
        g.hi_y[lane] = hy;
        g.hi_z[lane] = hz;
      }
    }
  }

  Bounds bounds_;
  float scale_x_ = 0.0f;
  float scale_y_ = 0.0f;
  float scale_z_ = 0.0f;
  std::vector<Vec3> pos_;
  std::vector<std::uint8_t> live_;
  std::size_t live_count_ = 0;

  mutable bool dirty_ = true;
  mutable std::vector<std::uint64_t> keys_;
  mutable std::vector<std::uint64_t> spare_;
  mutable std::vector<FloatLine> sx_;
  mutable std::vector<FloatLine> sy_;
  mutable std::vector<FloatLine> sz_;
  mutable std::vector<IdLine> sid_;
  mutable std::vector<BoxGroup> boxes_;
  mutable std::array<std::uint32_t, kMaxLevels> level_count_{};
  mutable std::array<std::uint32_t, kMaxLevels> level_group_{};
  mutable std::uint32_t levels_ = 0;
  mutable std::vector<Pending> frontier_;
  mutable std::vector<Neighbour> best_;
};

}  // namespace gds::candidates
