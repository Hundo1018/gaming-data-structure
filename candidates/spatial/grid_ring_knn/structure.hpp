#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "gds/spatial/types.hpp"
#include "spatial/uniform_grid/structure.hpp"

namespace gds::candidates {

using namespace gds::spatial;

// The table of cell offsets the k-nearest walk follows, nearest first.
//
// It is sorted by the least distance any point of an offset cell can be from
// any point of the query's cell: cell * sqrt(Q), Q = sum over axes of
// max(0, |d| - 1)^2. Q is an integer, so the order is exact and offsets of equal
// Q form a shell sharing one bound. The bound depends on |d| alone, so only one
// octant is stored and each entry stands for its sign reflections.
//
// The table depends on nothing but its reach, so the compiler builds it once
// and every instance shares it. A per-instance table would be rebuilt with
// every snapshot-and-rebuild rewind, which constructs a new structure, and the
// rewind would then pay for something the parent's does not.
namespace grid_ring_knn_table {

// Offsets whose bound is under this many cell edges are in the table. See
// hypothesis.md: it covers the sparsest public k-nearest configuration with
// margin, for 3134 four-byte entries in each of two orders.
inline constexpr int kReach = 16;
inline constexpr int kQLimit = kReach * kReach;

// Every bound is shortened by this much of a cell edge. A point can sit
// outside the cell it is filed in by the rounding of (v - lo) * inv_cell,
// under 6.2e-5 of a cell edge with at most 258 cells per axis, and both the
// query and the entity carry that error. This is over ten times that.
inline constexpr float kSlack = 1.0f / 256.0f;

// One octant offset in cells, |dx|, |dy|, |dz|, and the key its order sorts a
// shell's steps by. Four bytes so each order stays near twelve kilobytes.
struct Step {
  std::uint8_t x, y, z, lead;
};

// A run of steps of equal Q, ending at `end`; sqrt(Q) less the slack, which is
// the bound they share in cell edges; and the largest |d| any of them has on
// any axis.
struct Shell {
  std::uint32_t end;
  float root;
  int reach;
};

constexpr int q_of(int a) { return a > 1 ? (a - 1) * (a - 1) : 0; }

// Every octant offset with Q under the limit, z then y then x. Q only grows
// with each component and q_of(kReach + 1) is the limit, so no component of a
// kept offset exceeds kReach and each run of x stops at its first offset past.
template <class F>
constexpr void for_each_offset(F&& f) {
  for (int z = 0; z <= kReach; ++z)
    for (int y = 0; y <= kReach; ++y) {
      const int qyz = q_of(y) + q_of(z);
      for (int x = 0; x <= kReach; ++x) {
        const int q = qyz + q_of(x);
        if (q >= kQLimit) break;
        f(x, y, z, q);
      }
    }
}

constexpr std::size_t count_steps() {
  std::size_t n = 0;
  for_each_offset([&](int, int, int, int) { ++n; });
  return n;
}

constexpr std::size_t count_shells() {
  std::array<bool, kQLimit> used{};
  for_each_offset([&](int, int, int, int q) { used[static_cast<std::size_t>(q)] = true; });
  std::size_t n = 0;
  for (bool u : used) n += u ? 1 : 0;
  return n;
}

// std::sqrt is not usable in a constant expression. Newton's iteration from
// x = q decreases monotonically towards sqrt(q) and stops when rounding stops
// it, within an ulp of a double; the slack it is compared against is ten
// orders of magnitude larger.
constexpr double root_of(int q) {
  if (q == 0) return 0.0;
  double x = q;
  for (int i = 0; i < 64; ++i) {
    const double next = 0.5 * (x + q / x);
    if (!(next < x)) break;
    x = next;
  }
  return x;
}

using Steps = std::array<Step, count_steps()>;

// The same offsets in two orders. Both are sorted by Q first, so they share
// their shells; within a shell one ascends in |dz| and the other in
// max(|dx|, |dy|). A grid shorter in z than across walks the first and a grid
// narrower across than in z the second, so that in either the first step of a
// shell too long for the grid along the leading key ends the shell, and the
// steps after it are never read.
struct Octant {
  Steps by_z{};
  Steps by_xy{};
  std::array<Shell, count_shells()> shells{};
};

// Counting sort on (Q, lead). It is stable, so the offsets of each (Q, lead)
// stay in the order they were enumerated.
template <class Lead>
constexpr void sort_steps(Steps& out, Lead lead) {
  constexpr std::size_t kLeads = kReach + 1;
  std::array<std::uint32_t, kQLimit * kLeads + 1> next{};
  auto bucket = [&](int x, int y, int z, int q) {
    return static_cast<std::size_t>(q) * kLeads + static_cast<std::size_t>(lead(x, y, z));
  };
  for_each_offset([&](int x, int y, int z, int q) { ++next[bucket(x, y, z, q) + 1]; });
  for (std::size_t b = 0; b + 1 < next.size(); ++b) next[b + 1] += next[b];
  for_each_offset([&](int x, int y, int z, int q) {
    out[next[bucket(x, y, z, q)]++] =
        Step{static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y),
             static_cast<std::uint8_t>(z), static_cast<std::uint8_t>(lead(x, y, z))};
  });
}

constexpr Octant build_octant() {
  Octant t{};
  sort_steps(t.by_z, [](int, int, int z) { return z; });
  sort_steps(t.by_xy, [](int x, int y, int) { return std::max(x, y); });
  std::array<std::uint32_t, kQLimit + 1> end{};
  for_each_offset([&](int, int, int, int q) { ++end[static_cast<std::size_t>(q) + 1]; });
  for (std::size_t q = 0; q < kQLimit; ++q) end[q + 1] += end[q];
  std::size_t s = 0;
  for (int q = 0; q < kQLimit; ++q) {
    if (end[static_cast<std::size_t>(q) + 1] == end[static_cast<std::size_t>(q)]) continue;
    // q_of(d) <= Q on every axis, so |d| <= isqrt(Q) + 1.
    int root = 0;
    while ((root + 1) * (root + 1) <= q) ++root;
    const float bound = q == 0 ? 0.0f : static_cast<float>(root_of(q) - 1.0 / 256.0);
    t.shells[s++] = Shell{end[static_cast<std::size_t>(q) + 1], bound, std::min(kReach, root + 1)};
  }
  return t;
}

inline constexpr Octant kOctant = build_octant();

// The walk relies on this: in both orders every step of a shell has the
// shell's Q, and its lead never decreases within the shell.
constexpr bool well_ordered(const Steps& steps, const Octant& t) {
  std::uint32_t e = 0;
  for (const Shell& shell : t.shells) {
    const Step first = steps[e];
    const int q = q_of(first.x) + q_of(first.y) + q_of(first.z);
    for (std::uint32_t i = e; i < shell.end; ++i) {
      const Step st = steps[i];
      if (q_of(st.x) + q_of(st.y) + q_of(st.z) != q) return false;
      if (i > e && st.lead < steps[i - 1].lead) return false;
    }
    e = shell.end;
  }
  return e == steps.size();
}
static_assert(well_ordered(kOctant.by_z, kOctant) && well_ordered(kOctant.by_xy, kOctant));

}  // namespace grid_ring_knn_table

