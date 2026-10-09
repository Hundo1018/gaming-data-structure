# Predictions

Each candidate's falsifiable predictions, as written in its manifest before it was measured, judged against the measurements by `runner/predictions.py`. A verdict is computed, not written: HELD means every point the prediction names satisfied its bound, FALSIFIED means at least one did not, UNTESTED means a point it needs was not measured.

*within noise* marks a point closer to its bound than the spread between repetitions of the measurements it came from; a verdict resting on such a point is weak whichever way it went. *poor fit* marks an exponent whose power-law fit has r2 below 0.9.

- main suite: run `20261009T042136Z`
- sweeps: run `sweep-20261009T044632Z`

**38 held, 15 falsified, 0 untested.**

| candidate | prediction | verdict | weak |
|---|---|---|---|
| `aos` | P1: on point access to whole entities (w07_point_wide) within noise of the layouts that split components apart | **FALSIFIED** |  |
| `archetype` | P2: beaten by sparse_set on w03_structural_churn | **FALSIFIED** |  |
| `axis_sorted` | P1: better than scanning everything, on every spatial workload | **FALSIFIED** |  |
| `bitset_soa` | P1: h05_sparse_component at most 0.6x soa's | **HELD** |  |
| `bitset_soa` | P2: dense-mask workloads within 10% of soa either way | **FALSIFIED** |  |
| `bitset_soa` | P3: peak memory within 5% above soa's on every ECS workload | **HELD** |  |
| `cell_rows` | P1: no slower than cell_sorted by more than 3% on any spatial workload | **HELD** | within noise |
| `cell_rows` | P2: hs06_mixed_reach at most 0.9x cell_sorted's | **FALSIFIED** |  |
| `cell_rows` | P3: hs03_knn_heavy at most 0.9x cell_sorted's | **HELD** |  |
| `cell_sorted` | P1: beats morton_sorted on all ten spatial workloads | **HELD** |  |
| `cell_sorted` | P2: beats uniform_grid where every entity moves every tick | **HELD** |  |
| `cell_sorted` | P3: loses to uniform_grid on hs02_rewind_few_moving by at least 1.5x | **HELD** |  |
| `cell_sorted` | P4: spatial_move time exponent at least 0.8 in both regimes | **FALSIFIED** |  |
| `cell_sorted` | P5: s04_teleport within 5% of its own s01_steady_uniform | **HELD** | within noise |
| `delta_grid` | P1a: hs01_rewind_all_moving median below uniform_grid and grid_undo_log | **HELD** |  |
| `delta_grid` | P1b: hs01_rewind_all_moving p99 below uniform_grid and grid_undo_log | **HELD** | within noise |
| `delta_grid` | P2a: hs02_rewind_few_moving median within 1.3x of grid_undo_log | **FALSIFIED** | within noise |
| `delta_grid` | P2b: hs02_rewind_few_moving p99 within 1.5x of grid_undo_log | **HELD** | within noise |
| `delta_grid` | P2c: hs02_rewind_few_moving p99 below uniform_grid under snapshot-and-rebuild | **HELD** | within noise |
| `delta_grid` | P3: within 1.2x of cell_sorted where everything moves and nothing rewinds | **HELD** | within noise |
| `delta_grid` | P4: spatial_move time exponent at most 0.3 in both regimes | **HELD** |  |
| `delta_grid` | P5: rewind_move_fraction p99 within 1.2x of the better history strategy at every value | **FALSIFIED** |  |
| `fused_archetype` | P1: w02_query_heavy at most 0.95x archetype's | **FALSIFIED** |  |
| `fused_archetype` | P2: w01_steady_uniform at most 0.95x archetype's | **FALSIFIED** | within noise |
| `fused_archetype` | P3: no ECS workload slower than archetype by more than 5% | **HELD** | within noise |
| `grid_ring_knn` | P1: hs03_knn_heavy at most 0.5x uniform_grid's | **HELD** |  |
| `grid_ring_knn` | P2: s05_small_world at most 0.7x uniform_grid's | **HELD** |  |
| `grid_ring_knn` | P3: on no spatial workload slower than uniform_grid by more than 10% | **HELD** | within noise |
| `grid_ring_knn` | P4a: spatial_knn fixed_world at population 1000 at least 3x faster than uniform_grid | **FALSIFIED** |  |
| `grid_ring_knn` | P4b: spatial_knn fixed_density time exponent no higher than uniform_grid's | **HELD** |  |
| `grid_undo_log` | P1: hs02_rewind_few_moving tail latency below uniform_grid under snapshot-and-rebuild | **HELD** | within noise |
| `grid_undo_log` | P2: hs02_rewind_few_moving memory below uniform_grid under snapshot-and-rebuild | **HELD** |  |
| `grid_undo_log` | P3: hs01_rewind_all_moving tail latency above uniform_grid's (the log should lose) | **HELD** |  |
| `grouped_sparse_set` | P1: h05_sparse_component closes at least half the gap from sparse_set to archetype | **HELD** |  |
| `grouped_sparse_set` | P2: w01 and w02 at least 15% faster than sparse_set | **FALSIFIED** |  |
| `grouped_sparse_set` | P3: w03_structural_churn within 15% of sparse_set either way | **FALSIFIED** |  |
| `morton_lbvh` | P1a: s02_dense_clustered median tick lower than uniform_grid's | **HELD** |  |
| `morton_lbvh` | P1b: s02_dense_clustered peak memory at most half of uniform_grid's | **HELD** |  |
| `morton_lbvh` | P2: hs03_knn_heavy median tick lower than uniform_grid's | **HELD** |  |
| `morton_lbvh` | P3: s01_steady_uniform within 1.5x of uniform_grid | **HELD** |  |
| `morton_lbvh` | P4: beats morton_sorted on all ten spatial workloads | **HELD** |  |
| `morton_lbvh` | P5a: spatial_move time exponent at least 0.8 | **HELD** |  |
| `morton_lbvh` | P5b: spatial_query_radius fixed_density time exponent at most 0.25 | **HELD** |  |
| `morton_lbvh` | P6a: spatial_move fixed_world memory exponent at least 0.9 | **HELD** |  |
| `morton_lbvh` | P6b: spatial_query_radius fixed_world memory exponent at least 0.9 | **HELD** |  |
| `morton_lbvh` | P6c: spatial_knn fixed_world memory exponent at least 0.9 | **HELD** |  |
| `soa` | P1: narrow queries faster than aos (w02_query_heavy) | **FALSIFIED** |  |
| `soa` | P2: multi-component point access slower than aos (w07_point_wide) | **HELD** | within noise |
| `soa` | P3: smaller footprint than aos on every ECS workload | **HELD** |  |
| `sparse_set` | P1: on h05_sparse_component beats every layout that scans the whole index space | **HELD** |  |
| `spatial_hash` | P1: slower than uniform_grid on every spatial workload | **FALSIFIED** | within noise |
| `spatial_hash` | P2: less memory than uniform_grid on s02_dense_clustered | **HELD** |  |
| `uniform_grid` | P1: the fastest candidate on an evenly spread world (s01_steady_uniform) | **HELD** |  |

