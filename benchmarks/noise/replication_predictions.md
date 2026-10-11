# Predictions

Each candidate's falsifiable predictions, as written in its manifest before it was measured, judged against the measurements by `runner/predictions.py`. A verdict is computed, not written: HELD means every point the prediction names satisfied its bound, FALSIFIED means at least one did not, UNTESTED means a point it needs was not measured.

*within noise* marks a point closer to its bound than the spread between repetitions of the measurements it came from; a verdict resting on such a point is weak whichever way it went. *poor fit* marks an exponent whose power-law fit has r2 below 0.9.

- main suite: run `20261011T183114Z`
- sweeps: run `None`

**14 held, 16 falsified, 23 untested.**

*95% interval*: each point's quantity recomputed on 2000 bootstrap resamples of the repetitions of every measurement it uses. A verdict is *resolved* when the intervals carry it (every point of a HELD prediction inside its bound, one point of a FALSIFIED one outside), *unresolved* when the repetitions measured do not settle it, and *no interval* when its points have no repetitions to resample (sweeps, memory). The verdict itself is still the preregistered rule applied to the medians.

falsified resolved: 5; falsified unresolved: 11; held no interval: 3; held resolved: 10; held unresolved: 1.

| candidate | prediction | verdict | weak | 95% interval |
|---|---|---|---|---|
| `aos` | P1: on point access to whole entities (w07_point_wide) within noise of the layouts that split components apart | **UNTESTED** |  |  |
| `archetype` | P2: beaten by sparse_set on w03_structural_churn | **FALSIFIED** | within noise | unresolved |
| `axis_sorted` | P1: better than scanning everything, on every spatial workload | **UNTESTED** |  |  |
| `bitset_soa` | P1: h05_sparse_component at most 0.6x soa's | **FALSIFIED** | within noise | unresolved |
| `bitset_soa` | P2: dense-mask workloads within 10% of soa either way | **FALSIFIED** | within noise | unresolved |
| `bitset_soa` | P3: peak memory within 5% above soa's on every ECS workload | **HELD** |  | no interval |
| `cell_rows` | P1: no slower than cell_sorted by more than 3% on any spatial workload | **FALSIFIED** | within noise | resolved |
| `cell_rows` | P2: hs06_mixed_reach at most 0.9x cell_sorted's | **FALSIFIED** | within noise | unresolved |
| `cell_rows` | P3: hs03_knn_heavy at most 0.9x cell_sorted's | **HELD** | within noise | resolved |
| `cell_sorted` | P1: beats morton_sorted on all ten spatial workloads | **UNTESTED** |  |  |
| `cell_sorted` | P2: beats uniform_grid where every entity moves every tick | **FALSIFIED** | within noise | unresolved |
| `cell_sorted` | P3: loses to uniform_grid on hs02_rewind_few_moving by at least 1.5x | **HELD** |  | resolved |
| `cell_sorted` | P4: spatial_move time exponent at least 0.8 in both regimes | **UNTESTED** |  |  |
| `cell_sorted` | P5: s04_teleport within 5% of its own s01_steady_uniform | **FALSIFIED** | within noise | unresolved |
| `delta_grid` | P1a: hs01_rewind_all_moving median below uniform_grid and grid_undo_log | **HELD** | within noise | resolved |
| `delta_grid` | P1b: hs01_rewind_all_moving p99 below uniform_grid and grid_undo_log | **HELD** | within noise | resolved |
| `delta_grid` | P2a: hs02_rewind_few_moving median within 1.3x of grid_undo_log | **FALSIFIED** | within noise | resolved |
| `delta_grid` | P2b: hs02_rewind_few_moving p99 within 1.5x of grid_undo_log | **FALSIFIED** | within noise | unresolved |
| `delta_grid` | P2c: hs02_rewind_few_moving p99 below uniform_grid under snapshot-and-rebuild | **HELD** | within noise | resolved |
| `delta_grid` | P3: within 1.2x of cell_sorted where everything moves and nothing rewinds | **FALSIFIED** | within noise | resolved |
| `delta_grid` | P4: spatial_move time exponent at most 0.3 in both regimes | **UNTESTED** |  |  |
| `delta_grid` | P5: rewind_move_fraction p99 within 1.2x of the better history strategy at every value | **UNTESTED** |  |  |
| `fused_archetype` | P1: w02_query_heavy at most 0.95x archetype's | **HELD** | within noise | unresolved |
| `fused_archetype` | P2: w01_steady_uniform at most 0.95x archetype's | **FALSIFIED** | within noise | unresolved |
| `fused_archetype` | P3: no ECS workload slower than archetype by more than 5% | **FALSIFIED** | within noise | unresolved |
| `grid_ring_knn` | P1: hs03_knn_heavy at most 0.5x uniform_grid's | **HELD** | within noise | resolved |
| `grid_ring_knn` | P2: s05_small_world at most 0.7x uniform_grid's | **HELD** | within noise | resolved |
| `grid_ring_knn` | P3: on no spatial workload slower than uniform_grid by more than 10% | **FALSIFIED** | within noise | unresolved |
| `grid_ring_knn` | P4a: spatial_knn fixed_world at population 1000 at least 3x faster than uniform_grid | **UNTESTED** |  |  |
| `grid_ring_knn` | P4b: spatial_knn fixed_density time exponent no higher than uniform_grid's | **UNTESTED** |  |  |
| `grid_undo_log` | P1: hs02_rewind_few_moving tail latency below uniform_grid under snapshot-and-rebuild | **HELD** | within noise | resolved |
| `grid_undo_log` | P2: hs02_rewind_few_moving memory below uniform_grid under snapshot-and-rebuild | **HELD** |  | no interval |
| `grid_undo_log` | P3: hs01_rewind_all_moving tail latency above uniform_grid's (the log should lose) | **HELD** | within noise | resolved |
| `grouped_sparse_set` | P1: h05_sparse_component closes at least half the gap from sparse_set to archetype | **HELD** | within noise | resolved |
| `grouped_sparse_set` | P2: w01 and w02 at least 15% faster than sparse_set | **FALSIFIED** | within noise | unresolved |
| `grouped_sparse_set` | P3: w03_structural_churn within 15% of sparse_set either way | **FALSIFIED** | within noise | resolved |
| `morton_lbvh` | P1a: s02_dense_clustered median tick lower than uniform_grid's | **UNTESTED** |  |  |
| `morton_lbvh` | P1b: s02_dense_clustered peak memory at most half of uniform_grid's | **UNTESTED** |  |  |
| `morton_lbvh` | P2: hs03_knn_heavy median tick lower than uniform_grid's | **UNTESTED** |  |  |
| `morton_lbvh` | P3: s01_steady_uniform within 1.5x of uniform_grid | **UNTESTED** |  |  |
| `morton_lbvh` | P4: beats morton_sorted on all ten spatial workloads | **UNTESTED** |  |  |
| `morton_lbvh` | P5a: spatial_move time exponent at least 0.8 | **UNTESTED** |  |  |
| `morton_lbvh` | P5b: spatial_query_radius fixed_density time exponent at most 0.25 | **UNTESTED** |  |  |
| `morton_lbvh` | P6a: spatial_move fixed_world memory exponent at least 0.9 | **UNTESTED** |  |  |
| `morton_lbvh` | P6b: spatial_query_radius fixed_world memory exponent at least 0.9 | **UNTESTED** |  |  |
| `morton_lbvh` | P6c: spatial_knn fixed_world memory exponent at least 0.9 | **UNTESTED** |  |  |
| `soa` | P1: narrow queries faster than aos (w02_query_heavy) | **UNTESTED** |  |  |
| `soa` | P2: multi-component point access slower than aos (w07_point_wide) | **UNTESTED** |  |  |
| `soa` | P3: smaller footprint than aos on every ECS workload | **UNTESTED** |  |  |
| `sparse_set` | P1: on h05_sparse_component beats every layout that scans the whole index space | **UNTESTED** |  |  |
| `spatial_hash` | P1: slower than uniform_grid on every spatial workload | **FALSIFIED** | within noise | resolved |
| `spatial_hash` | P2: less memory than uniform_grid on s02_dense_clustered | **HELD** |  | no interval |
| `uniform_grid` | P1: the fastest candidate on an evenly spread world (s01_steady_uniform) | **UNTESTED** |  |  |