// uniform_grid with its k-nearest search replaced and nothing else changed.
//
// The parent widens a box by doubling its half-width and re-walks every cell of
// it each round. This visits each cell at most once, nearest first, in the
// order of the table above, and stops as soon as no unvisited cell can hold
// anything nearer than the k-th best found so far.
class GridRingKnn : public UniformGrid {
 public:
  static const char* name() { return "grid_ring_knn"; }

  explicit GridRingKnn(const WorldConfig& cfg) : UniformGrid(cfg) { fit_grid(); }

  std::uint64_t query_knn(Vec3 c, std::uint32_t k) const {
    if (k == 0 || live_count_ == 0) return KnnDigest{}.value();

    const Axis ax = axis_frame(c.x, bounds_.min.x, nx_);
    const Axis ay = axis_frame(c.y, bounds_.min.y, ny_);
    const Axis az = axis_frame(c.z, bounds_.min.z, nz_);
    Lookup lx, ly, lz;
    int filled = -1;
    const std::int32_t base = static_cast<std::int32_t>(
        (static_cast<std::uint32_t>(az.i) * ny_ + static_cast<std::uint32_t>(ay.i)) * nx_ +
        static_cast<std::uint32_t>(ax.i));

    heap_.clear();
    const std::size_t want = std::min<std::size_t>(k, live_count_);
    if (heap_.capacity() < want) heap_.reserve(want);
    Search s{c, k, 0, kFar};

    const float cell = cell_;
    const Step* const order = order_;
    std::uint32_t e = 0;
    for (const Shell& shell : kOctant.shells) {
      // Strictly greater: an unvisited entity at exactly the k-th best dist2
      // with a smaller id would displace the current k-th, so a shell whose
      // bound equals that distance still has to be walked.
      if (squared(shell.root * cell) > s.worst) return finish();
      if (shell.reach > filled) {
        extend(lx, ax, 1, filled + 1, shell.reach);
        extend(ly, ay, static_cast<std::int32_t>(nx_), filled + 1, shell.reach);
        extend(lz, az, static_cast<std::int32_t>(nx_ * ny_), filled + 1, shell.reach);
        filled = shell.reach;
      }
      // A step longer than the grid on some axis has no reflection inside it.
      // Skipping it is only a saving, since its reflections would all be
      // rejected anyway, and only a shell that can hold one tests for it. The
      // first step whose lead is past the grid ends the shell, because the
      // rest of the shell's leads are larger still; the other axes are tested
      // a step at a time. One call site rather than a loop per case keeps
      // visit_step inlined.
      const bool clip = shell.reach > fit_;
      for (; e < shell.end; ++e) {
        const Step st = order[e];
        if (clip) {
          if (st.lead > lead_) {
            e = shell.end;
            break;
          }
          if (st.x > ex_ || st.y > ey_ || st.z > ez_) continue;
        }
        visit_step(st, lx, ly, lz, base, s);
      }
      if (s.seen == live_count_) return finish();
    }
    if (covers_grid_ || tail_bound2_ > s.worst) return finish();
    // The k-th neighbour may lie beyond the table. The parent's widening
    // search answers from scratch rather than this one extending, so the answer
    // is then the parent's, exact wherever the parent's is: its stop test
    // squares a half-width, and once that overflows float (worlds wider than
    // about 1.8e19) neither search is exact.
    return UniformGrid::query_knn(c, k);
  }

