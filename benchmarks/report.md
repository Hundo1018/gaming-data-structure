# Benchmark report

- run: `20261009T042136Z`
- commit: `ed47f82bf736c04a277502050e885d27d0b1fad9`
- cpu: Intel(R) Xeon(R) Processor @ 2.10GHz
- compiler: c++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
- build flags: `-O2 -DNDEBUG -fno-omit-frame-pointer -ffp-contract=off -Wall -Wextra -march=native`
- repetitions per measurement: 5 (plus 1 warmup), the median repetition is reported

Compare within this report, not against an earlier one. Every number here was taken on one machine in one sitting under one set of flags, and the machine is shared. Absolute times move between runs; the orderings and the ratios inside a single workload are what the run establishes.

> Hardware counters (cycles, instructions, cache references and misses, branch instructions and misses) are **not** in this report. `perf_event_open` failed on this machine: `perf_event_open(cycles) failed: No such file or directory`. No counter has been estimated or modelled; the fields are simply absent.

## Correctness

Every candidate replays the identical op stream beside the oracle. A candidate is measured only after it passes every workload.

| candidate | expectation | result | note |
|---|---|---|---|
| `aos` | pass | passed all 12 |  |
| `archetype` | pass | passed all 12 |  |
| `axis_sorted` | pass | passed all 13 |  |
| `bitset_soa` | pass | passed all 12 |  |
| `broken_recycle` | fail | failed 9 of 12 | negative control: rejected by 9 of 12 workloads; passed `w04_random_access`, `w06_point_narrow`, `w07_point_wide` |
| `brute_force` | pass | passed all 13 |  |
| `cell_rows` | pass | passed all 13 |  |
| `cell_sorted` | pass | passed all 13 |  |
| `delta_grid` | pass | passed all 13 |  |
| `fused_archetype` | pass | passed all 12 |  |
| `grid_ring_knn` | pass | passed all 13 |  |
| `grid_undo_log` | pass | passed all 13 |  |
| `grouped_sparse_set` | pass | passed all 12 |  |
| `morton_lbvh` | pass | passed all 13 |  |
| `morton_sorted` | pass | passed all 13 |  |
| `query_memo` | fail | failed 1 of 12 | negative control: rejected by 1 of 12 workloads; passed `h01_zipf_hotspot`, `h02_bursty_spawn`, `h03_stale_handles`, `h04_recent_locality`, `h05_sparse_component`, `w01_steady_uniform`, `w02_query_heavy`, `w03_structural_churn`, `w05_small_world`, `w06_point_narrow`, `w07_point_wide` |
| `reference` | pass | passed all 12 |  |
| `soa` | pass | passed all 12 |  |
| `sparse_set` | pass | passed all 12 |  |
| `spatial_hash` | pass | passed all 13 |  |
| `uniform_grid` | pass | passed all 13 |  |

All verified candidates produced identical observation checksums on every workload, so they are answering the same questions the same way.

## Measurements

Timing is per step, where a step is a frame in the ECS track and a tick in the spatial track. Step 0 creates the initial population and nothing else, so it is left out of every step percentile and of `max`, which describe steady state; the load step is archived on its own as `load_step_ns`.

`bytes/entity` is the allocated footprint standing at the end of the run divided by the live population at the end of the run. `peak` is the high-water mark of live allocated bytes during the run, which on a bursty workload occurs at a different moment and a different population.

### Track: spatial

#### `s01_steady_uniform` (public)

20000 entities, world 1024x1024x256, 200 ticks, 1.0 of them moving per tick at speed 0.5-4.0, query radius 8-16, placement uniform

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `cell_rows` | 712.7 | 951.8 | 971.9 | 1030.6 | 1.26 | 65.5 | 48457 | yes |
| `grid_ring_knn` | 726.2 | 777.7 | 837.6 | 1057.6 | 1.10 | 57.5 | 7 | yes |
| `cell_sorted` | 759.6 | 1002.8 | 1022.1 | 1126.9 | 1.26 | 65.5 | 48457 |  |
| `grid_undo_log` | 819.9 | 1093.7 | 1142.7 | 1190.8 | 1.10 | 57.5 | 48455 |  |
| `uniform_grid` | 831.9 | 918.9 | 982.5 | 1110.3 | 1.10 | 57.5 | 48455 |  |
| `morton_lbvh` | 863.9 | 975.6 | 1064.3 | 1149.6 | 0.89 | 46.8 | 21 | yes |
| `delta_grid` | 881.8 | 1087.1 | 1156.5 | 1670.4 | 2.22 | 116.1 | 48467 |  |
| `spatial_hash` | 1763.7 | 2350.6 | 2512.4 | 3626.1 | 6.55 | 238.7 | 48463 |  |
| `axis_sorted` | 3293.9 | 3463.5 | 4091.8 | 4468.4 | 0.75 | 37.0 | 77899 | yes |
| `morton_sorted` | 3617.2 | 4181.2 | 6773.2 | 9441.5 | 1.02 | 53.0 | 48455 |  |
| `brute_force` | 10769.7 | 11364.6 | 11659.3 | 14326.0 | 0.74 | 26.0 | 6772 | yes |

#### `s02_dense_clustered` (public)