## `aos`

**P1 — UNTESTED.** on point access to whole entities (w07_point_wide) within noise of the layouts that split components apart

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| w07_point_wide | | | | | untested | aos not measured here |

## `archetype`

**P2 — FALSIFIED.** beaten by sparse_set on w03_structural_churn

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| w03_structural_churn | 0.9199 | 0.8151–1.1196 | / sparse_set | above 1.0 | falsified | within noise (band 87.5%), interval crosses the bound |

## `axis_sorted`

**P1 — UNTESTED.** better than scanning everything, on every spatial workload

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|

## `bitset_soa`

**P1 — FALSIFIED.** h05_sparse_component at most 0.6x soa's

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| h05_sparse_component | 0.6787 | 0.5829–0.8238 | / soa | at most 0.6 | falsified | within noise (band 112.5%), interval crosses the bound |

**P2 — FALSIFIED.** dense-mask workloads within 10% of soa either way

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| w01_steady_uniform | 1.0819 | 0.9558–1.2245 | / soa | within 0.1 | held | within noise (band 91.1%), interval crosses the bound |
| w02_query_heavy | 1.1569 | 0.94–1.2458 | / soa | within 0.1 | falsified | within noise (band 102.6%), interval crosses the bound |
| w04_random_access | 0.9608 | 0.8204–1.1774 | / soa | within 0.1 | held | within noise (band 84.3%), interval crosses the bound |

