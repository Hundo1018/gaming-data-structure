# Benchmark report

- run: `20260904T064132Z`
- commit: `406493c69621450e34e267bef84bc312818cfb39`
- cpu: Intel(R) Xeon(R) Processor @ 2.80GHz
- compiler: c++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
- build flags: `-O2 -DNDEBUG -fno-omit-frame-pointer -ffp-contract=off -Wall -Wextra -march=native`
- repetitions per measurement: 3 (plus 1 warmup), the median repetition is reported

Compare within this report, not against an earlier one. Every number here was taken on one machine in one sitting under one set of flags, and the machine is shared. Absolute times move between runs; the orderings and the ratios inside a single workload are what the run establishes.

> Hardware counters (cycles, instructions, cache references and misses, branch instructions and misses) are **not** in this report. `perf_event_open` failed on this machine: `perf_event_open(cycles) failed: No such file or directory`. No counter has been estimated or modelled; the fields are simply absent.

## Correctness

Every candidate replays the identical op stream beside the oracle. A candidate is measured only after it passes every workload.

| candidate | expectation | result | note |
|---|---|---|---|
| `aos` | pass | passed all 10 |  |
| `archetype` | pass | passed all 10 |  |
| `axis_sorted` | pass | passed all 10 |  |
| `broken_recycle` | fail | failed 9 of 10 | negative control: rejected by 9 of 10 workloads; passed `w04_random_access` |
| `brute_force` | pass | passed all 10 |  |
| `grid_undo_log` | pass | passed all 10 |  |
| `morton_sorted` | pass | passed all 10 |  |
| `reference` | pass | passed all 10 |  |
| `soa` | pass | passed all 10 |  |
| `sparse_set` | pass | passed all 10 |  |
| `spatial_hash` | pass | passed all 10 |  |
| `uniform_grid` | pass | passed all 10 |  |

All verified candidates produced identical observation checksums on every workload, so they are answering the same questions the same way.

## Measurements

Timing is per step, where a step is a frame in the ECS track and a tick in the spatial track. Step 0 carries the initial population load, which is why the `max` column sits far above `p99` on the larger workloads: that column is almost always the load step, not steady state.

`bytes/entity` is the allocated footprint standing at the end of the run divided by the live population at the end of the run. `peak` is the high-water mark of live allocated bytes during the run, which on a bursty workload occurs at a different moment and a different population.

### Track: spatial

#### `s01_steady_uniform` (public)

20000 entities, world 1024x1024x256, 200 ticks, 1.0 of them moving per tick at speed 0.5-4.0, query radius 8-16, placement uniform

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `uniform_grid` | 968.0 | 1039.9 | 1117.5 | 1166.8 | 1.10 | 57.5 | 48455 | yes |
| `grid_undo_log` | 984.4 | 1085.8 | 1187.7 | 1879.9 | 1.10 | 57.5 | 48455 |  |
| `spatial_hash` | 1837.0 | 2516.4 | 2760.9 | 3587.2 | 6.55 | 238.7 | 48463 |  |
| `axis_sorted` | 3142.8 | 3348.0 | 3579.0 | 3975.8 | 0.75 | 37.0 | 77899 | yes |
| `morton_sorted` | 3731.0 | 4010.8 | 4625.7 | 4785.5 | 1.02 | 53.0 | 48455 |  |
| `brute_force` | 15339.9 | 16902.8 | 19530.4 | 20616.7 | 0.74 | 26.0 | 6772 | yes |

#### `s02_dense_clustered` (public)

40000 entities, world 1024x1024x256, 150 ticks, 1.0 of them moving per tick at speed 0.2-1.5, query radius 4-8, placement clustered

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `uniform_grid` | 5769.6 | 9590.4 | 10080.8 | 10366.4 | 5.84 | 150.7 | 31250 | yes |
| `grid_undo_log` | 5961.9 | 9761.6 | 10510.7 | 10552.6 | 5.84 | 150.7 | 31250 |  |
| `spatial_hash` | 6304.4 | 10038.9 | 10699.1 | 11464.1 | 1.29 | 32.3 | 31253 | yes |
| `morton_sorted` | 8886.4 | 12036.5 | 12992.7 | 13288.1 | 2.12 | 53.0 | 31250 |  |
| `axis_sorted` | 9077.4 | 12183.6 | 12930.7 | 13110.1 | 1.51 | 37.0 | 31599 |  |
| `brute_force` | 18016.3 | 21197.8 | 22062.7 | 25769.9 | 1.49 | 26.0 | 2688 |  |

