# Benchmark report

- run: `20260902T092528Z`
- commit: `06a287429df285e685f94c685c83fe1ee4f59e9d`
- cpu: Intel(R) Xeon(R) Processor @ 2.10GHz
- compiler: c++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
- build flags: `-O2 -DNDEBUG -fno-omit-frame-pointer -Wall -Wextra -march=native`
- repetitions per measurement: 5 (plus 1 warmup), the median repetition is reported

> Hardware counters (cycles, instructions, cache references and misses, branch instructions and misses) are **not** in this report. `perf_event_open` failed on this machine: `perf_event_open(cycles) failed: No such file or directory`. No counter has been estimated or modelled; the fields are simply absent.

## Correctness

Every candidate replays the identical op stream beside the oracle. A candidate is measured only after it passes every workload.

| candidate | expectation | result | note |
|---|---|---|---|
| `aos` | pass | passed all 10 |  |
| `archetype` | pass | passed all 10 |  |
| `broken_recycle` | fail | failed 9 of 10 | negative control: rejected by 9 of 10 workloads; passed `w04_random_access` |
| `reference` | pass | passed all 10 |  |
| `soa` | pass | passed all 10 |  |
| `sparse_set` | pass | passed all 10 |  |

All verified candidates produced identical observation checksums on every workload, so they are answering the same questions the same way.

## Measurements

Timing is per frame. Frame 0 carries the initial population load, which is why the `max` column sits far above `p99` on the larger workloads: that column is almost always the load frame, not steady state.

`bytes/entity` is the allocated footprint standing at the end of the run divided by the live population at the end of the run. `peak` is the high-water mark of live allocated bytes during the run, which on a bursty workload occurs at a different moment and a different population.

### `w01_steady_uniform` (public)

entities 50000 initial / 80000 cap, 300 frames, 2000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 726.5 | 950.5 | 1008.7 | 2232.0 | 2.99 | 60.2 | 611 | yes |
| `soa` | 743.2 | 949.9 | 976.8 | 1768.8 | 2.75 | 54.6 | 128 | yes |
| `aos` | 882.3 | 1148.1 | 1255.3 | 2147.0 | 5.25 | 72.8 | 26 |  |
| `sparse_set` | 885.5 | 1074.0 | 1908.1 | 2403.8 | 4.56 | 91.0 | 260 |  |
| `reference` | 3727.6 | 4874.6 | 6222.2 | 7052.0 | 4.11 | 85.5 | 110065 |  |

### `w02_query_heavy` (public)

entities 100000 initial / 120000 cap, 200 frames, 200 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 3377.8 | 3484.3 | 3588.1 | 8620.1 | 6.16 | 63.6 | 560 | yes |
| `soa` | 4004.7 | 4324.2 | 4618.7 | 9300.7 | 5.50 | 55.0 | 134 | yes |
| `aos` | 4845.1 | 5147.4 | 5750.8 | 12516.0 | 10.50 | 73.4 | 26 |  |
| `sparse_set` | 4847.7 | 5225.2 | 5663.1 | 11035.5 | 9.12 | 91.8 | 274 |  |
| `reference` | 5674.0 | 6451.9 | 7491.4 | 9495.4 | 8.19 | 85.8 | 106217 |  |

### `w03_structural_churn` (public)

entities 30000 initial / 60000 cap, 300 frames, 3000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 278.0 | 348.5 | 406.3 | 1198.6 | 2.02 | 70.2 | 636 | yes |
| `aos` | 354.0 | 529.8 | 638.4 | 1164.3 | 2.62 | 61.1 | 26 |  |
| `sparse_set` | 355.6 | 412.5 | 486.7 | 1197.7 | 2.03 | 67.6 | 244 |  |
| `soa` | 405.6 | 513.4 | 570.2 | 1276.6 | 1.38 | 45.8 | 122 | yes |
| `reference` | 2216.5 | 2969.2 | 3150.2 | 3645.5 | 2.39 | 83.2 | 70936 |  |

### `w04_random_access` (public)

entities 200000 initial / 200000 cap, 200 frames, 20000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 3193.6 | 3451.5 | 4543.1 | 7444.7 | 11.94 | 59.0 | 459 | yes |
| `soa` | 3451.6 | 3673.0 | 4579.2 | 7371.1 | 11.00 | 55.0 | 133 | yes |
| `aos` | 4258.4 | 4607.5 | 5235.7 | 9726.7 | 21.00 | 73.4 | 19 |  |
| `sparse_set` | 4265.4 | 4536.0 | 5036.6 | 12160.0 | 18.00 | 86.5 | 279 |  |
| `reference` | 5162.1 | 6394.9 | 7113.3 | 11290.5 | 16.41 | 86.0 | 200015 |  |

### `w05_small_world` (public)

entities 2000 initial / 4000 cap, 400 frames, 2000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `aos` | 65.7 | 78.8 | 92.9 | 146.4 | 0.33 | 85.9 | 23 | yes |
| `soa` | 76.5 | 95.9 | 109.3 | 190.4 | 0.17 | 64.6 | 101 | yes |
| `sparse_set` | 76.9 | 100.8 | 115.3 | 164.3 | 0.25 | 95.0 | 199 |  |
| `archetype` | 79.5 | 96.4 | 110.6 | 117.7 | 0.21 | 77.6 | 457 |  |
| `reference` | 115.8 | 134.4 | 146.7 | 154.4 | 0.23 | 87.1 | 74853 |  |