## `aos`

**P1 — FALSIFIED.** on point access to whole entities (w07_point_wide) within noise of the layouts that split components apart

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| w07_point_wide | 0.9559 | / soa | within 0.1 | held | within noise (band 6.6%) |
| w07_point_wide | 0.6329 | / sparse_set | within 0.1 | falsified |  |

## `archetype`

**P2 — FALSIFIED.** beaten by sparse_set on w03_structural_churn

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| w03_structural_churn | 0.7073 | / sparse_set | above 1.0 | falsified |  |

## `axis_sorted`

**P1 — FALSIFIED.** better than scanning everything, on every spatial workload

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs01_rewind_all_moving | 0.6415 | / brute_force | below 1.0 | held |  |
| hs02_rewind_few_moving | 0.6293 | / brute_force | below 1.0 | held |  |
| hs03_knn_heavy | 0.8944 | / brute_force | below 1.0 | held |  |
| hs04_flat_world | 0.4836 | / brute_force | below 1.0 | held |  |
| hs05_spawn_churn | 0.1019 | / brute_force | below 1.0 | held |  |
| hs06_mixed_reach | 0.5428 | / brute_force | below 1.0 | held |  |
| hs07_crowd | 0.6417 | / brute_force | below 1.0 | held |  |
| hs08_crowd_rollback | 0.703 | / brute_force | below 1.0 | held |  |
| s01_steady_uniform | 0.305 | / brute_force | below 1.0 | held |  |
| s02_dense_clustered | 0.6712 | / brute_force | below 1.0 | held |  |
| s03_wide_radius | 1.0993 | / brute_force | below 1.0 | falsified |  |
| s04_teleport | 0.2978 | / brute_force | below 1.0 | held |  |
| s05_small_world | 0.5576 | / brute_force | below 1.0 | held |  |

