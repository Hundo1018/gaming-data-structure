# The C11 port, and why the suite stayed C++20

On 2026-10-09 the whole suite was ported from C++20 to C11 at the user's
request: substrate, both oracles, both workload generators, the allocation
tracker, all 21 candidates and the floor tool. The port was accepted only on
bit-level equivalence with the C++ build. The question was then whether to
keep it. The user put experimental rigour and comparability first, and on the
evidence below chose to return to C++20.

The C sources are in git history. The port begins at `1e2fc34`, the last
commit that changes the C is `a6804f7`, the sources are present through
`f321511`, and `2c24f2d` removes them. The tree is the C++ of `83a58a6` again.
A rebuild of it is byte-identical, binary for binary (22 of 22), to the C++
reference build the port was checked against, which was built from `83a58a6`
before the port began. `benchmarks/results.json` records commit `ed47f82`,
whose C++ sources and `CMakeLists.txt` are identical to `83a58a6`'s. The
binaries it ran were not kept, so their identity with this build is inferred
from the same sources, compiler and flags, not compared. No earlier
measurement changes; the noise finding at the end bears on how earlier
verdicts that sit close to their bounds should be read.

Every number below comes from a command that can be re-run. The deterministic
measurements are instruction counts, symbol tables and checksums. Instruction
counts come from `valgrind --tool=cachegrind --cache-sim=no`, where one
measured repetition is `--repeats 2` minus `--repeats 1` unless stated
otherwise. The wall-clock figures come from `runner/build_ab.py`, run serially
on a shared 4-core container with no hardware counters.

## Answers and memory

`runner/equivalence.py --a <C++ build> --b <C build>` ran all 21 candidate
binaries on every workload of their track. That covered the 25 workloads of
the suite and 32 of the 33 edge-case workloads written during the port: 27 by
its reviewers and 6 by the porter of the cell-based candidates. All 33 are now
in `workloads/port_review/`. Each binary ran in verify and bench mode, which
gives 1,206 comparisons on 57 workloads. The 33rd file, the NaN-radius probe,
was run separately with a 30 s timeout. Six candidates never finish it (see
below).

| what was compared | result |
|---|---|
| verify and bench checksum, status, failure message, op counts | identical in the 1,199 runs that finished in both builds |
| runs that did not finish | 7, all of them the bench run of `xb4_overflow_world` (900 s timeout, three jobs in parallel), in both builds; their verify runs agree |
| reported_bytes, peak_bytes, alloc_count | identical for 20 of 21 candidates on every workload where both bench runs finished. The ECS oracle differs because the C version was a hand-written chained hash map, against `std::unordered_map` in C++ |
| `gds_floor_spatial`, every non-timing field | identical on all 13 spatial workloads and on the NaN probe |

The C substrate was translated from the C++ function by function: `GDS_VEC`
follows libstdc++'s vector growth and the sort follows libstdc++'s introsort.
It was accepted only once its checksums matched, so agreement on the 25 suite
workloads was the acceptance test, not an independent result. It also agreed
on the 32 edge-case workloads it was not tuned on, and in allocation figures
for every candidate but the ECS oracle. On this evidence (one port, GCC 13.3,
glibc 2.39), changing the language did not alter the op streams, digests or
allocation accounting. Equal op streams are inferred from equal checksums and
op counts; the streams themselves were not compared.

## Speed and ranking

**Wall clock** (`benchmarks/port_timing.md`, `benchmarks/port_timing.json`).
Each candidate and workload got five rounds, alternating which build ran
first, with one warm-up and one measured repetition per run. C/C++ `total_ns`
as a geometric mean of the per-pair median ratios:

| pairs | geometric mean |
|---|---:|
| all 263 | 0.967 |
| 238 without the two oracles | 0.987 |
| 214 also without the two negative controls | 0.985 |
| ECS, without its oracle (108) | 1.003 |
| spatial, without its oracle (130) | 0.974 |

