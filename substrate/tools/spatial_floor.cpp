// The cost no spatial structure can avoid, measured per tick.
//
// A candidate's tick time says how fast it is. It does not say how far from
// possible it is, and that is what decides whether a new representation is
// worth building for a workload. This tool measures the floor: what a tick
// would cost if finding every answer were free.
//
// Two things are irreducible whatever the structure:
//
//   state    applying the tick's inserts, removes and moves to flat position
//            and liveness arrays — knowing where everything is.
//   answers  folding the digest of every entity that is in a radius answer,
//            and of the k results of every k-nearest query, from buffers that
//            already hold exactly those entities, packed.
//
// The floor of a tick is the sum. Everything a candidate spends above it is
// search: culling, walking, testing points that turn out not to match.
//
// It also counts, for every radius query, how many entities the reference
// grid's query box admits against how many are actually in the answer. That
// ratio is the over-admission a tighter broad phase could remove, and the
// answer size is the part nothing can remove.
//
// Answers are found with the oracle, outside any timed region.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gds/json.hpp"
#include "gds/measure.hpp"
#include "gds/spatial/oracle.hpp"
#include "gds/spatial/workload.hpp"
#include "spatial/uniform_grid/structure.hpp"

namespace {

using namespace gds::spatial;
using Clock = std::chrono::steady_clock;

// The reference grid with one addition: a count of what its broad phase admits.
class CountingGrid : public gds::candidates::UniformGrid {
 public:
  using UniformGrid::UniformGrid;
  std::uint64_t admitted(Vec3 c, float r) const {
    std::uint64_t n = 0;
    for_each_in_box(c, r, [&](EntityId) { ++n; });
    return n;
  }
  std::uint64_t admitted_of(EntityId self, float r) const {
    Vec3 p{};
    if (!position_of(self, p)) return 0;
    return admitted(p, r) - 1;
  }
};

struct Hit {
  EntityId id;
  Vec3 p;
  std::uint64_t salt;
};

// The oracle's own answer, as a list rather than a digest.
void collect_radius(const BruteForceOracle& o, std::uint32_t max_id, Vec3 c, float r,
                    EntityId exclude, std::uint64_t salt, std::vector<Hit>& out,
                    std::uint64_t& count) {
  const float r2 = r * r;
  for (EntityId id = 0; id <= max_id; ++id) {
    if (id == exclude) continue;
    Vec3 p{};
    if (!o.position_of(id, p)) continue;
    if (dist2(p, c) <= r2) {
      out.push_back(Hit{id, p, salt});
      ++count;
    }
  }
}

void collect_knn(const BruteForceOracle& o, std::uint32_t max_id, Vec3 c, std::uint32_t k,
                 std::vector<Hit>& out) {
  std::vector<Neighbour> all;
  for (EntityId id = 0; id <= max_id; ++id) {
    Vec3 p{};
    if (o.position_of(id, p)) all.push_back(Neighbour{dist2(p, c), id});
  }
  const std::size_t want = std::min<std::size_t>(k, all.size());
  std::partial_sort(all.begin(), all.begin() + static_cast<std::ptrdiff_t>(want), all.end(),
                    nearer);
  for (std::size_t i = 0; i < want; ++i) {
    Vec3 p{};
    o.position_of(all[i].id, p);
    // kNN results are delimited by a sentinel so the timed loop can restart
    // its ordered fold for each query.
    out.push_back(Hit{all[i].id, p, i == 0 ? 1u : 0u});
  }
}

// The hits gathered for one query must digest to exactly what the oracle
// answers, or the floor would be measuring a different question.
bool same_answer(const std::vector<Hit>& buf, std::size_t first, std::uint64_t oracle_value) {
  std::uint64_t acc = 0;
  for (std::size_t i = first; i < buf.size(); ++i) acc += digest_hit(buf[i].id, buf[i].p, buf[i].salt);
  return acc == oracle_value;
}

template <class F>
std::uint64_t time_min_ns(int reps, F&& f) {
  std::uint64_t best = ~0ull;
  for (int i = 0; i < reps; ++i) {
    const auto t0 = Clock::now();
    f();
    const auto t1 = Clock::now();
    const auto ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    best = std::min(best, ns);
  }
  return best;
}

double percentile(std::vector<std::uint64_t> v, double q) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  const double idx = q * static_cast<double>(v.size() - 1);
  const std::size_t lo = static_cast<std::size_t>(idx);
  const std::size_t hi = std::min(lo + 1, v.size() - 1);
  const double frac = idx - static_cast<double>(lo);
  return static_cast<double>(v[lo]) * (1.0 - frac) + static_cast<double>(v[hi]) * frac;
}

}  // namespace