**P3 — HELD.** peak memory within 5% above soa's on every ECS workload

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| w01_steady_uniform | 1.0071 |  | / soa | at most 1.05 | held |  |
| w02_query_heavy | 1.0071 |  | / soa | at most 1.05 | held |  |
| w03_structural_churn | 1.0071 |  | / soa | at most 1.05 | held |  |
| w04_random_access | 1.0071 |  | / soa | at most 1.05 | held |  |
| w05_small_world | 1.0071 |  | / soa | at most 1.05 | held |  |
| h01_zipf_hotspot | 1.0071 |  | / soa | at most 1.05 | held |  |
| h02_bursty_spawn | 1.0071 |  | / soa | at most 1.05 | held |  |
| h03_stale_handles | 1.0071 |  | / soa | at most 1.05 | held |  |
| h04_recent_locality | 1.0071 |  | / soa | at most 1.05 | held |  |
| h05_sparse_component | 1.0071 |  | / soa | at most 1.05 | held |  |

## `cell_rows`

**P1 — FALSIFIED.** no slower than cell_sorted by more than 3% on any spatial workload

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| hs01_rewind_all_moving | 1.1323 | 1.0439–1.4591 | / cell_sorted | at most 1.03 | falsified | within noise (band 68.2%) |
| hs02_rewind_few_moving | 1.1508 | 0.9383–1.3009 | / cell_sorted | at most 1.03 | falsified | within noise (band 84.2%), interval crosses the bound |
| hs03_knn_heavy | 0.7254 | 0.6378–0.8042 | / cell_sorted | at most 1.03 | held | within noise (band 71.7%) |
| hs04_flat_world | 1.337 | 1.0939–1.4789 | / cell_sorted | at most 1.03 | falsified | within noise (band 81.1%) |
| hs05_spawn_churn | 1.3908 | 1.0511–1.4788 | / cell_sorted | at most 1.03 | falsified | within noise (band 92.7%) |
| hs06_mixed_reach | 0.9581 | 0.7636–1.1983 | / cell_sorted | at most 1.03 | held | within noise (band 123.1%), interval crosses the bound |
| hs07_crowd | 1.0416 | 0.93–1.112 | / cell_sorted | at most 1.03 | falsified | within noise (band 89.7%), interval crosses the bound |
| hs08_crowd_rollback | 1.005 | 0.9349–1.1159 | / cell_sorted | at most 1.03 | held | within noise (band 80.5%), interval crosses the bound |
| s01_steady_uniform | 0.7664 | 0.6562–0.915 | / cell_sorted | at most 1.03 | held | within noise (band 132.4%) |
| s02_dense_clustered | 0.9473 | 0.8531–1.0073 | / cell_sorted | at most 1.03 | held | within noise (band 71.9%) |
| s03_wide_radius | 1.0982 | 0.8398–1.2582 | / cell_sorted | at most 1.03 | falsified | within noise (band 95.4%), interval crosses the bound |
| s04_teleport | 0.7493 | 0.7064–0.9256 | / cell_sorted | at most 1.03 | held | within noise (band 96.9%) |
| s05_small_world | 0.8723 | 0.7463–0.9856 | / cell_sorted | at most 1.03 | held | within noise (band 81.4%) |