Per candidate, excluding the oracles, the geometric mean runs from 0.920
(`delta_grid`) to 1.021 (`soa`). The ECS oracle, which the port rewrote as a
different structure, came out at 0.699, and the spatial oracle at 0.904.

**Instructions** (42 candidate-workload pairs on shortened copies of w02, w04,
s01 and hs01, built with `-mno-avx512f` because valgrind 3.22 cannot decode the
shipped AVX-512 binaries):
- C/C++ ranges from 0.848 (`axis_sorted`, s01) to 1.105 (the ECS oracle, w02).
- The geometric mean is 1.008 on ECS, 0.958 on spatial and 0.982 overall.
- As built, C executes about 2% fewer instructions over the 38 non-oracle
  pairs (0.980), and about 3.6% fewer on spatial without its oracle.

29 of the 42 pairs fall within 0.97–1.03. Most of the 13 outside that band
were located in specific code shapes, but changing those shapes did not close
every gap:
- In C, GCC did not inline `gds_digest_entity` into `grouped_sparse_set`'s
  query, while in C++ it did.
- The C++ passes `nearer` to `std::partial_sort` and the heap functions as a
  function pointer. In the spatial oracle, where it was measured, every
  comparison became an out-of-line call. Replacing it there, together with the
  `push_back` below, moved the oracle's ratio from 0.906 to 0.986. The C sort
  template inlined the comparison.
- In the C++ spatial oracle's k-nearest gather, `std::vector::push_back` took 35
  instructions per element against 23 for the C port's plain array store.
- `move_by` keeps one out-of-line helper per move in C++. On hs01, C still ran
  7–10% fewer instructions after those helpers were forced inline (C/C++
  0.895–0.928). The gap is located but not explained.
- The ECS oracle's ratio comes from the change of representation, not from
  code generation.

Two candidates were compiled from the same C source as C++20 instead: `soa`
on w05 and `uniform_grid` on s05, one repetition each, without
`-march=native`, counting the whole executable. Their instruction count changed
by at most 0.46% under gcc and 0.008% under clang. For those two, the language a
file is compiled as did not move the numbers; how the code was written did.

**Ranking** (`benchmarks/port_ranking.md`, from `runner/rank_stability.py`,
oracles and negative controls excluded). A pair is *separated* by a build when
that build's per-round ranges for the two candidates do not overlap.

| metric | median Kendall τ_b | pairs in a different order | pairs both builds separate, same order / reversed | separated by one build, reversed by the other |
|---|---:|---:|---:|---:|
| `step_ns_p50` | 0.822 | 86 of 837 | 372 / 0 | 2 |
| `total_ns` | 0.867 | 75 of 837 | 400 / 0 | 3 |

Where both builds separate two candidates, they put them in the same order.
The 2 and 3 pairs in the last column are separated by one build and come out
the other way in the other, inside that build's noise. Instruction counts,
which have no noise, show one reversal: `grouped_sparse_set` against
`sparse_set` on shortened w02 is 1.066 in C and 0.969 in C++. The C/C++ shift
also differs by candidate (wall-clock geometric mean 0.920 to 1.021), which is
as large as some prediction margins. The two builds rank alike within this
noise. Whether they would also agree with less noise has not been tested.

## What decided it

Most of the following are properties of the ecosystem and the toolchain rather
than of the language. The allocation point is partly a property of the two
standards.

