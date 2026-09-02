#include "gds/workload.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace gds {

const char* access_name(Access a) {
  switch (a) {
    case Access::Uniform:
      return "uniform";
    case Access::Zipf:
      return "zipf";
    case Access::Recent:
      return "recent";
  }
  return "unknown";
}

namespace {

std::string trim(const std::string& s) {
  std::size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  std::size_t e = s.find_last_not_of(" \t\r\n");
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

bool parse_bool(const std::string& v, bool& out) {
  if (v == "true" || v == "1" || v == "yes") { out = true; return true; }
  if (v == "false" || v == "0" || v == "no") { out = false; return true; }
  return false;
}

bool parse_mask_token(const std::string& t, ComponentMask& out) {
  ComponentMask m = 0;
  std::stringstream ss(t);
  std::string part;
  while (std::getline(ss, part, '+')) {
    part = trim(part);
    if (part == "position") m |= kPosition;
    else if (part == "velocity") m |= kVelocity;
    else if (part == "health") m |= kHealth;
    else if (part == "tag") m |= kTag;
    else return false;
  }
  if (m == 0) return false;
  out = m;
  return true;
}

// Rejection-inversion sampling for Zipf, after Hörmann & Derflinger (1996).
// Exact for the given (n, exponent) and needs no precomputed table, so the
// distribution stays correct while the live-entity count moves every frame.
class ZipfSampler {
 public:
  explicit ZipfSampler(double exponent) : s_(exponent == 1.0 ? 1.000001 : exponent) {}

  std::uint32_t sample(std::uint32_t n, std::uint64_t& rng) const {
    if (n <= 1) return 0;
    const double h_x1 = h(1.5) - 1.0;
    const double h_n = h(static_cast<double>(n) + 0.5);
    const double s_const = 2.0 - h_inv(h(2.5) - std::pow(2.0, -s_));
    for (int guard = 0; guard < 64; ++guard) {
      const double u = h_n + next_unit(rng) * (h_x1 - h_n);
      const double x = h_inv(u);
      double k = std::floor(x + 0.5);
      if (k < 1.0) k = 1.0;
      if (k > static_cast<double>(n)) k = static_cast<double>(n);
      if (k - x <= s_const || u >= h(k + 0.5) - std::pow(k, -s_)) {
        return static_cast<std::uint32_t>(k) - 1;  // 0-based rank
      }
    }
    return 0;
  }

  static double next_unit(std::uint64_t& rng) {
    rng = splitmix64(rng);
    return static_cast<double>(rng >> 11) * (1.0 / 9007199254740992.0);
  }

 private:
  double h(double x) const { return (std::pow(x, 1.0 - s_) - 1.0) / (1.0 - s_); }
  double h_inv(double y) const { return std::pow((1.0 - s_) * y + 1.0, 1.0 / (1.0 - s_)); }
  double s_;
};

struct Rng {
  std::uint64_t state;
  std::uint64_t next() {
    state = splitmix64(state + 0x9E3779B97F4A7C15ull);
    return state;
  }
  std::uint32_t below(std::uint32_t n) {
    if (n == 0) return 0;
    return static_cast<std::uint32_t>(next() % n);
  }
  float unit() { return static_cast<float>((next() >> 40) * (1.0 / 16777216.0)); }
  float range(float lo, float hi) { return lo + unit() * (hi - lo); }
};

}  // namespace

bool parse_workload_text(const std::string& text, WorkloadSpec& out, std::string& error) {
  std::stringstream ss(text);
  std::string line;
  int line_no = 0;
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
    bool b = false;

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
    else if (key == "track") out.track = val;
    else if (key == "note") out.note = val;
    else if (key == "seed") { if (!parse_u64(val, u)) { error = "seed: expected an integer"; return false; } out.seed = u; }
    else if (key == "initial_entities") ok = need_u(out.initial_entities);
    else if (key == "max_entities") ok = need_u(out.max_entities);
    else if (key == "frames") ok = need_u(out.frames);
    else if (key == "ops_per_frame") ok = need_u(out.ops_per_frame);
    else if (key == "w_create") ok = need_f(out.w_create);
    else if (key == "w_destroy") ok = need_f(out.w_destroy);
    else if (key == "w_add") ok = need_f(out.w_add);
    else if (key == "w_remove") ok = need_f(out.w_remove);
    else if (key == "w_get") ok = need_f(out.w_get);
    else if (key == "w_set") ok = need_f(out.w_set);
    else if (key == "access") {
      if (val == "uniform") out.access = Access::Uniform;
      else if (val == "zipf") out.access = Access::Zipf;
      else if (val == "recent") out.access = Access::Recent;
      else { error = "access must be uniform, zipf or recent"; return false; }
    }
    else if (key == "zipf_exponent") { if (!parse_double(val, d)) { error = "zipf_exponent: expected a number"; return false; } out.zipf_exponent = d; }
    else if (key == "recency_window") ok = need_u(out.recency_window);
    else if (key == "burst_frame_ratio") ok = need_f(out.burst_frame_ratio);
    else if (key == "burst_multiplier") ok = need_u(out.burst_multiplier);
    else if (key == "stale_access_ratio") ok = need_f(out.stale_access_ratio);
    else if (key == "p_position") ok = need_f(out.p_position);
    else if (key == "p_velocity") ok = need_f(out.p_velocity);
    else if (key == "p_health") ok = need_f(out.p_health);
    else if (key == "p_tag") ok = need_f(out.p_tag);
    else if (key == "integrate_per_frame") { if (!parse_bool(val, b)) { error = "integrate_per_frame: expected a boolean"; return false; } out.integrate_per_frame = b; }
    else if (key == "dt") ok = need_f(out.dt);
    else if (key == "verify_sweep_frames") ok = need_u(out.verify_sweep_frames);
    else if (key == "query_masks") {
      out.query_masks.clear();
      std::stringstream ms(val);
      std::string tok;
      while (std::getline(ms, tok, ',')) {
        tok = trim(tok);
        if (tok.empty()) continue;
        ComponentMask m = 0;
        if (!parse_mask_token(tok, m)) {
          error = "query_masks: unknown component set '" + tok + "'";
          return false;
        }
        out.query_masks.push_back(m);
      }
    }
    else {
      error = "line " + std::to_string(line_no) + ": unknown key '" + key + "'";
      return false;
    }
    if (!ok) return false;
  }
  if (out.max_entities < out.initial_entities) out.max_entities = out.initial_entities;
  return true;
}