**P2 — FALSIFIED.** hs06_mixed_reach at most 0.9x cell_sorted's

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| hs06_mixed_reach | 0.9581 | 0.7807–1.1962 | / cell_sorted | at most 0.9 | falsified | within noise (band 123.1%), interval crosses the bound |

**P3 — HELD.** hs03_knn_heavy at most 0.9x cell_sorted's

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| hs03_knn_heavy | 0.7254 | 0.6384–0.7928 | / cell_sorted | at most 0.9 | held | within noise (band 71.7%) |

## `cell_sorted`

**P1 — UNTESTED.** beats morton_sorted on all ten spatial workloads

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| s01_steady_uniform | | | | | untested | morton_sorted not measured here |
| s02_dense_clustered | | | | | untested | morton_sorted not measured here |
| s03_wide_radius | | | | | untested | morton_sorted not measured here |
| s04_teleport | | | | | untested | morton_sorted not measured here |
| s05_small_world | | | | | untested | morton_sorted not measured here |
| hs01_rewind_all_moving | | | | | untested | morton_sorted not measured here |
| hs02_rewind_few_moving | | | | | untested | morton_sorted not measured here |
| hs03_knn_heavy | | | | | untested | morton_sorted not measured here |
| hs04_flat_world | | | | | untested | morton_sorted not measured here |
| hs05_spawn_churn | | | | | untested | morton_sorted not measured here |

**P2 — FALSIFIED.** beats uniform_grid where every entity moves every tick

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| s01_steady_uniform | 1.0083 | 0.9729–1.1055 | / uniform_grid | below 1.0 | falsified | within noise (band 79.7%), interval crosses the bound |
| s03_wide_radius | 0.7365 | 0.6028–0.8453 | / uniform_grid | below 1.0 | held | within noise (band 128.1%) |
| s04_teleport | 0.8325 | 0.7049–0.9106 | / uniform_grid | below 1.0 | held | within noise (band 74.7%) |
| hs04_flat_world | 0.616 | 0.5622–0.7176 | / uniform_grid | below 1.0 | held | within noise (band 83.3%) |

**P3 — HELD.** loses to uniform_grid on hs02_rewind_few_moving by at least 1.5x

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| hs02_rewind_few_moving | 4.7165 | 4.1511–5.3513 | / uniform_grid | at least 1.5 | held |  |

**P4 — UNTESTED.** spatial_move time exponent at least 0.8 in both regimes

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| spatial_move·fixed_world | | | | | untested | cell_sorted not measured here |
| spatial_move·fixed_density | | | | | untested | cell_sorted not measured here |

**P5 — FALSIFIED.** s04_teleport within 5% of its own s01_steady_uniform

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| s04_teleport | 0.9294 | 0.7849–0.9803 | / own s01_steady_uniform | within 0.05 | falsified | within noise (band 91.1%), interval crosses the bound |

