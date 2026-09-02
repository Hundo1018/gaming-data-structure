# Workloads

A workload is a research object. Each file is generated once from its seed into
a concrete op stream, which the oracle and every candidate then replay
identically. Generation cost never lands inside a measurement, and two
candidates are never compared on different work.

`public/` may be seen by whatever proposes candidates. `hidden/` is held out;
the report ranks candidates on the two sets separately and prints the
difference, which is how a candidate tuned to what it could see becomes visible.

## Format

Flat `key: value`, one per line, `#` starts a comment. An unknown key is an
error, not a warning: a silently ignored field would make two different
experiments look like the same one. The C++ parser
(`substrate/src/workload.cpp`) and the Python one (`runner/orchestrate.py`) read
the same grammar.

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
- **Concurrency.** Everything here is single-threaded.
