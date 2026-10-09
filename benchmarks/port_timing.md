# Paired timing: C against C++

Metric `total_ns`, 5 rounds per pair alternating which build runs first, 1 repetition(s) inside each run, 1 warmup. Ratio is C/C++ within a round; below 1 means the second build is faster. The range is the lowest and highest per-round ratio.

| binary | geomean ratio | workloads | lowest median | highest median |
|---|---:|---:|---:|---:|
| `gds_ecs_aos` | 0.993 | 12 | 0.773 (w03_structural_churn) | 1.175 (w06_point_narrow) |
| `gds_ecs_archetype` | 1.004 | 12 | 0.862 (h01_zipf_hotspot) | 1.188 (w05_small_world) |
| `gds_ecs_bitset_soa` | 1.010 | 12 | 0.885 (w01_steady_uniform) | 1.156 (w07_point_wide) |
| `gds_ecs_broken_recycle` | 0.983 | 12 | 0.896 (w04_random_access) | 1.132 (w01_steady_uniform) |
| `gds_ecs_fused_archetype` | 1.004 | 12 | 0.860 (h02_bursty_spawn) | 1.127 (w02_query_heavy) |
| `gds_ecs_grouped_sparse_set` | 0.993 | 12 | 0.829 (w03_structural_churn) | 1.220 (w06_point_narrow) |
| `gds_ecs_query_memo` | 1.021 | 12 | 0.913 (h04_recent_locality) | 1.264 (w01_steady_uniform) |
| `gds_ecs_reference` | 0.699 | 12 | 0.478 (h05_sparse_component) | 1.132 (w05_small_world) |
| `gds_ecs_soa` | 1.021 | 12 | 0.941 (h05_sparse_component) | 1.153 (w04_random_access) |
| `gds_ecs_sparse_set` | 1.003 | 12 | 0.941 (w01_steady_uniform) | 1.080 (w03_structural_churn) |
| `gds_spatial_axis_sorted` | 0.956 | 13 | 0.814 (s05_small_world) | 1.066 (hs08_crowd_rollback) |
| `gds_spatial_brute_force` | 0.904 | 13 | 0.755 (hs03_knn_heavy) | 1.054 (s05_small_world) |
| `gds_spatial_cell_rows` | 0.997 | 13 | 0.910 (hs04_flat_world) | 1.113 (s01_steady_uniform) |
| `gds_spatial_cell_sorted` | 0.960 | 13 | 0.782 (s03_wide_radius) | 1.194 (hs07_crowd) |
| `gds_spatial_delta_grid` | 0.920 | 13 | 0.781 (hs02_rewind_few_moving) | 1.034 (hs07_crowd) |
| `gds_spatial_grid_ring_knn` | 0.970 | 13 | 0.877 (hs05_spawn_churn) | 1.098 (hs03_knn_heavy) |
| `gds_spatial_grid_undo_log` | 0.972 | 13 | 0.799 (hs02_rewind_few_moving) | 1.342 (hs01_rewind_all_moving) |
| `gds_spatial_morton_lbvh` | 0.944 | 13 | 0.825 (s05_small_world) | 1.065 (hs01_rewind_all_moving) |
| `gds_spatial_morton_sorted` | 1.009 | 13 | 0.889 (hs07_crowd) | 1.137 (hs01_rewind_all_moving) |
| `gds_spatial_spatial_hash` | 1.003 | 13 | 0.951 (s01_steady_uniform) | 1.041 (hs06_mixed_reach) |
| `gds_spatial_uniform_grid` | 1.010 | 13 | 0.832 (s04_teleport) | 1.307 (hs02_rewind_few_moving) |

Geometric mean over every pair: 0.967.

## Every pair