  // The table is static data, not a heap allocation, and the same for every
  // instance; it is counted all the same, because the search cannot work
  // without it.
  std::size_t reported_bytes() const {
    return UniformGrid::reported_bytes() + sizeof(kOctant) +
           heap_.capacity() * sizeof(Neighbour);
  }

 protected:
  using Step = grid_ring_knn_table::Step;
  using Shell = grid_ring_knn_table::Shell;
  static constexpr int kReach = grid_ring_knn_table::kReach;
  static constexpr int kQLimit = grid_ring_knn_table::kQLimit;
  static constexpr float kSlack = grid_ring_knn_table::kSlack;
  static constexpr const grid_ring_knn_table::Octant& kOctant = grid_ring_knn_table::kOctant;

  // Every squared bound is multiplied by this. dist2 can round below the real
  // squared distance by a relative 5 * 2^-24, and computing a bound can round
  // up by about as much; 2^-16 is 256 * 2^-24.
  static constexpr float kShrink = 1.0f - 1.0f / 65536.0f;

  // The query's position along one axis, in the coordinates that file points
  // into cells: cell i + a starts (a - up) cell edges above it and cell i - a
  // ends (a - down) below it, both already shortened by the slack.
  struct Axis {
    int i;
    int n;
    float up;
    float down;
  };

  // Per query and per axis, indexed by kReach + d: the change in linear cell
  // index for moving d cells along the axis, or kOutside if that leaves the
  // grid, and the squared gap from the query to that slab of cells, in world
  // units. Filled only as far as the walk has reached.
  struct Lookup {
    std::int32_t step[2 * kReach + 1];
    float gap2[2 * kReach + 1];
  };

  // Added to a linear index, any one of these makes it negative, and three
  // together still do not overflow: an index past every grid this parent can
  // build (at most 258 cells per axis) is rejected by one unsigned comparison.
  static constexpr std::int32_t kOutside = std::numeric_limits<std::int32_t>::min() / 4;
  static constexpr float kFar = std::numeric_limits<float>::infinity();

