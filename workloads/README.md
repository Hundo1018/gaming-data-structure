# Workloads

A workload is a research object. Each file is generated once from its seed into
a concrete op stream, which the oracle and every candidate then replay
identically. Generation cost never lands inside a measurement, and two
candidates are never compared on different work.

`public/` may be seen by whatever proposes candidates. `hidden/` is held out;
the report ranks candidates on the two sets separately and prints the
difference, which is how a candidate tuned to what it could see becomes visible.

Each file names its `track`. A candidate only ever meets workloads of its own
track: the two tracks ask different questions of different structures, and a
category error would show up as a parse failure rather than as what it is.

## Format

Flat `key: value`, one per line, `#` starts a comment. An unknown key is an
error, not a warning: a silently ignored field would make two different
experiments look like the same one. The C++ parsers
(`substrate/src/workload.cpp` for the ECS track,
`substrate/src/spatial_workload.cpp` for the spatial track) and the Python one
(`runner/orchestrate.py`) read the same grammar.

## ECS track keys

| key | meaning |
|---|---|
| `id` | name used in results and in the archive |
| `visibility` | `public` or `hidden` |
| `seed` | everything generated is a function of this |
| `initial_entities` | population created in frame 0 |
| `max_entities` | cap; at the cap, creates become destroys |
| `frames` | number of frames, including the load frame |
| `ops_per_frame` | operations per frame before any burst multiplier |
| `w_create` `w_destroy` `w_add` `w_remove` `w_get` `w_set` | relative weights, normalised at generation |
| `access` | `uniform`, `zipf`, or `recent` |
| `zipf_exponent` | skew for `access: zipf`; sampled exactly, by rejection inversion |
| `recency_window` | how far back `access: recent` reaches |
| `burst_frame_ratio` `burst_multiplier` | fraction of frames carrying a burst, and how much larger |
| `stale_access_ratio` | fraction of accesses aimed at already-destroyed handles |
| `p_position` `p_velocity` `p_health` `p_tag` | probability a new entity carries each component |
| `integrate_per_frame` | run `p += v*dt` over matching entities each frame |
| `dt` | timestep for integrate |
| `query_masks` | comma-separated, each a `+`-joined component set, e.g. `position+velocity, position+health` |
| `verify_sweep_frames` | how often verification re-checks every slot ever created |

## Spatial track keys

A spatial tick is: an optional rewind, then inserts, then removes, then moves,
then queries. Everything is generated once from the seed into an op stream, and
the oracle and every candidate replay it identically.

| key | meaning |
|---|---|
| `world_size` | side of the world box in x and y; the world is centred on the origin |
| `world_height` | z extent, so a flat world is expressible |
| `initial_entities` | population created in tick 0 |
| `ticks` | number of ticks, including the load tick |
| `inserts_per_tick` `removes_per_tick` | churn |
| `move_fraction` | share of live entities that move each tick |
| `speed_min` `speed_max` | per-tick displacement in world units |
| `teleport_ratio` | share of moves that jump anywhere in the world |
| `placement` | `uniform` or `clustered` initial positions |
| `clusters` `cluster_radius` | number of clumps and their spread |
| `radius_queries_per_tick` | "what is near this point" |
| `entity_radius_queries_per_tick` | "what is near this entity", excluding itself |
| `knn_queries_per_tick` `knn_k` | "the k nearest to this point" |
| `query_radius_min` `query_radius_max` | query reach; the mean is what a structure is told to expect |
| `query_focus` | `uniform` or `clustered` query centres |
| `rewind_every` `rewind_depth` | how often the world is rolled back and by how much |
| `history_ticks` | history a structure is told it must retain |
| `verify_sweep_ticks` | how often verification re-checks every id ever issued |

Movement is a delta, not a destination, and the new position is
`wrap_into(current + delta, bounds)` using the one shared wrap. That is what
lets the generator hold no positions at all: a rewind restores them from the
structure's own history, and nothing here needs to know where anything is.

A rewind is a real branch, not a replay. After rolling back, the generator
continues with fresh operations, and ids issued in the discarded ticks are never
reissued.

## Scaling experiments

`sweep/` holds the workloads that differ in exactly one thing. Everything in
`public/` and `hidden/` differs in several at once, which is right for comparing
candidates and useless for measuring growth: `s05_small_world` changes the
population, the world size and the query radius together, so no curve can be
fitted through it and anything else.

`sweep/sweeps.yaml` declares the families, the regimes and the populations;
`runner/sweep.py` applies them. The rule that makes the numbers mean anything is
that **the number of operations per step is held constant while the population
grows** — with `moves_per_tick` rather than `move_fraction`, with a fixed count
of queries per tick, and with `ops_per_frame` fixed on the ECS side. If the
operation count grew with the population, every candidate would measure linear
regardless of what it does.

Two regimes exist because they answer different questions. Holding the world
fixed lets density grow, so a query of fixed radius returns proportionally more
and the exponent includes the growth of the answer. Growing the world as the
cube root of the population holds density constant, so the exponent is the cost
of finding the answer — which is what a complexity claim is about.

## Dimensions the current set does not cover

Recorded here rather than left implicit, because an uncovered dimension is a
claim nobody has tested:

- **Multi-component point access.** Every `get` and `set` names one randomly
  chosen component, so no workload reads several components of one entity in one
  operation. That is the access pattern most gameplay code has, and it is the
  case `aos` is built for; see `candidates/ecs/aos/notes.md`.
- **Wide components.** The four component types total 36 bytes. Nothing here
  tests a layout whose cost hinges on a copy too large to stay in cache; see
  `candidates/ecs/archetype/notes.md`.
- **Spatial locality.** A workload dimension in `PROJECT.md`, but one that
  belongs to the spatial-query track. The ECS workloads vary temporal locality,
  skew and burstiness only.
- **Concurrency.** Everything here is single-threaded, in both tracks.
- **Spatial: non-uniform query reach.** Every structure is told one typical
  query radius and sizes itself from it. Nothing tests a world where some
  systems ask for two metres and others for two hundred.
- **Spatial: correlated movement.** Entities move independently. Nothing tests
  a crowd moving together, which is what would keep a cluster dense while it
  travels rather than letting it diffuse.
- **Spatial: rewind under churn shape.** `hs01` and `hs02` vary how much moves.
  Nothing varies rewind depth against a fixed movement rate, which is the other
  axis of the same trade. No sweep family rewinds at all, so the history
  strategies have a comparison at two points and no curve.
