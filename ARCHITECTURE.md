# Architecture

What each part of this repository is, what reads what, and where to put a new
thing. `PROJECT.md` is the research specification; this file is the map of the
implementation.

## The pipeline

```
manifest ──▶ compile ──▶ correctness ──▶ measurement ──▶ Pareto archive
                │             │               │
                │             │               └─▶ scaling (growth exponents)
                │             └─▶ oracle comparison, per operation
                └─▶ one executable per candidate
```

Each arrow is a gate that can reject. A candidate that does not compile removes
only itself, because every candidate is its own executable. A candidate that
fails correctness on any workload is never measured, and the rejection is
recorded rather than left as a gap.

## Layout

```
substrate/include/gds/          shared by every track
  types.hpp                     hashing and digest primitives
  measure.hpp                   repetitions, percentiles, the reported metric block
  alloc_tracker.hpp             global operator new/delete replacement
  pmu.hpp                       perf_event_open counters, degrading honestly
  json.hpp
  api.hpp  harness.hpp  entry.hpp  workload.hpp  reference.hpp    ── ecs track
  spatial/                                                        ── spatial track
    api.hpp        the candidate contract, as a C++20 concept
    types.hpp      Vec3, the shared dist2 and wrap, the digests
    oracle.hpp     linear scan with full-copy history
    workload.hpp   the spec and op stream
    harness.hpp    verify and measure
    entry.hpp      per-candidate main, and the rewind-strategy choice
    rebuild_rewind.hpp   history strategy: snapshot the world, rebuild the index
    undo_log_rewind.hpp  history strategy: record what changed, replay it back

substrate/src/                  the non-header parts of the above
candidates/<track>/<name>/      manifest.yaml hypothesis.md structure.hpp
                                structure.cpp notes.md
workloads/public/               workloads a search may see
workloads/hidden/               held out, used to detect overfitting
workloads/sweep/                scaling experiment templates and sweeps.yaml
runner/                         orchestrate, sweep, archive, pareto, reports, manifest
benchmarks/                     results.json report.md scaling.json scaling.md
archive/                        SQLite, git-ignored
```

**The boundary rule.** Anything that decides *how a measurement is taken* is
shared (`measure.hpp`, `alloc_tracker`, `pmu`), so two tracks cannot quietly
measure different things under the same name. Anything that decides *what
question is asked* is per-track (`api.hpp`, `workload.hpp`, `harness.hpp`,
`entry.hpp`). When something needs to move between the two, that is a decision
worth making explicitly rather than by copying.

## The data flow of one measurement

1. `runner/orchestrate.py` loads every `candidates/*/*/manifest.yaml` and
   validates it against `runner/manifest.py`. A schema violation stops the run.
2. CMake globs `candidates/*/*/structure.cpp` and builds one executable per
   candidate, named `gds_<track>_<name>`. A candidate's own directory and the
   candidates root are on its include path, so a descendant can include its
   parent by path rather than by copy.
3. For each candidate, the orchestrator runs its executable once per workload of
   its own track, in `--mode verify`.
4. The executable parses the workload file, expands it **once** into a concrete
   op stream, and replays that stream through both the candidate and the track's
   oracle, comparing after every operation.
5. Candidates that passed are run again in `--mode bench`. Same op stream, same
   code path. The binary prints one JSON object on stdout.
6. The orchestrator stores every result in `archive/archive.db`, computes Pareto
   fronts, and writes `benchmarks/results.json` and `benchmarks/report.md`.
7. `runner/sweep.py` is a separate pass over the same binaries: it generates
   workloads that differ only in population, fits a growth exponent to each
   candidate's curve, and writes `benchmarks/scaling.json` and
   `benchmarks/scaling.md`.

## The manifest schema

Every field, and the code that reads it. `documentation` means no code path
depends on it — a legitimate answer, but one that has to be stated rather than
being what happens by default. Validation rejects an unknown key, so adding a
field forces a decision about who consumes it.

This table is generated: `python3 runner/manifest.py`.

| field | required | read by |
|---|---|---|
| `id` | required | archive.candidates.id, genealogy |
| `name` | required | orchestrate: identity in every result and table |
| `track` | required | orchestrate: decides which workloads it meets |
| `binary` | required | orchestrate: which executable to run |
| `island` | optional | archive.candidates.island; population bookkeeping |
| `parents` | required | archive.candidates.parents; genealogy |
| `origin` | required | orchestrate: 'oracle' anchors the bench checksum, 'negative_control' is excluded from measurement and from sweeps |
| `novelty_status` | required | archive.candidates.novelty_status; the Historian stage's field, not yet written by any code |
| `expect_verify` | required | orchestrate: 'pass' gates measurement, 'fail' asserts the correctness gate rejects it |
| `complexity` | required | sweep: the claim each measured growth exponent is compared against |
| `hypothesis` | required | archive.candidates.hypothesis; report |
| `representation` | required | archive.candidates.representation |
| `operations` | required | documentation |
| `assumptions` | required | documentation |
| `expected_advantages` | required | documentation |
| `expected_disadvantages` | required | documentation |
| `mutation_operator` | optional | documentation: which operator from PROJECT.md produced this candidate from its parent |
| `notes` | optional | documentation |