int main(int argc, char** argv) {
  std::string path;
  int reps = 3;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--workload") && i + 1 < argc) path = argv[++i];
    else if (!std::strcmp(argv[i], "--reps") && i + 1 < argc) reps = std::atoi(argv[++i]);
  }
  if (path.empty()) {
    std::fprintf(stderr, "usage: gds_floor_spatial --workload FILE [--reps N]\n");
    return 2;
  }
  SpatialSpec spec;
  std::string error;
  if (!parse_spatial_file(path, spec, error)) {
    std::printf("{\"status\":\"workload_error\",\"error\":\"%s\"}\n", gds::json_escape(error).c_str());
    return 3;
  }
  const SpatialWorkload w = generate_spatial_workload(spec);
  WorldConfig cfg;
  cfg.bounds = w.bounds;
  cfg.expected_entities = spec.initial_entities;
  cfg.max_entity_id = w.max_entity_id;
  cfg.typical_query_radius = w.typical_query_radius;
  cfg.history_ticks = w.rewind_count ? spec.history_ticks : 0;

  BruteForceOracle oracle(cfg);
  CountingGrid grid(cfg);

  // The floor's own state: flat arrays and nothing else. History, when the
  // workload rewinds, is a full copy per tick taken outside the timed region,
  // and restoring it is timed as a copy: what the data alone costs to put back.
  const std::size_t n = static_cast<std::size_t>(w.max_entity_id) + 1;
  std::vector<Vec3> pos(n, Vec3{0, 0, 0});
  std::vector<std::uint8_t> live(n, 0);
  struct Snap {
    std::uint64_t tick;
    std::vector<Vec3> pos;
    std::vector<std::uint8_t> live;
  };
  std::vector<Snap> history;

  std::vector<std::uint64_t> state_ns, answer_ns, floor_ns;
  std::uint64_t radius_queries = 0, radius_hits = 0, radius_admitted = 0, knn_queries = 0;
  std::uint64_t max_hits = 0;
  std::vector<Hit> radius_buf, knn_buf;
  std::uint64_t sink = 0;

  for (std::uint32_t t = 0; t < w.ticks.size(); ++t) {
    const Tick& tk = w.ticks[t];
    radius_buf.clear();
    knn_buf.clear();

    // State, timed, on the flat arrays.
    std::uint64_t t_state = 0;
    {
      const auto t0 = Clock::now();
      for (std::uint32_t i = tk.op_begin; i < tk.op_end; ++i) {
        const Op& op = w.ops[i];
        switch (op.kind) {
          case SpatialOp::Insert: live[op.id] = 1; pos[op.id] = op.v; break;
          case SpatialOp::Remove: live[op.id] = 0; break;
          case SpatialOp::MoveBy:
            if (live[op.id]) pos[op.id] = wrap_into(add(pos[op.id], op.v), w.bounds);
            break;
          case SpatialOp::Rewind:
            for (std::size_t s = history.size(); s-- > 0;) {
              if (history[s].tick != op.k) continue;
              std::memcpy(pos.data(), history[s].pos.data(), n * sizeof(Vec3));
              std::memcpy(live.data(), history[s].live.data(), n);
              break;
            }
            break;
          default: break;
        }
      }
      const auto t1 = Clock::now();
      t_state = static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    }

    // Answers, found by the oracle and the grid in op order, untimed.
    for (std::uint32_t i = tk.op_begin; i < tk.op_end; ++i) {
      const Op& op = w.ops[i];
      switch (op.kind) {
        case SpatialOp::Insert: oracle.insert(op.id, op.v); grid.insert(op.id, op.v); break;
        case SpatialOp::Remove: oracle.remove(op.id); grid.remove(op.id); break;
        case SpatialOp::MoveBy: oracle.move_by(op.id, op.v); grid.move_by(op.id, op.v); break;
        case SpatialOp::Rewind: {
          oracle.rewind_to(op.k);
          grid = CountingGrid(cfg);
          for (EntityId id = 0; id <= w.max_entity_id; ++id) {
            Vec3 p{};
            if (oracle.position_of(id, p)) grid.insert(id, p);
          }
          break;
        }
        case SpatialOp::QueryRadius: {
          std::uint64_t c = 0;
          const std::size_t first = radius_buf.size();
          collect_radius(oracle, w.max_entity_id, op.v, op.radius, kNoEntity,
                         radius_salt(op.v, op.radius), radius_buf, c);
          if (!same_answer(radius_buf, first, oracle.query_radius(op.v, op.radius))) {
            std::printf("{\"status\":\"internal_error\",\"tick\":%u}\n", t);
            return 4;
          }
          radius_hits += c;
          max_hits = std::max(max_hits, c);
          radius_admitted += grid.admitted(op.v, op.radius);
          ++radius_queries;
          break;
        }
        case SpatialOp::QueryRadiusOf: {
          Vec3 centre{};
          if (!oracle.position_of(op.id, centre)) { ++radius_queries; break; }
          std::uint64_t c = 0;
          const std::size_t first = radius_buf.size();
          collect_radius(oracle, w.max_entity_id, centre, op.radius, op.id,
                         radius_salt(centre, op.radius), radius_buf, c);
          if (!same_answer(radius_buf, first, oracle.query_radius_of(op.id, op.radius))) {
            std::printf("{\"status\":\"internal_error\",\"tick\":%u}\n", t);
            return 4;
          }
          radius_hits += c;
          max_hits = std::max(max_hits, c);
          radius_admitted += grid.admitted_of(op.id, op.radius);
          ++radius_queries;
          break;
        }
        case SpatialOp::QueryKnn:
          collect_knn(oracle, w.max_entity_id, op.v, op.k, knn_buf);
          ++knn_queries;
          break;
      }
    }
    oracle.end_tick(t);
    if (cfg.history_ticks) {
      history.push_back(Snap{t, pos, live});
      while (history.size() > static_cast<std::size_t>(cfg.history_ticks) + 1)
        history.erase(history.begin());
    }

    // Answers, timed: the digest of exactly what is in them, from packed buffers.
    const std::uint64_t t_answers = time_min_ns(reps, [&] {
      std::uint64_t acc = 0;
      for (const Hit& h : radius_buf) acc += digest_hit(h.id, h.p, h.salt);
      KnnDigest d;
      for (const Hit& h : knn_buf) {
        if (h.salt) { acc ^= d.value(); d = KnnDigest{}; }
        d.push(h.id, h.p);
      }
      acc ^= d.value();
      gds::keep(acc);
      sink ^= acc;
    });

    if (t == 0) continue;  // the load tick is a different question
    state_ns.push_back(t_state);
    answer_ns.push_back(t_answers);
    floor_ns.push_back(t_state + t_answers);
  }
  gds::keep(sink);

  const double hits_per_query = radius_queries ? double(radius_hits) / radius_queries : 0.0;
  const double admitted_per_query = radius_queries ? double(radius_admitted) / radius_queries : 0.0;
  std::printf("{\n");
  std::printf("  \"status\": \"ok\",\n");
  std::printf("  \"workload\": \"%s\",\n", gds::json_escape(spec.id).c_str());
  std::printf("  \"visibility\": \"%s\",\n", gds::json_escape(spec.visibility).c_str());
  std::printf("  \"ticks_measured\": %zu,\n", floor_ns.size());
  std::printf("  \"floor_ns_p50\": %.0f,\n", percentile(floor_ns, 0.50));
  std::printf("  \"floor_ns_p99\": %.0f,\n", percentile(floor_ns, 0.99));
  std::printf("  \"state_ns_p50\": %.0f,\n", percentile(state_ns, 0.50));
  std::printf("  \"answer_ns_p50\": %.0f,\n", percentile(answer_ns, 0.50));
  std::printf("  \"radius_queries\": %llu,\n", (unsigned long long)radius_queries);
  std::printf("  \"knn_queries\": %llu,\n", (unsigned long long)knn_queries);
  std::printf("  \"hits_per_radius_query\": %.2f,\n", hits_per_query);
  std::printf("  \"max_hits_in_one_query\": %llu,\n", (unsigned long long)max_hits);
  std::printf("  \"grid_admitted_per_radius_query\": %.2f,\n", admitted_per_query);
  std::printf("  \"grid_over_admission\": %.3f\n",
              hits_per_query > 0 ? admitted_per_query / hits_per_query : 0.0);
  std::printf("}\n");
  return 0;
}