bool parse_workload_file(const std::string& path, WorkloadSpec& out, std::string& error) {
  std::ifstream in(path);
  if (!in) {
    error = "cannot open workload file: " + path;
    return false;
  }
  std::stringstream buf;
  buf << in.rdbuf();
  return parse_workload_text(buf.str(), out, error);
}

Workload generate_workload(const WorkloadSpec& spec) {
  Workload w;
  w.spec = spec;
  Rng rng{splitmix64(spec.seed | 1)};
  const ZipfSampler zipf(spec.zipf_exponent);
  std::uint64_t zipf_rng = splitmix64(spec.seed ^ 0xA5A5A5A5A5A5A5A5ull);

  // live[] holds slot indices of entities the generator believes are alive;
  // dead[] holds slots whose handles are stale. The generator mirrors only
  // liveness, never component values, so it stays cheap.
  std::vector<std::uint32_t> live;
  std::vector<std::uint32_t> dead;
  std::vector<std::uint32_t> live_pos;  // slot -> index in live, or UINT32_MAX
  std::uint32_t next_slot = 0;

  auto random_mask = [&]() {
    ComponentMask m = 0;
    if (rng.unit() < spec.p_position) m |= kPosition;
    if (rng.unit() < spec.p_velocity) m |= kVelocity;
    if (rng.unit() < spec.p_health) m |= kHealth;
    if (rng.unit() < spec.p_tag) m |= kTag;
    if (m == 0) m = kPosition;
    return m;
  };

  auto fill_values = [&](ComponentValue* v) {
    v[0].position = Position{rng.range(-512.f, 512.f), rng.range(-64.f, 64.f),
                             rng.range(-512.f, 512.f)};
    v[1].velocity = Velocity{rng.range(-8.f, 8.f), rng.range(-2.f, 2.f), rng.range(-8.f, 8.f)};
    v[2].health = Health{static_cast<std::int32_t>(1 + rng.below(100)), 100};
    v[3].tag = Tag{static_cast<std::uint32_t>(rng.next() >> 32)};
  };

  // Create carries no payload: the four initial component values are a pure
  // function of the slot index (create_values), so the oracle and every
  // candidate construct identical entities without widening the op record.
  auto emit_create = [&](std::vector<Op>& ops) {
    Op op{};
    op.kind = OpKind::Create;
    op.mask = random_mask();
    op.slot = next_slot;
    ops.push_back(op);
    live_pos.push_back(static_cast<std::uint32_t>(live.size()));
    live.push_back(next_slot);
    ++next_slot;
  };

  auto pick_live = [&]() -> std::uint32_t {
    const std::uint32_t n = static_cast<std::uint32_t>(live.size());
    if (n == 0) return UINT32_MAX;
    switch (spec.access) {
      case Access::Uniform:
        return live[rng.below(n)];
      case Access::Zipf: {
        const std::uint32_t rank = zipf.sample(n, zipf_rng);
        return live[rank < n ? rank : n - 1];
      }
      case Access::Recent: {
        const std::uint32_t win = std::min<std::uint32_t>(spec.recency_window, n);
        return live[n - 1 - rng.below(win)];
      }
    }
    return live[0];
  };

  auto pick_target = [&]() -> std::uint32_t {
    if (!dead.empty() && rng.unit() < spec.stale_access_ratio) {
      return dead[rng.below(static_cast<std::uint32_t>(dead.size()))];
    }
    return pick_live();
  };

  auto kill_slot = [&](std::uint32_t slot) {
    const std::uint32_t idx = live_pos[slot];
    if (idx == UINT32_MAX) return;
    const std::uint32_t last = live.back();
    live[idx] = last;
    live_pos[last] = idx;
    live.pop_back();
    live_pos[slot] = UINT32_MAX;
    dead.push_back(slot);
  };

  const float total_w = std::max(0.0001f, spec.w_create + spec.w_destroy + spec.w_add +
                                              spec.w_remove + spec.w_get + spec.w_set);

  for (std::uint32_t i = 0; i < spec.initial_entities; ++i) {
    emit_create(w.ops);
  }
  w.frames.push_back(Frame{0, static_cast<std::uint32_t>(w.ops.size())});

  for (std::uint32_t f = 1; f < spec.frames; ++f) {
    const std::uint32_t begin = static_cast<std::uint32_t>(w.ops.size());
    std::uint32_t count = spec.ops_per_frame;
    if (spec.burst_frame_ratio > 0.f && rng.unit() < spec.burst_frame_ratio) {
      count *= std::max<std::uint32_t>(1, spec.burst_multiplier);
    }
    for (std::uint32_t i = 0; i < count; ++i) {
      const float r = rng.unit() * total_w;
      float acc = spec.w_create;
      if (r < acc && live.size() < spec.max_entities) {
        emit_create(w.ops);
        continue;
      }
      acc += spec.w_destroy;
      if (r < acc) {
        const std::uint32_t slot = pick_live();
        if (slot == UINT32_MAX) continue;
        Op op{};
        op.kind = OpKind::Destroy;
        op.slot = slot;
        w.ops.push_back(op);
        kill_slot(slot);
        continue;
      }
      const std::uint32_t slot = pick_target();
      if (slot == UINT32_MAX) continue;
      Op op{};
      op.slot = slot;
      op.comp = static_cast<ComponentId>(rng.below(kComponentCount));
      ComponentValue vals[kComponentCount];
      fill_values(vals);
      op.value = vals[static_cast<int>(op.comp)];
      acc += spec.w_add;
      if (r < acc) { op.kind = OpKind::Add; w.ops.push_back(op); continue; }
      acc += spec.w_remove;
      if (r < acc) { op.kind = OpKind::Remove; w.ops.push_back(op); continue; }
      acc += spec.w_get;
      if (r < acc) { op.kind = OpKind::Get; w.ops.push_back(op); continue; }
      op.kind = OpKind::Set;
      w.ops.push_back(op);
    }
    w.frames.push_back(Frame{begin, static_cast<std::uint32_t>(w.ops.size())});
  }

  w.slot_count = next_slot;
  return w;
}

}  // namespace gds
