# The C11 port, and why the suite stayed C++20

On 2026-10-09 the whole suite was ported from C++20 to C11 at the user's
request: substrate, both oracles, both workload generators, the allocation
tracker, all 21 candidates and the floor tool. The port was accepted only on
bit-level equivalence with the C++ build. The question was then whether to keep
it, and the user put experimental rigour and comparability first. On the
evidence below the user chose to return to C++20. The C sources are in git
history (the last commit that has them is `a6804f7`; the port begins at
`1e2fc34`). The tree is the C++ of `83a58a6` again, and a rebuild of it is
byte-identical, binary for binary (22 of 22), to the build that
`benchmarks/results.json` was measured with. No earlier result changes.

Every number below comes from a command that can be re-run. The deterministic
measurements are instruction counts (`valgrind --tool=cachegrind
--cache-sim=no`, one measured repetition taken as `--repeats 2` minus
`--repeats 1`), symbol tables and checksums. The wall-clock figures come from
`runner/build_ab.py`, serial, on a shared 4-core container with no hardware
counters.

## Answers and memory: the two builds were the same experiment

`runner/equivalence.py --a <C++ build> --b <C build>` ran all 21 candidate
binaries on every workload of their track: the 25 of the suite, plus 32 of
the 33 edge-case workloads written by the port's reviewers (all 33 are now in
`workloads/port_review/`). That is 1,206 comparisons in verify and bench mode
on 57 workloads. The 33rd, the NaN-radius probe, was run on its own with a
short timeout, because three candidates never finish it (see below).

| what was compared | result |
|---|---|
| verify and bench checksum, status, failure message, op counts | identical in all 1,206 |
| runs that did not finish | 7, all the bench run of `xb4_overflow_world`, timed out (900 s) in both builds; their verify runs agree |
| reported_bytes, peak_bytes, alloc_count | identical for 20 of 21 candidates on every workload. The ECS oracle differs because it was a hand-written chained hash map in C against `std::unordered_map` in C++ |
| `gds_floor_spatial`, every non-timing field | identical on all 13 spatial workloads and on the NaN probe |

Two independent implementations of the substrate produce the same op streams,
the same digests and the same allocation figures, so those parts of the
measurement do not depend on the language they are written in.

## Speed and ranking

**Wall clock** (`benchmarks/port_timing.md`, `benchmarks/port_timing.json`):
five rounds per candidate and workload, alternating which build ran first, one
warm-up and one measured repetition per run. C/C++ `total_ns`, geometric mean
over 263 pairs: 0.967. Per candidate, excluding the oracles, the geometric mean
runs from 0.920 (`delta_grid`) to 1.021 (`soa`). The ECS oracle came out at
0.699 and the spatial oracle at 0.904.

**Instructions** (42 candidate-workload pairs on shortened copies of w02, w04,
s01 and hs01, built with `-mno-avx512f` because valgrind 3.22 cannot decode the
shipped AVX-512 binaries): C/C++ from 0.848 (`axis_sorted`, s01) to 1.106 (the
ECS oracle, w02); geometric mean 1.008 on ECS, 0.958 on spatial, 0.982 overall;
29 of 42 within 0.97–1.03. Every ratio outside that band traced to a code shape,
not to the language:
- In C, GCC did not inline `gds_digest_entity` into `grouped_sparse_set`'s
  query, though it did in C++.
- In the C++, `std::partial_sort` takes `nearer` as a function pointer, so every
  comparison is an out-of-line call. The C sort template inlined it.
- `std::vector::push_back` costs 35 instructions per element against 23 for a
  plain array store.
- `move_by` keeps one out-of-line helper per move in C++. A 6–9% residual on
  hs01 is located but not fully explained.

The same C source compiled as C++20 instead changed instruction counts by at
most 0.46% (gcc) and 0.008% (clang). The language a file is compiled as was not
what moved the numbers; how the code was written was.

**Ranking** (`benchmarks/port_ranking.md`, from `runner/rank_stability.py`,
oracles and negative controls excluded): does the language change which
candidate is faster?

| metric | median Kendall τ_b | pairs in a different order | pairs both builds separate | of those, same order |
|---|---:|---:|---:|---:|
| `step_ns_p50` | 0.822 | 86 of 837 | 372 | 372 (0 reversed) |
| `total_ns` | 0.867 | 75 of 837 | 400 | 400 (0 reversed) |

A pair is *separated* by a build when that build's per-round ranges for the two
candidates do not overlap. Every pair whose order differs between the builds
is one that at least one build cannot separate. Where the measurement can tell
two candidates apart, the two languages always agree on which is faster.

## What decided it

None of the following was a property of the language itself.

- **Third-party baselines.** The ones a reviewer of this research would expect
  have no C API: EnTT, gaia-ecs, EntityX, nanoflann, Boost.Geometry, CGAL,
  PhysX, Bullet's btDbvt and Jolt are C++ only. flecs, libspatialindex, Embree,
  Box2D v3, Chipmunk2D, FLANN, ODE and pico_ecs have C APIs. A C harness reaches
  a C++ library through `extern "C"` wrappers. On `w06_point_narrow`, with 4.67M
  contract calls per repetition, the wrapper cost 14.9 instructions per call
  without LTO (1.113x the inlined C, 1.070x the C++). Of that, 11.6 is the cost
  of any separately compiled boundary. With gcc `-flto`, 12 of 15 wrappers were
  inlined; with clang `-flto=thin`, all 15 were. In C++ such a baseline compiles
  against the harness directly.