40000 entities, world 1024x1024x256, 150 ticks, 1.0 of them moving per tick at speed 0.2-1.5, query radius 4-8, placement clustered

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `morton_lbvh` | 2812.0 | 4449.7 | 4802.4 | 4986.7 | 1.79 | 46.8 | 22 | yes |
| `cell_rows` | 3330.1 | 5218.7 | 5735.4 | 5879.6 | 6.15 | 158.7 | 31252 |  |
| `delta_grid` | 3651.6 | 5865.3 | 6317.7 | 10111.4 | 11.63 | 302.5 | 31262 |  |
| `cell_sorted` | 3655.8 | 5266.4 | 7261.8 | 8130.1 | 6.15 | 158.7 | 31252 |  |
| `grid_ring_knn` | 4662.2 | 7487.9 | 7974.4 | 8818.5 | 5.75 | 150.7 | 7 |  |
| `spatial_hash` | 4976.0 | 8068.6 | 8425.7 | 8518.5 | 1.29 | 32.3 | 31253 | yes |
| `uniform_grid` | 5060.7 | 7940.8 | 8646.8 | 12025.4 | 5.84 | 150.7 | 31250 |  |
| `grid_undo_log` | 5118.1 | 8210.3 | 8960.9 | 15136.7 | 5.84 | 150.7 | 31250 |  |
| `morton_sorted` | 8425.8 | 11094.7 | 11926.3 | 12233.3 | 2.12 | 53.0 | 31250 |  |
| `axis_sorted` | 9399.5 | 12462.0 | 13074.4 | 13463.1 | 1.51 | 37.0 | 31599 |  |
| `brute_force` | 13365.6 | 17971.3 | 19060.5 | 21438.8 | 1.49 | 26.0 | 2688 |  |

#### `s03_wide_radius` (public)

20000 entities, world 1024x1024x256, 150 ticks, 1.0 of them moving per tick at speed 0.5-4.0, query radius 64-128, placement uniform

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `cell_rows` | 783.2 | 909.6 | 978.4 | 1414.8 | 0.65 | 33.1 | 13836 | yes |
| `cell_sorted` | 790.6 | 846.3 | 1026.7 | 1656.3 | 0.65 | 33.1 | 13836 |  |
| `delta_grid` | 934.5 | 1789.9 | 2380.7 | 2775.0 | 1.00 | 51.1 | 13846 |  |
| `morton_lbvh` | 1017.4 | 1125.0 | 1621.2 | 2188.4 | 0.90 | 46.9 | 24 |  |
| `grid_ring_knn` | 1078.1 | 1156.0 | 1210.1 | 1247.4 | 0.48 | 25.1 | 7 | yes |
| `grid_undo_log` | 1124.8 | 1218.3 | 1340.8 | 1509.5 | 0.50 | 25.1 | 13834 |  |
| `spatial_hash` | 1185.2 | 1549.5 | 1716.0 | 1764.8 | 0.59 | 29.8 | 13834 |  |
| `uniform_grid` | 1191.8 | 1316.6 | 1881.0 | 3395.3 | 0.50 | 25.1 | 13834 |  |
| `morton_sorted` | 3080.3 | 3784.8 | 3959.3 | 3982.9 | 1.03 | 53.0 | 13834 |  |
| `brute_force` | 3321.5 | 4301.4 | 5492.6 | 5988.5 | 0.74 | 26.0 | 1496 |  |
| `axis_sorted` | 3642.6 | 4138.9 | 4336.5 | 5166.9 | 0.80 | 37.0 | 15420 |  |

#### `s04_teleport` (public)

20000 entities, world 1024x1024x256, 200 ticks, 1.0 of them moving per tick at speed 0.5-4.0, query radius 8-16, placement uniform, teleport ratio 0.33

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `cell_rows` | 725.2 | 984.3 | 1236.0 | 1399.4 | 1.25 | 65.5 | 48483 | yes |
| `cell_sorted` | 763.0 | 1009.3 | 1029.5 | 1086.8 | 1.25 | 65.5 | 48483 | yes |
| `morton_lbvh` | 853.6 | 1125.0 | 1257.0 | 1383.0 | 0.89 | 46.8 | 21 | yes |
| `delta_grid` | 881.5 | 1093.2 | 1135.8 | 1163.5 | 2.22 | 116.1 | 48493 |  |
| `grid_ring_knn` | 916.8 | 1230.8 | 1825.0 | 2203.9 | 1.10 | 57.5 | 7 |  |
| `grid_undo_log` | 980.8 | 1283.0 | 1934.9 | 2162.3 | 1.10 | 57.5 | 48481 |  |
| `uniform_grid` | 1000.4 | 1064.8 | 1158.6 | 1873.4 | 1.10 | 57.5 | 48481 | yes |
| `spatial_hash` | 2118.4 | 2450.8 | 2780.8 | 3894.6 | 6.55 | 238.7 | 48489 |  |
| `axis_sorted` | 3299.8 | 3563.0 | 4210.6 | 4412.8 | 0.75 | 37.0 | 77897 | yes |
| `morton_sorted` | 3653.6 | 3870.9 | 4328.8 | 4763.2 | 1.01 | 53.0 | 48481 |  |
| `brute_force` | 11075.0 | 13833.2 | 17325.3 | 17865.7 | 0.74 | 26.0 | 6772 | yes |

#### `s05_small_world` (public)

1500 entities, world 256x256x64, 400 ticks, 1.0 of them moving per tick at speed 0.5-3.0, query radius 4-12, placement uniform

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `grid_ring_knn` | 160.5 | 189.4 | 234.7 | 564.6 | 0.07 | 51.2 | 7 | yes |
| `cell_rows` | 179.4 | 208.1 | 269.3 | 314.6 | 0.09 | 59.2 | 94537 |  |
| `cell_sorted` | 225.5 | 255.4 | 283.4 | 1261.3 | 0.09 | 59.2 | 94537 |  |
| `morton_lbvh` | 236.1 | 260.8 | 291.5 | 733.0 | 0.07 | 48.2 | 21 | yes |
| `grid_undo_log` | 239.3 | 263.6 | 293.2 | 320.5 | 0.08 | 51.1 | 94535 |  |
| `uniform_grid` | 243.5 | 265.5 | 278.2 | 344.5 | 0.08 | 51.1 | 94535 |  |
| `delta_grid` | 247.3 | 286.1 | 405.3 | 1134.6 | 0.15 | 103.3 | 94547 |  |
| `axis_sorted` | 510.6 | 559.3 | 614.5 | 697.5 | 0.06 | 37.0 | 124097 | yes |
| `spatial_hash` | 716.6 | 917.5 | 1016.5 | 1087.9 | 0.42 | 203.8 | 94539 |  |
| `brute_force` | 897.4 | 1428.4 | 1480.7 | 1644.3 | 0.06 | 26.1 | 13572 | yes |
| `morton_sorted` | 1357.0 | 1472.9 | 1558.2 | 1951.4 | 0.08 | 53.0 | 94535 |  |

