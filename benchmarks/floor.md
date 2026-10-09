# Headroom

The floor of a tick is what it would cost if finding every answer were free: applying the tick's inserts, removes and moves to flat arrays, plus folding the digest of exactly the entities in every answer from packed buffers. Everything a candidate spends above it is search. It is measured by `gds_floor_spatial` and every answer it digests is checked against the oracle's.

`over-admission` is how many entities the reference grid's query box hands to the exact distance test per entity actually in the answer: what a tighter broad phase could remove. `hits/query` is the answer size, which nothing can remove.

Ratios divide a median tick from `benchmarks/results.json` by a floor measured in a separate process. They estimate headroom; they are not a measurement of it.

- candidates: run `20261009T042136Z`
- floor: 3 repetitions per tick, minimum taken

| workload | floor p50 (us) | of which state | of which answers | hits/query | over-admission | best candidate | its p50 (us) | x floor |
|---|---:|---:|---:|---:|---:|---|---:|---:|
| `s01_steady_uniform` | 233.7 | 229.9 | 3.8 | 0.6 | 5.90 | `cell_rows` | 712.7 | 3.0 |
| `s02_dense_clustered` | 1403.6 | 589.5 | 818.7 | 999.2 | 2.76 | `morton_lbvh` | 2812.0 | 2.0 |
| `s03_wide_radius` | 328.2 | 205.7 | 120.7 | 237.4 | 4.70 | `cell_rows` | 783.2 | 2.4 |
| `s04_teleport` | 221.4 | 217.5 | 3.8 | 0.6 | 5.97 | `cell_rows` | 725.2 | 3.3 |
| `s05_small_world` | 25.5 | 20.3 | 4.4 | 0.9 | 5.36 | `grid_ring_knn` | 160.5 | 6.3 |
| `hs01_rewind_all_moving` | 328.9 | 327.3 | 1.7 | 0.9 | 6.00 | `delta_grid` | 1220.7 | 3.7 |
| `hs02_rewind_few_moving` | 8.8 | 7.1 | 1.7 | 0.9 | 6.02 | `grid_ring_knn` | 100.2 | 11.4 |
| `hs03_knn_heavy` | 376.7 | 294.6 | 83.0 | 0.7 | 5.95 | `grid_ring_knn` | 2196.9 | 5.8 |
| `hs04_flat_world` | 301.3 | 289.5 | 11.7 | 10.2 | 3.01 | `cell_rows` | 647.8 | 2.1 |
| `hs05_spawn_churn` | 527.3 | 525.1 | 1.9 | 0.6 | 5.92 | `grid_ring_knn` | 745.0 | 1.4 |
| `hs06_mixed_reach` | 364.2 | 226.8 | 134.0 | 131.8 | 3.96 | `cell_rows` | 865.7 | 2.4 |
| `hs07_crowd` | 1475.8 | 617.0 | 834.9 | 989.1 | 2.75 | `morton_lbvh` | 3001.7 | 2.0 |
| `hs08_crowd_rollback` | 963.4 | 495.1 | 430.9 | 784.4 | 2.43 | `morton_lbvh` | 2560.0 | 2.7 |

## Every candidate, as a multiple of the floor

| candidate | `s01_steady_uniform` | `s02_dense_clustered` | `s03_wide_radius` | `s04_teleport` | `s05_small_world` | `hs01_rewind_all_moving` | `hs02_rewind_few_moving` | `hs03_knn_heavy` | `hs04_flat_world` | `hs05_spawn_churn` | `hs06_mixed_reach` | `hs07_crowd` | `hs08_crowd_rollback` |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `axis_sorted` | 14.1 | 6.7 | 11.1 | 14.9 | 20.0 | 14.0 | 447.4 | 40.4 | 11.7 | 5.9 | 10.8 | 5.9 | 8.0 |
| `brute_force` | 46.1 | 9.5 | 10.1 | 50.0 | 35.2 | 21.7 | 710.5 | 45.0 | 24.2 | 58.2 | 19.9 | 9.3 | 11.4 |
| `cell_rows` | 3.0 | 2.4 | 2.4 | 3.3 | 7.0 | 4.0 | 70.3 | 10.2 | 2.1 | 1.7 | 2.4 | 2.5 | 3.3 |
| `cell_sorted` | 3.2 | 2.6 | 2.4 | 3.4 | 8.8 | 4.0 | 73.6 | 14.1 | 2.2 | 1.8 | 2.4 | 2.3 | 3.2 |
| `delta_grid` | 3.8 | 2.6 | 2.8 | 4.0 | 9.7 | 3.7 | 17.9 | 15.2 | 2.6 | 2.2 | 2.7 | 2.4 | 2.7 |
| `grid_ring_knn` | 3.1 | 3.3 | 3.3 | 4.1 | 6.3 | 4.4 | 11.4 | 5.8 | 2.7 | 1.4 | 3.0 | 3.3 | 3.9 |
| `grid_undo_log` | 3.5 | 3.6 | 3.4 | 4.4 | 9.4 | 5.2 | 13.8 | 15.0 | 2.6 | 1.5 | 3.3 | 3.4 | 4.1 |
| `morton_lbvh` | 3.7 | 2.0 | 3.1 | 3.9 | 9.3 | 4.1 | 78.2 | 7.9 | 3.8 | 1.7 | 3.3 | 2.0 | 2.7 |
| `morton_sorted` | 15.5 | 6.0 | 9.4 | 16.5 | 53.2 | 12.5 | 388.3 | 86.1 | 10.0 | 5.7 | 9.3 | 5.5 | 7.4 |
| `spatial_hash` | 7.5 | 3.5 | 3.6 | 9.6 | 28.1 | 6.4 | 42.2 | 57.5 | 3.1 | 2.6 | 3.6 | 4.3 | 4.4 |
| `uniform_grid` | 3.6 | 3.6 | 3.6 | 4.5 | 9.5 | 4.3 | 16.3 | 15.1 | 2.6 | 1.5 | 3.4 | 3.5 | 3.7 |
