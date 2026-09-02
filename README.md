# Game Data Structure Discovery Agent

Read [`PROJECT.md`](PROJECT.md) for the research goal and the intended
architecture. This file describes what is implemented and how to run it.

## What is here

The C++ research substrate, the workload interface, the baseline ECS
candidates and the benchmark runner — the implementation step
`PROJECT.md` names after the specification.

```
substrate/          the candidate contract, oracle, workload generator, harness
candidates/ecs/     one directory per candidate, each its own executable
workloads/public/   workloads a search may see
workloads/hidden/   held-out workloads, used to detect overfitting
runner/             build, verify, measure, archive, report
archive/            SQLite archive of every run (git-ignored)
benchmarks/         results.json and report.md from the last run
```

## Running it

```
python3 runner/orchestrate.py --repeats 5
```

That configures and builds with CMake, verifies every candidate against the
oracle on every workload, measures the ones that pass, writes the archive, and
regenerates `benchmarks/report.md`.

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

Every candidate replays a byte-identical op stream beside
`gds::ReferenceStructure`, a hash-map implementation written to be obviously
correct by reading it. Three things are compared:

- **Every observation, as it happens.** Each `get` and `set` result and each
  end-of-frame query and entity count is folded into a running checksum on both
  sides and compared after every operation, so a divergence is reported at the
  operation that caused it.
- **A full sweep, periodically.** Every slot ever created is re-checked,
  destroyed ones included: liveness, mask, and every component. This is what
  verifies handle invalidation without a workload having to think of testing it.
- **The benchmark-mode checksum.** Measurement folds the same observations into
  a checksum, and the orchestrator compares it against the oracle's for the same
  workload. A candidate cannot be fast by answering incorrectly in the mode
  where nobody is checking.

`candidates/ecs/broken_recycle` is a negative control: `aos` with the generation
counter deleted. It exists so the gate is known to have teeth. It is rejected by
9 of the 10 workloads; see its `notes.md` for why the tenth is expected to pass
it.

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

## Adding a candidate

Create `candidates/<track>/<name>/` with `manifest.yaml`, `hypothesis.md`,
`structure.hpp`, `structure.cpp` and `notes.md`. The structure implements the
contract in `substrate/include/gds/api.hpp`; `structure.cpp` ends with
`GDS_CANDIDATE_MAIN(YourType)`. CMake picks the directory up on the next
configure. The contract is checked at compile time by a concept, so a structure
that does not satisfy it fails to build with a message saying so.

The contract names no array, index, chunk or pointer. It constrains observable
answers only. Deferring work is allowed; answering with stale data is not.

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
- **One track.** ECS only. The other five research domains in `PROJECT.md`
  have no substrate yet. Spatial locality is a workload dimension that belongs
  to the spatial-query track and is not simulated in the ECS workloads; what
  the ECS workloads vary is temporal locality, skew and burstiness.

## Current results

[`benchmarks/report.md`](benchmarks/report.md), regenerated by every run. Each
candidate's `notes.md` records what its own hypothesis predicted and whether the
measurements agreed. Two of the four predictions were falsified, which is the
system doing its job.
