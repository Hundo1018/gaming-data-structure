# Ranking stability: C against C++, on step_ns_p50 and total_ns

Metric `step_ns_p50`, medians over the rounds in `benchmarks/port_timing.json`. Excluded: gds_ecs_reference, gds_spatial_brute_force, gds_ecs_broken_recycle, gds_ecs_query_memo. A flip is robust when, in both builds, the two candidates' per-round ranges do not overlap.

Median tau-b 0.822, lowest 0.378; 86 of 837 candidate pairs change order, 0 of them robustly.

Resolution, the part of the comparison the noise lets through: C++ separates 414 pairs and C 438 (per-round ranges that do not overlap); of the pairs both separate, 372 are in the same order and 0 in opposite orders. 2 pairs that only one build separates come out the other way in the other build, inside its noise.

Run-to-run spread, (max - min) / median over the rounds per candidate and workload: C++ median 29.4% (p25 19.7%, p75 42.3%); C median 29.3% (p25 20.5%, p75 42.3%).

| workload | candidates | tau-b | flipped pairs (a ratio → b ratio) |
|---|---:|---:|---|
| h01_zipf_hotspot | 7 | 0.905 | bitset_soa vs sparse_set 0.817 → 1.061 |
| h02_bursty_spawn | 7 | 1.000 | — |
| h03_stale_handles | 7 | 0.714 | archetype vs bitset_soa 1.020 → 0.983; archetype vs grouped_sparse_set 0.979 → 1.001; bitset_soa vs grouped_sparse_set 0.960 → 1.019 |
| h04_recent_locality | 7 | 0.524 | aos vs archetype 1.008 → 0.842; aos vs grouped_sparse_set 0.985 → 1.070; archetype vs grouped_sparse_set 0.977 → 1.271; archetype vs sparse_set 0.887 → 1.153; bitset_soa vs fused_archetype 0.990 → 1.012 |
| h05_sparse_component | 7 | 0.810 | archetype vs fused_archetype 1.044 → 0.938; bitset_soa vs grouped_sparse_set 1.046 → 0.933 |
| hs01_rewind_all_moving | 10 | 0.956 | grid_ring_knn vs uniform_grid 1.006 → 0.938 |
| hs02_rewind_few_moving | 10 | 0.911 | delta_grid vs uniform_grid 1.165 → 0.816; grid_undo_log vs uniform_grid 1.048 → 0.620 |
| hs03_knn_heavy | 10 | 0.911 | delta_grid vs grid_undo_log 0.934 → 1.073; grid_undo_log vs uniform_grid 1.135 → 0.972 |
| hs04_flat_world | 10 | 0.956 | delta_grid vs uniform_grid 0.995 → 1.033 |
| hs05_spawn_churn | 10 | 0.733 | cell_rows vs grid_ring_knn 0.920 → 1.267; cell_sorted vs grid_ring_knn 0.893 → 1.218; cell_sorted vs morton_lbvh 1.132 → 0.964; grid_ring_knn vs grid_undo_log 1.299 → 0.957; grid_ring_knn vs morton_lbvh 1.267 → 0.792; grid_ring_knn vs uniform_grid 1.375 → 0.966 |
| hs06_mixed_reach | 10 | 0.956 | cell_rows vs cell_sorted 0.948 → 1.020 |
| hs07_crowd | 10 | 0.822 | cell_sorted vs delta_grid 0.908 → 1.007; grid_ring_knn vs grid_undo_log 1.126 → 0.901; grid_ring_knn vs uniform_grid 1.081 → 0.965; grid_undo_log vs uniform_grid 0.960 → 1.071 |
| hs08_crowd_rollback | 10 | 0.822 | cell_rows vs cell_sorted 1.084 → 0.944; cell_rows vs morton_lbvh 1.183 → 0.915; cell_sorted vs morton_lbvh 1.091 → 0.969; grid_ring_knn vs grid_undo_log 0.892 → 1.007 |
| s01_steady_uniform | 10 | 0.378 | cell_rows vs delta_grid 0.582 → 1.016; cell_rows vs grid_ring_knn 0.834 → 1.209; cell_rows vs grid_undo_log 0.854 → 1.082; cell_rows vs morton_lbvh 0.926 → 1.109; cell_rows vs uniform_grid 0.845 → 1.015; cell_sorted vs delta_grid 0.616 → 1.089; cell_sorted vs grid_ring_knn 0.883 → 1.296; cell_sorted vs grid_undo_log 0.904 → 1.161; cell_sorted vs morton_lbvh 0.980 → 1.189; cell_sorted vs uniform_grid 0.894 → 1.089; delta_grid vs uniform_grid 1.453 → 1.000; grid_ring_knn vs grid_undo_log 1.025 → 0.895; grid_ring_knn vs morton_lbvh 1.111 → 0.917; grid_ring_knn vs uniform_grid 1.013 → 0.840 |
| s02_dense_clustered | 10 | 0.867 | cell_rows vs cell_sorted 0.960 → 1.116; cell_sorted vs delta_grid 1.100 → 0.984; spatial_hash vs uniform_grid 0.966 → 1.023 |
| s03_wide_radius | 10 | 0.600 | cell_rows vs cell_sorted 0.749 → 1.101; cell_sorted vs morton_lbvh 1.030 → 0.767; delta_grid vs morton_lbvh 1.134 → 0.900; delta_grid vs uniform_grid 1.000 → 0.645; grid_ring_knn vs grid_undo_log 1.074 → 0.938; grid_ring_knn vs spatial_hash 1.086 → 0.943; grid_ring_knn vs uniform_grid 1.112 → 0.804; grid_undo_log vs uniform_grid 1.035 → 0.857; spatial_hash vs uniform_grid 1.023 → 0.852 |
| s04_teleport | 10 | 0.822 | cell_sorted vs morton_lbvh 1.061 → 0.985; delta_grid vs grid_ring_knn 1.212 → 0.943; delta_grid vs grid_undo_log 1.117 → 0.896; delta_grid vs uniform_grid 1.066 → 0.825 |
| s05_small_world | 10 | 0.867 | cell_rows vs grid_ring_knn 0.922 → 1.081; cell_sorted vs morton_lbvh 0.931 → 1.031; delta_grid vs uniform_grid 1.026 → 0.872 |
| w01_steady_uniform | 7 | 0.810 | archetype vs grouped_sparse_set 0.793 → 1.043; grouped_sparse_set vs soa 1.025 → 0.852 |
| w02_query_heavy | 7 | 0.905 | aos vs sparse_set 1.022 → 0.967 |
| w03_structural_churn | 7 | 0.524 | aos vs soa 1.185 → 0.923; aos vs sparse_set 1.227 → 0.939; archetype vs fused_archetype 1.083 → 0.972; archetype vs grouped_sparse_set 0.881 → 1.025; fused_archetype vs grouped_sparse_set 0.814 → 1.055 |
| w04_random_access | 7 | 0.619 | aos vs grouped_sparse_set 0.875 → 1.086; archetype vs soa 1.035 → 0.959; fused_archetype vs soa 1.055 → 0.974; grouped_sparse_set vs sparse_set 1.027 → 0.820 |
| w05_small_world | 7 | 0.714 | fused_archetype vs soa 1.006 → 0.933; fused_archetype vs sparse_set 1.045 → 0.995; grouped_sparse_set vs soa 1.093 → 0.956 |
| w06_point_narrow | 7 | 0.619 | aos vs soa 0.833 → 1.211; archetype vs grouped_sparse_set 1.140 → 0.853; archetype vs sparse_set 1.045 → 0.930; grouped_sparse_set vs sparse_set 0.917 → 1.090 |
| w07_point_wide | 7 | 0.810 | aos vs soa 1.051 → 0.909; archetype vs fused_archetype 1.031 → 0.965 |