  struct Search {
    Vec3 c;
    std::size_t k;
    std::size_t seen;
    float worst;  // the k-th best dist2 once k are held, infinity before
  };

  // A length in world units, squared and shrunk. Every bound is scaled into
  // world units before it is squared: squaring the cell edge first overflows
  // float once the edge passes 2^64, and a bound of infinity would then prune
  // cells whose entities are a fraction of an edge away at a finite dist2.
  // Squared after scaling, a bound overflows only where the distance it bounds
  // is larger still, so dist2 of anything it prunes is infinite as well.
  static float squared(float length) { return length * length * kShrink; }

  // Which of the table's steps can fit inside this grid, and what is left
  // beyond the table. Constant time: nothing here depends on the population,
  // and the table itself is not rebuilt.
  void fit_grid() {
    ex_ = std::min(kReach, static_cast<int>(nx_) - 1);
    ey_ = std::min(kReach, static_cast<int>(ny_) - 1);
    ez_ = std::min(kReach, static_cast<int>(nz_) - 1);
    fit_ = std::min(ex_, std::min(ey_, ez_));
    // Of the two orders, the one whose leading key this grid cuts shorter.
    const int across = std::max(ex_, ey_);
    const bool by_xy = across < ez_;
    order_ = by_xy ? kOctant.by_xy.data() : kOctant.by_z.data();
    lead_ = by_xy ? across : ez_;
    // Anything not in the table is outside the grid or has Q of at least the
    // limit, so this bounds every cell the table walk did not reach. When the
    // largest offset the grid can hold is in the table, nothing is unreached.
    tail_bound2_ = squared((static_cast<float>(kReach) - kSlack) * cell_);
    covers_grid_ = grid_ring_knn_table::q_of(static_cast<int>(nx_) - 1) +
                       grid_ring_knn_table::q_of(static_cast<int>(ny_) - 1) +
                       grid_ring_knn_table::q_of(static_cast<int>(nz_) - 1) <
                   kQLimit;
    cell_count_ = static_cast<std::uint32_t>(head_.size());
  }

  Axis axis_frame(float v, float lo, std::uint32_t n) const {
    const int i = static_cast<int>(axis_index(v, lo, n));
    // The same expression axis_index floors. Clamping a query that lies
    // outside the grid toward it only shortens its gaps, which keeps every
    // bound below the truth.
    float s = (v - lo) * inv_cell_;
    if (!(s >= -1.0f)) s = -1.0f;
    if (s > static_cast<float>(n) + 1.0f) s = static_cast<float>(n) + 1.0f;
    const float f = s - static_cast<float>(i);
    return Axis{i, static_cast<int>(n), f + kSlack, (1.0f - f) + kSlack};
  }

  // The squared world-unit gap to a slab `cells` cell edges away, or none.
  float slab_gap2(float cells) const { return cells > 0.0f ? squared(cells * cell_) : 0.0f; }

  // Fills offsets from..to, both signs, of one axis's lookup. The query's own
  // slab has no gap: if the query lies outside the grid, the edge cell it was
  // clamped into also holds whatever entities were clamped there with it.
  void extend(Lookup& t, const Axis& a, std::int32_t stride, int from, int to) const {
    for (int off = from; off <= to; ++off) {
      t.step[kReach + off] = a.i + off < a.n ? off * stride : kOutside;
      t.step[kReach - off] = a.i - off >= 0 ? -off * stride : kOutside;
      const float d = static_cast<float>(off);
      t.gap2[kReach + off] = off == 0 ? 0.0f : slab_gap2(d - a.up);
      t.gap2[kReach - off] = off == 0 ? 0.0f : slab_gap2(d - a.down);
    }
  }