The rule exists because it was broken. `complexity:` sat in twelve manifests
across two tracks, required by `PROJECT.md`, and no line of the runner ever read
it — a hand-written claim that nothing consumes is indistinguishable from a
comment. `runner/sweep.py` now consumes it and `benchmarks/scaling.md` puts it
beside the measurement.

## Workload formats

Flat `key: value`, `#` starts a comment. **An unknown key is an error**, in both
the C++ and the Python parser: a silently ignored field would make two different
experiments look like the same one.

| track | C++ parser | Python parser | key reference |
|---|---|---|---|
| ecs | `substrate/src/workload.cpp` | `runner/orchestrate.py: parse_workload` | `workloads/README.md` |
| spatial | `substrate/src/spatial_workload.cpp` | same | `workloads/README.md` |

A workload names its `track`, and a candidate only ever meets workloads of its
own track.

## The contracts

Neither contract names an array, an index, a chunk, a cell or a pointer. Both
constrain observable answers only, and both are checked at compile time by a
C++20 concept, so a structure that does not satisfy one fails to build with a
message naming the requirement it missed.

| | ecs (`gds/api.hpp`) | spatial (`gds/spatial/api.hpp`) |
|---|---|---|
| identity | the structure chooses its own opaque handle | the harness assigns dense ids, because identity is part of the answer to "what is near me" |
| mutation | create, destroy, add, remove, set | insert, remove, `move_by` |
| observation | get, mask, query, entity_count | `position_of`, `query_radius`, `query_radius_of`, `query_knn`, entity_count |
| batching | allowed; `sync()` is a declared point for it | allowed; `end_tick()` is a declared point for it |
| staleness | never: every observation must be correct when it is made | same |
| history | not part of the contract | `rewind_to`, with `kNativeRewind` declaring whether the structure keeps its own |

Two invariants hold across both, and exist so that candidates are comparable
rather than merely each correct:

- **Shared arithmetic.** The distance test, the toroidal wrap and every digest
  are single functions in the substrate that candidates call rather than
  reimplement, and the whole project builds with `-ffp-contract=off`. Without
  that the compiler could fuse a multiply and an add in one candidate and not in
  another, and two structures would disagree about a point sitting exactly on a
  query radius.
- **A broad phase may over-admit.** Culling can be as loose as a candidate
  likes; the accept test must be the shared one. This is what lets a
  representation be genuinely different without changing the question.

## Adding things

**A candidate** — create `candidates/<track>/<name>/` with the five files.
`structure.cpp` ends with `GDS_CANDIDATE_MAIN(T)` (ecs) or
`GDS_SPATIAL_CANDIDATE_MAIN(T)` (spatial). CMake picks it up on the next
configure. Fill in `complexity:`; the sweep will check it.

**A workload** — a file in `workloads/public/` or `workloads/hidden/` naming its
`track`. Say in a comment which hypothesis it exists to break.

**A scaling family** — a template in `workloads/sweep/` plus an entry in
`workloads/sweep/sweeps.yaml`. Hold the operation count per step fixed; if it
grows with the population, every candidate measures linear regardless of what it
does.

**A track** — a directory under `substrate/include/gds/`, carrying its own
`api.hpp`, `workload.hpp`, `harness.hpp`, `entry.hpp` and oracle, reusing
`measure.hpp` and the allocation tracker unchanged. Then `candidates/<track>/`
and workloads naming it.

## Not here

- **The agent loop.** Nothing calls a model. The six roles in `PROJECT.md` exist
  as prompts in `prompts/`; the archive carries `island`, `parents`, `origin`
  and `novelty_status` so a generation loop can be added without migrating
  existing evidence. See `agents/README.md`.
- **Third-party baselines.** Every candidate is written here, so the numbers
  compare implementations in this repository and say nothing about EnTT, flecs,
  or any production library. Two manifests already record this limit explicitly.
- **Hardware counters on this machine.** The code is in `pmu.cpp`; this
  container's kernel returns `ENOENT` from `perf_event_open`, so the counters
  are reported unavailable with that reason and never estimated.
- **Four of the six research domains.** Event streams, graph and navigation, and
  streaming world partition have no substrate.