### Track: ecs

#### `w01_steady_uniform` (public)

entities 50000 initial / 80000 cap, 300 frames, 2000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `archetype` | 823.9 | 980.4 | 1156.8 | 1371.5 | 2.99 | 60.2 | 611 | yes |
| `fused_archetype` | 827.3 | 954.3 | 1071.0 | 2901.4 | 2.99 | 60.2 | 611 | yes |
| `bitset_soa` | 847.1 | 1008.1 | 1144.8 | 1230.0 | 2.77 | 55.5 | 183 | yes |
| `grouped_sparse_set` | 866.4 | 1191.8 | 1374.5 | 2014.9 | 4.56 | 91.0 | 260 |  |
| `soa` | 1060.3 | 1251.3 | 1550.8 | 1784.0 | 2.75 | 54.6 | 128 | yes |
| `aos` | 1066.1 | 1183.0 | 1329.7 | 2042.1 | 5.25 | 72.8 | 26 |  |
| `sparse_set` | 1138.9 | 1263.4 | 1428.3 | 1944.4 | 4.56 | 91.0 | 260 |  |
| `reference` | 3017.6 | 4195.8 | 4429.0 | 4573.6 | 4.11 | 85.5 | 110065 |  |

#### `w02_query_heavy` (public)

entities 100000 initial / 120000 cap, 200 frames, 200 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `fused_archetype` | 4043.3 | 4526.9 | 5530.2 | 6862.9 | 6.16 | 63.6 | 560 | yes |
| `archetype` | 4132.2 | 4744.9 | 5506.6 | 5877.7 | 6.16 | 63.6 | 560 | yes |
| `bitset_soa` | 4139.2 | 4404.8 | 5532.4 | 7515.5 | 5.54 | 55.9 | 194 | yes |
| `aos` | 4318.7 | 4696.4 | 6148.7 | 7183.1 | 10.50 | 73.4 | 26 |  |
| `grouped_sparse_set` | 4630.4 | 5073.1 | 5737.2 | 9915.1 | 9.12 | 91.8 | 274 |  |
| `soa` | 4809.9 | 5460.5 | 7354.2 | 7394.6 | 5.50 | 55.0 | 134 | yes |
| `sparse_set` | 5265.7 | 6089.2 | 6543.1 | 6947.2 | 9.12 | 91.8 | 274 |  |
| `reference` | 5521.3 | 5839.8 | 6038.8 | 6085.1 | 8.19 | 85.8 | 106217 |  |

#### `w03_structural_churn` (public)

entities 30000 initial / 60000 cap, 300 frames, 3000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `bitset_soa` | 268.1 | 349.0 | 414.0 | 1299.4 | 1.38 | 46.5 | 172 | yes |
| `grouped_sparse_set` | 296.9 | 376.2 | 409.6 | 505.2 | 2.03 | 67.6 | 244 | yes |
| `fused_archetype` | 326.8 | 415.5 | 450.1 | 518.8 | 2.02 | 70.2 | 636 |  |
| `archetype` | 343.9 | 468.7 | 534.2 | 683.3 | 2.02 | 70.2 | 636 |  |
| `aos` | 476.9 | 658.4 | 843.8 | 879.0 | 2.62 | 61.1 | 26 |  |
| `soa` | 480.5 | 600.1 | 632.1 | 648.1 | 1.38 | 45.8 | 122 | yes |
| `sparse_set` | 487.8 | 551.0 | 649.9 | 846.9 | 2.03 | 67.6 | 244 |  |
| `reference` | 1549.7 | 2018.2 | 2183.9 | 3107.9 | 2.39 | 83.2 | 70936 |  |

#### `w04_random_access` (public)

entities 200000 initial / 200000 cap, 200 frames, 20000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `aos` | 3777.8 | 4048.9 | 4232.7 | 4356.9 | 21.00 | 73.4 | 19 | yes |
| `bitset_soa` | 3852.3 | 4019.1 | 4345.8 | 5595.6 | 11.08 | 55.9 | 198 | yes |
| `fused_archetype` | 3961.9 | 4486.2 | 4781.7 | 4887.6 | 11.94 | 59.0 | 459 |  |
| `archetype` | 4020.2 | 4621.4 | 4956.4 | 6153.5 | 11.94 | 59.0 | 459 |  |
| `grouped_sparse_set` | 4222.5 | 4736.2 | 5277.0 | 5836.5 | 18.00 | 86.5 | 279 |  |
| `soa` | 4275.8 | 4809.9 | 5689.5 | 5858.4 | 11.00 | 55.0 | 133 | yes |
| `sparse_set` | 4711.6 | 5199.9 | 5439.4 | 9806.1 | 18.00 | 86.5 | 279 |  |
| `reference` | 4939.5 | 6233.4 | 7470.8 | 8726.2 | 16.41 | 86.0 | 200015 |  |

#### `w05_small_world` (public)

entities 2000 initial / 4000 cap, 400 frames, 2000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `bitset_soa` | 81.4 | 104.0 | 173.8 | 667.7 | 0.17 | 65.6 | 136 | yes |
| `grouped_sparse_set` | 87.1 | 117.1 | 154.1 | 182.2 | 0.25 | 95.0 | 199 | yes |
| `aos` | 92.6 | 106.9 | 132.2 | 156.8 | 0.33 | 85.9 | 23 | yes |
| `fused_archetype` | 94.0 | 108.9 | 119.8 | 143.2 | 0.21 | 77.6 | 457 | yes |
| `archetype` | 94.7 | 114.3 | 156.7 | 186.5 | 0.21 | 77.6 | 457 |  |
| `sparse_set` | 95.1 | 114.2 | 141.3 | 209.8 | 0.25 | 95.0 | 199 |  |
| `soa` | 98.9 | 114.1 | 130.2 | 168.9 | 0.17 | 64.6 | 101 | yes |
| `reference` | 134.9 | 165.6 | 187.2 | 816.0 | 0.23 | 87.1 | 74853 |  |