- **Third-party baselines.** The ones a reviewer of this research would most
  likely expect have no first-party C API: EnTT, gaia-ecs, EntityX, nanoflann,
  Boost.Geometry, CGAL, PhysX, Bullet's btDbvt and Jolt. PhysX and Jolt have
  third-party C wrappers. CGAL's GPL licence, and a broadphase in Jolt that is
  bound to its BodyManager, rule those two out as candidates in either
  language. flecs, libspatialindex, Embree, Box2D v3, Chipmunk2D, FLANN, ODE and
  pico_ecs have C APIs. No third-party library was built against either
  harness.

  A C harness reaches a C++ library through `extern "C"` wrappers. The test case
  was the suite's own C++ `soa` behind a 15-function shim, on
  `w06_point_narrow` (4.67M contract calls per repetition):
  - Without LTO, the shim cost 14.9 more instructions per call. The whole run
    executed 1.113x the instructions of the inlined C and 1.070x those of the
    C++ build.
  - The same C `soa` compiled separately behind the same boundary cost 11.6 per
    call, so about 3.3 of the 14.9 comes from C++.
  - With gcc `-flto`, 12 of the 15 wrappers were inlined; with clang
    `-flto=thin`, all 15 were.

  In C++ such a baseline should compile against the harness directly. That is
  inferred, not tried.
- **Allocation accounting and sanitizers.** The C tracker defined `malloc` and
  forwarded to glibc's private `__libc_*` entry points:
  - It did not link against musl, where those symbols are hidden and
    `__libc_memalign` does not exist.
  - Apple's libmalloc and Libc sources contain no such symbols.
  - ISO C 7.1.3 makes defining `malloc` undefined behaviour, whereas C++
    sanctions replacing `operator new`.
  - It defeats AddressSanitizer. This was tried on `soa` with GCC 13.3:
    - with `alloc.c` instrumented, the binary segfaulted in ASan's start-up;
    - with `alloc.c` uninstrumented, it aborted with `free(): invalid pointer`;
    - a probe program linked with the interposer let a plain heap overflow go
      unreported.

    That every candidate is affected is inferred from the mechanism.

  The C++ `soa` passed verify under ASan and UBSan on `w05_small_world`; it was
  the one candidate tried. A portable C wrapper allocator matched the
  interposer on 526 of 526 comparisons. It counts none of the 106 allocations a
  C++ `std::` container test makes, so a mixed build would need both kinds of
  hook.
- **Continuity.** Every result in `benchmarks/` from before the port was
  measured with the C++ build. Staying keeps them.

What C would have kept:
- Sorting is defined in the repository (`sort.inc.h`) rather than by the
  standard library's vendor. libstdc++ and libc++ use different sort
  algorithms, so the order of equal elements can differ. Their vector growth
  coincides for `push_back` and differs only in some resize cases. libc++ was
  not available to test.
- The prototype contract rejected a `uint32_t salt` that the C++ concept
  accepts by conversion. An exact-type `static_assert` in C++ closes that gap.
- About 1.5% less wall time on the candidates (0.985 without the oracles and
  negative controls; 1.003 on the ECS track). No ranking that both builds
  separate changed.
- The two candidate translation units measured, `soa` and `uniform_grid`, cost
  5.5x and 3.6x fewer compiler instructions in C.
- A C interface that Unity, Rust and Python can bind to, once forwarders export
  the static inline candidates and supply the two `gds_vec` helpers from
  `alloc.c`. Unity's Burst would still need pointer parameters in place of
  `Vec3` passed by value.

The cost of keeping C was one of two things: the shim, or a dual C/C++
substrate. The dual substrate was tested in scratch: +98/−37 lines over 19
files, after which all 21 candidates compile as C++20 and reproduce all 263
verify results. On top of that it needed a portable allocator and a full
re-measurement.

## Defects in the C++ suite that the port found (open)

The port's reviewers found these with workloads and probes the suite does not
have. Where both builds were run, they failed identically. None is reachable
from `workloads/public` or `workloads/hidden`.

1. **A NaN query radius hangs six candidates.** The workload parser accepts
   `nan`, because it uses `strtod`. With `query_radius_min: nan`, six
   candidates never return from a k-nearest query: `uniform_grid`,
   `grid_undo_log`, `cell_sorted`, `cell_rows`, `morton_sorted` and
   `spatial_hash`. Each starts its box-doubling loop at
   `std::max(typical_radius, floor_cell)`, which is NaN, so neither exit test
   can become true. Building the grid also converts NaN to `int`, which is
   undefined behaviour. `grid_ring_knn`, `delta_grid`, `axis_sorted`,
   `morton_lbvh` and the oracle pass verify on it in under a second. Probe:
   `workloads/port_review/dgr01_nan_typical_radius.workload`.