| binary | workload | a median | b median | median ratio | range | peak a → b |
|---|---|---:|---:|---:|---|---|
| `gds_ecs_aos` | w01_steady_uniform | 341.24 ms | 345.50 ms | 1.100 | 0.928–1.279 | 5505024 → 5505024 |
| `gds_ecs_aos` | w02_query_heavy | 1029.15 ms | 1091.19 ms | 1.022 | 0.958–1.123 | 11010048 → 11010048 |
| `gds_ecs_aos` | w03_structural_churn | 167.17 ms | 133.47 ms | 0.773 | 0.629–1.294 | 2752512 → 2752512 |
| `gds_ecs_aos` | w04_random_access | 899.89 ms | 930.99 ms | 1.030 | 0.953–1.245 | 22020096 → 22020096 |
| `gds_ecs_aos` | w05_small_world | 32.09 ms | 32.30 ms | 1.011 | 0.753–1.031 | 345088 → 345088 |
| `gds_ecs_aos` | w06_point_narrow | 253.12 ms | 295.63 ms | 1.175 | 0.572–1.623 | 22020096 → 22020096 |
| `gds_ecs_aos` | w07_point_wide | 194.71 ms | 172.30 ms | 0.869 | 0.571–0.917 | 22020096 → 22020096 |
| `gds_ecs_aos` | h01_zipf_hotspot | 832.97 ms | 823.78 ms | 0.955 | 0.906–1.101 | 22020096 → 22020096 |
| `gds_ecs_aos` | h02_bursty_spawn | 972.72 ms | 936.57 ms | 0.998 | 0.909–1.067 | 44040224 → 44040224 |
| `gds_ecs_aos` | h03_stale_handles | 354.41 ms | 364.30 ms | 1.028 | 0.736–1.338 | 5505024 → 5505024 |
| `gds_ecs_aos` | h04_recent_locality | 261.33 ms | 275.93 ms | 1.130 | 0.974–1.165 | 5505024 → 5505024 |
| `gds_ecs_aos` | h05_sparse_component | 437.20 ms | 384.29 ms | 0.900 | 0.810–1.069 | 22020096 → 22020096 |
| `gds_ecs_archetype` | w01_steady_uniform | 271.29 ms | 258.35 ms | 0.985 | 0.874–1.296 | 3130368 → 3130368 |
| `gds_ecs_archetype` | w02_query_heavy | 797.08 ms | 823.33 ms | 1.034 | 0.857–1.119 | 6456320 → 6456320 |
| `gds_ecs_archetype` | w03_structural_churn | 107.76 ms | 96.41 ms | 0.928 | 0.805–1.262 | 2113536 → 2113536 |
| `gds_ecs_archetype` | w04_random_access | 792.17 ms | 833.23 ms | 1.035 | 0.876–1.063 | 12518400 → 12518400 |
| `gds_ecs_archetype` | w05_small_world | 33.54 ms | 39.84 ms | 1.188 | 0.875–1.594 | 221824 → 221824 |
| `gds_ecs_archetype` | w06_point_narrow | 354.10 ms | 348.32 ms | 0.987 | 0.943–1.110 | 15204480 → 15204480 |
| `gds_ecs_archetype` | w07_point_wide | 269.48 ms | 248.57 ms | 0.922 | 0.537–1.041 | 15204480 → 15204480 |
| `gds_ecs_archetype` | h01_zipf_hotspot | 568.26 ms | 531.90 ms | 0.862 | 0.725–1.391 | 11011072 → 11011072 |
| `gds_ecs_archetype` | h02_bursty_spawn | 620.48 ms | 586.62 ms | 0.989 | 0.845–1.028 | 22939680 → 22939680 |
| `gds_ecs_archetype` | h03_stale_handles | 262.60 ms | 298.10 ms | 1.135 | 0.918–1.313 | 4417536 → 4417536 |
| `gds_ecs_archetype` | h04_recent_locality | 246.85 ms | 296.97 ms | 1.038 | 0.979–1.408 | 3120768 → 3120768 |
| `gds_ecs_archetype` | h05_sparse_component | 119.78 ms | 107.97 ms | 0.984 | 0.751–1.174 | 13958592 → 13958592 |
| `gds_ecs_bitset_soa` | w01_steady_uniform | 261.56 ms | 274.31 ms | 0.885 | 0.654–1.625 | 2904064 → 2904064 |
| `gds_ecs_bitset_soa` | w02_query_heavy | 831.47 ms | 907.20 ms | 1.066 | 0.964–1.187 | 5808128 → 5808128 |
| `gds_ecs_bitset_soa` | w03_structural_churn | 82.25 ms | 85.59 ms | 1.001 | 0.862–1.099 | 1452032 → 1452032 |
| `gds_ecs_bitset_soa` | w04_random_access | 863.99 ms | 842.67 ms | 1.036 | 0.872–1.283 | 11616256 → 11616256 |
| `gds_ecs_bitset_soa` | w05_small_world | 30.32 ms | 28.66 ms | 0.964 | 0.647–1.071 | 182528 → 182528 |
| `gds_ecs_bitset_soa` | w06_point_narrow | 252.81 ms | 236.38 ms | 0.938 | 0.833–1.122 | 11616256 → 11616256 |
| `gds_ecs_bitset_soa` | w07_point_wide | 199.17 ms | 200.46 ms | 1.156 | 0.663–1.337 | 11616256 → 11616256 |
| `gds_ecs_bitset_soa` | h01_zipf_hotspot | 575.52 ms | 648.40 ms | 1.084 | 0.911–1.590 | 11616256 → 11616256 |
| `gds_ecs_bitset_soa` | h02_bursty_spawn | 707.39 ms | 683.30 ms | 0.968 | 0.887–1.130 | 23232544 → 23232544 |
| `gds_ecs_bitset_soa` | h03_stale_handles | 263.12 ms | 283.36 ms | 1.082 | 0.656–1.179 | 2904064 → 2904064 |
| `gds_ecs_bitset_soa` | h04_recent_locality | 238.02 ms | 232.07 ms | 1.074 | 0.852–1.146 | 2904064 → 2904064 |
| `gds_ecs_bitset_soa` | h05_sparse_component | 212.22 ms | 188.81 ms | 0.900 | 0.811–1.097 | 11616256 → 11616256 |
| `gds_ecs_broken_recycle` | w01_steady_uniform | 291.26 ms | 322.48 ms | 1.132 | 0.929–1.272 | 5111808 → 5111808 |
| `gds_ecs_broken_recycle` | w02_query_heavy | 1029.73 ms | 1037.70 ms | 1.008 | 0.778–1.101 | 10223616 → 10223616 |
| `gds_ecs_broken_recycle` | w03_structural_churn | 142.61 ms | 140.53 ms | 0.905 | 0.727–1.290 | 2555904 → 2555904 |
| `gds_ecs_broken_recycle` | w04_random_access | 915.46 ms | 875.99 ms | 0.896 | 0.831–1.185 | 20447232 → 20447232 |
| `gds_ecs_broken_recycle` | w05_small_world | 36.21 ms | 37.39 ms | 0.911 | 0.807–1.237 | 320512 → 320512 |
| `gds_ecs_broken_recycle` | w06_point_narrow | 226.14 ms | 230.36 ms | 1.059 | 0.935–1.160 | 20447232 → 20447232 |
| `gds_ecs_broken_recycle` | w07_point_wide | 188.59 ms | 176.81 ms | 0.989 | 0.895–1.199 | 20447232 → 20447232 |
| `gds_ecs_broken_recycle` | h01_zipf_hotspot | 745.86 ms | 819.87 ms | 0.982 | 0.933–1.360 | 20447232 → 20447232 |
| `gds_ecs_broken_recycle` | h02_bursty_spawn | 846.64 ms | 829.96 ms | 1.000 | 0.887–1.180 | 40894496 → 40894496 |
| `gds_ecs_broken_recycle` | h03_stale_handles | 284.34 ms | 342.69 ms | 1.005 | 0.948–1.335 | 5111808 → 5111808 |
| `gds_ecs_broken_recycle` | h04_recent_locality | 269.27 ms | 251.86 ms | 0.935 | 0.748–1.061 | 5111808 → 5111808 |
| `gds_ecs_broken_recycle` | h05_sparse_component | 373.91 ms | 392.59 ms | 1.004 | 0.969–1.127 | 20447232 → 20447232 |
| `gds_ecs_fused_archetype` | w01_steady_uniform | 225.75 ms | 247.56 ms | 1.097 | 0.949–1.240 | 3130368 → 3130368 |
| `gds_ecs_fused_archetype` | w02_query_heavy | 799.31 ms | 849.88 ms | 1.127 | 0.801–1.336 | 6456320 → 6456320 |
| `gds_ecs_fused_archetype` | w03_structural_churn | 98.21 ms | 105.99 ms | 1.011 | 0.504–1.314 | 2113536 → 2113536 |
| `gds_ecs_fused_archetype` | w04_random_access | 828.95 ms | 823.76 ms | 1.005 | 0.953–1.287 | 12518400 → 12518400 |
| `gds_ecs_fused_archetype` | w05_small_world | 39.06 ms | 38.91 ms | 0.893 | 0.864–1.081 | 221824 → 221824 |
| `gds_ecs_fused_archetype` | w06_point_narrow | 328.33 ms | 330.17 ms | 1.102 | 0.949–1.200 | 15204480 → 15204480 |
| `gds_ecs_fused_archetype` | w07_point_wide | 246.15 ms | 273.40 ms | 0.883 | 0.860–1.239 | 15204480 → 15204480 |
| `gds_ecs_fused_archetype` | h01_zipf_hotspot | 531.18 ms | 525.69 ms | 0.937 | 0.910–1.167 | 11011072 → 11011072 |
| `gds_ecs_fused_archetype` | h02_bursty_spawn | 663.33 ms | 598.91 ms | 0.860 | 0.844–1.047 | 22939680 → 22939680 |
| `gds_ecs_fused_archetype` | h03_stale_handles | 247.65 ms | 278.15 ms | 1.117 | 1.034–1.153 | 4417536 → 4417536 |
| `gds_ecs_fused_archetype` | h04_recent_locality | 263.72 ms | 258.18 ms | 1.031 | 0.819–1.186 | 3120768 → 3120768 |
| `gds_ecs_fused_archetype` | h05_sparse_component | 120.55 ms | 124.19 ms | 1.030 | 0.737–1.318 | 13958592 → 13958592 |
| `gds_ecs_grouped_sparse_set` | w01_steady_uniform | 317.82 ms | 269.01 ms | 0.893 | 0.846–1.020 | 4785152 → 4785152 |
| `gds_ecs_grouped_sparse_set` | w02_query_heavy | 1054.58 ms | 1044.81 ms | 1.029 | 0.686–1.116 | 9568256 → 9568256 |
| `gds_ecs_grouped_sparse_set` | w03_structural_churn | 114.52 ms | 97.14 ms | 0.829 | 0.580–1.496 | 2130176 → 2130176 |
| `gds_ecs_grouped_sparse_set` | w04_random_access | 938.86 ms | 912.15 ms | 1.016 | 0.757–1.276 | 18874368 → 18874368 |
| `gds_ecs_grouped_sparse_set` | w05_small_world | 39.56 ms | 37.02 ms | 0.978 | 0.728–1.035 | 267264 → 267264 |
| `gds_ecs_grouped_sparse_set` | w06_point_narrow | 349.46 ms | 406.65 ms | 1.220 | 0.799–1.468 | 24117248 → 24117248 |
| `gds_ecs_grouped_sparse_set` | w07_point_wide | 303.61 ms | 277.21 ms | 0.968 | 0.830–1.088 | 24117248 → 24117248 |
| `gds_ecs_grouped_sparse_set` | h01_zipf_hotspot | 562.87 ms | 542.32 ms | 1.003 | 0.694–1.059 | 15990784 → 15990784 |
| `gds_ecs_grouped_sparse_set` | h02_bursty_spawn | 796.31 ms | 797.66 ms | 0.954 | 0.865–1.460 | 24117280 → 24117280 |
| `gds_ecs_grouped_sparse_set` | h03_stale_handles | 282.81 ms | 291.98 ms | 1.031 | 0.820–1.070 | 4784128 → 4784128 |
| `gds_ecs_grouped_sparse_set` | h04_recent_locality | 253.53 ms | 239.23 ms | 1.008 | 0.803–1.308 | 4325376 → 4325376 |
| `gds_ecs_grouped_sparse_set` | h05_sparse_component | 215.64 ms | 212.36 ms | 1.039 | 0.850–1.230 | 16056320 → 16056320 |
| `gds_ecs_query_memo` | w01_steady_uniform | 329.11 ms | 401.14 ms | 1.264 | 0.804–1.326 | 2883584 → 2883584 |
| `gds_ecs_query_memo` | w02_query_heavy | 934.04 ms | 886.26 ms | 0.938 | 0.689–1.079 | 5767168 → 5767168 |
| `gds_ecs_query_memo` | w03_structural_churn | 204.35 ms | 206.94 ms | 1.044 | 0.779–1.260 | 1441792 → 1441792 |
| `gds_ecs_query_memo` | w04_random_access | 464.55 ms | 447.89 ms | 1.007 | 0.874–1.200 | 11534336 → 11534336 |
| `gds_ecs_query_memo` | w05_small_world | 57.17 ms | 56.47 ms | 0.988 | 0.918–1.323 | 181248 → 181248 |
| `gds_ecs_query_memo` | w06_point_narrow | 307.26 ms | 324.54 ms | 1.063 | 0.801–1.440 | 11534336 → 11534336 |
| `gds_ecs_query_memo` | w07_point_wide | 294.73 ms | 276.33 ms | 0.953 | 0.718–1.202 | 11534336 → 11534336 |
| `gds_ecs_query_memo` | h01_zipf_hotspot | 789.50 ms | 894.56 ms | 1.047 | 0.914–1.600 | 11534336 → 11534336 |
| `gds_ecs_query_memo` | h02_bursty_spawn | 913.75 ms | 902.09 ms | 0.970 | 0.812–1.183 | 23068704 → 23068704 |
| `gds_ecs_query_memo` | h03_stale_handles | 367.76 ms | 380.80 ms | 1.019 | 0.933–1.429 | 2883584 → 2883584 |
| `gds_ecs_query_memo` | h04_recent_locality | 328.81 ms | 307.41 ms | 0.913 | 0.798–1.130 | 2883584 → 2883584 |
| `gds_ecs_query_memo` | h05_sparse_component | 318.77 ms | 322.30 ms | 1.090 | 0.897–1.255 | 11534336 → 11534336 |
| `gds_ecs_reference` | w01_steady_uniform | 1285.73 ms | 813.86 ms | 0.597 | 0.567–0.670 | 4311208 → 4153664 |
| `gds_ecs_reference` | w02_query_heavy | 1325.42 ms | 1183.21 ms | 0.867 | 0.752–0.935 | 8590592 → 8255704 |
| `gds_ecs_reference` | w03_structural_churn | 660.79 ms | 337.81 ms | 0.496 | 0.485–0.573 | 2504624 → 2430424 |
| `gds_ecs_reference` | w04_random_access | 1038.79 ms | 913.99 ms | 0.850 | 0.752–0.905 | 17208488 → 16497152 |
| `gds_ecs_reference` | w05_small_world | 46.85 ms | 61.08 ms | 1.132 | 0.489–1.616 | 240136 → 232208 |
| `gds_ecs_reference` | w06_point_narrow | 509.83 ms | 338.64 ms | 0.721 | 0.569–0.972 | 17208488 → 16497152 |
| `gds_ecs_reference` | w07_point_wide | 345.31 ms | 247.42 ms | 0.713 | 0.611–0.813 | 17208488 → 16497152 |
| `gds_ecs_reference` | h01_zipf_hotspot | 1255.76 ms | 1021.04 ms | 0.807 | 0.684–0.844 | 12184976 → 12898664 |
| `gds_ecs_reference` | h02_bursty_spawn | 2079.40 ms | 1269.96 ms | 0.602 | 0.588–0.629 | 21858680 → 25165896 |
| `gds_ecs_reference` | h03_stale_handles | 1663.95 ms | 929.99 ms | 0.595 | 0.468–0.715 | 5037976 → 4880432 |
| `gds_ecs_reference` | h04_recent_locality | 367.94 ms | 296.23 ms | 0.781 | 0.688–0.977 | 3247808 → 3435752 |
| `gds_ecs_reference` | h05_sparse_component | 1531.79 ms | 708.27 ms | 0.478 | 0.449–0.492 | 17217416 → 16506080 |
| `gds_ecs_soa` | w01_steady_uniform | 326.53 ms | 334.12 ms | 1.100 | 0.891–1.280 | 2883584 → 2883584 |
| `gds_ecs_soa` | w02_query_heavy | 992.39 ms | 955.10 ms | 0.953 | 0.859–1.295 | 5767168 → 5767168 |
| `gds_ecs_soa` | w03_structural_churn | 149.12 ms | 146.05 ms | 0.976 | 0.775–1.396 | 1441792 → 1441792 |
| `gds_ecs_soa` | w04_random_access | 756.73 ms | 882.94 ms | 1.153 | 0.970–1.247 | 11534336 → 11534336 |
| `gds_ecs_soa` | w05_small_world | 36.92 ms | 36.31 ms | 1.044 | 0.679–1.297 | 181248 → 181248 |
| `gds_ecs_soa` | w06_point_narrow | 286.38 ms | 254.18 ms | 0.966 | 0.690–0.999 | 11534336 → 11534336 |
| `gds_ecs_soa` | w07_point_wide | 194.94 ms | 186.74 ms | 1.012 | 0.792–1.151 | 11534336 → 11534336 |
| `gds_ecs_soa` | h01_zipf_hotspot | 754.48 ms | 752.22 ms | 1.067 | 0.931–1.208 | 11534336 → 11534336 |
| `gds_ecs_soa` | h02_bursty_spawn | 817.92 ms | 793.75 ms | 1.004 | 0.901–1.072 | 23068704 → 23068704 |
| `gds_ecs_soa` | h03_stale_handles | 310.13 ms | 328.84 ms | 1.004 | 0.888–1.171 | 2883584 → 2883584 |
| `gds_ecs_soa` | h04_recent_locality | 241.06 ms | 247.10 ms | 1.053 | 0.936–1.150 | 2883584 → 2883584 |
| `gds_ecs_soa` | h05_sparse_component | 319.17 ms | 312.64 ms | 0.941 | 0.882–1.176 | 11534336 → 11534336 |
| `gds_ecs_sparse_set` | w01_steady_uniform | 446.96 ms | 420.41 ms | 0.941 | 0.822–1.319 | 4785152 → 4785152 |
| `gds_ecs_sparse_set` | w02_query_heavy | 1056.64 ms | 1149.56 ms | 0.957 | 0.929–1.120 | 9568256 → 9568256 |
| `gds_ecs_sparse_set` | w03_structural_churn | 144.72 ms | 148.77 ms | 1.080 | 0.630–1.850 | 2130176 → 2130176 |
| `gds_ecs_sparse_set` | w04_random_access | 1021.82 ms | 1034.83 ms | 0.995 | 0.911–1.314 | 18874368 → 18874368 |
| `gds_ecs_sparse_set` | w05_small_world | 36.13 ms | 35.69 ms | 0.964 | 0.904–1.308 | 267264 → 267264 |
| `gds_ecs_sparse_set` | w06_point_narrow | 391.43 ms | 405.94 ms | 1.037 | 0.949–1.144 | 24117248 → 24117248 |
| `gds_ecs_sparse_set` | w07_point_wide | 334.71 ms | 329.35 ms | 1.015 | 0.806–1.110 | 24117248 → 24117248 |
| `gds_ecs_sparse_set` | h01_zipf_hotspot | 714.29 ms | 720.15 ms | 0.997 | 0.906–1.088 | 15990784 → 15990784 |
| `gds_ecs_sparse_set` | h02_bursty_spawn | 1063.74 ms | 1089.41 ms | 1.056 | 0.859–1.221 | 24117280 → 24117280 |
| `gds_ecs_sparse_set` | h03_stale_handles | 444.99 ms | 483.88 ms | 1.056 | 0.857–1.391 | 4784128 → 4784128 |
| `gds_ecs_sparse_set` | h04_recent_locality | 290.83 ms | 285.55 ms | 0.995 | 0.919–1.167 | 4325376 → 4325376 |
| `gds_ecs_sparse_set` | h05_sparse_component | 389.17 ms | 351.35 ms | 0.951 | 0.748–1.192 | 16056320 → 16056320 |
| `gds_spatial_axis_sorted` | s01_steady_uniform | 595.70 ms | 549.60 ms | 0.998 | 0.765–1.134 | 789152 → 789152 |
| `gds_spatial_axis_sorted` | s02_dense_clustered | 1253.97 ms | 1213.18 ms | 0.939 | 0.780–1.002 | 1578304 → 1578304 |
| `gds_spatial_axis_sorted` | s03_wide_radius | 505.53 ms | 487.12 ms | 0.936 | 0.819–1.028 | 838304 → 838304 |
| `gds_spatial_axis_sorted` | s04_teleport | 637.35 ms | 616.05 ms | 0.967 | 0.902–1.259 | 789152 → 789152 |
| `gds_spatial_axis_sorted` | s05_small_world | 201.71 ms | 164.19 ms | 0.814 | 0.758–1.118 | 61644 → 61644 |
| `gds_spatial_axis_sorted` | hs01_rewind_all_moving | 973.35 ms | 909.36 ms | 0.938 | 0.837–1.029 | 8405764 → 8405764 |
| `gds_spatial_axis_sorted` | hs02_rewind_few_moving | 749.09 ms | 778.40 ms | 1.039 | 0.960–1.103 | 7771364 → 7771364 |
| `gds_spatial_axis_sorted` | hs03_knn_heavy | 2226.71 ms | 2193.74 ms | 0.981 | 0.934–1.056 | 1023304 → 1023304 |
| `gds_spatial_axis_sorted` | hs04_flat_world | 494.54 ms | 455.66 ms | 0.921 | 0.873–1.054 | 949576 → 949576 |
| `gds_spatial_axis_sorted` | hs05_spawn_churn | 487.46 ms | 466.41 ms | 1.043 | 0.756–1.076 | 1563952 → 1563952 |
| `gds_spatial_axis_sorted` | hs06_mixed_reach | 832.61 ms | 714.70 ms | 0.889 | 0.717–1.082 | 789152 → 789152 |
| `gds_spatial_axis_sorted` | hs07_crowd | 1371.18 ms | 1228.09 ms | 0.926 | 0.785–1.030 | 1676608 → 1676608 |
| `gds_spatial_axis_sorted` | hs08_crowd_rollback | 1490.58 ms | 1499.85 ms | 1.066 | 0.930–1.178 | 13724740 → 13724740 |
| `gds_spatial_brute_force` | s01_steady_uniform | 2132.99 ms | 2025.27 ms | 0.943 | 0.750–1.105 | 780192 → 780192 |
| `gds_spatial_brute_force` | s02_dense_clustered | 1846.70 ms | 1714.99 ms | 0.873 | 0.826–1.072 | 1560192 → 1560192 |
| `gds_spatial_brute_force` | s03_wide_radius | 474.25 ms | 474.32 ms | 0.917 | 0.797–1.087 | 780192 → 780192 |
| `gds_spatial_brute_force` | s04_teleport | 1974.76 ms | 1685.95 ms | 0.870 | 0.759–0.982 | 780192 → 780192 |
| `gds_spatial_brute_force` | s05_small_world | 286.32 ms | 322.73 ms | 1.054 | 0.836–1.403 | 58692 → 58692 |
| `gds_spatial_brute_force` | hs01_rewind_all_moving | 1539.02 ms | 1366.56 ms | 0.930 | 0.638–1.285 | 6627124 → 6627124 |
| `gds_spatial_brute_force` | hs02_rewind_few_moving | 1248.07 ms | 1100.09 ms | 0.831 | 0.787–0.982 | 6045049 → 6045049 |
| `gds_spatial_brute_force` | hs03_knn_heavy | 2413.95 ms | 1798.97 ms | 0.755 | 0.705–0.792 | 975192 → 975192 |
| `gds_spatial_brute_force` | hs04_flat_world | 921.30 ms | 885.72 ms | 0.920 | 0.861–0.969 | 975192 → 975192 |
| `gds_spatial_brute_force` | hs05_spawn_churn | 3890.40 ms | 3944.08 ms | 1.008 | 0.952–1.070 | 3104592 → 3104592 |
| `gds_spatial_brute_force` | hs06_mixed_reach | 1313.65 ms | 1178.55 ms | 0.777 | 0.717–1.010 | 780192 → 780192 |
| `gds_spatial_brute_force` | hs07_crowd | 1749.11 ms | 1654.86 ms | 0.950 | 0.852–0.983 | 1560192 → 1560192 |
| `gds_spatial_brute_force` | hs08_crowd_rollback | 2306.55 ms | 2007.79 ms | 0.979 | 0.686–1.062 | 11356638 → 11356638 |
| `gds_spatial_cell_rows` | s01_steady_uniform | 128.08 ms | 159.09 ms | 1.113 | 0.787–1.667 | 1317108 → 1317108 |
| `gds_spatial_cell_rows` | s02_dense_clustered | 567.79 ms | 587.76 ms | 1.056 | 0.808–1.244 | 6447872 → 6447872 |
| `gds_spatial_cell_rows` | s03_wide_radius | 105.78 ms | 112.34 ms | 1.006 | 0.875–1.098 | 686144 → 686144 |
| `gds_spatial_cell_rows` | s04_teleport | 144.76 ms | 143.63 ms | 1.051 | 0.875–1.170 | 1314036 → 1314036 |
| `gds_spatial_cell_rows` | s05_small_world | 60.69 ms | 62.99 ms | 1.064 | 0.832–1.220 | 91892 → 91892 |
| `gds_spatial_cell_rows` | hs01_rewind_all_moving | 302.60 ms | 280.35 ms | 0.947 | 0.883–1.268 | 8828020 → 8828020 |
| `gds_spatial_cell_rows` | hs02_rewind_few_moving | 128.95 ms | 124.56 ms | 0.997 | 0.888–1.315 | 8185472 → 8185472 |
| `gds_spatial_cell_rows` | hs03_knn_heavy | 637.67 ms | 622.60 ms | 0.973 | 0.884–1.417 | 1488252 → 1488252 |
| `gds_spatial_cell_rows` | hs04_flat_world | 100.35 ms | 91.93 ms | 0.910 | 0.858–1.203 | 856236 → 856236 |
| `gds_spatial_cell_rows` | hs05_spawn_churn | 159.22 ms | 147.71 ms | 0.956 | 0.736–1.036 | 2325700 → 2325700 |
| `gds_spatial_cell_rows` | hs06_mixed_reach | 165.43 ms | 157.69 ms | 0.992 | 0.634–1.166 | 676500 → 676500 |
| `gds_spatial_cell_rows` | hs07_crowd | 540.18 ms | 532.12 ms | 0.985 | 0.948–1.063 | 6447872 → 6447872 |
| `gds_spatial_cell_rows` | hs08_crowd_rollback | 711.91 ms | 669.42 ms | 0.929 | 0.899–1.081 | 18455168 → 18455168 |
| `gds_spatial_cell_sorted` | s01_steady_uniform | 137.33 ms | 159.72 ms | 0.964 | 0.887–1.393 | 1317108 → 1317108 |
| `gds_spatial_cell_sorted` | s02_dense_clustered | 594.48 ms | 556.90 ms | 0.937 | 0.879–1.037 | 6447872 → 6447872 |
| `gds_spatial_cell_sorted` | s03_wide_radius | 137.53 ms | 112.22 ms | 0.782 | 0.611–1.025 | 686144 → 686144 |
| `gds_spatial_cell_sorted` | s04_teleport | 156.94 ms | 167.65 ms | 1.049 | 0.871–1.253 | 1314036 → 1314036 |
| `gds_spatial_cell_sorted` | s05_small_world | 79.83 ms | 80.55 ms | 0.997 | 0.938–1.069 | 91892 → 91892 |
| `gds_spatial_cell_sorted` | hs01_rewind_all_moving | 311.89 ms | 300.44 ms | 0.909 | 0.751–1.342 | 8828020 → 8828020 |
| `gds_spatial_cell_sorted` | hs02_rewind_few_moving | 165.38 ms | 140.57 ms | 0.850 | 0.764–1.167 | 8185472 → 8185472 |
| `gds_spatial_cell_sorted` | hs03_knn_heavy | 759.25 ms | 754.19 ms | 1.009 | 0.782–1.038 | 1488252 → 1488252 |
| `gds_spatial_cell_sorted` | hs04_flat_world | 96.59 ms | 94.25 ms | 0.976 | 0.859–1.355 | 856236 → 856236 |
| `gds_spatial_cell_sorted` | hs05_spawn_churn | 144.72 ms | 143.31 ms | 0.996 | 0.713–1.115 | 2325700 → 2325700 |
| `gds_spatial_cell_sorted` | hs06_mixed_reach | 172.98 ms | 158.95 ms | 0.886 | 0.801–0.973 | 676500 → 676500 |
| `gds_spatial_cell_sorted` | hs07_crowd | 517.67 ms | 549.04 ms | 1.194 | 0.856–1.284 | 6447872 → 6447872 |
| `gds_spatial_cell_sorted` | hs08_crowd_rollback | 658.71 ms | 689.21 ms | 0.989 | 0.948–1.267 | 18455168 → 18455168 |
| `gds_spatial_delta_grid` | s01_steady_uniform | 214.14 ms | 166.87 ms | 0.848 | 0.659–1.004 | 2327960 → 2327960 |
| `gds_spatial_delta_grid` | s02_dense_clustered | 585.83 ms | 565.03 ms | 0.982 | 0.886–1.088 | 12197328 → 12197328 |
| `gds_spatial_delta_grid` | s03_wide_radius | 141.32 ms | 127.10 ms | 0.894 | 0.713–1.168 | 1047600 → 1047600 |
| `gds_spatial_delta_grid` | s04_teleport | 187.68 ms | 177.53 ms | 0.844 | 0.782–1.049 | 2324888 → 2324888 |
| `gds_spatial_delta_grid` | s05_small_world | 94.29 ms | 90.35 ms | 0.954 | 0.272–1.352 | 158100 → 158100 |
| `gds_spatial_delta_grid` | hs01_rewind_all_moving | 242.50 ms | 242.64 ms | 0.957 | 0.671–1.101 | 9907060 → 9907060 |
| `gds_spatial_delta_grid` | hs02_rewind_few_moving | 38.31 ms | 28.28 ms | 0.781 | 0.694–0.871 | 3681538 → 3681538 |
| `gds_spatial_delta_grid` | hs03_knn_heavy | 833.44 ms | 826.70 ms | 0.988 | 0.854–1.225 | 2589104 → 2589104 |
| `gds_spatial_delta_grid` | hs04_flat_world | 114.29 ms | 122.33 ms | 0.916 | 0.746–1.232 | 1335824 → 1335824 |
| `gds_spatial_delta_grid` | hs05_spawn_churn | 176.27 ms | 176.89 ms | 1.026 | 0.812–1.275 | 4409352 → 4409352 |
| `gds_spatial_delta_grid` | hs06_mixed_reach | 194.39 ms | 168.70 ms | 0.874 | 0.818–1.055 | 1040600 → 1040600 |
| `gds_spatial_delta_grid` | hs07_crowd | 552.88 ms | 550.55 ms | 1.034 | 0.874–1.166 | 12197328 → 12197328 |
| `gds_spatial_delta_grid` | hs08_crowd_rollback | 706.29 ms | 648.25 ms | 0.900 | 0.729–1.352 | 24218418 → 24218418 |
| `gds_spatial_grid_ring_knn` | s01_steady_uniform | 150.45 ms | 133.09 ms | 0.919 | 0.607–0.928 | 1150912 → 1150912 |
| `gds_spatial_grid_ring_knn` | s02_dense_clustered | 752.06 ms | 710.66 ms | 0.973 | 0.841–0.992 | 6029516 → 6029516 |
| `gds_spatial_grid_ring_knn` | s03_wide_radius | 175.43 ms | 158.75 ms | 0.961 | 0.650–0.994 | 501708 → 501708 |
| `gds_spatial_grid_ring_knn` | s04_teleport | 176.19 ms | 183.77 ms | 1.038 | 0.970–1.156 | 1150912 → 1150912 |
| `gds_spatial_grid_ring_knn` | s05_small_world | 64.01 ms | 58.55 ms | 0.926 | 0.846–1.588 | 76768 → 76768 |
| `gds_spatial_grid_ring_knn` | hs01_rewind_all_moving | 354.68 ms | 344.67 ms | 1.010 | 0.867–1.332 | 8127536 → 8127536 |
| `gds_spatial_grid_ring_knn` | hs02_rewind_few_moving | 53.03 ms | 53.84 ms | 1.033 | 0.698–1.110 | 7470836 → 7470836 |
| `gds_spatial_grid_ring_knn` | hs03_knn_heavy | 313.00 ms | 357.60 ms | 1.098 | 1.079–1.211 | 1276360 → 1276360 |
| `gds_spatial_grid_ring_knn` | hs04_flat_world | 114.75 ms | 102.38 ms | 0.899 | 0.678–1.062 | 654648 → 654648 |
| `gds_spatial_grid_ring_knn` | hs05_spawn_churn | 153.85 ms | 117.15 ms | 0.877 | 0.699–1.026 | 2640912 → 2640912 |
| `gds_spatial_grid_ring_knn` | hs06_mixed_reach | 230.96 ms | 240.67 ms | 1.054 | 0.651–1.120 | 504160 → 504160 |
| `gds_spatial_grid_ring_knn` | hs07_crowd | 863.68 ms | 788.65 ms | 0.935 | 0.860–1.162 | 6029516 → 6029516 |
| `gds_spatial_grid_ring_knn` | hs08_crowd_rollback | 875.92 ms | 939.19 ms | 0.922 | 0.873–1.207 | 17535404 → 17535404 |
| `gds_spatial_grid_undo_log` | s01_steady_uniform | 151.58 ms | 152.85 ms | 1.025 | 0.609–1.084 | 1156992 → 1156992 |
| `gds_spatial_grid_undo_log` | s02_dense_clustered | 763.06 ms | 769.27 ms | 0.979 | 0.879–1.129 | 6127756 → 6127756 |
| `gds_spatial_grid_undo_log` | s03_wide_radius | 156.89 ms | 153.97 ms | 1.033 | 0.840–1.093 | 526028 → 526028 |
| `gds_spatial_grid_undo_log` | s04_teleport | 197.40 ms | 185.72 ms | 0.941 | 0.886–1.205 | 1153920 → 1153920 |
| `gds_spatial_grid_undo_log` | s05_small_world | 91.85 ms | 82.80 ms | 0.951 | 0.784–1.120 | 79776 → 79776 |
| `gds_spatial_grid_undo_log` | hs01_rewind_all_moving | 405.96 ms | 491.26 ms | 1.342 | 0.749–1.629 | 11717160 → 11717160 |
| `gds_spatial_grid_undo_log` | hs02_rewind_few_moving | 34.93 ms | 22.68 ms | 0.799 | 0.478–0.965 | 3059658 → 3059658 |
| `gds_spatial_grid_undo_log` | hs03_knn_heavy | 867.18 ms | 781.34 ms | 0.906 | 0.756–0.977 | 1288136 → 1288136 |
| `gds_spatial_grid_undo_log` | hs04_flat_world | 128.96 ms | 132.90 ms | 0.953 | 0.729–1.468 | 656120 → 656120 |
| `gds_spatial_grid_undo_log` | hs05_spawn_churn | 131.41 ms | 122.66 ms | 0.916 | 0.664–1.224 | 2642384 → 2642384 |
| `gds_spatial_grid_undo_log` | hs06_mixed_reach | 237.53 ms | 228.23 ms | 0.961 | 0.819–1.016 | 516384 → 516384 |
| `gds_spatial_grid_undo_log` | hs07_crowd | 769.50 ms | 815.15 ms | 0.980 | 0.909–1.125 | 6127756 → 6127756 |
| `gds_spatial_grid_undo_log` | hs08_crowd_rollback | 1121.97 ms | 1061.87 ms | 0.935 | 0.863–1.136 | 31393416 → 31393416 |
| `gds_spatial_morton_lbvh` | s01_steady_uniform | 146.13 ms | 151.66 ms | 1.022 | 0.868–1.201 | 937312 → 937312 |
| `gds_spatial_morton_lbvh` | s02_dense_clustered | 480.04 ms | 484.12 ms | 1.053 | 0.842–1.316 | 1873792 → 1873792 |
| `gds_spatial_morton_lbvh` | s03_wide_radius | 143.66 ms | 128.05 ms | 0.891 | 0.870–1.185 | 939808 → 939808 |
| `gds_spatial_morton_lbvh` | s04_teleport | 145.16 ms | 147.21 ms | 1.002 | 0.766–1.059 | 937248 → 937248 |
| `gds_spatial_morton_lbvh` | s05_small_world | 89.92 ms | 78.45 ms | 0.825 | 0.751–1.071 | 73004 → 73004 |
| `gds_spatial_morton_lbvh` | hs01_rewind_all_moving | 274.58 ms | 280.75 ms | 1.065 | 0.883–1.216 | 9115776 → 9115776 |
| `gds_spatial_morton_lbvh` | hs02_rewind_few_moving | 133.93 ms | 118.05 ms | 0.878 | 0.688–1.226 | 8478640 → 8478640 |
| `gds_spatial_morton_lbvh` | hs03_knn_heavy | 471.47 ms | 396.46 ms | 0.840 | 0.698–1.211 | 1173832 → 1173832 |
| `gds_spatial_morton_lbvh` | hs04_flat_world | 169.08 ms | 157.23 ms | 0.874 | 0.812–1.180 | 1173384 → 1173384 |
| `gds_spatial_morton_lbvh` | hs05_spawn_churn | 138.13 ms | 141.68 ms | 1.026 | 0.764–1.239 | 1712048 → 1712048 |
| `gds_spatial_morton_lbvh` | hs06_mixed_reach | 219.01 ms | 213.10 ms | 0.991 | 0.742–1.299 | 939616 → 939616 |
| `gds_spatial_morton_lbvh` | hs07_crowd | 464.38 ms | 512.94 ms | 0.913 | 0.723–1.678 | 1873792 → 1873792 |
| `gds_spatial_morton_lbvh` | hs08_crowd_rollback | 622.90 ms | 640.41 ms | 0.932 | 0.820–1.216 | 14669896 → 14669896 |
| `gds_spatial_morton_sorted` | s01_steady_uniform | 691.74 ms | 653.03 ms | 0.947 | 0.858–1.100 | 1066144 → 1066144 |
| `gds_spatial_morton_sorted` | s02_dense_clustered | 1219.00 ms | 1111.80 ms | 0.912 | 0.858–0.978 | 2218304 → 2218304 |
| `gds_spatial_morton_sorted` | s03_wide_radius | 417.50 ms | 392.89 ms | 0.952 | 0.777–1.022 | 1084576 → 1084576 |
| `gds_spatial_morton_sorted` | s04_teleport | 655.78 ms | 716.88 ms | 1.058 | 1.011–1.226 | 1063072 → 1063072 |
| `gds_spatial_morton_sorted` | s05_small_world | 491.31 ms | 531.40 ms | 1.095 | 0.786–1.283 | 82572 → 82572 |
| `gds_spatial_morton_sorted` | hs01_rewind_all_moving | 742.66 ms | 833.06 ms | 1.137 | 0.798–1.161 | 9058884 → 9058884 |
| `gds_spatial_morton_sorted` | hs02_rewind_few_moving | 603.63 ms | 649.59 ms | 1.031 | 0.970–1.177 | 8451401 → 8451401 |
| `gds_spatial_morton_sorted` | hs03_knn_heavy | 4399.01 ms | 4767.79 ms | 1.070 | 1.056–1.162 | 1337288 → 1337288 |
| `gds_spatial_morton_sorted` | hs04_flat_world | 368.91 ms | 406.24 ms | 1.032 | 0.994–1.230 | 1326536 → 1326536 |
| `gds_spatial_morton_sorted` | hs05_spawn_churn | 400.02 ms | 391.08 ms | 0.957 | 0.739–1.233 | 1836336 → 1836336 |
| `gds_spatial_morton_sorted` | hs06_mixed_reach | 583.54 ms | 601.25 ms | 1.008 | 0.894–1.086 | 1072288 → 1072288 |
| `gds_spatial_morton_sorted` | hs07_crowd | 1141.51 ms | 990.26 ms | 0.889 | 0.801–0.919 | 2218304 → 2218304 |
| `gds_spatial_morton_sorted` | hs08_crowd_rollback | 1477.56 ms | 1552.15 ms | 1.063 | 0.956–1.100 | 14577902 → 14577902 |
| `gds_spatial_spatial_hash` | s01_steady_uniform | 389.85 ms | 382.30 ms | 0.951 | 0.897–1.115 | 6871456 → 6871456 |
| `gds_spatial_spatial_hash` | s02_dense_clustered | 777.06 ms | 738.17 ms | 1.023 | 0.889–1.139 | 1356608 → 1356608 |
| `gds_spatial_spatial_hash` | s03_wide_radius | 159.31 ms | 153.96 ms | 0.984 | 0.834–1.236 | 620960 → 620960 |
| `gds_spatial_spatial_hash` | s04_teleport | 457.91 ms | 438.97 ms | 0.964 | 0.793–1.058 | 6871456 → 6871456 |
| `gds_spatial_spatial_hash` | s05_small_world | 267.81 ms | 260.20 ms | 0.994 | 0.763–1.208 | 436716 → 436716 |
| `gds_spatial_spatial_hash` | hs01_rewind_all_moving | 558.22 ms | 532.60 ms | 0.954 | 0.808–1.223 | 9709696 → 9709696 |
| `gds_spatial_spatial_hash` | hs02_rewind_few_moving | 114.58 ms | 113.89 ms | 1.021 | 0.891–1.260 | 7992480 → 7992480 |
| `gds_spatial_spatial_hash` | hs03_knn_heavy | 3344.48 ms | 3595.38 ms | 1.036 | 1.023–1.096 | 7016456 → 7016456 |
| `gds_spatial_spatial_hash` | hs04_flat_world | 159.21 ms | 146.56 ms | 1.007 | 0.873–1.296 | 1118216 → 1118216 |
| `gds_spatial_spatial_hash` | hs05_spawn_churn | 238.00 ms | 257.33 ms | 1.003 | 0.815–1.303 | 8599856 → 8599856 |
| `gds_spatial_spatial_hash` | hs06_mixed_reach | 244.65 ms | 257.08 ms | 1.041 | 0.843–1.105 | 629152 → 629152 |
| `gds_spatial_spatial_hash` | hs07_crowd | 993.86 ms | 1014.15 ms | 1.026 | 0.884–1.111 | 2732864 → 2732864 |
| `gds_spatial_spatial_hash` | hs08_crowd_rollback | 1265.99 ms | 1324.65 ms | 1.038 | 0.933–1.182 | 12820840 → 12820840 |
| `gds_spatial_uniform_grid` | s01_steady_uniform | 151.81 ms | 159.53 ms | 0.939 | 0.869–1.136 | 1156992 → 1156992 |
| `gds_spatial_uniform_grid` | s02_dense_clustered | 809.99 ms | 779.99 ms | 1.026 | 0.891–1.143 | 6127756 → 6127756 |
| `gds_spatial_uniform_grid` | s03_wide_radius | 152.11 ms | 178.64 ms | 1.116 | 1.032–1.506 | 526028 → 526028 |
| `gds_spatial_uniform_grid` | s04_teleport | 195.54 ms | 201.24 ms | 0.832 | 0.723–1.304 | 1153920 → 1153920 |
| `gds_spatial_uniform_grid` | s05_small_world | 90.40 ms | 96.08 ms | 1.040 | 0.878–1.065 | 79776 → 79776 |
| `gds_spatial_uniform_grid` | hs01_rewind_all_moving | 374.25 ms | 404.30 ms | 0.935 | 0.764–1.438 | 8127472 → 8127472 |
| `gds_spatial_uniform_grid` | hs02_rewind_few_moving | 41.98 ms | 46.85 ms | 1.307 | 0.766–1.652 | 7470772 → 7470772 |
| `gds_spatial_uniform_grid` | hs03_knn_heavy | 791.33 ms | 812.00 ms | 0.992 | 0.923–1.586 | 1288136 → 1288136 |
| `gds_spatial_uniform_grid` | hs04_flat_world | 116.18 ms | 112.26 ms | 0.971 | 0.804–1.202 | 656120 → 656120 |
| `gds_spatial_uniform_grid` | hs05_spawn_churn | 121.87 ms | 128.63 ms | 1.053 | 0.869–1.402 | 2642384 → 2642384 |
| `gds_spatial_uniform_grid` | hs06_mixed_reach | 213.29 ms | 227.54 ms | 1.062 | 0.849–1.117 | 516384 → 516384 |
| `gds_spatial_uniform_grid` | hs07_crowd | 766.69 ms | 738.64 ms | 0.954 | 0.903–1.091 | 6127756 → 6127756 |
| `gds_spatial_uniform_grid` | hs08_crowd_rollback | 845.17 ms | 852.46 ms | 0.970 | 0.956–1.067 | 17535340 → 17535340 |