## `delta_grid`

**P1a — HELD.** hs01_rewind_all_moving median below uniform_grid and grid_undo_log

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| hs01_rewind_all_moving | 0.6839 | 0.6129–0.9164 | / uniform_grid | below 1.0 | held | within noise (band 139.0%) |
| hs01_rewind_all_moving | 0.5504 | 0.4709–0.7617 | / grid_undo_log | below 1.0 | held | within noise (band 151.2%) |

**P1b — HELD.** hs01_rewind_all_moving p99 below uniform_grid and grid_undo_log

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| hs01_rewind_all_moving | 0.6968 | 0.589–0.823 | / uniform_grid | below 1.0 | held | within noise (band 173.9%) |
| hs01_rewind_all_moving | 0.4032 | 0.3473–0.4812 | / grid_undo_log | below 1.0 | held | within noise (band 164.3%) |

**P2a — FALSIFIED.** hs02_rewind_few_moving median within 1.3x of grid_undo_log

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| hs02_rewind_few_moving | 1.817 | 1.5133–2.1509 | / grid_undo_log | at most 1.3 | falsified | within noise (band 122.1%) |

**P2b — FALSIFIED.** hs02_rewind_few_moving p99 within 1.5x of grid_undo_log

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| hs02_rewind_few_moving | 1.6195 | 1.3215–2.0366 | / grid_undo_log | at most 1.5 | falsified | within noise (band 196.2%), interval crosses the bound |

**P2c — HELD.** hs02_rewind_few_moving p99 below uniform_grid under snapshot-and-rebuild

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| hs02_rewind_few_moving | 0.7568 | 0.6534–0.9696 | / uniform_grid | below 1.0 | held | within noise (band 181.5%) |

**P3 — FALSIFIED.** within 1.2x of cell_sorted where everything moves and nothing rewinds

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| s01_steady_uniform | 0.8616 | 0.8098–0.9001 | / cell_sorted | at most 1.2 | held | within noise (band 100.6%) |
| s03_wide_radius | 1.5348 | 1.3278–1.6942 | / cell_sorted | at most 1.2 | falsified | within noise (band 105.9%) |
| s04_teleport | 0.9624 | 0.8739–1.2174 | / cell_sorted | at most 1.2 | held | within noise (band 96.5%), interval crosses the bound |
| hs04_flat_world | 1.1679 | 1.1253–1.2469 | / cell_sorted | at most 1.2 | held | within noise (band 100.8%), interval crosses the bound |

**P4 — UNTESTED.** spatial_move time exponent at most 0.3 in both regimes

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| spatial_move·fixed_world | | | | | untested | delta_grid not measured here |
| spatial_move·fixed_density | | | | | untested | delta_grid not measured here |

**P5 — UNTESTED.** rewind_move_fraction p99 within 1.2x of the better history strategy at every value

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|

## `fused_archetype`

**P1 — HELD.** w02_query_heavy at most 0.95x archetype's

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| w02_query_heavy | 0.8999 | 0.8176–1.0138 | / archetype | at most 0.95 | held | within noise (band 92.2%), interval crosses the bound |

**P2 — FALSIFIED.** w01_steady_uniform at most 0.95x archetype's

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| w01_steady_uniform | 1.0752 | 0.9055–1.2447 | / archetype | at most 0.95 | falsified | within noise (band 98.7%), interval crosses the bound |

