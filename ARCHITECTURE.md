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
  common.h                      token pasting, hashing, bit casts, the optimiser barrier
  vec.h                         GDS_VEC: a growable array with std::vector's growth policy
  sort.inc.h                    template: introsort, partial sort, heaps, binary search
  alloc.h                       allocation counters (the allocator is src/alloc.c)
  measure.h                     repetitions, percentiles, the reported metric block
  pmu.h                         perf_event_open counters, degrading honestly
  json.h
  types.h  api.h  workload.h  reference.h                     ── ecs track
  replay.inc.h  run.inc.h  entry.h                            ── ecs harness
  spatial/                                                    ── spatial track
    api.h          the candidate contract, as prototypes checked at compile time
    types.h        Vec3, the shared dist2 and wrap, the digests
    knn.h          the (dist2, id) order as sort and heap instances
    oracle.h       linear scan with full-copy history
    workload.h     the spec and op stream
    replay.inc.h  run.inc.h   verify, measure and report, per structure
    entry.h        per-candidate main, and the rewind-strategy choice
    rebuild_rewind.inc.h   history strategy: snapshot the world, rebuild the index
    undo_log_rewind.inc.h  history strategy: record what changed, replay it back

substrate/src/                  alloc.c (the counting malloc), measure.c, pmu.c,
                                json.c, workload.c, spatial_workload.c
substrate/tools/                research instruments, never ranked
  spatial_floor.c               gds_floor_spatial: the irreducible cost of a tick
candidates/<track>/<name>/      manifest.yaml hypothesis.md structure.h
                                structure.c notes.md
workloads/public/               workloads a search may see
workloads/hidden/               held out, used to detect overfitting
workloads/sweep/                scaling experiment templates and sweeps.yaml
runner/
  run_all.py                    the whole experiment, serially, in order
  orchestrate.py                build, verify, measure, Pareto, report
  floor.py                      headroom: candidates as multiples of the floor
  sweep.py                      growth with population; families that vary a key
  predictions.py                preregistered predictions judged against results
  verify.py  ab.py              one binary against its track; interleaved A/B
  equivalence.py  build_ab.py   two builds: same answers? then paired timing
  archive.py pareto.py report.py scaling_report.py manifest.py
benchmarks/                     results.json report.md floor.json floor.md
                                scaling.json scaling.md predictions.json predictions.md
archive/                        SQLite, git-ignored
```

**The boundary rule.** Anything that decides *how a measurement is taken* is
shared (`measure.h`, `alloc.c`, `pmu.c`, `vec.h`), so two tracks cannot quietly
measure different things under the same name. Anything that decides *what
question is asked* is per-track (`api.h`, `workload.h`, `run.inc.h`,
`entry.h`). When something needs to move between the two, that is a decision
worth making explicitly rather than by copying.

## The language

Everything that is compiled is C11 (`-std=c11`, GNU extensions off) with three
compiler builtins (`__builtin_ctzll`, `__builtin_clzll`,
`__builtin_popcountll`) and one empty `asm` barrier. It was C++20 until the
port recorded in `benchmarks/port_timing.md`; `runner/equivalence.py` showed
that every binary of the C build returns the same checksums as the C++ build on
every workload before the C++ was removed.

C has no templates, classes or concepts, and three conventions stand in for
them:

- **A structure is a prefix.** A candidate named `uniform_grid` is a type
  `uniform_grid` and `static inline` functions `uniform_grid_insert`,
  `uniform_grid_query_radius` and so on, defined in its `structure.h`.
  `structure.c` defines `GDS_CANDIDATE` to the prefix and includes the track's
  `entry.h`. Nothing is called through a function pointer: every call from the
  harness into a candidate is to a static inline function the compiler can see.
- **Generic code is a template header.** A file ending in `.inc.h` is included
  with a macro naming the structure it is instantiated for, and defines
  functions whose names are built from that macro with `GDS_CAT`. `entry.h`
  includes `replay.inc.h` once for the oracle and once for the candidate, so
  both replay through the same code; the spatial `entry.h` also instantiates
  the rebuild wrapper around the candidate, as `<prefix>_rr`, and the workload
  decides at run time which of the two is measured. `sort.inc.h` follows
  libstdc++'s `std::sort` and heap algorithms step for step, which keeps the
  order of equal elements, and with it each structure's memory layout, what it
  was in C++.
- **The contract is a list of prototypes.** `GDS_ECS_CONTRACT(P)` and
  `GDS_SPATIAL_CONTRACT(P)` redeclare every operation with its required
  signature. A function with a different signature fails to compile
  (`conflicting types for 'soa_query'`); a missing one is a compiler warning
  naming it (`'soa_reported_bytes' used but never defined`) and then a link
  error. A spatial candidate also declares the enumeration constant
  `<prefix>_native_rewind`, which a `_Static_assert` checks is 0 or 1.

Two parts exist only to keep measurements comparable with the C++ ones.
`GDS_VEC` grows as libstdc++'s `std::vector` does (doubling on push, size +
max(size, added) on resize, exact on reserve and assign). `alloc.c` defines
`malloc` and its relatives in every executable, forwards to glibc through its
`__libc_` entry points with a 16-byte header recording the requested size, and
implements `realloc` as allocate, copy, free, so that the peak is charged as it
was when every container was a `std::vector`. On the five candidates checked
first, reported bytes, peak bytes and allocation counts came out identical to
the C++ build.

## The data flow of one measurement

1. `runner/orchestrate.py` loads every `candidates/*/*/manifest.yaml` and
   validates it against `runner/manifest.py`. A schema violation stops the run.
2. CMake globs `candidates/*/*/structure.c` and builds one executable per
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
7. `runner/floor.py` runs `gds_floor_spatial` on every spatial workload and
   writes `benchmarks/floor.json` and `benchmarks/floor.md`, dividing each
   candidate's median tick in `results.json` by the floor.
8. `runner/sweep.py` is a separate pass over the same binaries: it generates
   workloads that differ only in population, fits a growth exponent to each
   candidate's curve, and writes `benchmarks/scaling.json` and
   `benchmarks/scaling.md`.
9. `runner/predictions.py` reads every manifest's `predictions:`, evaluates them
   against `results.json` and `scaling.json`, and writes
   `benchmarks/predictions.json` and `benchmarks/predictions.md`.

`runner/run_all.py` runs 1 to 9 in that order.

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
| `predictions` | optional | runner/predictions.py: every entry is judged against results.json and scaling.json; verdicts in benchmarks/predictions.md |

The rule exists because it was broken. `complexity:` sat in twelve manifests
across two tracks, required by `PROJECT.md`, and no line of the runner ever read
it — a hand-written claim that nothing consumes is indistinguishable from a
comment. `runner/sweep.py` now consumes it and `benchmarks/scaling.md` puts it
beside the measurement.

## Workload formats

Flat `key: value`, `#` starts a comment. **An unknown key is an error**, in both
the C and the Python parser: a silently ignored field would make two different
experiments look like the same one.