  // The up to eight sign reflections of one octant step, unrolled. Reflections
  // that leave the grid, and the minus reflection of a zero component (which is
  // the plus one again), carry kOutside and are rejected inside visit() without
  // a branch of their own: a walk whose branches follow the table rather than
  // the data lets the cell loads overlap.
  void visit_step(Step st, const Lookup& lx, const Lookup& ly, const Lookup& lz,
                  std::int32_t base, Search& s) const {
    const int px = kReach + st.x, mx = kReach - st.x;
    const int py = kReach + st.y, my = kReach - st.y;
    const int pz = kReach + st.z, mz = kReach - st.z;
    const std::int32_t xp = lx.step[px], xm = st.x ? lx.step[mx] : kOutside;
    const std::int32_t yp = ly.step[py], ym = st.y ? ly.step[my] : kOutside;
    const std::int32_t zp = lz.step[pz], zm = st.z ? lz.step[mz] : kOutside;
    const float gxp = lx.gap2[px], gxm = lx.gap2[mx];
    const float gpp = ly.gap2[py] + lz.gap2[pz], gmp = ly.gap2[my] + lz.gap2[pz];
    const float gpm = ly.gap2[py] + lz.gap2[mz], gmm = ly.gap2[my] + lz.gap2[mz];
    visit(base + xp + yp + zp, gxp + gpp, s);
    visit(base + xm + yp + zp, gxm + gpp, s);
    visit(base + xp + ym + zp, gxp + gmp, s);
    visit(base + xm + ym + zp, gxm + gmp, s);
    visit(base + xp + yp + zm, gxp + gpm, s);
    visit(base + xm + yp + zm, gxm + gpm, s);
    visit(base + xp + ym + zm, gxp + gmm, s);
    visit(base + xm + ym + zm, gxm + gmm, s);
  }

  // Small enough to inline at all eight call sites, which keeps an empty cell
  // to a load and a compare; the walk of a cell that holds something is not.
  void visit(std::int32_t index, float gap2, Search& s) const {
    const std::uint32_t cell = static_cast<std::uint32_t>(index);
    const bool inside = cell < cell_count_;
    EntityId id = head_[inside ? cell : 0u];
    id = inside ? id : kNoEntity;
    if (id != kNoEntity) walk(id, gap2, s);
  }

  void walk(EntityId id, float gap2, Search& s) const {
    // The exact gap from the query point to this cell's box, tested only for
    // cells that hold something. Once k are held, a cell strictly beyond the
    // k-th best cannot change the answer; equal is walked, for the same reason
    // as at a shell boundary.
    if (gap2 > s.worst) return;
    for (; id != kNoEntity; id = next_[id]) {
      ++s.seen;
      const Neighbour nb{dist2(pos_[id], s.c), id};
      if (heap_.size() < s.k) {
        heap_.push_back(nb);
        std::push_heap(heap_.begin(), heap_.end(), nearer);
        if (heap_.size() == s.k) s.worst = heap_.front().d2;
      } else if (nearer(nb, heap_.front())) {
        replace_top(nb);
        s.worst = heap_.front().d2;
      }
    }
  }

  // The heap's top is the farthest of the best k. Replacing it and sifting down
  // is one pass where a pop and a push would be two.
  void replace_top(Neighbour nb) const {
    Neighbour* h = heap_.data();
    const std::size_t n = heap_.size();
    std::size_t i = 0;
    for (;;) {
      std::size_t child = 2 * i + 1;
      if (child >= n) break;
      if (child + 1 < n && nearer(h[child], h[child + 1])) ++child;
      if (!nearer(nb, h[child])) break;
      h[i] = h[child];
      i = child;
    }
    h[i] = nb;
  }

  std::uint64_t finish() const {
    std::sort_heap(heap_.begin(), heap_.end(), nearer);
    KnnDigest d;
    for (const Neighbour& nb : heap_) d.push(nb.id, pos_[nb.id]);
    return d.value();
  }

  // The largest step component that can fit inside the grid on each axis, and
  // the least of the three: a shell whose reach is within it needs no check.
  int ex_ = 0, ey_ = 0, ez_ = 0, fit_ = 0;
  // The order this grid walks, and the largest lead in it that can fit.
  const Step* order_ = nullptr;
  int lead_ = 0;
  float tail_bound2_ = 0.0f;
  std::uint32_t cell_count_ = 0;
  bool covers_grid_ = false;
  // Scratch for the best k of one query, kept so a query allocates nothing once
  // it has grown to the largest k asked for.
  mutable std::vector<Neighbour> heap_;
};

}  // namespace gds::candidates