#### `s03_wide_radius` (public)

20000 entities, world 1024x1024x256, 150 ticks, 1.0 of them moving per tick at speed 0.5-4.0, query radius 64-128, placement uniform

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `uniform_grid` | 1308.2 | 1459.9 | 1488.8 | 1493.1 | 0.50 | 25.1 | 13834 | yes |
| `spatial_hash` | 1315.1 | 1489.4 | 1646.5 | 1836.9 | 0.59 | 29.8 | 13834 |  |
| `grid_undo_log` | 1320.4 | 1440.4 | 1476.8 | 1720.1 | 0.50 | 25.1 | 13834 | yes |
| `morton_sorted` | 3069.0 | 3231.1 | 3355.5 | 3386.9 | 1.03 | 53.0 | 13834 |  |
| `axis_sorted` | 3513.9 | 3833.9 | 4417.1 | 5349.1 | 0.80 | 37.0 | 15420 |  |
| `brute_force` | 4463.4 | 4734.2 | 5102.9 | 5694.9 | 0.74 | 26.0 | 1496 |  |

#### `s04_teleport` (public)

20000 entities, world 1024x1024x256, 200 ticks, 1.0 of them moving per tick at speed 0.5-4.0, query radius 8-16, placement uniform, teleport ratio 0.33

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `grid_undo_log` | 1138.3 | 1223.4 | 1338.7 | 1452.0 | 1.10 | 57.5 | 48481 | yes |
| `uniform_grid` | 1154.3 | 1300.2 | 1366.4 | 1390.2 | 1.10 | 57.5 | 48481 |  |
| `spatial_hash` | 2328.1 | 2748.8 | 3535.8 | 4290.5 | 6.55 | 238.7 | 48489 |  |
| `axis_sorted` | 3192.5 | 3418.3 | 3712.0 | 4271.8 | 0.75 | 37.0 | 77897 | yes |
| `morton_sorted` | 3658.5 | 3820.0 | 4027.0 | 4207.2 | 1.01 | 53.0 | 48481 |  |
| `brute_force` | 15246.7 | 15758.6 | 19224.7 | 25310.0 | 0.74 | 26.0 | 6772 | yes |

#### `s05_small_world` (public)

1500 entities, world 256x256x64, 400 ticks, 1.0 of them moving per tick at speed 0.5-3.0, query radius 4-12, placement uniform

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `uniform_grid` | 239.5 | 288.2 | 312.3 | 319.2 | 0.08 | 51.1 | 94535 | yes |
| `grid_undo_log` | 253.5 | 322.6 | 358.0 | 410.2 | 0.08 | 51.1 | 94535 |  |
| `axis_sorted` | 494.3 | 642.6 | 810.7 | 851.4 | 0.06 | 37.0 | 124097 | yes |
| `spatial_hash` | 642.3 | 813.0 | 948.3 | 2683.6 | 0.42 | 203.8 | 94539 |  |
| `brute_force` | 1205.7 | 1436.1 | 1774.8 | 2522.5 | 0.06 | 26.1 | 13572 | yes |
| `morton_sorted` | 1316.9 | 1459.1 | 1597.7 | 1689.5 | 0.08 | 53.0 | 94535 |  |

### Track: ecs

#### `w01_steady_uniform` (public)