## `bitset_soa`

**P1 — HELD.** h05_sparse_component at most 0.6x soa's

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| h05_sparse_component | 0.5615 | / soa | at most 0.6 | held |  |

**P2 — FALSIFIED.** dense-mask workloads within 10% of soa either way

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| w01_steady_uniform | 0.8068 | / soa | within 0.1 | falsified |  |
| w02_query_heavy | 0.8728 | / soa | within 0.1 | falsified |  |
| w04_random_access | 0.901 | / soa | within 0.1 | held |  |

**P3 — HELD.** peak memory within 5% above soa's on every ECS workload

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| w01_steady_uniform | 1.0071 | / soa | at most 1.05 | held |  |
| w02_query_heavy | 1.0071 | / soa | at most 1.05 | held |  |
| w03_structural_churn | 1.0071 | / soa | at most 1.05 | held |  |
| w04_random_access | 1.0071 | / soa | at most 1.05 | held |  |
| w05_small_world | 1.0071 | / soa | at most 1.05 | held |  |
| h01_zipf_hotspot | 1.0071 | / soa | at most 1.05 | held |  |
| h02_bursty_spawn | 1.0071 | / soa | at most 1.05 | held |  |
| h03_stale_handles | 1.0071 | / soa | at most 1.05 | held |  |
| h04_recent_locality | 1.0071 | / soa | at most 1.05 | held |  |
| h05_sparse_component | 1.0071 | / soa | at most 1.05 | held |  |

## `cell_rows`

**P1 — HELD.** no slower than cell_sorted by more than 3% on any spatial workload

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs01_rewind_all_moving | 0.9953 | / cell_sorted | at most 1.03 | held | within noise (band 10.7%) |
| hs02_rewind_few_moving | 0.9668 | / cell_sorted | at most 1.03 | held |  |
| hs03_knn_heavy | 0.715 | / cell_sorted | at most 1.03 | held |  |
| hs04_flat_world | 0.9709 | / cell_sorted | at most 1.03 | held |  |
| hs05_spawn_churn | 0.9653 | / cell_sorted | at most 1.03 | held |  |
| hs06_mixed_reach | 0.9666 | / cell_sorted | at most 1.03 | held |  |
| hs07_crowd | 1.0061 | / cell_sorted | at most 1.03 | held | within noise (band 3.3%) |
| hs08_crowd_rollback | 1.0227 | / cell_sorted | at most 1.03 | held | within noise (band 5.1%) |
| s01_steady_uniform | 0.9383 | / cell_sorted | at most 1.03 | held |  |
| s02_dense_clustered | 0.9551 | / cell_sorted | at most 1.03 | held |  |
| s03_wide_radius | 0.9966 | / cell_sorted | at most 1.03 | held |  |
| s04_teleport | 0.9125 | / cell_sorted | at most 1.03 | held |  |
| s05_small_world | 0.7956 | / cell_sorted | at most 1.03 | held |  |

**P2 — FALSIFIED.** hs06_mixed_reach at most 0.9x cell_sorted's

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs06_mixed_reach | 0.9666 | / cell_sorted | at most 0.9 | falsified |  |

**P3 — HELD.** hs03_knn_heavy at most 0.9x cell_sorted's

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs03_knn_heavy | 0.715 | / cell_sorted | at most 0.9 | held |  |

## `cell_sorted`