**P3 — FALSIFIED.** no ECS workload slower than archetype by more than 5%

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| w01_steady_uniform | 1.0752 | 0.9075–1.2446 | / archetype | at most 1.05 | falsified | within noise (band 98.7%), interval crosses the bound |
| w02_query_heavy | 0.8999 | 0.8203–1.0128 | / archetype | at most 1.05 | held | within noise (band 92.2%) |
| w03_structural_churn | 1.0208 | 0.8152–1.1041 | / archetype | at most 1.05 | held | within noise (band 99.6%), interval crosses the bound |
| w04_random_access | 0.9678 | 0.7317–1.0164 | / archetype | at most 1.05 | held | within noise (band 74.5%) |
| w05_small_world | 1.0267 | 1.0083–1.0591 | / archetype | at most 1.05 | held | within noise (band 56.0%), interval crosses the bound |
| h01_zipf_hotspot | 0.7943 | 0.7055–0.9285 | / archetype | at most 1.05 | held | within noise (band 87.2%) |
| h02_bursty_spawn | 0.827 | 0.7414–0.9333 | / archetype | at most 1.05 | held | within noise (band 77.9%) |
| h03_stale_handles | 0.7338 | 0.697–0.9323 | / archetype | at most 1.05 | held | within noise (band 95.7%) |
| h04_recent_locality | 1.0051 | 0.8662–1.2381 | / archetype | at most 1.05 | held | within noise (band 116.2%), interval crosses the bound |
| h05_sparse_component | 0.9166 | 0.6511–1.12 | / archetype | at most 1.05 | held | within noise (band 137.0%), interval crosses the bound |

## `grid_ring_knn`

**P1 — HELD.** hs03_knn_heavy at most 0.5x uniform_grid's

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| hs03_knn_heavy | 0.4062 | 0.3295–0.4418 | / uniform_grid | at most 0.5 | held | within noise (band 73.0%) |

**P2 — HELD.** s05_small_world at most 0.7x uniform_grid's

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| s05_small_world | 0.4912 | 0.4825–0.5329 | / uniform_grid | at most 0.7 | held | within noise (band 40.2%) |

**P3 — FALSIFIED.** on no spatial workload slower than uniform_grid by more than 10%

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| s01_steady_uniform | 0.9022 | 0.8752–1.002 | / uniform_grid | at most 1.1 | held | within noise (band 86.9%) |
| s02_dense_clustered | 0.8036 | 0.7576–0.8719 | / uniform_grid | at most 1.1 | held | within noise (band 86.5%) |
| s03_wide_radius | 1.1336 | 0.937–1.2213 | / uniform_grid | at most 1.1 | falsified | within noise (band 126.2%), interval crosses the bound |
| s04_teleport | 0.7331 | 0.708–0.8315 | / uniform_grid | at most 1.1 | held | within noise (band 69.7%) |
| s05_small_world | 0.4912 | 0.483–0.5325 | / uniform_grid | at most 1.1 | held |  |
| hs01_rewind_all_moving | 0.9546 | 0.8328–1.1816 | / uniform_grid | at most 1.1 | held | within noise (band 143.6%), interval crosses the bound |
| hs02_rewind_few_moving | 0.8577 | 0.7937–0.9873 | / uniform_grid | at most 1.1 | held | within noise (band 95.0%) |
| hs03_knn_heavy | 0.4062 | 0.3302–0.4437 | / uniform_grid | at most 1.1 | held | within noise (band 73.0%) |
| hs04_flat_world | 0.8285 | 0.7548–1.03 | / uniform_grid | at most 1.1 | held | within noise (band 91.1%) |
| hs05_spawn_churn | 0.9413 | 0.8534–1.0878 | / uniform_grid | at most 1.1 | held | within noise (band 117.9%) |

**P4a — UNTESTED.** spatial_knn fixed_world at population 1000 at least 3x faster than uniform_grid

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| spatial_knn·fixed_world·n=1000 | | | | | untested | grid_ring_knn not measured here |

**P4b — UNTESTED.** spatial_knn fixed_density time exponent no higher than uniform_grid's

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| spatial_knn·fixed_density | | | | | untested | grid_ring_knn not measured here |

## `grid_undo_log`

**P1 — HELD.** hs02_rewind_few_moving tail latency below uniform_grid under snapshot-and-rebuild

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| hs02_rewind_few_moving | 0.4673 | 0.4189–0.5304 | / uniform_grid | below 1.0 | held | within noise (band 153.1%) |