#### `w06_point_narrow` (public)

entities 200000 initial / 200000 cap, 150 frames, 30000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `aos` | 988.6 | 1044.0 | 1157.3 | 1488.9 | 21.00 | 73.4 | 19 | yes |
| `bitset_soa` | 995.9 | 1035.6 | 1073.4 | 2600.6 | 11.08 | 55.9 | 198 | yes |
| `soa` | 1079.6 | 1230.4 | 1538.6 | 1824.3 | 11.00 | 55.0 | 133 | yes |
| `archetype` | 1339.1 | 1451.9 | 1509.5 | 1760.5 | 14.50 | 73.4 | 115 |  |
| `fused_archetype` | 1425.5 | 1694.4 | 1960.3 | 2074.5 | 14.50 | 73.4 | 115 |  |
| `grouped_sparse_set` | 1437.3 | 1609.7 | 1845.6 | 2358.5 | 23.00 | 112.7 | 285 |  |
| `sparse_set` | 1517.6 | 1999.0 | 2633.0 | 3945.2 | 23.00 | 112.7 | 285 |  |
| `reference` | 2024.5 | 2527.6 | 2872.7 | 3215.0 | 16.41 | 86.0 | 200015 |  |

#### `w07_point_wide` (public)

entities 200000 initial / 200000 cap, 150 frames, 30000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `aos` | 826.1 | 910.7 | 994.3 | 1049.9 | 21.00 | 73.4 | 19 | yes |
| `bitset_soa` | 831.7 | 941.1 | 1076.3 | 1172.2 | 11.08 | 55.9 | 198 | yes |
| `soa` | 899.6 | 1112.3 | 1346.8 | 1482.2 | 11.00 | 55.0 | 133 | yes |
| `fused_archetype` | 1090.3 | 1657.6 | 2007.7 | 2448.0 | 14.50 | 73.4 | 115 |  |
| `archetype` | 1144.4 | 1506.7 | 2010.9 | 3248.8 | 14.50 | 73.4 | 115 |  |
| `sparse_set` | 1305.2 | 1568.6 | 2212.6 | 3317.9 | 23.00 | 112.7 | 285 |  |
| `grouped_sparse_set` | 1330.4 | 2222.8 | 2485.4 | 2655.3 | 23.00 | 112.7 | 285 |  |
| `reference` | 1664.8 | 1983.5 | 2335.9 | 2593.6 | 16.41 | 86.0 | 200015 |  |

#### `h01_zipf_hotspot` (hidden)

entities 150000 initial / 180000 cap, 250 frames, 4000 ops per frame, access zipf

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `fused_archetype` | 2165.7 | 2787.0 | 3092.6 | 4013.5 | 10.50 | 71.2 | 641 | yes |
| `archetype` | 2204.7 | 2377.4 | 2573.0 | 2698.9 | 10.50 | 71.2 | 641 | yes |
| `bitset_soa` | 2259.9 | 2393.1 | 2502.9 | 2688.6 | 11.08 | 74.7 | 208 | yes |
| `grouped_sparse_set` | 2316.2 | 2473.4 | 2585.9 | 2801.2 | 15.25 | 101.6 | 287 |  |
| `sparse_set` | 2795.4 | 3264.8 | 4174.7 | 5760.8 | 15.25 | 101.6 | 287 |  |
| `aos` | 2929.5 | 3367.7 | 3678.7 | 3788.9 | 21.00 | 98.1 | 29 |  |
| `soa` | 3079.5 | 3354.4 | 3695.2 | 5539.6 | 11.00 | 73.6 | 143 |  |
| `reference` | 4398.5 | 5362.7 | 6782.3 | 8178.6 | 11.62 | 81.2 | 191530 |  |

#### `h02_bursty_spawn` (hidden)

entities 20000 initial / 400000 cap, 300 frames, 500 ops per frame, access recent

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `fused_archetype` | 2231.5 | 3788.3 | 4736.6 | 7274.5 | 21.88 | 70.8 | 699 | yes |
| `bitset_soa` | 2269.4 | 5273.7 | 5693.9 | 12030.6 | 22.16 | 84.5 | 214 |  |
| `grouped_sparse_set` | 2343.9 | 4234.1 | 5355.1 | 8064.1 | 23.00 | 87.2 | 290 |  |
| `archetype` | 2373.8 | 4256.8 | 5261.2 | 7336.2 | 21.88 | 70.8 | 699 |  |
| `aos` | 2960.4 | 4959.3 | 5739.2 | 15491.1 | 42.00 | 111.0 | 24 |  |
| `soa` | 2987.4 | 5187.0 | 5819.2 | 10718.9 | 22.00 | 83.2 | 144 |  |
| `sparse_set` | 3299.3 | 5847.3 | 7222.9 | 10987.3 | 23.00 | 87.2 | 290 |  |
| `reference` | 6039.0 | 9400.0 | 10538.2 | 11724.6 | 20.85 | 82.6 | 313911 | yes |

#### `h03_stale_handles` (hidden)

