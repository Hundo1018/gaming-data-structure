# Game Data Structure Discovery Agent

Read [`PROJECT.md`](PROJECT.md) for the research goal and the intended
architecture, and [`ARCHITECTURE.md`](ARCHITECTURE.md) for the map of the
implementation: what each module is, what reads what, and where a new candidate,
workload, scaling family or track goes. This file describes what is implemented
and how to run it.

## What is here

Two of the six research domains in `PROJECT.md` have a working substrate,
baselines and measured results.

```
ARCHITECTURE.md                 module map, data flow, manifest schema, contracts
substrate/include/gds/          shared: measurement, allocation tracking, counters
substrate/include/gds/spatial/  the spatial track's contract, oracle and harness
candidates/ecs/                 entity management candidates
candidates/spatial/             proximity and rollback candidates
workloads/public/               workloads a search may see
workloads/hidden/               held-out workloads, used to detect overfitting
workloads/sweep/                scaling experiment templates and sweeps.yaml
runner/                         build, verify, measure, archive, report
archive/                        SQLite archive of every run (git-ignored)
benchmarks/                     results.json, report.md, scaling.json, scaling.md
```

### Track: ecs

Entity and component management. Domain 1. The question is how to store which
entities hold which components so that both point access and iteration are
cheap. Candidates: `aos`, `soa`, `sparse_set`, `archetype`, a hash-map oracle,
and `broken_recycle` as a negative control.

### Track: spatial

Proximity queries over a world where everything moves, and rollback over that
world. Domains 2 and 3, together, because they are not separable in practice:
what makes rollback hard is that the thing being rolled back is an index whose
whole content changed. The question is how to find what is near a point or near
an entity when every entity may move anywhere every tick, and how to put the
world back the way it was N ticks ago.

Candidates: `uniform_grid`, `grid_undo_log`, `spatial_hash`, `morton_sorted`,
`axis_sorted`, and a linear-scan oracle.

A candidate declares whether it keeps its own history. One that does not is
measured wrapped in `RebuildRewind`, which snapshots the world every tick and
rebuilds the index on rewind — what an engine does today, where the game state
is the authority and the index is derived. `grid_undo_log` is `uniform_grid`
with that strategy replaced by a log of what changed and nothing else altered,
so the pair measures the history strategy rather than two different indexes.

A candidate is only ever run against workloads of its own track.

## Running it

```
python3 runner/run_all.py --repeats 5         # everything below, serially, in order

python3 runner/orchestrate.py --repeats 5     # compare candidates at one size
python3 runner/floor.py                       # how far each one is from the irreducible cost
python3 runner/sweep.py                       # measure how each one grows
python3 runner/predictions.py                 # judge every preregistered prediction
```

The first configures and builds with CMake, verifies every candidate against the
oracle on every workload, measures the ones that pass, writes the archive, and
regenerates `benchmarks/report.md`.

The second is the scaling pass: it generates workloads that differ **only** in
population, fits a growth exponent to each candidate's curve, and writes
`benchmarks/scaling.md`. It exists because the first cannot answer "how does
this grow" — no two of its workloads differ in one thing.

The third measures the floor of each spatial workload: what a tick would cost
if finding every answer were free, so a candidate's time can be read as a
multiple of what is achievable rather than only against the other candidates.
The fourth reads the `predictions:` each manifest carries and says, by code,
which held; see *Predictions, judged by code* below.

While a candidate is being written, two smaller tools avoid running the whole
suite: `runner/verify.py <binary> [--extra files]` checks one binary against
every workload of its track without reading any manifest, and `runner/ab.py
<candidates> --workload <file>` measures a few candidates in interleaved
rounds, so drift on a shared machine is spread across all of them instead of
landing on whichever ran during a slow minute. `orchestrate.py --verify-only`
runs the compile and correctness gates and stops.

Requirements: CMake 3.20+, a C++20 compiler, Python 3.9+ with PyYAML (used only
to read candidate manifests). One run of the current population takes about four
minutes on four cores.

Single candidate, single workload:

```
./build/gds_ecs_archetype --workload workloads/public/w02_query_heavy.workload --mode verify
./build/gds_ecs_archetype --workload workloads/public/w02_query_heavy.workload --mode bench --repeats 5
```

Both modes print JSON on stdout.

## The pipeline

```
compile -> correctness -> measurement -> Pareto archive
```

Each gate is a real filter. A candidate that does not compile removes only
itself, because every candidate is a separate executable. A candidate that
fails correctness on any workload is never measured, and its rejection is
recorded rather than left as a gap in the results.

## How correctness is decided

Every candidate replays a byte-identical op stream beside its track's oracle: a
hash-map implementation for the ECS track, a linear scan with full-copy history
for the spatial track. Both are written to be obviously correct by reading them.
Three things are compared:

- **Every observation, as it happens.** Each `get` and `set` result and each
  end-of-frame query and entity count is folded into a running checksum on both
  sides and compared after every operation, so a divergence is reported at the
  operation that caused it.
- **A full sweep, periodically.** Every slot or id ever created is re-checked,
  destroyed ones included. In the ECS track that is liveness, mask and every
  component, which verifies handle invalidation without a workload having to
  think of testing it. In the spatial track it is liveness and the exact bits of
  every position, which is what catches a structure that resurrects an entity on
  rewind or drops one that should have come back.
- **The benchmark-mode checksum.** Measurement folds the same observations into
  a checksum, and the orchestrator compares it against the oracle's for the same
  workload. A candidate cannot be fast by answering incorrectly in the mode
  where nobody is checking.

`candidates/ecs/broken_recycle` is a negative control: `aos` with the generation
counter deleted. It exists so the gate is known to have teeth. It is rejected by
every ECS workload that recycles a slot; see its `notes.md` for why the others
are expected to pass it.

### An answer must be found, not computed

Comparing answers is not enough on its own, and the suite once showed it. A
query's digest was an order-independent sum of per-entity terms, and a sum can
be kept as a running total: subtract an entity's term before it changes, add it
back after. `candidates/ecs/query_memo` does exactly that on top of `soa`. Under
that contract it passed every ECS workload and ran `w04_random_access` at 0.36x
of `soa`'s frame time, 2.6x ahead of the best honest candidate, without ever
enumerating a matching entity.

Every query digest is now salted per call. An ECS query is `query(required,
salt)`, the harness drawing a fresh salt for every call; a spatial radius query
salts each hit with a value derived from the query's own centre and radius. An
entity's term under one call says nothing about its term under the next, so a
total carried between calls is worthless. Work may still be deferred or fused
inside one call. `query_memo` is kept, unchanged in logic, as the negative
control that the fix holds: it is rejected wherever it reuses a total
(`w04_random_access`) and passes where it never does.

Float results have to be bit-identical across candidates or two structures would
disagree about a point sitting exactly on a query radius. Everything is built
with `-ffp-contract=off` so the compiler cannot fuse a multiply and an add in one
candidate and not in another, and the distance test, the wrap and the digest are
single shared functions that every candidate calls rather than reimplements.

Three independent rewind implementations — the spatial oracle's full copies,
`RebuildRewind`'s snapshot and rebuild, and `UndoLogRewind`'s replayed log —
produce identical checksums on every temporal workload. They deliberately share
no code, so agreement between them is evidence rather than a tautology.

## How performance is decided

Measurement is per frame, because that is the unit a game actually has to meet.
Reported: median, p95, p99 and maximum frame time, operations per second, peak
and final allocated bytes, bytes per entity, allocation and free counts.

Memory is measured, not asked for. The substrate replaces the global allocation
operators and records the high-water mark of live bytes. Each candidate also
reports its own footprint through `reported_bytes()`; both are archived, and
the disagreement between a claim and a measurement is itself informative.

Fitness is a Pareto front over median frame time, p99 frame time and peak bytes,
all minimised. There is no weighted score: a weighting would decide in advance
which trade-off matters.

### Growth, and the claims about it

