// Spatial workloads.
//
// The dimensions that decide a spatial index are how fast things move relative
// to how far a query reaches, how unevenly they are spread, where the queries
// are aimed, and how often the world is asked to go back. Each is a field here.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "gds/spatial/types.hpp"

namespace gds::spatial {

enum class SpatialOp : std::uint8_t {
  Insert = 0,
  Remove,
  MoveBy,
  QueryRadius,
  QueryRadiusOf,
  QueryKnn,
  Rewind,
};

struct Op {
  SpatialOp kind;
  EntityId id = kNoEntity;   // Insert/Remove/MoveBy/QueryRadiusOf
  Vec3 v{0, 0, 0};           // Insert position, MoveBy delta, query centre
  float radius = 0.0f;       // QueryRadius / QueryRadiusOf
  std::uint32_t k = 0;       // QueryKnn: neighbours. Rewind: target tick.
};

struct Tick {
  std::uint32_t op_begin;
  std::uint32_t op_end;
};

enum class Placement { Uniform, Clustered };

struct SpatialSpec {
  std::string id = "unnamed";
  std::string visibility = "public";
  std::string track = "spatial";
  std::string note;

  std::uint64_t seed = 1;
  float world_size = 1024.0f;       // cube side; the world is centred on the origin
  float world_height = 1024.0f;     // z extent, so a flat world can be expressed

  std::uint32_t initial_entities = 20000;
  std::uint32_t ticks = 300;
  std::uint32_t inserts_per_tick = 0;
  std::uint32_t removes_per_tick = 0;

  // How many entities move each tick, given either as a share of the live
  // population or as an absolute count. A scaling experiment needs the absolute
  // form: holding the operation count fixed while the population grows is what
  // separates the cost of one operation from the number of them.
  float move_fraction = 1.0f;
  std::uint32_t moves_per_tick = 0;  // 0 leaves move_fraction in charge
  float speed_min = 0.0f;           // per-tick displacement, world units
  float speed_max = 2.0f;
  float teleport_ratio = 0.0f;      // share of moves that jump anywhere in the world

  Placement placement = Placement::Uniform;
  std::uint32_t clusters = 16;
  float cluster_radius = 32.0f;

  std::uint32_t radius_queries_per_tick = 64;
  std::uint32_t entity_radius_queries_per_tick = 64;
  std::uint32_t knn_queries_per_tick = 16;
  float query_radius_min = 8.0f;
  float query_radius_max = 16.0f;
  std::uint32_t knn_k = 8;
  Placement query_focus = Placement::Uniform;

  std::uint32_t rewind_every = 0;   // 0 disables rewinds
  std::uint32_t rewind_depth = 8;
  std::uint32_t history_ticks = 0;  // retained history; 0 means none is needed

  std::uint32_t verify_sweep_ticks = 16;
};

struct SpatialWorkload {
  SpatialSpec spec;
  std::vector<Op> ops;
  std::vector<Tick> ticks;
  std::uint32_t max_entity_id = 0;
  std::uint32_t rewind_count = 0;
  Bounds bounds{};
  float typical_query_radius = 1.0f;
};

bool parse_spatial_file(const std::string& path, SpatialSpec& out, std::string& error);
bool parse_spatial_text(const std::string& text, SpatialSpec& out, std::string& error);
SpatialWorkload generate_spatial_workload(const SpatialSpec& spec);
const char* placement_name(Placement p);

}  // namespace gds::spatial