## The same, on total_ns

Metric `total_ns`, medians over the rounds in `benchmarks/port_timing.json`. Excluded: gds_ecs_reference, gds_spatial_brute_force, gds_ecs_broken_recycle, gds_ecs_query_memo. A flip is robust when, in both builds, the two candidates' per-round ranges do not overlap.

Median tau-b 0.867, lowest 0.429; 75 of 837 candidate pairs change order, 0 of them robustly.

Resolution, the part of the comparison the noise lets through: C++ separates 450 pairs and C 458 (per-round ranges that do not overlap); of the pairs both separate, 400 are in the same order and 0 in opposite orders. 3 pairs that only one build separates come out the other way in the other build, inside its noise.

Run-to-run spread, (max - min) / median over the rounds per candidate and workload: C++ median 26.1% (p25 16.4%, p75 36.6%); C median 25.7% (p25 17.6%, p75 36.4%).

| workload | candidates | tau-b | flipped pairs (a ratio → b ratio) |
|---|---:|---:|---|
| h01_zipf_hotspot | 7 | 0.905 | archetype vs grouped_sparse_set 1.010 → 0.981 |
| h02_bursty_spawn | 7 | 0.905 | grouped_sparse_set vs soa 0.974 → 1.005 |
| h03_stale_handles | 7 | 0.810 | archetype vs bitset_soa 0.998 → 1.052; archetype vs grouped_sparse_set 0.929 → 1.021 |
| h04_recent_locality | 7 | 0.429 | aos vs archetype 1.059 → 0.929; aos vs fused_archetype 0.991 → 1.069; archetype vs fused_archetype 0.936 → 1.150; archetype vs grouped_sparse_set 0.974 → 1.241; archetype vs sparse_set 0.849 → 1.040; grouped_sparse_set vs soa 1.052 → 0.968 |
| h05_sparse_component | 7 | 1.000 | — |
| hs01_rewind_all_moving | 10 | 0.956 | cell_rows vs morton_lbvh 1.102 → 0.999 |
| hs02_rewind_few_moving | 10 | 0.956 | cell_rows vs morton_lbvh 0.963 → 1.055 |
| hs03_knn_heavy | 10 | 0.911 | delta_grid vs grid_undo_log 0.961 → 1.058; grid_undo_log vs uniform_grid 1.096 → 0.962 |
| hs04_flat_world | 10 | 0.867 | cell_rows vs cell_sorted 1.039 → 0.975; delta_grid vs grid_ring_knn 0.996 → 1.195; delta_grid vs uniform_grid 0.984 → 1.090 |
| hs05_spawn_churn | 10 | 0.778 | cell_sorted vs grid_ring_knn 0.941 → 1.223; grid_ring_knn vs grid_undo_log 1.171 → 0.955; grid_ring_knn vs morton_lbvh 1.114 → 0.827; grid_ring_knn vs uniform_grid 1.262 → 0.911; grid_undo_log vs uniform_grid 1.078 → 0.954 |
| hs06_mixed_reach | 10 | 0.911 | grid_ring_knn vs grid_undo_log 0.972 → 1.055; morton_lbvh vs uniform_grid 1.027 → 0.937 |
| hs07_crowd | 10 | 0.867 | cell_rows vs cell_sorted 1.043 → 0.969; grid_ring_knn vs grid_undo_log 1.122 → 0.967; morton_sorted vs spatial_hash 1.149 → 0.976 |
| hs08_crowd_rollback | 10 | 0.867 | axis_sorted vs morton_sorted 1.009 → 0.966; cell_rows vs cell_sorted 1.081 → 0.971; cell_sorted vs delta_grid 0.933 → 1.063 |
| s01_steady_uniform | 10 | 0.644 | cell_rows vs grid_ring_knn 0.851 → 1.195; cell_rows vs grid_undo_log 0.845 → 1.041; cell_rows vs morton_lbvh 0.876 → 1.049; cell_sorted vs grid_ring_knn 0.913 → 1.200; cell_sorted vs grid_undo_log 0.906 → 1.045; cell_sorted vs morton_lbvh 0.940 → 1.053; cell_sorted vs uniform_grid 0.905 → 1.001; grid_ring_knn vs morton_lbvh 1.029 → 0.878 |
| s02_dense_clustered | 10 | 0.822 | cell_rows vs cell_sorted 0.955 → 1.055; cell_rows vs delta_grid 0.969 → 1.040; cell_sorted vs delta_grid 1.015 → 0.986; grid_undo_log vs spatial_hash 0.982 → 1.042 |
| s03_wide_radius | 10 | 0.778 | cell_rows vs cell_sorted 0.769 → 1.001; grid_ring_knn vs uniform_grid 1.153 → 0.889; grid_undo_log vs spatial_hash 0.985 → 1.000; grid_undo_log vs uniform_grid 1.031 → 0.862; spatial_hash vs uniform_grid 1.047 → 0.862 |
| s04_teleport | 10 | 0.911 | delta_grid vs grid_ring_knn 1.065 → 0.966; grid_undo_log vs uniform_grid 1.009 → 0.923 |
| s05_small_world | 10 | 0.822 | cell_rows vs grid_ring_knn 0.948 → 1.076; cell_sorted vs morton_lbvh 0.888 → 1.027; delta_grid vs uniform_grid 1.043 → 0.940; grid_undo_log vs uniform_grid 1.016 → 0.862 |
| w01_steady_uniform | 7 | 0.810 | archetype vs bitset_soa 1.037 → 0.942; bitset_soa vs grouped_sparse_set 0.823 → 1.020 |
| w02_query_heavy | 7 | 0.905 | aos vs grouped_sparse_set 0.976 → 1.044 |
| w03_structural_churn | 7 | 0.524 | aos vs soa 1.121 → 0.914; aos vs sparse_set 1.155 → 0.897; archetype vs fused_archetype 1.097 → 0.910; fused_archetype vs grouped_sparse_set 0.858 → 1.091; soa vs sparse_set 1.030 → 0.982 |
| w04_random_access | 7 | 0.524 | aos vs grouped_sparse_set 0.958 → 1.021; archetype vs fused_archetype 0.956 → 1.011; archetype vs soa 1.047 → 0.944; bitset_soa vs soa 1.142 → 0.954; fused_archetype vs soa 1.095 → 0.933 |
| w05_small_world | 7 | 0.524 | archetype vs fused_archetype 0.859 → 1.024; archetype vs grouped_sparse_set 0.848 → 1.076; archetype vs soa 0.909 → 1.097; archetype vs sparse_set 0.928 → 1.116; fused_archetype vs grouped_sparse_set 0.987 → 1.051 |
| w06_point_narrow | 7 | 0.714 | aos vs soa 0.884 → 1.163; archetype vs grouped_sparse_set 1.013 → 0.857; grouped_sparse_set vs sparse_set 0.893 → 1.002 |
| w07_point_wide | 7 | 0.905 | archetype vs fused_archetype 1.095 → 0.909 |