entities 60000 initial / 90000 cap, 250 frames, 4000 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `fused_archetype` | 1018.9 | 1107.5 | 1251.0 | 2554.5 | 4.21 | 73.0 | 643 | yes |
| `archetype` | 1054.5 | 1599.9 | 1899.6 | 2829.6 | 4.21 | 73.0 | 643 |  |
| `bitset_soa` | 1075.0 | 1229.8 | 1423.8 | 2055.0 | 2.77 | 46.3 | 184 | yes |
| `grouped_sparse_set` | 1081.9 | 1170.6 | 1374.0 | 1562.3 | 4.56 | 76.0 | 261 |  |
| `aos` | 1282.5 | 1413.9 | 1540.8 | 1675.1 | 5.25 | 60.8 | 27 |  |
| `soa` | 1323.5 | 1399.5 | 1694.5 | 2118.4 | 2.75 | 45.6 | 129 | yes |
| `sparse_set` | 1640.1 | 2374.6 | 2619.8 | 3413.5 | 4.56 | 76.0 | 261 |  |
| `reference` | 4833.1 | 6056.2 | 6550.2 | 7386.6 | 4.80 | 83.3 | 202779 |  |

#### `h04_recent_locality` (hidden)

entities 40000 initial / 120000 cap, 300 frames, 5000 ops per frame, access recent

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `fused_archetype` | 829.9 | 958.2 | 1208.2 | 1422.9 | 2.98 | 78.4 | 489 | yes |
| `aos` | 836.9 | 900.7 | 954.3 | 1362.3 | 5.25 | 92.4 | 29 | yes |
| `archetype` | 854.3 | 948.0 | 1094.1 | 1820.0 | 2.98 | 78.4 | 489 | yes |
| `grouped_sparse_set` | 867.0 | 935.1 | 1116.0 | 1288.7 | 4.12 | 99.0 | 257 |  |
| `bitset_soa` | 877.1 | 1098.1 | 1137.5 | 1283.7 | 2.77 | 70.4 | 186 | yes |
| `soa` | 928.7 | 1238.6 | 1936.3 | 2437.2 | 2.75 | 69.4 | 131 | yes |
| `sparse_set` | 1002.6 | 1181.5 | 1633.8 | 2047.6 | 4.12 | 99.0 | 257 |  |
| `reference` | 1089.0 | 1518.9 | 1658.2 | 1750.6 | 3.10 | 80.5 | 320460 |  |

#### `h05_sparse_component` (hidden)

entities 200000 initial / 220000 cap, 200 frames, 500 ops per frame, access uniform

| candidate | frame p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `fused_archetype` | 419.3 | 451.3 | 469.9 | 483.2 | 13.31 | 66.2 | 470 | yes |
| `archetype` | 455.8 | 542.4 | 682.2 | 720.0 | 13.31 | 66.2 | 470 |  |
| `bitset_soa` | 672.9 | 853.7 | 1022.4 | 3188.8 | 11.08 | 55.9 | 208 | yes |
| `grouped_sparse_set` | 679.7 | 1033.6 | 1133.0 | 1396.7 | 15.31 | 74.1 | 279 |  |
| `sparse_set` | 996.7 | 1166.5 | 1862.4 | 2223.9 | 15.31 | 74.1 | 279 |  |
| `soa` | 1184.4 | 1271.8 | 1312.2 | 1319.9 | 11.00 | 55.1 | 143 | yes |
| `aos` | 1751.0 | 2197.3 | 2810.5 | 3821.6 | 21.00 | 73.5 | 29 |  |
| `reference` | 5351.5 | 6255.7 | 7105.3 | 14540.8 | 16.42 | 86.0 | 215035 |  |

#### `hs01_rewind_all_moving` (hidden)

30000 entities, world 1024x1024x256, 200 ticks, 1.0 of them moving per tick at speed 0.5-4.0, query radius 8-16, placement uniform, rewind every 10 ticks by 6

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | rewind | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|---|:--:|
| `delta_grid` | 1220.7 | 1607.7 | 1939.3 | 2037.2 | 9.45 | 325.5 | 18494 | native | yes |
| `cell_rows` | 1319.6 | 1543.3 | 1804.4 | 8079.6 | 8.42 | 275.6 | 19073 | snapshot_rebuild | yes |
| `cell_sorted` | 1320.0 | 1497.5 | 1677.1 | 2624.7 | 8.42 | 275.6 | 19073 | snapshot_rebuild | yes |
| `morton_lbvh` | 1335.7 | 1543.2 | 1742.8 | 1940.6 | 8.69 | 283.5 | 955 | snapshot_rebuild |  |
| `uniform_grid` | 1412.3 | 2019.3 | 2251.4 | 2639.6 | 7.75 | 252.6 | 18953 | snapshot_rebuild | yes |
| `grid_ring_knn` | 1442.7 | 2079.0 | 2291.3 | 2566.2 | 7.75 | 252.6 | 547 | snapshot_rebuild |  |
| `grid_undo_log` | 1714.9 | 3989.8 | 4145.7 | 4620.4 | 11.17 | 352.8 | 21640 | native |  |
| `spatial_hash` | 2116.1 | 3868.2 | 4230.4 | 7481.4 | 9.26 | 304.6 | 19093 | snapshot_rebuild |  |
| `morton_sorted` | 4105.4 | 4631.7 | 5867.3 | 9887.5 | 8.64 | 281.8 | 19192 | snapshot_rebuild |  |
| `axis_sorted` | 4590.1 | 4889.9 | 5245.8 | 6217.0 | 8.02 | 261.8 | 29546 | snapshot_rebuild |  |
| `brute_force` | 7142.9 | 8460.1 | 9512.1 | 12104.0 | 6.32 | 203.3 | 2795 | native | yes |

#### `hs02_rewind_few_moving` (hidden)