**P1 — HELD.** beats morton_sorted on all ten spatial workloads

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| s01_steady_uniform | 0.2085 | / morton_sorted | below 1.0 | held |  |
| s02_dense_clustered | 0.4138 | / morton_sorted | below 1.0 | held |  |
| s03_wide_radius | 0.2571 | / morton_sorted | below 1.0 | held |  |
| s04_teleport | 0.2172 | / morton_sorted | below 1.0 | held |  |
| s05_small_world | 0.1661 | / morton_sorted | below 1.0 | held |  |
| hs01_rewind_all_moving | 0.3236 | / morton_sorted | below 1.0 | held |  |
| hs02_rewind_few_moving | 0.1875 | / morton_sorted | below 1.0 | held |  |
| hs03_knn_heavy | 0.1644 | / morton_sorted | below 1.0 | held |  |
| hs04_flat_world | 0.2206 | / morton_sorted | below 1.0 | held |  |
| hs05_spawn_churn | 0.313 | / morton_sorted | below 1.0 | held |  |

**P2 — HELD.** beats uniform_grid where every entity moves every tick

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| s01_steady_uniform | 0.9131 | / uniform_grid | below 1.0 | held |  |
| s03_wide_radius | 0.6633 | / uniform_grid | below 1.0 | held |  |
| s04_teleport | 0.7912 | / uniform_grid | below 1.0 | held |  |
| hs04_flat_world | 0.8489 | / uniform_grid | below 1.0 | held |  |

**P3 — HELD.** loses to uniform_grid on hs02_rewind_few_moving by at least 1.5x

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs02_rewind_few_moving | 4.4719 | / uniform_grid | at least 1.5 | held |  |

**P4 — FALSIFIED.** spatial_move time exponent at least 0.8 in both regimes

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| spatial_move·fixed_world | 0.585 |  | at least 0.8 | falsified | poor fit |
| spatial_move·fixed_density | 1.011 |  | at least 0.8 | held |  |

**P5 — HELD.** s04_teleport within 5% of its own s01_steady_uniform

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| s04_teleport | 1.0447 | / own s01_steady_uniform | within 0.05 | held | within noise (band 6.1%) |

## `delta_grid`

**P1a — HELD.** hs01_rewind_all_moving median below uniform_grid and grid_undo_log

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs01_rewind_all_moving | 0.8509 | / uniform_grid | below 1.0 | held |  |
| hs01_rewind_all_moving | 0.7034 | / grid_undo_log | below 1.0 | held |  |

**P1b — HELD.** hs01_rewind_all_moving p99 below uniform_grid and grid_undo_log

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs01_rewind_all_moving | 0.8614 | / uniform_grid | below 1.0 | held | within noise (band 38.6%) |
| hs01_rewind_all_moving | 0.4678 | / grid_undo_log | below 1.0 | held |  |

**P2a — FALSIFIED.** hs02_rewind_few_moving median within 1.3x of grid_undo_log

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs02_rewind_few_moving | 1.318 | / grid_undo_log | at most 1.3 | falsified | within noise (band 1.8%) |

**P2b — HELD.** hs02_rewind_few_moving p99 within 1.5x of grid_undo_log

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs02_rewind_few_moving | 1.4639 | / grid_undo_log | at most 1.5 | held | within noise (band 74.3%) |

**P2c — HELD.** hs02_rewind_few_moving p99 below uniform_grid under snapshot-and-rebuild

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs02_rewind_few_moving | 0.479 | / uniform_grid | below 1.0 | held | within noise (band 100.2%) |

**P3 — HELD.** within 1.2x of cell_sorted where everything moves and nothing rewinds

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| s01_steady_uniform | 1.164 | / cell_sorted | at most 1.2 | held |  |
| s03_wide_radius | 1.1827 | / cell_sorted | at most 1.2 | held | within noise (band 2.8%) |
| s04_teleport | 1.1109 | / cell_sorted | at most 1.2 | held |  |
| hs04_flat_world | 1.1855 | / cell_sorted | at most 1.2 | held | within noise (band 4.6%) |

**P4 — HELD.** spatial_move time exponent at most 0.3 in both regimes

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| spatial_move·fixed_world | -0.033 |  | at most 0.3 | held | poor fit |
| spatial_move·fixed_density | 0.086 |  | at most 0.3 | held | poor fit |