### `h01_zipf_hotspot` (hidden)

entities 150000 initial / 180000 cap, 250 frames, 4000 ops per frame, access zipf

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 1925.9 | 2041.7 | 2396.6 | 7149.5 | 10.50 | 71.2 | 641 | yes |
| `sparse_set` | 2193.6 | 2594.9 | 3189.4 | 10663.6 | 15.25 | 101.6 | 287 |  |
| `aos` | 2594.3 | 2877.0 | 3038.3 | 8425.7 | 21.00 | 98.1 | 29 |  |
| `soa` | 2737.3 | 2985.5 | 3439.0 | 7974.2 | 11.00 | 73.6 | 143 |  |
| `reference` | 4139.2 | 4570.7 | 4786.4 | 9835.0 | 11.62 | 81.2 | 191530 |  |

### `h02_bursty_spawn` (hidden)

entities 20000 initial / 400000 cap, 300 frames, 500 ops per frame, access recent

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 1844.7 | 3225.0 | 3936.7 | 6575.5 | 21.88 | 70.8 | 699 | yes |
| `soa` | 2083.8 | 4490.9 | 4902.0 | 9758.1 | 22.00 | 83.2 | 144 |  |
| `aos` | 2767.3 | 5032.0 | 5576.7 | 14940.9 | 42.00 | 111.0 | 24 |  |
| `sparse_set` | 2770.2 | 5336.6 | 6146.0 | 10333.1 | 23.00 | 87.2 | 290 |  |
| `reference` | 6559.9 | 11673.1 | 12457.0 | 13068.8 | 20.85 | 82.6 | 313911 | yes |

### `h03_stale_handles` (hidden)

entities 60000 initial / 90000 cap, 250 frames, 4000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 848.9 | 941.4 | 1105.3 | 2378.3 | 4.21 | 73.0 | 643 | yes |
| `soa` | 1001.5 | 1114.7 | 1300.9 | 2357.0 | 2.75 | 45.6 | 129 | yes |
| `aos` | 1014.3 | 1174.3 | 1524.2 | 2365.6 | 5.25 | 60.8 | 27 |  |
| `sparse_set` | 1712.9 | 2004.2 | 2530.9 | 2922.5 | 4.56 | 76.0 | 261 |  |
| `reference` | 6230.5 | 7520.1 | 7817.0 | 7910.3 | 4.80 | 83.3 | 202779 |  |

### `h04_recent_locality` (hidden)

entities 40000 initial / 120000 cap, 300 frames, 5000 ops per frame, access recent

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `soa` | 634.4 | 711.0 | 832.6 | 1682.1 | 2.75 | 69.4 | 131 | yes |
| `archetype` | 653.0 | 732.6 | 802.8 | 1774.1 | 2.98 | 78.4 | 489 | yes |
| `aos` | 704.9 | 902.6 | 1155.3 | 2140.2 | 5.25 | 92.4 | 29 |  |
| `sparse_set` | 768.9 | 842.2 | 986.0 | 2615.7 | 4.12 | 99.0 | 257 |  |
| `reference` | 1121.4 | 1323.5 | 1834.6 | 2610.4 | 3.10 | 80.5 | 320460 |  |

### `h05_sparse_component` (hidden)

entities 200000 initial / 220000 cap, 200 frames, 500 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 367.3 | 435.0 | 494.6 | 9531.9 | 13.31 | 66.2 | 470 | yes |
| `soa` | 1143.6 | 1371.7 | 1720.2 | 9606.2 | 11.00 | 55.1 | 143 | yes |
| `sparse_set` | 1317.1 | 1517.1 | 1687.3 | 13921.6 | 15.31 | 74.1 | 279 |  |
| `aos` | 1545.6 | 1645.8 | 2146.3 | 15287.9 | 21.00 | 73.5 | 29 |  |
| `reference` | 6576.1 | 7992.0 | 8194.2 | 15283.5 | 16.42 | 86.0 | 215035 |  |

## Pareto fronts

Objectives, all minimised: frame_ns_p50, frame_ns_p99, peak_bytes.

| workload | non-dominated |
|---|---|
| `w01_steady_uniform` | `archetype`, `soa` |
| `w02_query_heavy` | `archetype`, `soa` |
| `w03_structural_churn` | `archetype`, `soa` |
| `w04_random_access` | `archetype`, `soa` |
| `w05_small_world` | `aos`, `soa` |
| `h01_zipf_hotspot` | `archetype` |
| `h02_bursty_spawn` | `archetype`, `reference` |
| `h03_stale_handles` | `archetype`, `soa` |
| `h04_recent_locality` | `archetype`, `soa` |
| `h05_sparse_component` | `archetype`, `soa` |

## Public versus held-out standing

Mean rank by p99 frame time, 0 is best. A positive gap means the candidate ranks worse on workloads it was not designed against.

| candidate | public | hidden | gap |
|---|---:|---:|---:|
| `archetype` | 0.6 | 0.0 | -0.60 |
| `reference` | 4.0 | 4.0 | +0.00 |
| `aos` | 2.2 | 2.2 | +0.00 |
| `sparse_set` | 2.2 | 2.2 | +0.00 |
| `soa` | 1.0 | 1.6 | +0.60 |

## Notes

- broken_recycle is a negative control; it is excluded from measurement.