**P2 — HELD.** hs02_rewind_few_moving memory below uniform_grid under snapshot-and-rebuild

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| hs02_rewind_few_moving | 0.4096 |  | / uniform_grid | below 1.0 | held |  |

**P3 — HELD.** hs01_rewind_all_moving tail latency above uniform_grid's (the log should lose)

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| hs01_rewind_all_moving | 1.7281 | 1.5298–1.8964 | / uniform_grid | above 1.0 | held | within noise (band 112.3%) |

## `grouped_sparse_set`

**P1 — HELD.** h05_sparse_component closes at least half the gap from sparse_set to archetype

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| h05_sparse_component | 0.8111 | 0.5427–0.9769 | gap sparse_set to archetype closed | at least 0.5 | held | within noise (band 157.1%) |

**P2 — FALSIFIED.** w01 and w02 at least 15% faster than sparse_set

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| w01_steady_uniform | 0.7673 | 0.6007–1.027 | / sparse_set | at most 0.85 | held | within noise (band 121.8%), interval crosses the bound |
| w02_query_heavy | 0.878 | 0.7869–1.0017 | / sparse_set | at most 0.85 | falsified | within noise (band 95.1%), interval crosses the bound |

**P3 — FALSIFIED.** w03_structural_churn within 15% of sparse_set either way

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| w03_structural_churn | 0.5809 | 0.5264–0.7679 | / sparse_set | within 0.15 | falsified | within noise (band 111.8%) |

## `morton_lbvh`

**P1a — UNTESTED.** s02_dense_clustered median tick lower than uniform_grid's

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| s02_dense_clustered | | | | | untested | morton_lbvh not measured here |

**P1b — UNTESTED.** s02_dense_clustered peak memory at most half of uniform_grid's

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| s02_dense_clustered | | | | | untested | morton_lbvh not measured here |

**P2 — UNTESTED.** hs03_knn_heavy median tick lower than uniform_grid's

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| hs03_knn_heavy | | | | | untested | morton_lbvh not measured here |

**P3 — UNTESTED.** s01_steady_uniform within 1.5x of uniform_grid

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| s01_steady_uniform | | | | | untested | morton_lbvh not measured here |

**P4 — UNTESTED.** beats morton_sorted on all ten spatial workloads

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| s01_steady_uniform | | | | | untested | morton_lbvh not measured here |
| s02_dense_clustered | | | | | untested | morton_lbvh not measured here |
| s03_wide_radius | | | | | untested | morton_lbvh not measured here |
| s04_teleport | | | | | untested | morton_lbvh not measured here |
| s05_small_world | | | | | untested | morton_lbvh not measured here |
| hs01_rewind_all_moving | | | | | untested | morton_lbvh not measured here |
| hs02_rewind_few_moving | | | | | untested | morton_lbvh not measured here |
| hs03_knn_heavy | | | | | untested | morton_lbvh not measured here |
| hs04_flat_world | | | | | untested | morton_lbvh not measured here |
| hs05_spawn_churn | | | | | untested | morton_lbvh not measured here |

**P5a — UNTESTED.** spatial_move time exponent at least 0.8

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| spatial_move·fixed_world | | | | | untested | morton_lbvh not measured here |
| spatial_move·fixed_density | | | | | untested | morton_lbvh not measured here |

**P5b — UNTESTED.** spatial_query_radius fixed_density time exponent at most 0.25

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| spatial_query_radius·fixed_density | | | | | untested | morton_lbvh not measured here |

**P6a — UNTESTED.** spatial_move fixed_world memory exponent at least 0.9

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| spatial_move·fixed_world | | | | | untested | morton_lbvh not measured here |

**P6b — UNTESTED.** spatial_query_radius fixed_world memory exponent at least 0.9

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| spatial_query_radius·fixed_world | | | | | untested | morton_lbvh not measured here |