Every `manifest.yaml` carries a `complexity:` field. For two tracks it sat there
required by `PROJECT.md` and read by nothing, which makes a hand-written claim
indistinguishable from a comment. `runner/sweep.py` now consumes it: each family
holds the number of operations per step constant while the population grows,
fits an exponent, and `benchmarks/scaling.md` prints the claim beside the
measurement.

`runner/manifest.py` declares every manifest field together with the code that
reads it, and validation rejects an unknown key, so adding a field forces a
decision about who consumes it. `documentation` is a legitimate answer; being
one by accident is not.

`brute_force` is the positive control: it looks at everything, so its query
exponent has to be 1. It measures 1.009 with an r2 of 0.9999.

A disagreement between a claim and a measurement has two possible causes and the
report says so rather than picking one. `brute_force` declares `O(1)` for move,
which is true — one array write — and measures n^0.358, because 2000 random
writes into an array growing from 16 KB to 2 MB stop hitting L1. The claim is
right about operations and wrong about time. Claims are not edited to match
measurements.

### Headroom

Ranking candidates against each other says which is best, not whether anything
better is possible. `gds_floor_spatial` replays each spatial workload and times,
per tick, the two things no structure can avoid: applying the tick's mutations
to flat position and liveness arrays, and digesting exactly the entities in
every answer from packed buffers. Every answer it digests is checked against the
oracle's. It also counts how many entities the reference grid's query box admits
per entity actually in the answer, which is what a tighter broad phase could
remove; the answer's own size is what nothing can remove.

`benchmarks/floor.md` gives each candidate's median tick as a multiple of that
floor. A workload where the best candidate is near 1x has little left to win;
one where it is at 10x is where a new representation is worth proposing. The
floor and the candidates are timed in separate processes, so a ratio is an
estimate of headroom rather than a measurement of it.

### Predictions, judged by code

Each `hypothesis.md` states falsifiable predictions in prose, and for a long time
a person read the report and wrote in `notes.md` whether each held. That leaves
the verdict to whoever writes the notes. A manifest may now carry the same
predictions as data:

```yaml
predictions:
  - id: P1
    claim: hs03_knn_heavy median tick at most 0.5x uniform_grid's
    metric: step_ns_p50
    workloads: [hs03_knn_heavy]
    against: uniform_grid
    at_most: 0.5
```

`runner/predictions.py` evaluates every one against `benchmarks/results.json`
and `benchmarks/scaling.json` and writes `benchmarks/predictions.md`: HELD,
FALSIFIED, or UNTESTED when a point it needs was not measured. A point closer to
its bound than the spread between repetitions of the measurements it came from
is marked *within noise*, because a verdict resting on it is weak whichever way
it went. The prediction is written before the candidate is measured and is never
edited to match a number.

The first time the old prose predictions were judged this way, one verdict
disagreed with the notes. `sparse_set` predicted it would beat the layouts that
scan the whole index space on `h05_sparse_component`, which it did; its notes had
recorded a falsification against a stronger paraphrase, "win `h05`".

### Hardware counters

`substrate/src/pmu.cpp` opens cycles, instructions, cache references and misses,
and branch instructions and misses through `perf_event_open`. On a machine that
refuses — no PMU exposed to the guest, or `perf_event_paranoid` too high — every
counter is reported unavailable with the kernel's reason. Nothing is estimated
or modelled to fill the gap. On the container these results were produced in,
`perf_event_open` returns `ENOENT` and the counters are absent from the report.

## Workloads

A workload is generated once from a seed into a concrete op stream and then
replayed identically by the oracle and by every candidate, so generation cost is
never inside a measurement and no two candidates ever see different work.

The format is flat `key: value`; unknown keys are an error rather than being
ignored, because a silently dropped field makes two different experiments look
like the same one. `workloads/public/*.workload` and `workloads/hidden/*.workload`
carry the current set, and each file's comment header states which hypothesis it
is meant to break.

Held-out workloads are not merely unused during development. The report ranks
every candidate on public and hidden workloads separately and prints the
difference, so a candidate that does better on what it could see than on what it
could not is visible in the results.