entities 50000 initial / 80000 cap, 300 frames, 2000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 1115.7 | 1320.0 | 1701.3 | 3513.5 | 2.99 | 60.2 | 611 | yes |
| `aos` | 1293.3 | 1409.8 | 1626.5 | 3941.9 | 5.25 | 72.8 | 26 | yes |
| `soa` | 1408.5 | 1534.6 | 1818.4 | 3545.2 | 2.75 | 54.6 | 128 | yes |
| `sparse_set` | 1739.5 | 2070.9 | 2573.8 | 5542.1 | 4.56 | 91.0 | 260 |  |
| `reference` | 3034.8 | 3758.8 | 4010.4 | 5811.9 | 4.11 | 85.5 | 110065 |  |

#### `w02_query_heavy` (public)

entities 100000 initial / 120000 cap, 200 frames, 200 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 4974.1 | 5230.4 | 5512.5 | 12873.5 | 6.16 | 63.6 | 560 | yes |
| `aos` | 5596.9 | 6190.0 | 8204.2 | 22781.1 | 10.50 | 73.4 | 26 |  |
| `soa` | 5979.9 | 6226.3 | 7026.5 | 13142.6 | 5.50 | 55.0 | 134 | yes |
| `reference` | 6769.5 | 7226.3 | 7347.4 | 17376.0 | 8.19 | 85.8 | 106217 |  |
| `sparse_set` | 7059.2 | 8109.8 | 9095.1 | 17761.6 | 9.12 | 91.8 | 274 |  |

#### `w03_structural_churn` (public)

entities 30000 initial / 60000 cap, 300 frames, 3000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 398.0 | 498.4 | 553.2 | 1774.3 | 2.02 | 70.2 | 636 | yes |
| `aos` | 546.7 | 691.0 | 732.0 | 2036.4 | 2.62 | 61.1 | 26 |  |
| `soa` | 560.0 | 725.6 | 853.5 | 1994.5 | 1.38 | 45.8 | 122 | yes |
| `sparse_set` | 587.5 | 635.8 | 659.4 | 2365.3 | 2.03 | 67.6 | 244 |  |
| `reference` | 1564.9 | 2040.7 | 2161.5 | 3234.6 | 2.39 | 83.2 | 70936 |  |

#### `w04_random_access` (public)

entities 200000 initial / 200000 cap, 200 frames, 20000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `soa` | 6400.8 | 7191.3 | 7574.4 | 14615.2 | 11.00 | 55.0 | 133 | yes |
| `aos` | 6489.6 | 7284.7 | 8275.1 | 15346.3 | 21.00 | 73.4 | 19 |  |
| `archetype` | 6570.6 | 7581.0 | 8182.1 | 13689.0 | 11.94 | 59.0 | 459 |  |
| `sparse_set` | 7341.2 | 8658.3 | 11642.2 | 20914.1 | 18.00 | 86.5 | 279 |  |
| `reference` | 10063.0 | 12145.5 | 13106.2 | 24887.2 | 16.41 | 86.0 | 200015 |  |

#### `w05_small_world` (public)

entities 2000 initial / 4000 cap, 400 frames, 2000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `aos` | 101.6 | 128.2 | 157.2 | 191.8 | 0.33 | 85.9 | 23 | yes |
| `sparse_set` | 104.2 | 130.6 | 146.9 | 200.6 | 0.25 | 95.0 | 199 | yes |
| `soa` | 108.7 | 135.5 | 152.5 | 197.3 | 0.17 | 64.6 | 101 | yes |
| `archetype` | 113.3 | 150.3 | 166.6 | 197.0 | 0.21 | 77.6 | 457 |  |
| `reference` | 154.6 | 192.8 | 241.0 | 411.2 | 0.23 | 87.1 | 74853 |  |

#### `h01_zipf_hotspot` (hidden)

entities 150000 initial / 180000 cap, 250 frames, 4000 ops per frame, access zipf

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 2671.7 | 2865.9 | 3404.4 | 11212.1 | 10.50 | 71.2 | 641 | yes |
| `sparse_set` | 3415.0 | 3579.9 | 3773.5 | 18255.3 | 15.25 | 101.6 | 287 |  |
| `aos` | 3684.4 | 3911.4 | 4590.6 | 12968.0 | 21.00 | 98.1 | 29 |  |
| `soa` | 4005.1 | 4439.1 | 5921.6 | 12460.3 | 11.00 | 73.6 | 143 |  |
| `reference` | 5125.3 | 6090.6 | 6445.8 | 18520.8 | 11.62 | 81.2 | 191530 |  |