2. **Worlds near the float limit.** `xb4_overflow_world` is a world 3e38
   across, where squared distances overflow to infinity. On it, 9 of the 10
   spatial candidates that are not the oracle fail verify; only `morton_lbvh`
   passes. The first failure is the same in both builds: `tick 1, op 230:
   observation mismatch on op kind 3 ... radius 53358885879597236224.000000`.
   Nine of the ten are therefore not correct for worlds whose squared extents
   overflow, and the suite tests no such world.
3. **`delta_grid` k-nearest with infinite distances.** Once `r * r` overflows,
   `found[want-1].d2 <= r*r` holds as `inf <= inf`, and the query returns the k
   lowest ids of the column it has gathered. This was found with a direct probe
   of the structure, not a workload; the generator never aims a query outside
   the world.

Two more inherited defects are latent:
- `sparse_set`'s `query(required == 0)` returns 0, where the contract asks for
  the digest over every live entity. The parser rejects empty masks, so no
  workload reaches it.
- In `spatial_hash` and `morton_sorted`, the float-to-integer conversions are
  undefined outside the integer range or for non-finite values.

The reviewers also found coverage gaps in the ECS replay:
- `integrate` is called only at the end of a frame, and `sync` resolves pending
  work there. So `fused_archetype`'s resolve-on-pending paths in create, add,
  remove, get and set are reached by no workload.
- No workload produces a mask with bits 4–7 or a query with `required == 0`.
- The suite queries only three masks.

Two reviewers covered these paths with differential drivers that called the C
and C++ implementations directly, and the outputs were byte-identical.

Defects that existed only in the C port went with it:
- `std::max` was written with the wrong operand order for NaN and signed zero.
- Two vector macros read their argument after freeing it.

## A finding about the measurement itself

The run-to-run spread is (max − min) / median over the five rounds, per
candidate and workload. Its median is 29% for `step_ns_p50` (29.4% for C++,
29.3% for C) and 26% for `total_ns` (26.1% and 25.7%). It is nearly the same in
both builds, so it does not come from the language. That it comes from the
shared machine and the single-repetition rounds is inferred. A difference
smaller than that cannot be called from one round.

Example: `delta_grid`'s P3 predicts `step_ns_p50` at most 1.2x `cell_sorted`'s
on four workloads, one of them `s01_steady_uniform`. `benchmarks/predictions.md`
records P3 as held, with s01 at 1.164; its "within noise" mark comes from s03
and hs04. Over these rounds the ratio of the median `step_ns_p50` values on s01
was 1.62 in the C++ build and 0.92 in the C build, and the per-round ranges
overlap.

`benchmarks/results.json` was measured with five repetitions per run, so its
medians should be steadier than these single-repetition rounds.
`predictions.md` uses the spread between those five repetitions to mark points
"within noise". The spread from one run to the next was never measured, so
whether its verdicts near a bound would survive a re-run is unknown. Reducing
and quantifying that spread is the next change in measurement, and it does not
depend on the language.

## Sources

`benchmarks/language_port_evidence.md` holds the raw reports of the six agents
that gathered this evidence, with the command behind each number. Two later
checks corrected those reports, and this file incorporates the corrections.
The first was the skeptic's report, the last section of that file. The second
was a two-agent fact-check of this summary: 74 and 78 claims checked, 27 and 41
problems, all fixed here.

## Tools this left behind

- `runner/equivalence.py`: do two builds give the same answers?
- `runner/build_ab.py`: paired, interleaved timing of two builds.
- `runner/rank_stability.py`: do two builds order the candidates the same, and
  how much of that question does the noise let through?
- `workloads/port_review/`: the 33 edge-case workloads described above.
