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
cheap. Candidates: `aos`, `soa`, `sparse_set`, `archetype`, and three children
of them — `bitset_soa` (packed occupancy bits over `soa`), `grouped_sparse_set`
(an owning Position+Velocity group over `sparse_set`) and `fused_archetype`
(integrate applied inside the next query's pass) — a hash-map oracle, and two
negative controls: `broken_recycle`, which answers wrongly, and `query_memo`,
which answered correctly without finding anything until the digests were
salted.

### Track: spatial

Proximity queries over a world where everything moves, and rollback over that
world. Domains 2 and 3, together, because they are not separable in practice:
what makes rollback hard is that the thing being rolled back is an index whose
whole content changed. The question is how to find what is near a point or near
an entity when every entity may move anywhere every tick, and how to put the
world back the way it was N ticks ago.

Candidates: `uniform_grid`, `grid_undo_log`, `spatial_hash`, `morton_sorted`,
`axis_sorted`, a linear-scan oracle, and five added in the latest generation:
`cell_sorted` (a counting sort into cells with a directory, rebuilt on demand),
its child `cell_rows` (a query box read one row at a time), `grid_ring_knn`
(`uniform_grid` with its k-nearest search replaced), `morton_lbvh` (a
bounding-volume hierarchy over a radix-sorted Morton order, the first
hierarchical candidate) and `delta_grid` (which chooses, per tick, how to store
history and whether to patch or rebuild its index).

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
to read candidate manifests). On four shared cores `run_all.py` took 37 minutes
for the current population: 24 for the suite, 1 for the floor, 12 for the
sweeps.

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
exponent has to be 1. It measured 1.009 (r2 0.9999) in the first sweep and
0.937 (r2 0.9996) in `sweep-20261009T044632Z`, on the same code: that spread is
the precision every other exponent in `benchmarks/scaling.md` is read at.

A disagreement between a claim and a measurement has two possible causes and the
report says so rather than picking one. `brute_force` declares `O(1)` for move,
which is true — one array write — and measures n^0.39 with the forcing query
subtracted, because 2000 random writes into an array growing from 16 KB to
2 MB stop hitting L1. The claim is
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
- **The LLM agent loop is not in this repository.** Explorer, Mutator,
  Assumption Breaker, Adversary and Historian exist as prompts in `prompts/` and
  as roles in `PROJECT.md`, and nothing here calls a model. The latest
  generation was produced by such a loop run from outside: a coordinating
  session wrote each candidate's mechanism and predictions, a separate agent
  implemented it, another attacked it with adversarial workloads and
  differential fuzzing, and a third fixed what the attack found. That loop is
  not reproducible from this repository; its products are, because every
  candidate is verified and measured by the code here.
- **Two tracks of six.** Domains 1, 2 and 3 have substrate. Event streams,
  graph and navigation, and streaming world partition do not.
- **One hierarchical spatial candidate.** `morton_lbvh` is the first; every
  other spatial candidate is a grid, a hash of a grid, or a sorted array. No
  octree, k-d tree or incrementally refitted hierarchy has been tried.

## Current results

[`benchmarks/report.md`](benchmarks/report.md), regenerated by every run. Each
candidate's `notes.md` records what its own hypothesis predicted and what the
measurements did to it.

Compare within a report, never across two. Every number in one is taken on one
machine in one sitting under one set of flags, and the machine is shared:
absolute times moved about 40% between two runs a couple of days apart. What a
run establishes is the orderings and the ratios inside it.

Run `20261009T042136Z` and its sweeps measure the 19 candidates that passed
every workload of their track, the two oracles among them; 53 preregistered
predictions are judged in [`benchmarks/predictions.md`](benchmarks/predictions.md):
38 held and 15 were falsified, and 13 of the 53 verdicts rest on a point closer
to its bound than the repetition spread.
[`benchmarks/floor.md`](benchmarks/floor.md) gives each candidate as a multiple of
the irreducible cost of its workload.

What the latest generation established, each against a prediction written before
it was measured:

- **The clustered case has an answer.** `morton_lbvh` takes
  `s02_dense_clustered` from 5061 us (`uniform_grid`) to 2812 us at 1.79 MB
  against 5.84 MB, and carried the result to two held-out workloads written after
  it: travelling crowds (0.58x) and crowds with rollback (0.71x). It loses where
  the world is even or little moves; see its notes.
- **The k-nearest cost on a grid was the search order.** `grid_ring_knn` changes
  only that and takes `hs03_knn_heavy` from 15.1x to 5.8x the floor (0.39x its
  parent).
- **A layout rebuilt every tick beats an incremental one when everything
  moves.** `cell_sorted` is `morton_sorted` with a counting sort and a cell
  directory, and beats `uniform_grid` on every workload where all entities move
  (0.66x to 0.91x) while losing 4.5x where 2% do, as predicted.
- **The rollback crossover is located.** The undo log's rewind beats
  snapshot-and-rebuild below somewhere between 10% and 20% of entities moving at
  depth 6, and below depth 4 to 8 at 10% moving. `delta_grid`, which chooses per
  tick, is on the lower envelope at both ends of the movement axis (at full
  movement its rewind is 1.7x cheaper than snapshotting) and above it in the
  middle, where its rebuild costs more than its cost argument assumed.
- **In the ECS track a bitset over a flat index space is the generalist.**
  `bitset_soa` is on 11 of 12 Pareto fronts, has the lowest frame on the
  structural-churn workload, and matches `archetype` on iteration at `soa`'s
  footprint. Grouping (`grouped_sparse_set`) closed 61% of `sparse_set`'s gap to
  `archetype` on the sparse-component workload, answering the question
  `sparse_set/notes.md` asked. Fusing integrate into the next query
  (`fused_archetype`) bought nothing measurable: both of its main predictions
  failed.
- **`aos`'s founding claim, tested for the first time, fails in direction.**
  On a controlled pair that differs only in how many components one access
  touches, `aos`'s lead over `soa` shrinks from 8% to 4% as the access widens.

And two findings about the measurement itself, which come before any of the
above: an exploit candidate was 2.6x faster than every honest one by never
finding its answers, until the digests were salted; and step percentiles
included the load step, which made the p99 of a short run the load step itself,
until it was reported apart.