#### `h02_bursty_spawn` (hidden)

entities 20000 initial / 400000 cap, 300 frames, 500 ops per frame, access recent

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 2789.3 | 4706.1 | 5915.8 | 9743.7 | 21.88 | 70.8 | 699 | yes |
| `aos` | 3531.9 | 7004.9 | 7704.0 | 21296.6 | 42.00 | 111.0 | 24 |  |
| `sparse_set` | 3776.6 | 7250.1 | 8609.1 | 14372.1 | 23.00 | 87.2 | 290 |  |
| `soa` | 3785.4 | 6707.7 | 7168.4 | 14606.7 | 22.00 | 83.2 | 144 |  |
| `reference` | 7625.1 | 22001.4 | 24946.9 | 30098.1 | 20.85 | 82.6 | 313911 | yes |

#### `h03_stale_handles` (hidden)

entities 60000 initial / 90000 cap, 250 frames, 4000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 1397.4 | 1566.6 | 1953.8 | 4206.0 | 4.21 | 73.0 | 643 | yes |
| `aos` | 1612.1 | 1733.1 | 1812.4 | 4268.9 | 5.25 | 60.8 | 27 | yes |
| `soa` | 1835.0 | 2152.3 | 2623.4 | 4172.8 | 2.75 | 45.6 | 129 | yes |
| `sparse_set` | 2339.5 | 2749.1 | 2994.0 | 5648.9 | 4.56 | 76.0 | 261 |  |
| `reference` | 4753.9 | 5984.0 | 8078.8 | 8890.9 | 4.80 | 83.3 | 202779 |  |

#### `h04_recent_locality` (hidden)

entities 40000 initial / 120000 cap, 300 frames, 5000 ops per frame, access recent

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 998.4 | 1118.2 | 1251.5 | 2929.3 | 2.98 | 78.4 | 489 | yes |
| `aos` | 1044.8 | 1143.2 | 1295.9 | 3001.2 | 5.25 | 92.4 | 29 |  |
| `soa` | 1152.7 | 1241.0 | 1364.4 | 2913.7 | 2.75 | 69.4 | 131 | yes |
| `sparse_set` | 1206.0 | 1774.1 | 2060.6 | 4056.3 | 4.12 | 99.0 | 257 |  |
| `reference` | 1317.2 | 1393.4 | 1439.6 | 4287.7 | 3.10 | 80.5 | 320460 |  |

#### `h05_sparse_component` (hidden)

entities 200000 initial / 220000 cap, 200 frames, 500 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 562.2 | 668.2 | 773.2 | 18550.4 | 13.31 | 66.2 | 470 | yes |
| `sparse_set` | 1242.7 | 1477.2 | 1562.5 | 23045.0 | 15.31 | 74.1 | 279 |  |
| `soa` | 1604.3 | 1790.1 | 2488.5 | 17647.9 | 11.00 | 55.1 | 143 | yes |
| `aos` | 1825.8 | 2068.3 | 2749.3 | 26868.3 | 21.00 | 73.5 | 29 |  |
| `reference` | 7547.8 | 9565.4 | 10178.0 | 28141.3 | 16.42 | 86.0 | 215035 |  |

#### `hs01_rewind_all_moving` (hidden)

30000 entities, world 1024x1024x256, 200 ticks, 1.0 of them moving per tick at speed 0.5-4.0, query radius 8-16, placement uniform, rewind every 10 ticks by 6

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | rewind | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|---|:--:|
| `uniform_grid` | 1997.5 | 2929.8 | 3086.0 | 3147.4 | 7.75 | 252.6 | 18953 | snapshot_rebuild | yes |
| `grid_undo_log` | 2297.3 | 5599.7 | 6063.9 | 7465.0 | 11.17 | 352.8 | 21640 | native |  |
| `spatial_hash` | 2674.2 | 4534.5 | 4823.6 | 6548.5 | 9.26 | 304.6 | 19093 | snapshot_rebuild |  |
| `morton_sorted` | 4499.9 | 4962.9 | 7003.9 | 7139.3 | 8.64 | 281.8 | 19192 | snapshot_rebuild |  |
| `axis_sorted` | 4720.4 | 4991.6 | 5091.2 | 5687.9 | 8.02 | 261.8 | 29546 | snapshot_rebuild |  |
| `brute_force` | 10231.4 | 11351.6 | 12680.4 | 13115.7 | 6.32 | 203.3 | 2795 | native | yes |

