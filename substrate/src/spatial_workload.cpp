#include "gds/spatial/workload.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace gds::spatial {

const char* placement_name(Placement p) {
  return p == Placement::Uniform ? "uniform" : "clustered";
}

namespace {

std::string trim(const std::string& s) {
  const std::size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  const std::size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

bool parse_u64(const std::string& v, std::uint64_t& out) {
  char* end = nullptr;
  const unsigned long long x = std::strtoull(v.c_str(), &end, 10);
  if (end == v.c_str() || *end != '\0') return false;
  out = x;
  return true;
}

bool parse_double(const std::string& v, double& out) {
  char* end = nullptr;
  const double x = std::strtod(v.c_str(), &end);
  if (end == v.c_str() || *end != '\0') return false;
  out = x;
  return true;
}

bool parse_placement(const std::string& v, Placement& out) {
  if (v == "uniform") { out = Placement::Uniform; return true; }
  if (v == "clustered") { out = Placement::Clustered; return true; }
  return false;
}

struct Rng {
  std::uint64_t state;
  std::uint64_t next() {
    state = splitmix64(state + 0x9E3779B97F4A7C15ull);
    return state;
  }
  std::uint32_t below(std::uint32_t n) { return n ? static_cast<std::uint32_t>(next() % n) : 0; }
  float unit() { return static_cast<float>((next() >> 40) * (1.0 / 16777216.0)); }
  float range(float lo, float hi) { return lo + unit() * (hi - lo); }
  // Sum of three uniforms: a cheap bell shape, so a cluster has a dense middle
  // and a thin edge instead of a hard rim.
  float bell() { return (unit() + unit() + unit()) * (2.0f / 3.0f) - 1.0f; }
};

}  // namespace

bool parse_spatial_text(const std::string& text, SpatialSpec& out, std::string& error) {
  std::stringstream ss(text);
  std::string line;
  int line_no = 0;
  bool seen_move_fraction = false;
  bool seen_moves_per_tick = false;
  while (std::getline(ss, line)) {
    ++line_no;
    const std::size_t hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    line = trim(line);
    if (line.empty()) continue;
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos) {
      error = "line " + std::to_string(line_no) + ": expected 'key: value'";
      return false;
    }
    const std::string key = trim(line.substr(0, colon));
    const std::string val = trim(line.substr(colon + 1));

    std::uint64_t u = 0;
    double d = 0;
    auto need_u = [&](std::uint32_t& dst) {
      if (!parse_u64(val, u)) { error = key + ": expected an integer"; return false; }
      dst = static_cast<std::uint32_t>(u);
      return true;
    };
    auto need_f = [&](float& dst) {
      if (!parse_double(val, d)) { error = key + ": expected a number"; return false; }
      dst = static_cast<float>(d);
      return true;
    };

    bool ok = true;
    if (key == "id") out.id = val;
    else if (key == "visibility") {
      if (val != "public" && val != "hidden") { error = "visibility must be public or hidden"; return false; }
      out.visibility = val;
    }
    else if (key == "track") {
      if (val != "spatial") { error = "this binary only runs track: spatial"; return false; }
      out.track = val;
    }
    else if (key == "note") out.note = val;
    else if (key == "seed") { if (!parse_u64(val, u)) { error = "seed: expected an integer"; return false; } out.seed = u; }
    else if (key == "world_size") ok = need_f(out.world_size);
    else if (key == "world_height") ok = need_f(out.world_height);
    else if (key == "initial_entities") ok = need_u(out.initial_entities);
    else if (key == "ticks") ok = need_u(out.ticks);
    else if (key == "inserts_per_tick") ok = need_u(out.inserts_per_tick);
    else if (key == "removes_per_tick") ok = need_u(out.removes_per_tick);
    else if (key == "move_fraction") { seen_move_fraction = true; ok = need_f(out.move_fraction); }
    else if (key == "moves_per_tick") { seen_moves_per_tick = true; ok = need_u(out.moves_per_tick); }
    else if (key == "speed_min") ok = need_f(out.speed_min);
    else if (key == "speed_max") ok = need_f(out.speed_max);
    else if (key == "teleport_ratio") ok = need_f(out.teleport_ratio);
    else if (key == "placement") { if (!parse_placement(val, out.placement)) { error = "placement must be uniform or clustered"; return false; } }
    else if (key == "clusters") ok = need_u(out.clusters);
    else if (key == "cluster_radius") ok = need_f(out.cluster_radius);
    else if (key == "radius_queries_per_tick") ok = need_u(out.radius_queries_per_tick);
    else if (key == "entity_radius_queries_per_tick") ok = need_u(out.entity_radius_queries_per_tick);
    else if (key == "knn_queries_per_tick") ok = need_u(out.knn_queries_per_tick);
    else if (key == "query_radius_min") ok = need_f(out.query_radius_min);
    else if (key == "query_radius_max") ok = need_f(out.query_radius_max);
    else if (key == "knn_k") ok = need_u(out.knn_k);
    else if (key == "query_focus") { if (!parse_placement(val, out.query_focus)) { error = "query_focus must be uniform or clustered"; return false; } }
    else if (key == "rewind_every") ok = need_u(out.rewind_every);
    else if (key == "rewind_depth") ok = need_u(out.rewind_depth);
    else if (key == "history_ticks") ok = need_u(out.history_ticks);
    else if (key == "verify_sweep_ticks") ok = need_u(out.verify_sweep_ticks);
    else {
      error = "line " + std::to_string(line_no) + ": unknown key '" + key + "'";
      return false;
    }
    if (!ok) return false;
  }
  if (seen_move_fraction && seen_moves_per_tick) {
    error = "set move_fraction or moves_per_tick, not both: they are two ways of "
            "saying the same thing and a file that sets both does not say which it means";
    return false;
  }
  if (out.rewind_every > 0 && out.history_ticks < out.rewind_depth) {
    out.history_ticks = out.rewind_depth;
  }
  return true;
}