| track | C parser | Python parser | key reference |
|---|---|---|---|
| ecs | `substrate/src/workload.c` | `runner/orchestrate.py: parse_workload` | `workloads/README.md` |
| spatial | `substrate/src/spatial_workload.c` | same | `workloads/README.md` |

A workload names its `track`, and a candidate only ever meets workloads of its
own track.

## The contracts

Neither contract names an array, an index, a chunk, a cell or a pointer. Both
constrain observable answers only, and both are checked at compile time by
prototype redeclaration (see "The language"), so a structure that does not
satisfy one fails to build with a message naming the function.

| | ecs (`gds/api.h`) | spatial (`gds/spatial/api.h`) |
|---|---|---|
| identity | the structure chooses its own opaque handle | the harness assigns dense ids, because identity is part of the answer to "what is near me" |
| mutation | create, destroy, add, remove, set | insert, remove, `move_by` |
| observation | get, mask, query, entity_count | `position_of`, `query_radius`, `query_radius_of`, `query_knn`, entity_count |
| batching | allowed; `sync()` is a declared point for it | allowed; `end_tick()` is a declared point for it |
| staleness | never: every observation must be correct when it is made | same |
| salting | `query(required, salt)`: a fresh salt per call, folded into every entity's digest | a radius query's digest is salted by the query's own centre and radius |
| history | not part of the contract | `rewind_to`, with `<prefix>_native_rewind` declaring whether the structure keeps its own |
| relocation | a structure may not point into itself: the harness moves it by copying the struct | same; the rebuild wrapper replaces its index that way |

Three invariants hold across both, and exist so that candidates are comparable
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
- **An answer is found, not computed.** A set-valued answer is digested as a
  sum, and an unsalted sum can be maintained as a running total without ever
  enumerating the set. `candidates/ecs/query_memo` did that and was 2.6x faster
  than any honest candidate on `w04_random_access` while passing every check.
  Salting every query's digest per call makes a carried total worthless; a
  candidate passes the salt it is given to the shared digest and does nothing
  else with it. Deferring or fusing work inside one call stays allowed.

## Adding things

**A candidate** — create `candidates/<track>/<name>/` with the five files.
`structure.h` defines the type `<name>` and its `<name>_<operation>`
functions; `structure.c` is three lines, `#include "structure.h"`, `#define
GDS_CANDIDATE <name>`, and `#include "gds/entry.h"` (ecs) or
`"gds/spatial/entry.h"` (spatial). `candidates/ecs/soa` and
`candidates/spatial/uniform_grid` are short examples of each. CMake picks the
directory up on the next configure. Fill in `complexity:`; the sweep will
check it.

**A workload** — a file in `workloads/public/` or `workloads/hidden/` naming its
`track`. Say in a comment which hypothesis it exists to break.

**A scaling family** — a template in `workloads/sweep/` plus an entry in
`workloads/sweep/sweeps.yaml`. Hold the operation count per step fixed; if it
grows with the population, every candidate measures linear regardless of what it
does.

**A track** — a directory under `substrate/include/gds/`, carrying its own
`api.h`, `workload.h`, `replay.inc.h`, `run.inc.h`, `entry.h` and oracle,
reusing `measure.h`, `vec.h` and the allocator unchanged. Then `candidates/<track>/`
and workloads naming it.

## Not here

- **The agent loop.** Nothing here calls a model. The six roles in `PROJECT.md`
  exist as prompts in `prompts/`; the archive carries `island`, `parents`,
  `origin` and `novelty_status` so a generation loop can be added without
  migrating existing evidence. The latest generation was produced by such a loop
  run from outside this repository (specify, implement, attack, fix, then a
  Historian with literature search); what it handed over is ordinary candidate
  directories, verified and measured by the code here. See `agents/README.md`.
- **Third-party baselines.** Every candidate is written here, so the numbers
  compare implementations in this repository and say nothing about EnTT, flecs,
  or any production library. Two manifests already record this limit explicitly.
- **Hardware counters on this machine.** The code is in `pmu.c`; this
  container's kernel returns `ENOENT` from `perf_event_open`, so the counters
  are reported unavailable with that reason and never estimated.
- **Four of the six research domains.** Event streams, graph and navigation, and
  streaming world partition have no substrate.