**P5 — FALSIFIED.** rewind_move_fraction p99 within 1.2x of the better history strategy at every value

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| rewind_move_fraction·0.01 | 1.671 | / min = grid_undo_log | at most 1.2 | falsified |  |
| rewind_move_fraction·0.02 | 1.2642 | / min = grid_undo_log | at most 1.2 | falsified |  |
| rewind_move_fraction·0.05 | 6.5462 | / min = grid_undo_log | at most 1.2 | falsified |  |
| rewind_move_fraction·0.1 | 1.699 | / min = grid_undo_log | at most 1.2 | falsified |  |
| rewind_move_fraction·0.2 | 1.2153 | / min = uniform_grid | at most 1.2 | falsified |  |
| rewind_move_fraction·0.35 | 1.6499 | / min = uniform_grid | at most 1.2 | falsified |  |
| rewind_move_fraction·0.5 | 0.53 | / min = uniform_grid | at most 1.2 | held |  |
| rewind_move_fraction·0.75 | 0.8148 | / min = uniform_grid | at most 1.2 | held |  |
| rewind_move_fraction·1.0 | 0.914 | / min = uniform_grid | at most 1.2 | held |  |

## `fused_archetype`

**P1 — FALSIFIED.** w02_query_heavy at most 0.95x archetype's

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| w02_query_heavy | 0.9829 | / archetype | at most 0.95 | falsified |  |

**P2 — FALSIFIED.** w01_steady_uniform at most 0.95x archetype's

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| w01_steady_uniform | 0.9954 | / archetype | at most 0.95 | falsified | within noise (band 6.8%) |

**P3 — HELD.** no ECS workload slower than archetype by more than 5%

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| w01_steady_uniform | 0.9954 | / archetype | at most 1.05 | held | within noise (band 6.8%) |
| w02_query_heavy | 0.9829 | / archetype | at most 1.05 | held |  |
| w03_structural_churn | 0.9518 | / archetype | at most 1.05 | held | within noise (band 9.5%) |
| w04_random_access | 0.9849 | / archetype | at most 1.05 | held |  |
| w05_small_world | 0.9957 | / archetype | at most 1.05 | held |  |
| h01_zipf_hotspot | 0.9823 | / archetype | at most 1.05 | held |  |
| h02_bursty_spawn | 0.9401 | / archetype | at most 1.05 | held |  |
| h03_stale_handles | 0.9423 | / archetype | at most 1.05 | held |  |
| h04_recent_locality | 0.9715 | / archetype | at most 1.05 | held |  |
| h05_sparse_component | 0.9264 | / archetype | at most 1.05 | held |  |

## `grid_ring_knn`

**P1 — HELD.** hs03_knn_heavy at most 0.5x uniform_grid's

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs03_knn_heavy | 0.386 | / uniform_grid | at most 0.5 | held |  |

**P2 — HELD.** s05_small_world at most 0.7x uniform_grid's

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| s05_small_world | 0.655 | / uniform_grid | at most 0.7 | held |  |

**P3 — HELD.** on no spatial workload slower than uniform_grid by more than 10%

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| s01_steady_uniform | 0.8726 | / uniform_grid | at most 1.1 | held |  |
| s02_dense_clustered | 0.9194 | / uniform_grid | at most 1.1 | held |  |
| s03_wide_radius | 0.8854 | / uniform_grid | at most 1.1 | held |  |
| s04_teleport | 0.9156 | / uniform_grid | at most 1.1 | held |  |
| s05_small_world | 0.655 | / uniform_grid | at most 1.1 | held |  |
| hs01_rewind_all_moving | 1.0177 | / uniform_grid | at most 1.1 | held | within noise (band 8.9%) |
| hs02_rewind_few_moving | 0.7085 | / uniform_grid | at most 1.1 | held |  |
| hs03_knn_heavy | 0.386 | / uniform_grid | at most 1.1 | held |  |
| hs04_flat_world | 1.0153 | / uniform_grid | at most 1.1 | held |  |
| hs05_spawn_churn | 0.9499 | / uniform_grid | at most 1.1 | held | within noise (band 21.6%) |

**P4a — FALSIFIED.** spatial_knn fixed_world at population 1000 at least 3x faster than uniform_grid

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| spatial_knn·fixed_world·n=1000 | 0.6201 | / uniform_grid | at most 0.3333 | falsified |  |