Workloads added after a generation of candidates was designed are held out by
default, because the candidates could not have been tuned to them:
`hs06_mixed_reach` asks for query radii from 2 to 128 when every structure sizes
itself from the mean, and `hs07_crowd` is the clustered world with its clumps
travelling across the map. A workload that tests an old prediction for the first
time can be public, because the prediction was written before it existed:
`w06_point_narrow` and `w07_point_wide` differ only in how many components one
point access touches, which is the case `aos` was built for and no earlier
workload had.

## Adding a candidate

Create `candidates/<track>/<name>/` with `manifest.yaml`, `hypothesis.md`,
`structure.hpp`, `structure.cpp` and `notes.md`. CMake picks the directory up on
the next configure, and the manifest's `track` decides which workloads it meets.

| track | contract | entry macro |
|---|---|---|
| `ecs` | `substrate/include/gds/api.hpp` | `GDS_CANDIDATE_MAIN(YourType)` |
| `spatial` | `substrate/include/gds/spatial/api.hpp` | `GDS_SPATIAL_CANDIDATE_MAIN(YourType)` |

Each contract is checked at compile time by a concept, so a structure that does
not satisfy it fails to build with a message saying which requirement it missed.

Neither contract names an array, an index, a chunk, a cell or a pointer. Both
constrain observable answers only. Deferring work is allowed; answering with
stale data is not. A spatial broad phase may over-admit as loosely as it likes,
as long as the accept test is the shared `dist2`.

A candidate descended from another includes its parent by path — the candidates
root is on the include path, so `#include "spatial/uniform_grid/structure.hpp"`
works. `grid_undo_log` is built that way, which is what makes it a measurement
of one changed variable rather than of two separately written structures.

## Deliberate deviations from PROJECT.md

- **No per-candidate `tests.cpp` or `benchmark.cpp`.** `PROJECT.md` lists both
  in the candidate layout. Correctness and measurement are shared harness code
  instead, because a candidate that supplies its own tests defines its own
  notion of correct, and one that supplies its own benchmark defines its own
  measurement. Neither is comparable across a population.
- **The LLM agent loop is not implemented.** Explorer, Mutator, Assumption
  Breaker, Adversary and Historian exist as prompts in `prompts/` and as roles
  in `PROJECT.md`. Nothing in this repository calls a model. The archive schema
  carries `island`, `parents`, `origin` and `novelty_status` so a generation
  loop can be added without migrating existing evidence, but the ten islands
  currently hold hand-written baselines.
- **Two tracks of six.** Domains 1, 2 and 3 have substrate. Event streams,
  graph and navigation, and streaming world partition do not.
- **No hierarchical spatial candidate.** Every spatial candidate here is flat:
  a grid, a hash of the same grid, or a sorted array. `s02_dense_clustered`
  exists precisely because a flat structure has no answer to clustering, and it
  is left unanswered on purpose — the workload states the gap that an octree,
  a BVH or a k-d tree would be proposed against.

## Current results

[`benchmarks/report.md`](benchmarks/report.md), regenerated by every run. Each
candidate's `notes.md` records what its own hypothesis predicted and what the
measurements did to it.

Compare within a report, never across two. Every number in one is taken on one
machine in one sitting under one set of flags, and the machine is shared:
absolute times moved about 40% between two runs a couple of days apart. What a
run establishes is the orderings and the ratios inside it.

Across the two tracks, five of the eight falsifiable predictions were falsified
and one that an earlier run recorded as confirmed turned out not to survive a
change of machine — see `candidates/ecs/soa/notes.md`, which now records it as
untested rather than confirmed.

The result that answers the most: `grid_undo_log` is `uniform_grid` with its
history strategy replaced and nothing else changed, and it predicted it would
beat snapshot-and-rebuild when few entities move and lose when they all do. Both
halves held. With 2% of entities moving per tick it is 3.0x better on p99 and
uses 2.4x less memory; with every entity moving it is 2.0x worse on p99 and uses
1.4x more. The choice is set by the movement rate, not by taste, and neither
strategy is the right default without that number.