#### `hs02_rewind_few_moving` (hidden)

30000 entities, world 1024x1024x256, 200 ticks, 0.02 of them moving per tick at speed 0.5-4.0, query radius 8-16, placement uniform, rewind every 10 ticks by 6

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | rewind | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|---|:--:|
| `grid_undo_log` | 141.1 | 226.5 | 288.4 | 1667.0 | 2.92 | 77.9 | 20647 | native | yes |
| `uniform_grid` | 184.2 | 745.8 | 871.9 | 1228.3 | 7.12 | 234.9 | 18955 | snapshot_rebuild |  |
| `spatial_hash` | 426.3 | 2110.6 | 2379.2 | 3030.6 | 7.62 | 252.3 | 19075 | snapshot_rebuild |  |
| `morton_sorted` | 3435.1 | 3821.4 | 4192.2 | 4242.1 | 8.06 | 264.9 | 19194 | snapshot_rebuild |  |
| `axis_sorted` | 3663.6 | 4059.9 | 4610.2 | 4691.4 | 7.41 | 244.9 | 29526 | snapshot_rebuild |  |
| `brute_force` | 8897.4 | 9501.5 | 11270.6 | 11930.7 | 5.77 | 187.5 | 2795 | native |  |

#### `hs03_knn_heavy` (hidden)

25000 entities, world 1024x1024x256, 150 ticks, 1.0 of them moving per tick at speed 0.5-4.0, query radius 8-16, placement uniform

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `uniform_grid` | 5723.3 | 6199.6 | 6979.9 | 9041.6 | 1.23 | 51.0 | 201847 | yes |
| `grid_undo_log` | 6123.8 | 6583.5 | 6740.7 | 6802.1 | 1.23 | 51.0 | 201847 | yes |
| `axis_sorted` | 15251.8 | 15842.9 | 17109.6 | 18212.7 | 0.98 | 37.0 | 249767 | yes |
| `spatial_hash` | 19890.3 | 25235.6 | 26206.0 | 26357.3 | 6.69 | 196.8 | 201855 |  |
| `brute_force` | 20115.4 | 21542.9 | 23905.5 | 26537.9 | 0.93 | 26.0 | 19376 | yes |
| `morton_sorted` | 34043.7 | 35355.4 | 36946.8 | 37556.5 | 1.28 | 53.0 | 201847 |  |

#### `hs04_flat_world` (hidden)

25000 entities, world 1024x1024x8, 150 ticks, 1.0 of them moving per tick at speed 0.5-4.0, query radius 8-16, placement uniform

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `grid_undo_log` | 997.0 | 1113.0 | 1514.5 | 1568.3 | 0.63 | 26.2 | 16173 | yes |
| `uniform_grid` | 1005.8 | 1217.3 | 1595.8 | 1710.7 | 0.63 | 26.2 | 16173 |  |
| `spatial_hash` | 1170.4 | 1438.4 | 1705.0 | 2151.7 | 1.07 | 39.5 | 16177 |  |
| `morton_sorted` | 3044.8 | 3299.8 | 3798.0 | 4466.7 | 1.27 | 53.0 | 16173 |  |
| `axis_sorted` | 3186.0 | 3620.8 | 3935.2 | 3986.0 | 0.91 | 37.0 | 26105 |  |
| `brute_force` | 9830.4 | 11606.0 | 13184.8 | 13748.5 | 0.93 | 26.0 | 2688 |  |

#### `hs05_spawn_churn` (hidden)