30000 entities, world 1024x1024x256, 200 ticks, 0.02 of them moving per tick at speed 0.5-4.0, query radius 8-16, placement uniform, rewind every 10 ticks by 6

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | rewind | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|---|:--:|
| `grid_ring_knn` | 100.2 | 492.3 | 560.1 | 607.3 | 7.12 | 234.9 | 547 | snapshot_rebuild | yes |
| `grid_undo_log` | 121.7 | 192.6 | 209.5 | 292.0 | 2.92 | 77.9 | 20647 | native | yes |
| `uniform_grid` | 143.5 | 524.1 | 640.3 | 963.3 | 7.12 | 234.9 | 18955 | snapshot_rebuild |  |
| `delta_grid` | 157.7 | 250.5 | 297.9 | 768.8 | 3.51 | 122.1 | 18608 | native |  |
| `spatial_hash` | 372.1 | 1983.2 | 2051.8 | 2159.8 | 7.62 | 252.3 | 19075 | snapshot_rebuild |  |
| `cell_rows` | 619.9 | 823.1 | 971.5 | 1287.2 | 7.81 | 258.7 | 19075 | snapshot_rebuild |  |
| `cell_sorted` | 648.5 | 811.7 | 885.6 | 965.1 | 7.81 | 258.7 | 19075 | snapshot_rebuild |  |
| `morton_lbvh` | 689.2 | 875.5 | 1159.2 | 1964.4 | 8.09 | 268.4 | 925 | snapshot_rebuild |  |
| `morton_sorted` | 3422.9 | 3697.1 | 3842.5 | 4062.9 | 8.06 | 264.9 | 19194 | snapshot_rebuild |  |
| `axis_sorted` | 3943.6 | 4202.4 | 4429.7 | 4534.9 | 7.41 | 244.9 | 29526 | snapshot_rebuild |  |
| `brute_force` | 6263.4 | 7672.7 | 13793.4 | 14537.6 | 5.77 | 187.5 | 2795 | native |  |

#### `hs03_knn_heavy` (hidden)

25000 entities, world 1024x1024x256, 150 ticks, 1.0 of them moving per tick at speed 0.5-4.0, query radius 8-16, placement uniform

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `grid_ring_knn` | 2196.9 | 2257.3 | 2281.0 | 2331.5 | 1.22 | 51.0 | 7 | yes |
| `morton_lbvh` | 2969.0 | 3139.4 | 3318.8 | 3471.5 | 1.12 | 46.9 | 25 | yes |
| `cell_rows` | 3832.3 | 4174.2 | 4708.0 | 5338.6 | 1.42 | 59.0 | 201849 |  |
| `cell_sorted` | 5323.7 | 7645.2 | 8318.9 | 8575.5 | 1.42 | 59.0 | 201849 |  |
| `grid_undo_log` | 5642.9 | 6183.0 | 6909.1 | 7177.8 | 1.23 | 51.0 | 201847 |  |
| `uniform_grid` | 5678.2 | 6330.9 | 7147.9 | 8013.6 | 1.23 | 51.0 | 201847 |  |
| `delta_grid` | 5721.6 | 6575.5 | 8761.4 | 10593.3 | 2.47 | 103.1 | 201859 |  |
| `axis_sorted` | 15207.3 | 19276.7 | 20389.2 | 20470.1 | 0.98 | 37.0 | 249767 | yes |
| `brute_force` | 16946.2 | 19679.3 | 22946.8 | 24275.6 | 0.93 | 26.0 | 19376 | yes |
| `spatial_hash` | 21666.0 | 25596.4 | 27963.4 | 31369.8 | 6.69 | 196.8 | 201855 |  |
| `morton_sorted` | 32454.3 | 36230.1 | 38857.3 | 42018.0 | 1.28 | 53.0 | 201847 |  |

#### `hs04_flat_world` (hidden)

25000 entities, world 1024x1024x8, 150 ticks, 1.0 of them moving per tick at speed 0.5-4.0, query radius 8-16, placement uniform

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `cell_rows` | 647.8 | 922.3 | 958.5 | 964.6 | 0.82 | 34.2 | 16175 | yes |
| `cell_sorted` | 670.3 | 962.7 | 1004.5 | 1049.5 | 0.82 | 34.2 | 16175 |  |
| `grid_undo_log` | 783.4 | 942.0 | 1071.9 | 1460.4 | 0.63 | 26.2 | 16173 | yes |
| `uniform_grid` | 790.5 | 824.8 | 833.1 | 834.4 | 0.63 | 26.2 | 16173 | yes |
| `delta_grid` | 794.6 | 1046.9 | 1085.6 | 1176.9 | 1.27 | 53.4 | 16185 |  |
| `grid_ring_knn` | 816.0 | 886.2 | 946.7 | 1151.1 | 0.62 | 26.2 | 7 | yes |
| `spatial_hash` | 927.7 | 1297.6 | 1589.5 | 1625.9 | 1.07 | 39.5 | 16177 |  |
| `morton_lbvh` | 1156.0 | 1580.4 | 2072.9 | 2327.3 | 1.12 | 46.9 | 22 |  |
| `morton_sorted` | 2999.6 | 3999.3 | 4057.2 | 4691.3 | 1.27 | 53.0 | 16173 |  |
| `axis_sorted` | 3520.1 | 3702.7 | 4139.0 | 4354.4 | 0.91 | 37.0 | 26105 |  |
| `brute_force` | 7296.1 | 10316.0 | 11185.4 | 12016.4 | 0.93 | 26.0 | 2688 |  |

#### `hs05_spawn_churn` (hidden)

20000 entities, world 1024x1024x256, 150 ticks, 1.0 of them moving per tick at speed 0.5-4.0, query radius 8-16, placement uniform

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `grid_ring_knn` | 745.0 | 1039.4 | 1177.7 | 2723.8 | 2.52 | 132.1 | 7 | yes |
| `grid_undo_log` | 764.9 | 824.9 | 878.5 | 980.9 | 2.52 | 132.0 | 18159 | yes |
| `uniform_grid` | 786.4 | 828.0 | 869.7 | 1034.5 | 2.52 | 132.0 | 18159 | yes |
| `morton_lbvh` | 901.3 | 1078.3 | 1391.5 | 1455.3 | 1.63 | 85.6 | 21 | yes |
| `cell_rows` | 921.4 | 1092.1 | 1299.0 | 1350.5 | 2.22 | 116.2 | 18161 | yes |
| `cell_sorted` | 954.5 | 1047.6 | 1119.3 | 1185.6 | 2.22 | 116.2 | 18161 | yes |
| `delta_grid` | 1148.1 | 1253.5 | 1332.8 | 1452.8 | 4.21 | 220.4 | 18171 |  |
| `spatial_hash` | 1386.4 | 1562.5 | 2116.2 | 3092.0 | 8.20 | 325.1 | 18167 |  |
| `morton_sorted` | 3029.9 | 3907.8 | 7016.7 | 7885.9 | 1.75 | 91.7 | 18159 |  |
| `axis_sorted` | 3129.7 | 3429.5 | 3612.3 | 3681.0 | 1.49 | 75.7 | 29175 | yes |
| `brute_force` | 30673.1 | 37605.9 | 43285.7 | 44347.7 | 2.96 | 103.5 | 2688 |  |