- **Allocation accounting and sanitizers.** The C tracker defined `malloc` and
  forwarded to glibc's `__libc_*` entry points:
  - It does not link against musl, where those symbols are hidden and
    `__libc_memalign` does not exist.
  - It has nothing to forward to on macOS.
  - ISO C 7.1.3 makes defining `malloc` undefined behaviour.
  - It defeats AddressSanitizer: the binary segfaults in ASan's start-up, aborts
    with `free(): invalid pointer`, or misses a plain heap overflow.

  The C++ tracker's `operator new` replacement is sanctioned by the standard,
  and the C++ `soa` passed verify under ASan and UBSan on `w05_small_world`,
  the one candidate this was tried on. A portable C wrapper
  allocator matched the interposer on 526 of 526 comparisons. It counts none of
  the 106 allocations a C++ `std::` container test makes, so a mixed build would
  need both.
- **Continuity.** Every result in `benchmarks/` was measured with the C++
  build. Staying keeps them.

What C would have kept:
- Container growth and sorting were defined in the repository (`GDS_VEC`,
  `sort.inc.h`) rather than by the standard library's vendor. libstdc++ and
  libc++ differ in both. This was inferred, not measured here.
- The prototype contract rejected a `uint32_t salt` that the C++ concept
  accepts by conversion. An exact-type `static_assert` in C++ closes that gap.
- About 3% less wall time with no change in ranking.
- C++ translation units cost 3.6x to 5.5x more compiler instructions.
- A C ABI that Unity, Rust and Python bind to directly.

The cost of keeping C was the shim or a dual C/C++ substrate (tested in
scratch: +98/−37 lines over 19 files, all 21 candidates then compile as C++20
and reproduce all 263 verify results), a portable allocator, and a full
re-measurement.

## Defects the port found that are in the C++ suite too (open)

The port's reviewers wrote workloads the suite does not have, and the C and
C++ builds failed them identically. None is reachable from
`workloads/public` or `workloads/hidden`.

1. **NaN query radius.** The workload parser accepts `nan` (it uses `strtod`).
   With `query_radius_min: nan`, `uniform_grid`, `grid_undo_log` and
   `cell_sorted` never return from a k-nearest query: the box-doubling loop
   cannot terminate on a NaN cell size, after a float-to-int conversion of NaN
   that is undefined behaviour. Probe: `workloads/port_review/dgr01_nan_typical_radius.workload`.
2. **Worlds near the float limit.** On `xb4_overflow_world` (a world 3e38
   across, where squared distances overflow to infinity), 9 of the 10 spatial
   candidates that are not the oracle fail verify. Only `morton_lbvh` passes,
   and the first failure is the same in both builds: `tick 1, op 230:
   observation mismatch on op kind 3 ... radius 53358885879597236224.000000`.
   The candidates are correct only for worlds whose squared extents stay finite.
3. **`delta_grid` k-nearest with infinite distances.** When `r * r` overflows,
   `found[want-1].d2 <= r*r` holds as `inf <= inf` and the query returns the k
   lowest ids of the column it has gathered (a probe outside the suite; the
   generator never aims a query outside the world).
4. **Coverage gaps in the ECS replay.** `integrate` is called only at the end
   of a frame and `sync` resolves pending work there, so `fused_archetype`'s
   resolve-on-pending paths in create, add, remove, get and set are reached by
   no workload. No workload produces a mask with bits 4–7 or a query with
   `required == 0`, and the suite queries only three masks. Two reviewers
   covered these paths with differential drivers that called the C and C++
   implementations directly. The outputs were byte-identical.

Defects that existed only in the C port (`std::max` written with the wrong
operand order for NaN and signed zero, and two vector macros that read their
argument after freeing it) went with it.

## A finding about the measurement itself

The run-to-run spread, (max − min) / median over five rounds per candidate and
workload, has a median of 29% for `step_ns_p50` and 26% for `total_ns`. It is
the same in both builds, so it is the machine and not the language. A
difference smaller than that cannot be called from one round, and a prediction
whose threshold sits inside it cannot be decided. Example: `delta_grid`'s P3
predicts `step_ns_p50` at most 1.2x `cell_sorted`'s on four workloads, one of
them `s01_steady_uniform`, and `benchmarks/predictions.md` records it as held,
within noise. Over these rounds the median ratio on s01 was 1.62 in the C++
build and 0.92 in the C build, and the per-round ranges overlap.

`benchmarks/results.json` was measured with five repetitions per run, so its
medians are steadier than these single-repetition rounds. Its run-to-run
spread was never measured. Reducing and quantifying this noise is the next
change in measurement, independent of the language.

## Tools this left behind

- `runner/equivalence.py`: do two builds give the same answers?
- `runner/build_ab.py`: paired, interleaved timing of two builds.
- `runner/rank_stability.py`: do two builds order the candidates the same,
  and how much of that question does the noise let through?
- `workloads/port_review/`: the 33 edge-case workloads above.