**P6c — UNTESTED.** spatial_knn fixed_world memory exponent at least 0.9

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| spatial_knn·fixed_world | | | | | untested | morton_lbvh not measured here |

## `soa`

**P1 — UNTESTED.** narrow queries faster than aos (w02_query_heavy)

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| w02_query_heavy | | | | | untested | aos not measured here |

**P2 — UNTESTED.** multi-component point access slower than aos (w07_point_wide)

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| w07_point_wide | | | | | untested | aos not measured here |

**P3 — UNTESTED.** smaller footprint than aos on every ECS workload

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| h01_zipf_hotspot | | | | | untested | aos not measured here |
| h02_bursty_spawn | | | | | untested | aos not measured here |
| h03_stale_handles | | | | | untested | aos not measured here |
| h04_recent_locality | | | | | untested | aos not measured here |
| h05_sparse_component | | | | | untested | aos not measured here |
| w01_steady_uniform | | | | | untested | aos not measured here |
| w02_query_heavy | | | | | untested | aos not measured here |
| w03_structural_churn | | | | | untested | aos not measured here |
| w04_random_access | | | | | untested | aos not measured here |
| w05_small_world | | | | | untested | aos not measured here |
| w06_point_narrow | | | | | untested | aos not measured here |
| w07_point_wide | | | | | untested | aos not measured here |

## `sparse_set`

**P1 — UNTESTED.** on h05_sparse_component beats every layout that scans the whole index space

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| h05_sparse_component | | | | | untested | aos not measured here |

## `spatial_hash`

**P1 — FALSIFIED.** slower than uniform_grid on every spatial workload

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| hs01_rewind_all_moving | 1.5601 | 1.3588–1.7822 | / uniform_grid | above 1.0 | held | within noise (band 96.0%) |
| hs02_rewind_few_moving | 2.5938 | 2.3717–3.0731 | / uniform_grid | above 1.0 | held |  |
| hs03_knn_heavy | 4.3235 | 4.1823–4.7498 | / uniform_grid | above 1.0 | held |  |
| hs04_flat_world | 0.9968 | 0.9344–1.2581 | / uniform_grid | above 1.0 | falsified | within noise (band 79.1%), interval crosses the bound |
| hs05_spawn_churn | 1.6676 | 1.5141–2.0942 | / uniform_grid | above 1.0 | held | within noise (band 141.6%) |
| hs06_mixed_reach | 1.0893 | 1.0335–1.1951 | / uniform_grid | above 1.0 | held | within noise (band 80.1%) |
| hs07_crowd | 1.1607 | 1.0454–1.261 | / uniform_grid | above 1.0 | held | within noise (band 71.7%) |
| hs08_crowd_rollback | 1.2329 | 1.0164–1.3791 | / uniform_grid | above 1.0 | held | within noise (band 123.5%) |
| s01_steady_uniform | 2.3399 | 2.0758–2.6959 | / uniform_grid | above 1.0 | held |  |
| s02_dense_clustered | 0.8886 | 0.8029–0.9813 | / uniform_grid | above 1.0 | falsified | within noise (band 82.2%) |
| s03_wide_radius | 0.9646 | 0.7888–1.0799 | / uniform_grid | above 1.0 | falsified | within noise (band 120.4%), interval crosses the bound |
| s04_teleport | 2.7991 | 2.5805–3.1174 | / uniform_grid | above 1.0 | held |  |
| s05_small_world | 2.2137 | 2.1663–2.3182 | / uniform_grid | above 1.0 | held |  |

**P2 — HELD.** less memory than uniform_grid on s02_dense_clustered

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| s02_dense_clustered | 0.2214 |  | / uniform_grid | below 1.0 | held |  |

## `uniform_grid`

**P1 — UNTESTED.** the fastest candidate on an evenly spread world (s01_steady_uniform)

| point | value | 95% interval | compared | bound | result | note |
|---|---:|---|---|---|---|---|
| s01_steady_uniform | | | | | untested | axis_sorted not measured here |