bool parse_spatial_file(const std::string& path, SpatialSpec& out, std::string& error) {
  std::ifstream in(path);
  if (!in) {
    error = "cannot open workload file: " + path;
    return false;
  }
  std::stringstream buf;
  buf << in.rdbuf();
  return parse_spatial_text(buf.str(), out, error);
}

SpatialWorkload generate_spatial_workload(const SpatialSpec& spec) {
  SpatialWorkload w;
  w.spec = spec;
  const float hx = spec.world_size * 0.5f;
  const float hz = spec.world_height * 0.5f;
  w.bounds = Bounds{Vec3{-hx, -hx, -hz}, Vec3{hx, hx, hz}};
  w.typical_query_radius = 0.5f * (spec.query_radius_min + spec.query_radius_max);

  Rng rng{splitmix64(spec.seed | 1)};

  // Cluster centres are fixed for the whole run, so a clustered world stays
  // clustered where the queries are aimed even as entities drift.
  std::vector<Vec3> centres;
  const std::uint32_t ncl = std::max<std::uint32_t>(1, spec.clusters);
  centres.reserve(ncl);
  for (std::uint32_t i = 0; i < ncl; ++i) {
    centres.push_back(Vec3{rng.range(-hx, hx), rng.range(-hx, hx), rng.range(-hz, hz)});
  }

  auto sample_point = [&](Placement mode) {
    if (mode == Placement::Uniform) {
      return Vec3{rng.range(-hx, hx), rng.range(-hx, hx), rng.range(-hz, hz)};
    }
    const Vec3& c = centres[rng.below(ncl)];
    const float r = spec.cluster_radius;
    return wrap_into(Vec3{c.x + rng.bell() * r, c.y + rng.bell() * r, c.z + rng.bell() * r},
                     w.bounds);
  };

  // The generator mirrors liveness only. Positions live in the structures, so a
  // rewind needs no position history here.
  const std::uint32_t max_ids =
      spec.initial_entities + spec.inserts_per_tick * spec.ticks + 1;
  std::vector<std::uint8_t> live(max_ids, 0);
  std::vector<EntityId> live_list;
  live_list.reserve(max_ids);
  std::uint32_t next_id = 0;

  struct TickDelta {
    std::vector<EntityId> inserted;
    std::vector<EntityId> removed;
  };
  std::vector<TickDelta> deltas(spec.ticks);

  auto rebuild_live_list = [&]() {
    live_list.clear();
    for (std::uint32_t i = 0; i < next_id; ++i) {
      if (live[i]) live_list.push_back(i);
    }
  };

  auto do_insert = [&](std::uint32_t tick, std::vector<Op>& ops) {
    const EntityId id = next_id++;
    live[id] = 1;
    live_list.push_back(id);
    deltas[tick].inserted.push_back(id);
    Op op{};
    op.kind = SpatialOp::Insert;
    op.id = id;
    op.v = sample_point(spec.placement);
    ops.push_back(op);
  };

  auto do_remove = [&](std::uint32_t tick, std::vector<Op>& ops) {
    if (live_list.empty()) return;
    const std::uint32_t idx = rng.below(static_cast<std::uint32_t>(live_list.size()));
    const EntityId id = live_list[idx];
    live_list[idx] = live_list.back();
    live_list.pop_back();
    live[id] = 0;
    deltas[tick].removed.push_back(id);
    Op op{};
    op.kind = SpatialOp::Remove;
    op.id = id;
    ops.push_back(op);
  };

  // Tick 0 is the load tick: it builds the initial population and nothing else.
  {
    std::vector<Op> ops;
    for (std::uint32_t i = 0; i < spec.initial_entities; ++i) do_insert(0, ops);
    const std::uint32_t begin = static_cast<std::uint32_t>(w.ops.size());
    w.ops.insert(w.ops.end(), ops.begin(), ops.end());
    w.ticks.push_back(Tick{begin, static_cast<std::uint32_t>(w.ops.size())});
  }

  for (std::uint32_t t = 1; t < spec.ticks; ++t) {
    const std::uint32_t begin = static_cast<std::uint32_t>(w.ops.size());
    deltas[t].inserted.clear();
    deltas[t].removed.clear();

    const bool rewinding = spec.rewind_every > 0 && (t % spec.rewind_every) == 0 &&
                           t > spec.rewind_depth;
    if (rewinding) {
      const std::uint32_t target = t - spec.rewind_depth - 1;
      Op op{};
      op.kind = SpatialOp::Rewind;
      op.k = target;
      w.ops.push_back(op);
      ++w.rewind_count;
      // Roll the generator's own liveness model back to the end of `target`,
      // then carry on down a different branch: ids created in the discarded
      // ticks are never reissued, so nothing is ambiguous after the branch.
      for (std::uint32_t back = t - 1; back > target; --back) {
        for (EntityId id : deltas[back].inserted) live[id] = 0;
        for (EntityId id : deltas[back].removed) live[id] = 1;
        deltas[back].inserted.clear();
        deltas[back].removed.clear();
      }
      rebuild_live_list();
    }

    for (std::uint32_t i = 0; i < spec.inserts_per_tick; ++i) do_insert(t, w.ops);
    for (std::uint32_t i = 0; i < spec.removes_per_tick; ++i) do_remove(t, w.ops);

    const std::size_t n_live = live_list.size();
    std::uint32_t movers =
        spec.moves_per_tick > 0
            ? spec.moves_per_tick
            : static_cast<std::uint32_t>(static_cast<double>(n_live) * spec.move_fraction);
    if (n_live == 0) movers = 0;
    for (std::uint32_t i = 0; i < movers; ++i) {
      const EntityId id = live_list[rng.below(static_cast<std::uint32_t>(n_live))];
      Op op{};
      op.kind = SpatialOp::MoveBy;
      op.id = id;
      if (spec.teleport_ratio > 0.0f && rng.unit() < spec.teleport_ratio) {
        // A delta large enough to land anywhere once wrapped: from the index's
        // point of view nothing distinguishes this from a very fast entity.
        op.v = Vec3{rng.range(-hx * 2.0f, hx * 2.0f), rng.range(-hx * 2.0f, hx * 2.0f),
                    rng.range(-hz * 2.0f, hz * 2.0f)};
      } else {
        const float speed = rng.range(spec.speed_min, spec.speed_max);
        float dx = rng.bell(), dy = rng.bell(), dz = rng.bell();
        const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float scale = len > 1e-6f ? speed / len : 0.0f;
        op.v = Vec3{dx * scale, dy * scale, dz * scale};
      }
      w.ops.push_back(op);
    }

    for (std::uint32_t i = 0; i < spec.radius_queries_per_tick; ++i) {
      Op op{};
      op.kind = SpatialOp::QueryRadius;
      op.v = sample_point(spec.query_focus);
      op.radius = rng.range(spec.query_radius_min, spec.query_radius_max);
      w.ops.push_back(op);
    }
    if (!live_list.empty()) {
      for (std::uint32_t i = 0; i < spec.entity_radius_queries_per_tick; ++i) {
        Op op{};
        op.kind = SpatialOp::QueryRadiusOf;
        op.id = live_list[rng.below(static_cast<std::uint32_t>(live_list.size()))];
        op.radius = rng.range(spec.query_radius_min, spec.query_radius_max);
        w.ops.push_back(op);
      }
    }
    for (std::uint32_t i = 0; i < spec.knn_queries_per_tick; ++i) {
      Op op{};
      op.kind = SpatialOp::QueryKnn;
      op.v = sample_point(spec.query_focus);
      op.k = spec.knn_k;
      w.ops.push_back(op);
    }

    w.ticks.push_back(Tick{begin, static_cast<std::uint32_t>(w.ops.size())});
  }

  w.max_entity_id = next_id ? next_id - 1 : 0;
  return w;
}

}  // namespace gds::spatial