**P4b — HELD.** spatial_knn fixed_density time exponent no higher than uniform_grid's

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| spatial_knn·fixed_density | -0.013 | - uniform_grid | at most 0.0 | held | poor fit |

## `grid_undo_log`

**P1 — HELD.** hs02_rewind_few_moving tail latency below uniform_grid under snapshot-and-rebuild

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs02_rewind_few_moving | 0.3272 | / uniform_grid | below 1.0 | held | within noise (band 73.0%) |

**P2 — HELD.** hs02_rewind_few_moving memory below uniform_grid under snapshot-and-rebuild

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs02_rewind_few_moving | 0.4096 | / uniform_grid | below 1.0 | held |  |

**P3 — HELD.** hs01_rewind_all_moving tail latency above uniform_grid's (the log should lose)

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs01_rewind_all_moving | 1.8414 | / uniform_grid | above 1.0 | held |  |

## `grouped_sparse_set`

**P1 — HELD.** h05_sparse_component closes at least half the gap from sparse_set to archetype

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| h05_sparse_component | 0.6086 | gap sparse_set to archetype closed | at least 0.5 | held |  |

**P2 — FALSIFIED.** w01 and w02 at least 15% faster than sparse_set

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| w01_steady_uniform | 0.7597 | / sparse_set | at most 0.85 | held |  |
| w02_query_heavy | 0.8793 | / sparse_set | at most 0.85 | falsified |  |

**P3 — FALSIFIED.** w03_structural_churn within 15% of sparse_set either way

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| w03_structural_churn | 0.6117 | / sparse_set | within 0.15 | falsified |  |

## `morton_lbvh`

**P1a — HELD.** s02_dense_clustered median tick lower than uniform_grid's

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| s02_dense_clustered | 0.5739 | / uniform_grid | below 1.0 | held |  |

**P1b — HELD.** s02_dense_clustered peak memory at most half of uniform_grid's

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| s02_dense_clustered | 0.3058 | / uniform_grid | at most 0.5 | held |  |

**P2 — HELD.** hs03_knn_heavy median tick lower than uniform_grid's

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs03_knn_heavy | 0.5216 | / uniform_grid | below 1.0 | held |  |

**P3 — HELD.** s01_steady_uniform within 1.5x of uniform_grid

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| s01_steady_uniform | 1.0385 | / uniform_grid | at most 1.5 | held |  |

**P4 — HELD.** beats morton_sorted on all ten spatial workloads

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| s01_steady_uniform | 0.2372 | / morton_sorted | below 1.0 | held |  |
| s02_dense_clustered | 0.3366 | / morton_sorted | below 1.0 | held |  |
| s03_wide_radius | 0.3321 | / morton_sorted | below 1.0 | held |  |
| s04_teleport | 0.2349 | / morton_sorted | below 1.0 | held |  |
| s05_small_world | 0.174 | / morton_sorted | below 1.0 | held |  |
| hs01_rewind_all_moving | 0.3247 | / morton_sorted | below 1.0 | held |  |
| hs02_rewind_few_moving | 0.2013 | / morton_sorted | below 1.0 | held |  |
| hs03_knn_heavy | 0.0912 | / morton_sorted | below 1.0 | held |  |
| hs04_flat_world | 0.3806 | / morton_sorted | below 1.0 | held |  |
| hs05_spawn_churn | 0.2955 | / morton_sorted | below 1.0 | held |  |

**P5a — HELD.** spatial_move time exponent at least 0.8

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| spatial_move·fixed_world | 0.931 |  | at least 0.8 | held |  |
| spatial_move·fixed_density | 0.92 |  | at least 0.8 | held |  |

**P5b — HELD.** spatial_query_radius fixed_density time exponent at most 0.25

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| spatial_query_radius·fixed_density | 0.182 |  | at most 0.25 | held | poor fit |

**P6a — HELD.** spatial_move fixed_world memory exponent at least 0.9

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| spatial_move·fixed_world | 0.999 |  | at least 0.9 | held |  |