20000 entities, world 1024x1024x256, 150 ticks, 1.0 of them moving per tick at speed 0.5-4.0, query radius 8-16, placement uniform

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `uniform_grid` | 1022.2 | 1212.7 | 1426.6 | 1442.5 | 2.52 | 132.0 | 18159 | yes |
| `grid_undo_log` | 1039.5 | 1197.0 | 1402.0 | 1494.1 | 2.52 | 132.0 | 18159 | yes |
| `spatial_hash` | 1749.2 | 2013.4 | 2306.0 | 3576.3 | 8.20 | 325.1 | 18167 |  |
| `axis_sorted` | 3041.5 | 3241.4 | 3455.0 | 3806.8 | 1.49 | 75.7 | 29175 | yes |
| `morton_sorted` | 3165.0 | 3351.5 | 3710.3 | 3971.0 | 1.75 | 91.7 | 18159 |  |
| `brute_force` | 36229.0 | 37274.4 | 42049.3 | 47485.4 | 2.96 | 103.5 | 2688 |  |

## Pareto fronts

Objectives, all minimised: step_ns_p50, step_ns_p99, peak_bytes.

| track | workload | non-dominated |
|---|---|---|
| spatial | `s01_steady_uniform` | `axis_sorted`, `brute_force`, `uniform_grid` |
| spatial | `s02_dense_clustered` | `spatial_hash`, `uniform_grid` |
| spatial | `s03_wide_radius` | `grid_undo_log`, `uniform_grid` |
| spatial | `s04_teleport` | `axis_sorted`, `brute_force`, `grid_undo_log` |
| spatial | `s05_small_world` | `axis_sorted`, `brute_force`, `uniform_grid` |
| ecs | `w01_steady_uniform` | `aos`, `archetype`, `soa` |
| ecs | `w02_query_heavy` | `archetype`, `soa` |
| ecs | `w03_structural_churn` | `archetype`, `soa` |
| ecs | `w04_random_access` | `soa` |
| ecs | `w05_small_world` | `aos`, `soa`, `sparse_set` |
| ecs | `h01_zipf_hotspot` | `archetype` |
| ecs | `h02_bursty_spawn` | `archetype`, `reference` |
| ecs | `h03_stale_handles` | `aos`, `archetype`, `soa` |
| ecs | `h04_recent_locality` | `archetype`, `soa` |
| ecs | `h05_sparse_component` | `archetype`, `soa` |
| spatial | `hs01_rewind_all_moving` | `brute_force`, `uniform_grid` |
| spatial | `hs02_rewind_few_moving` | `grid_undo_log` |
| spatial | `hs03_knn_heavy` | `axis_sorted`, `brute_force`, `grid_undo_log`, `uniform_grid` |
| spatial | `hs04_flat_world` | `grid_undo_log` |
| spatial | `hs05_spawn_churn` | `axis_sorted`, `grid_undo_log`, `uniform_grid` |

## Public versus held-out standing

Mean rank by p99 step time, 0 is best. A positive gap means the candidate ranks worse on workloads it was not designed against.

Ranks are computed within a track: the two tracks ask different questions of different structures and a rank across both would mean nothing.

| track | candidate | public | hidden | gap |
|---|---|---:|---:|---:|
| ecs | `archetype` | 1.0 | 0.2 | -0.80 |
| ecs | `aos` | 1.8 | 1.6 | -0.20 |
| ecs | `reference` | 3.6 | 3.8 | +0.20 |
| ecs | `sparse_set` | 2.2 | 2.4 | +0.20 |
| ecs | `soa` | 1.4 | 2.0 | +0.60 |
| spatial | `brute_force` | 5.0 | 4.6 | -0.40 |
| spatial | `axis_sorted` | 3.0 | 3.0 | +0.00 |
| spatial | `grid_undo_log` | 0.6 | 0.6 | +0.00 |
| spatial | `morton_sorted` | 3.8 | 3.8 | +0.00 |
| spatial | `spatial_hash` | 2.2 | 2.2 | +0.00 |
| spatial | `uniform_grid` | 0.4 | 0.8 | +0.40 |

## Notes

- broken_recycle is a negative control; it is excluded from measurement.