#### `hs06_mixed_reach` (hidden)

20000 entities, world 1024x1024x256, 200 ticks, 1.0 of them moving per tick at speed 0.5-4.0, query radius 2-128, placement uniform

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `cell_rows` | 865.7 | 922.1 | 1099.6 | 1321.8 | 0.65 | 33.2 | 65229 | yes |
| `cell_sorted` | 886.2 | 950.9 | 971.2 | 1145.9 | 0.65 | 33.2 | 65229 | yes |
| `delta_grid` | 996.3 | 1081.0 | 1197.6 | 1541.6 | 0.99 | 51.4 | 65239 |  |
| `grid_ring_knn` | 1093.5 | 1291.3 | 1848.8 | 2522.4 | 0.48 | 25.2 | 7 | yes |
| `grid_undo_log` | 1199.7 | 1349.6 | 1596.7 | 1736.3 | 0.49 | 25.2 | 65227 | yes |
| `morton_lbvh` | 1203.2 | 1275.7 | 1327.4 | 1467.4 | 0.90 | 46.9 | 22 |  |
| `uniform_grid` | 1256.2 | 1486.3 | 1671.9 | 2345.0 | 0.49 | 25.2 | 65227 |  |
| `spatial_hash` | 1294.6 | 1487.1 | 1787.3 | 2338.0 | 0.60 | 30.6 | 65228 |  |
| `morton_sorted` | 3386.6 | 4366.7 | 4556.7 | 9877.8 | 1.02 | 53.0 | 65227 |  |
| `axis_sorted` | 3938.9 | 4111.2 | 4365.4 | 5302.1 | 0.75 | 37.0 | 77944 |  |
| `brute_force` | 7257.2 | 9331.5 | 11760.0 | 11962.9 | 0.74 | 26.0 | 6772 |  |

#### `hs07_crowd` (hidden)

40000 entities, world 1024x1024x256, 150 ticks, 1.0 of them moving per tick at speed 0.2-1.5, query radius 4-8, placement clustered

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| `morton_lbvh` | 3001.7 | 4477.0 | 4961.0 | 5208.7 | 1.79 | 46.8 | 22 | yes |
| `cell_sorted` | 3349.5 | 5430.0 | 5832.5 | 5953.1 | 6.15 | 158.7 | 31193 |  |
| `delta_grid` | 3612.2 | 5557.1 | 6296.9 | 6548.1 | 11.63 | 302.5 | 31203 |  |
| `cell_rows` | 3718.1 | 5251.7 | 5561.4 | 5762.5 | 6.15 | 158.7 | 31193 |  |
| `grid_ring_knn` | 4804.4 | 7937.6 | 9586.2 | 10451.6 | 5.75 | 150.7 | 7 |  |
| `grid_undo_log` | 5061.0 | 8238.9 | 8966.7 | 12920.2 | 5.84 | 150.7 | 31191 |  |
| `uniform_grid` | 5211.0 | 8309.3 | 8876.1 | 9763.1 | 5.84 | 150.7 | 31191 |  |
| `spatial_hash` | 6349.6 | 9314.3 | 11448.7 | 12720.3 | 2.61 | 55.2 | 31197 |  |
| `morton_sorted` | 8052.8 | 11348.3 | 11755.7 | 12939.6 | 2.12 | 53.0 | 31191 |  |
| `axis_sorted` | 8695.5 | 11569.3 | 13285.6 | 15807.6 | 1.60 | 37.0 | 30865 | yes |
| `brute_force` | 13796.8 | 18548.5 | 20249.0 | 20766.9 | 1.49 | 26.0 | 2688 | yes |

#### `hs08_crowd_rollback` (hidden)

40000 entities, world 1024x1024x256, 200 ticks, 1.0 of them moving per tick at speed 0.2-1.5, query radius 4-8, placement clustered, rewind every 8 ticks by 12

| candidate | tick p50 (us) | p95 | p99 | max | peak (MB) | bytes/entity | allocs | rewind | Pareto |
|---|---:|---:|---:|---:|---:|---:|---:|---|:--:|
| `morton_lbvh` | 2560.0 | 4402.6 | 4757.6 | 4995.0 | 13.99 | 288.2 | 767 | snapshot_rebuild | yes |
| `delta_grid` | 2636.5 | 4632.6 | 4827.6 | 5151.9 | 23.10 | 596.2 | 17915 | native |  |
| `cell_sorted` | 3065.2 | 5055.8 | 6133.8 | 16460.2 | 17.60 | 383.9 | 18402 | snapshot_rebuild |  |
| `cell_rows` | 3134.9 | 5732.3 | 10465.4 | 21926.6 | 17.60 | 383.9 | 18402 | snapshot_rebuild |  |
| `uniform_grid` | 3583.4 | 7018.2 | 7270.7 | 8335.5 | 16.72 | 361.0 | 18324 | snapshot_rebuild |  |
| `grid_ring_knn` | 3783.5 | 6959.5 | 7736.9 | 10477.5 | 16.72 | 361.0 | 714 | snapshot_rebuild |  |
| `grid_undo_log` | 3927.5 | 13481.8 | 15392.7 | 20028.5 | 29.94 | 600.2 | 21253 | native |  |
| `spatial_hash` | 4214.4 | 9214.7 | 18119.7 | 104708.7 | 12.23 | 244.0 | 18357 | snapshot_rebuild | yes |
| `morton_sorted` | 7169.1 | 10649.6 | 16573.8 | 59843.2 | 13.90 | 286.5 | 18549 | snapshot_rebuild | yes |
| `axis_sorted` | 7695.6 | 11345.0 | 12258.1 | 14938.9 | 13.09 | 266.5 | 24419 | snapshot_rebuild | yes |
| `brute_force` | 10944.1 | 13903.9 | 16917.2 | 17971.4 | 10.83 | 207.7 | 2796 | native | yes |

