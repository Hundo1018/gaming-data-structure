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
| `access_width` | components one drawn `get` or `set` touches on the same entity, as consecutive operations over distinct components; counts as that many operations of the frame. Default 1 |
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
| `movement` | `independent` (each mover takes its own step) or `flock` (each mover adds its cluster's drift to its own step, so a clump travels as a clump) |
| `flock_speed` | per-tick drift of each cluster under `movement: flock` |
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
`runner/sweep.py` applies them. The rule that makes a population family's
numbers mean anything is that **the number of operations per step is held
constant while the population grows** — with `moves_per_tick` rather than
`move_fraction`, with a fixed count of queries per tick, and with
`ops_per_frame` fixed on the ECS side. If the operation count grew with the
population, every candidate would measure linear regardless of what it does.
The parameter sweeps below hold the population fixed instead, so the rule is
not theirs: a sweep of `move_fraction` changes the moves per tick because that
is what it varies.

Two regimes exist because they answer different questions. Holding the world
fixed lets density grow, so a query of fixed radius returns proportionally more
and the exponent includes the growth of the answer. Growing the world as the
cube root of the population holds density constant, so the exponent is the cost
of finding the answer — which is what a complexity claim is about.

### Parameter sweeps

A family that declares `vary: {key, values}` in place of regimes is a parameter
sweep. Its template fixes the population and the world; `runner/sweep.py` gives
the one named key each listed value in turn, verifies every candidate of the
track at every value, and then measures the candidates in turn at each value,
so that a slow minute on a shared machine falls on both sides of a comparison.
Nothing is fitted, because the axis is not a size and no complexity claim is
about it. `benchmarks/scaling.md` prints a table per objective — median tick,
p99 tick, peak bytes — with the rewind strategy each binary reported, and for
every pair of candidates whose order changes, the two measured values on either
side of the change. The crossing is stated as that interval and never placed
inside it: nothing between the two values was run.

The p99 tick is the harness's 99th percentile over every tick of the run, the
load tick included. It is the cost of a rewind only where a candidate's rewinds
are its costliest ticks; where a rewind costs about what an ordinary tick does,
the p99 is an ordinary tick, and a p99 crossing is one of the tick tail. The
report says which tick of each family's run the p99 is, and `sweeps.yaml` says
where that falls among the rewinds.

Two parameter sweeps compare history strategies on the spatial track, at 30000
entities with the churn and query mix of `hs01`:

- **`rewind_move_fraction`** varies `move_fraction` from 0.01 to 1.0 with a
  rewind every 10 ticks, 6 deep.
- **`rewind_depth`** varies `rewind_depth` from 1 to 32 at a `move_fraction` of
  0.1. It rewinds every 40 ticks rather than every 10, because a rewind at
  least as deep as the interval since the previous one targets a tick that the
  previous rewind discarded. Neither the oracle nor the candidate can rewind to
  it, both say so, verification passes, and the rewind silently does not
  happen. `sweep.py` refuses such a workload. `history_ticks` is 32 at every
  depth, so the retained history is the same at every point.

Both give every value the same 9 rewinds. `sweep.py` also refuses a sweep
point that sets `history_ticks` below its `rewind_depth`, which the parser
would raise without saying so, and a depth of 0 with no history, on which the
oracle and the substrate's history wrappers disagree. `sweeps.yaml` states each
constant and its reason.

## Dimensions the current set does not cover

Recorded here rather than left implicit, because an uncovered dimension is a
claim nobody has tested:

- **Multi-component point access** is covered by one controlled pair,
  `w06_point_narrow` and `w07_point_wide`, which differ only in `access_width`
  (1 and 3) and hold the operation count fixed (4,670,000 operations each). Both
  are pure point access: no integrate, no query, no structural change. The width
  is not swept beyond those two points.
- **Wide components.** The four component types total 36 bytes. Nothing here
  tests a layout whose cost hinges on a copy too large to stay in cache; see
  `candidates/ecs/archetype/notes.md`.
- **Spatial locality.** A workload dimension in `PROJECT.md`, but one that
  belongs to the spatial-query track. The ECS workloads vary temporal locality,
  skew and burstiness only.
- **Concurrency.** Everything here is single-threaded, in both tracks.
- **Spatial: non-uniform query reach** is now covered by one held-out
  workload, `hs06_mixed_reach`: `s01_steady_uniform` with radii from 2 to 128,
  so the typical radius a structure is told (65) is one almost no query has. It
  is one point, not a sweep; the spread of reach is not varied.
- **Spatial: correlated movement** is now covered by one held-out workload,
  `hs07_crowd`: `s02_dense_clustered` with `movement: flock`, so each clump
  keeps its density while it crosses the world. Under flocking the movers of a
  tick are distinct entities rather than drawn with replacement, or an entity
  drawn twice would take its cluster's drift twice and the clump would smear;
  `gds_floor_spatial` measures 989 entities per radius query on `hs07` against
  999 on `s02`, so the clump does hold together.
- **Spatial: the rest of the rewind trade.** The two parameter sweeps give the
  history strategies a curve against movement rate at depth 6 and against depth
  at 10% moving, at one population, in a uniform world, with no teleports.
  Nothing varies how often a rewind happens, how much history is retained
  against how much is used, or the population under rewind, and nothing rewinds
  deeper than 32 ticks or in a clustered world. The movement sweep also passes
  through the regime of `hs01` at 1.0 and close to that of `hs02` at 0.02
  (different seed, fewer ticks, and `hs01`'s churn rather than `hs02`'s), so
  those two no longer hold a rewind regime out: no hidden workload tests a
  history strategy somewhere the public set does not already reach.