**P6b — HELD.** spatial_query_radius fixed_world memory exponent at least 0.9

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| spatial_query_radius·fixed_world | 0.999 |  | at least 0.9 | held |  |

**P6c — HELD.** spatial_knn fixed_world memory exponent at least 0.9

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| spatial_knn·fixed_world | 0.994 |  | at least 0.9 | held |  |

## `soa`

**P1 — FALSIFIED.** narrow queries faster than aos (w02_query_heavy)

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| w02_query_heavy | 1.1084 | / aos | below 1.0 | falsified |  |

**P2 — HELD.** multi-component point access slower than aos (w07_point_wide)

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| w07_point_wide | 1.0462 | / aos | above 1.0 | held | within noise (band 6.6%) |

**P3 — HELD.** smaller footprint than aos on every ECS workload

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| h01_zipf_hotspot | 0.5238 | / aos | below 1.0 | held |  |
| h02_bursty_spawn | 0.5238 | / aos | below 1.0 | held |  |
| h03_stale_handles | 0.5238 | / aos | below 1.0 | held |  |
| h04_recent_locality | 0.5238 | / aos | below 1.0 | held |  |
| h05_sparse_component | 0.5238 | / aos | below 1.0 | held |  |
| w01_steady_uniform | 0.5238 | / aos | below 1.0 | held |  |
| w02_query_heavy | 0.5238 | / aos | below 1.0 | held |  |
| w03_structural_churn | 0.5238 | / aos | below 1.0 | held |  |
| w04_random_access | 0.5238 | / aos | below 1.0 | held |  |
| w05_small_world | 0.5252 | / aos | below 1.0 | held |  |
| w06_point_narrow | 0.5238 | / aos | below 1.0 | held |  |
| w07_point_wide | 0.5238 | / aos | below 1.0 | held |  |

## `sparse_set`

**P1 — HELD.** on h05_sparse_component beats every layout that scans the whole index space

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| h05_sparse_component | 0.5692 | / aos | below 1.0 | held |  |
| h05_sparse_component | 0.8317 | / soa | below 1.0 | held |  |

## `spatial_hash`

**P1 — FALSIFIED.** slower than uniform_grid on every spatial workload

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| hs01_rewind_all_moving | 1.5059 | / uniform_grid | above 1.0 | held |  |
| hs02_rewind_few_moving | 2.621 | / uniform_grid | above 1.0 | held |  |
| hs03_knn_heavy | 3.8069 | / uniform_grid | above 1.0 | held |  |
| hs04_flat_world | 1.1749 | / uniform_grid | above 1.0 | held | within noise (band 20.5%) |
| hs05_spawn_churn | 1.7678 | / uniform_grid | above 1.0 | held |  |
| hs06_mixed_reach | 1.0499 | / uniform_grid | above 1.0 | held |  |
| hs07_crowd | 1.2156 | / uniform_grid | above 1.0 | held |  |
| hs08_crowd_rollback | 1.1872 | / uniform_grid | above 1.0 | held |  |
| s01_steady_uniform | 2.1201 | / uniform_grid | above 1.0 | held |  |
| s02_dense_clustered | 0.9939 | / uniform_grid | above 1.0 | falsified | within noise (band 5.0%) |
| s03_wide_radius | 0.9928 | / uniform_grid | above 1.0 | falsified | within noise (band 6.1%) |
| s04_teleport | 2.0967 | / uniform_grid | above 1.0 | held |  |
| s05_small_world | 2.9369 | / uniform_grid | above 1.0 | held |  |

**P2 — HELD.** less memory than uniform_grid on s02_dense_clustered

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| s02_dense_clustered | 0.2214 | / uniform_grid | below 1.0 | held |  |

## `uniform_grid`

**P1 — HELD.** the fastest candidate on an evenly spread world (s01_steady_uniform)

| point | value | compared | bound | result | note |
|---|---:|---|---|---|---|
| s01_steady_uniform | 0.2526 | / axis_sorted | below 1.0 | held |  |
| s01_steady_uniform | 0.077 | / brute_force | below 1.0 | held |  |
| s01_steady_uniform | 0.2284 | / morton_sorted | below 1.0 | held |  |
| s01_steady_uniform | 0.4717 | / spatial_hash | below 1.0 | held |  |