## Pareto fronts

Objectives, all minimised: step_ns_p50, step_ns_p99, peak_bytes.

| track | workload | non-dominated |
|---|---|---|
| spatial | `s01_steady_uniform` | `axis_sorted`, `brute_force`, `cell_rows`, `grid_ring_knn`, `morton_lbvh` |
| spatial | `s02_dense_clustered` | `morton_lbvh`, `spatial_hash` |
| spatial | `s03_wide_radius` | `cell_rows`, `grid_ring_knn` |
| spatial | `s04_teleport` | `axis_sorted`, `brute_force`, `cell_rows`, `cell_sorted`, `morton_lbvh`, `uniform_grid` |
| spatial | `s05_small_world` | `axis_sorted`, `brute_force`, `grid_ring_knn`, `morton_lbvh` |
| ecs | `w01_steady_uniform` | `archetype`, `bitset_soa`, `fused_archetype`, `soa` |
| ecs | `w02_query_heavy` | `archetype`, `bitset_soa`, `fused_archetype`, `soa` |
| ecs | `w03_structural_churn` | `bitset_soa`, `grouped_sparse_set`, `soa` |
| ecs | `w04_random_access` | `aos`, `bitset_soa`, `soa` |
| ecs | `w05_small_world` | `aos`, `bitset_soa`, `fused_archetype`, `grouped_sparse_set`, `soa` |
| ecs | `w06_point_narrow` | `aos`, `bitset_soa`, `soa` |
| ecs | `w07_point_wide` | `aos`, `bitset_soa`, `soa` |
| ecs | `h01_zipf_hotspot` | `archetype`, `bitset_soa`, `fused_archetype` |
| ecs | `h02_bursty_spawn` | `fused_archetype`, `reference` |
| ecs | `h03_stale_handles` | `bitset_soa`, `fused_archetype`, `soa` |
| ecs | `h04_recent_locality` | `aos`, `archetype`, `bitset_soa`, `fused_archetype`, `soa` |
| ecs | `h05_sparse_component` | `bitset_soa`, `fused_archetype`, `soa` |
| spatial | `hs01_rewind_all_moving` | `brute_force`, `cell_rows`, `cell_sorted`, `delta_grid`, `uniform_grid` |
| spatial | `hs02_rewind_few_moving` | `grid_ring_knn`, `grid_undo_log` |
| spatial | `hs03_knn_heavy` | `axis_sorted`, `brute_force`, `grid_ring_knn`, `morton_lbvh` |
| spatial | `hs04_flat_world` | `cell_rows`, `grid_ring_knn`, `grid_undo_log`, `uniform_grid` |
| spatial | `hs05_spawn_churn` | `axis_sorted`, `cell_rows`, `cell_sorted`, `grid_ring_knn`, `grid_undo_log`, `morton_lbvh`, `uniform_grid` |
| spatial | `hs06_mixed_reach` | `cell_rows`, `cell_sorted`, `grid_ring_knn`, `grid_undo_log` |
| spatial | `hs07_crowd` | `axis_sorted`, `brute_force`, `morton_lbvh` |
| spatial | `hs08_crowd_rollback` | `axis_sorted`, `brute_force`, `morton_lbvh`, `morton_sorted`, `spatial_hash` |

## Public versus held-out standing

Mean rank by p99 step time, 0 is best. A positive gap means the candidate ranks worse on workloads it was not designed against.

Ranks are computed within a track: the two tracks ask different questions of different structures and a rank across both would mean nothing.

| track | candidate | public | hidden | gap |
|---|---|---:|---:|---:|
| ecs | `grouped_sparse_set` | 3.71 | 2.0 | -1.71 |
| ecs | `archetype` | 2.71 | 1.8 | -0.91 |
| ecs | `fused_archetype` | 1.86 | 1.4 | -0.46 |
| ecs | `bitset_soa` | 1.71 | 2.0 | +0.29 |
| ecs | `reference` | 6.43 | 6.8 | +0.37 |
| ecs | `sparse_set` | 5.0 | 5.6 | +0.60 |
| ecs | `soa` | 4.14 | 5.0 | +0.86 |
| ecs | `aos` | 2.43 | 3.4 | +0.97 |
| spatial | `grid_undo_log` | 5.2 | 3.75 | -1.45 |
| spatial | `delta_grid` | 4.4 | 3.25 | -1.15 |
| spatial | `uniform_grid` | 3.6 | 2.88 | -0.73 |
| spatial | `axis_sorted` | 8.2 | 8.0 | -0.20 |
| spatial | `morton_lbvh` | 3.2 | 3.0 | -0.20 |
| spatial | `brute_force` | 9.8 | 9.62 | -0.18 |
| spatial | `morton_sorted` | 8.8 | 8.62 | -0.18 |
| spatial | `cell_sorted` | 2.0 | 2.25 | +0.25 |
| spatial | `spatial_hash` | 6.4 | 7.38 | +0.97 |
| spatial | `grid_ring_knn` | 2.2 | 3.5 | +1.30 |
| spatial | `cell_rows` | 1.2 | 2.75 | +1.55 |

## Notes

- broken_recycle is a negative control; it is excluded from measurement.
- query_memo is a negative control; it is excluded from measurement.

