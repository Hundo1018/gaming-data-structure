// A workload is a research object, not a benchmark parameter.
//
// It is generated once from a seed into a concrete op stream, then replayed
// byte-identically by the oracle and by every candidate. Generation cost is
// therefore never inside a measurement, and two candidates are always compared
// on exactly the same sequence of operations.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "gds/types.hpp"

namespace gds {

enum class OpKind : std::uint8_t {
  Create = 0,
  Destroy,
  Add,
  Remove,
  Get,
  Set,
};

struct Op {
  OpKind kind;
  ComponentId comp;
  ComponentMask mask;   // Create only
  std::uint32_t slot;   // index into the harness slot table
  ComponentValue value;
};

// Initial component values for a created entity. A pure function of the slot
// index so that the oracle and every candidate build identical entities from an
// op record that carries no payload.
inline void create_values(std::uint32_t slot, ComponentValue* out) {
  const std::uint64_t a = splitmix64(0x9E3779B97F4A7C15ull ^ slot);
  const std::uint64_t b = splitmix64(a);
  const std::uint64_t c = splitmix64(b);
  auto f = [](std::uint64_t bits, float lo, float hi) {
    const float u = static_cast<float>((bits >> 40) * (1.0 / 16777216.0));
    return lo + u * (hi - lo);
  };
  out[0].position = Position{f(a, -512.f, 512.f), f(a >> 20, -64.f, 64.f), f(b, -512.f, 512.f)};
  out[1].velocity = Velocity{f(b >> 20, -8.f, 8.f), f(c, -2.f, 2.f), f(c >> 20, -8.f, 8.f)};
  out[2].health = Health{static_cast<std::int32_t>(1 + (a % 100)), 100};
  out[3].tag = Tag{static_cast<std::uint32_t>(c >> 32)};
}

struct Frame {
  std::uint32_t op_begin;
  std::uint32_t op_end;
};

// Access distribution over the currently live slots.
enum class Access {
  Uniform,   // every live entity equally likely
  Zipf,      // heavy head: a few entities take most of the traffic
  Recent,    // temporal locality: entities created most recently
};

struct WorkloadSpec {
  std::string id = "unnamed";
  std::string visibility = "public";  // public | hidden
  std::string track = "ecs";
  std::string note;

  std::uint64_t seed = 1;
  std::uint32_t initial_entities = 10000;
  std::uint32_t max_entities = 200000;
  std::uint32_t frames = 300;
  std::uint32_t ops_per_frame = 1000;

  // Relative weights, normalised at generation time.
  float w_create = 1.0f;
  float w_destroy = 1.0f;
  float w_add = 0.5f;
  float w_remove = 0.5f;
  float w_get = 4.0f;
  float w_set = 3.0f;

  Access access = Access::Uniform;
  double zipf_exponent = 1.1;
  std::uint32_t recency_window = 1024;

  float burst_frame_ratio = 0.0f;    // fraction of frames that carry a burst
  std::uint32_t burst_multiplier = 8;

  float stale_access_ratio = 0.0f;   // fraction of accesses aimed at dead handles

  // Probability that a newly created entity carries each component.
  float p_position = 1.0f;
  float p_velocity = 0.8f;
  float p_health = 0.5f;
  float p_tag = 0.3f;

  bool integrate_per_frame = true;
  float dt = 0.016666668f;
  std::vector<ComponentMask> query_masks = {kPosition | kVelocity};

  std::uint32_t verify_sweep_frames = 32;  // full oracle sweep every N frames
};

struct Workload {
  WorkloadSpec spec;
  std::vector<Op> ops;
  std::vector<Frame> frames;
  std::uint32_t slot_count = 0;  // total slots the replay will allocate
};

// Parses the flat `key: value` workload format. Returns false and fills `error`
// on an unknown key or an unparsable value: a silently ignored field would make
// two different experiments look like the same one.
bool parse_workload_file(const std::string& path, WorkloadSpec& out, std::string& error);
bool parse_workload_text(const std::string& text, WorkloadSpec& out, std::string& error);

Workload generate_workload(const WorkloadSpec& spec);

const char* access_name(Access a);

}  // namespace gds
