# Scaling report

- run: `sweep-20260904T081902Z`
- commit: `d54ea0064041f52f71f6a4b0a8b2d0710182ac4e`
- cpu: Intel(R) Xeon(R) Processor @ 2.10GHz
- compiler: c++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
- build flags: `-O2 -DNDEBUG -fno-omit-frame-pointer -ffp-contract=off -Wall -Wextra -march=native`
- populations: 1000, 2000, 4000, 8000, 16000, 32000, 64000, 128000 (base 8000)
- 3 repetitions per point plus 1 warmup, median repetition

`brute_force` is the positive control: it looks at every entity for every query, so its query exponent must come out at 1. It measures 1.01 with an r2 of 0.9999. An exponent elsewhere in this report is only worth reading because that one came out right.

Every exponent below is the least-squares slope of log(cost) against log(population). A cost that does not depend on the population gives 0; one that looks at everything gives 1. `r2` is the fit quality: below about 0.9 the points are not following a single power law over this range and the exponent should not be quoted.

The number of operations per step is held constant while the population grows. Without that, every candidate would measure linear regardless of what it does.

## Regimes

- **fixed_density** — The world grows as the cube root of the population, so density and the size of a query's answer stay constant. The exponent measured here is the cost of finding the answer, which is what a complexity claim is about.
- **fixed_world** — The world stays the same size, so density grows with the population and a query of fixed radius returns proportionally more entities. The exponent measured here includes the growth of the answer, not just the growth of the search.
- **population** — Population grows with the operation count per frame held fixed. The only regime for a track with no world.

## `ecs_mixed` · population

The same operation mix with integrate and one query per frame left on: the realistic shape of a frame rather than an isolated operation. Its exponent is expected to sit between the point-operation family's and 1, because a frame is a fixed number of operations plus two linear passes.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `archetype` | 0.696 | 0.9493 | 0.903 | 0.909 | 89.2 us | 2204.0 us | 24.7x |
| `soa` | 0.776 | 0.977 | 0.982 | 0.856 | 71.0 us | 2835.0 us | 39.9x |
| `aos` | 0.794 | 0.9731 | 1.048 | 0.857 | 68.3 us | 2964.3 us | 43.4x |
| `reference` | 0.795 | 0.9805 | 0.907 | 0.996 | 101.0 us | 3871.0 us | 38.3x |
| `sparse_set` | 0.805 | 0.9775 | 1.048 | 0.919 | 70.3 us | 3190.6 us | 45.4x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
archetype          89.2      92.8     114.5     175.6     304.6     630.5    1107.1    2204.0
  slope                      0.06      0.30      0.62      0.79      1.05      0.81      0.99
soa                71.0      91.3     130.1     203.8     365.3     726.4    1391.4    2835.0
  slope                      0.36      0.51      0.65      0.84      0.99      0.94      1.03
aos                68.3      85.7     122.3     191.6     353.3     692.9    1408.9    2964.3
  slope                      0.33      0.51      0.65      0.88      0.97      1.02      1.07
reference         101.0     122.9     180.8     314.7     558.9    1101.2    2363.4    3871.0
  slope                      0.28      0.56      0.80      0.83      0.98      1.10      0.71
sparse_set         70.3      88.8     128.4     215.5     389.4     746.2    1547.2    3190.6
  slope                      0.34      0.53      0.75      0.85      0.94      1.05      1.04
```

## `ecs_point_ops` · population

A fixed number of point operations per frame — get, set, add, remove, create, destroy — with integrate and the per-frame query switched off. Both of those are a full pass over the population, so leaving either on would make every candidate measure the pass rather than the operation.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `soa` | 0.059 | 0.7675 | 0.17 | 0.821 | 48.8 us | 68.1 us | 1.4x |
| `archetype` | 0.088 | 0.7983 | 0.238 | 0.887 | 63.9 us | 103.6 us | 1.6x |
| `aos` | 0.114 | 0.6813 | 0.339 | 0.821 | 45.6 us | 91.8 us | 2.0x |
| `sparse_set` | 0.115 | 0.7791 | 0.289 | 0.906 | 52.7 us | 92.8 us | 1.8x |
| `reference` | 0.129 | 0.8077 | 0.19 | 0.964 | 64.6 us | 122.5 us | 1.9x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
soa                48.8      49.2      50.8      50.5      51.2      53.9      59.1      68.1
  slope                      0.01      0.05     -0.01      0.02      0.07      0.13      0.21
archetype          63.9      68.1      66.4      68.1      71.5      74.5      89.0     103.6
  slope                      0.09     -0.04      0.04      0.07      0.06      0.26      0.22
aos                45.6      46.3      53.0      47.7      48.3      57.4      62.1      91.8
  slope                      0.02      0.19     -0.15      0.02      0.25      0.11      0.56
sparse_set         52.7      50.5      51.8      54.6      55.3      62.2      77.8      92.8
  slope                     -0.06      0.04      0.08      0.02      0.17      0.32      0.25
reference          64.6      73.7      74.3      71.4      72.2      94.2     117.3     122.5
  slope                      0.19      0.01     -0.06      0.02      0.38      0.32      0.06
```

## `spatial_knn` · fixed_density

One k-nearest query against a settled index. Separate from the radius family because a k-nearest query has to widen its search until it can prove it has the k closest, and that loop is where two candidates lost to a linear scan in the main suite.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `spatial_hash` | -0.003 | 0.004 | 0.172 | 1.0 | 1076.9 us | 1162.6 us | 1.1x |
| `grid_undo_log` | 0.037 | 0.7553 | 0.092 | 0.989 | 141.8 us | 171.4 us | 1.2x |
| `morton_sorted` | 0.037 | 0.7359 | 0.056 | 0.99 | 1090.8 us | 1326.3 us | 1.2x |
| `uniform_grid` | 0.04 | 0.69 | 0.137 | 0.989 | 129.3 us | 157.3 us | 1.2x |
| `axis_sorted` | 0.389 | 0.9809 | 0.577 | 0.968 | 239.4 us | 1631.8 us | 6.8x |
| `brute_force` | 0.948 | 0.998 | 1.023 | 0.999 | 185.1 us | 17916.1 us | 96.8x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
spatial_hash     1076.9    1012.3    1025.5     996.3     918.2     915.9     955.0    1162.6
  slope                     -0.09      0.02     -0.04     -0.12     -0.00      0.06      0.28
grid_undo_log     141.8     141.8     140.8     151.7     143.4     151.0     161.3     171.4
  slope                      0.00     -0.01      0.11     -0.08      0.07      0.10      0.09
morton_sorted    1090.8    1088.4    1169.1    1296.1    1228.1    1227.4    1253.0    1326.3
  slope                     -0.00      0.10      0.15     -0.08     -0.00      0.03      0.08
uniform_grid      129.3     124.0     127.1     131.0     132.5     130.0     148.1     157.3
  slope                     -0.06      0.04      0.04      0.02     -0.03      0.19      0.09
axis_sorted       239.4     304.7     345.3     498.7     610.9     732.9    1177.1    1631.8
  slope                      0.35      0.18      0.53      0.29      0.26      0.68      0.47
brute_force       185.1     318.3     578.2    1086.2    2159.9    4340.1    8507.8   17916.1
  slope                      0.78      0.86      0.91      0.99      1.01      0.97      1.07
```

## `spatial_knn` · fixed_world

One k-nearest query against a settled index. Separate from the radius family because a k-nearest query has to widen its search until it can prove it has the k closest, and that loop is where two candidates lost to a linear scan in the main suite.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `spatial_hash` | -0.709 | 0.9645 | -0.496 | 0.955 | 6841.0 us | 268.2 us | /25.5 |
| `morton_sorted` | -0.459 | 0.9491 | -0.639 | 0.99 | 4073.9 us | 458.0 us | /8.9 |
| `grid_undo_log` | -0.234 | 0.7291 | -0.036 | 0.346 | 468.6 us | 163.4 us | /2.9 |
| `uniform_grid` | -0.122 | 0.4121 | 0.106 | 0.346 | 291.1 us | 196.1 us | /1.5 |
| `axis_sorted` | 0.391 | 0.9848 | 0.557 | 0.968 | 239.5 us | 1618.1 us | 6.8x |
| `brute_force` | 0.948 | 0.9984 | 1.039 | 0.999 | 180.3 us | 17722.0 us | 98.3x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
spatial_hash     6841.0    3396.8    2688.8    1057.2     648.8     533.7     239.1     268.2
  slope                     -1.01     -0.34     -1.35     -0.70     -0.28     -1.16      0.17
morton_sorted    4073.9    2694.1    2559.8    1297.1    1121.9    1111.4     467.0     458.0
  slope                     -0.60     -0.07     -0.98     -0.21     -0.01     -1.25     -0.03
grid_undo_log     468.6     324.6     287.3     150.2     150.3     171.9     126.3     163.4
  slope                     -0.53     -0.18     -0.94      0.00      0.19     -0.44      0.37
uniform_grid      291.1     229.5     222.2     130.4     145.2     169.3     110.9     196.1
  slope                     -0.34     -0.05     -0.77      0.16      0.22     -0.61      0.82
axis_sorted       239.5     298.0     351.6     490.3     612.7     747.8    1183.1    1618.1
  slope                      0.32      0.24      0.48      0.32      0.29      0.66      0.45
brute_force       180.3     315.6     580.8    1083.3    2169.5    4199.5    8428.2   17722.0
  slope                      0.81      0.88      0.90      1.00      0.95      1.01      1.07
```

## `spatial_move` · fixed_density

The cost of keeping the structure current under a fixed number of moves per tick. One radius query per tick is included on purpose: a candidate that rebuilds lazily does its work on the first observation, so with no query at all its rebuild would never happen and the family would measure an array write.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `uniform_grid` | 0.08 | 0.8817 | 0.159 | 0.994 | 49.7 us | 72.6 us | 1.5x |
| `spatial_hash` | 0.086 | 0.7326 | 0.196 | 0.677 | 64.3 us | 106.1 us | 1.7x |
| `grid_undo_log` | 0.091 | 0.9088 | 0.134 | 0.994 | 50.1 us | 78.4 us | 1.6x |
| `brute_force` | 0.573 | 0.9252 | 1.022 | 0.999 | 19.2 us | 326.9 us | 17.0x |
| `morton_sorted` | 1.056 | 0.9992 | 1.098 | 1.0 | 82.0 us | 13249.2 us | 161.6x |
| `axis_sorted` | 1.099 | 0.9998 | 1.13 | 1.0 | 87.4 us | 17745.6 us | 203.0x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
uniform_grid       49.7      50.9      51.9      52.5      57.6      58.2      70.0      72.6
  slope                      0.03      0.03      0.02      0.13      0.01      0.27      0.05
spatial_hash       64.3      78.8      66.8      80.6      73.4      80.9      96.6     106.1
  slope                      0.29     -0.24      0.27     -0.14      0.14      0.26      0.14
grid_undo_log      50.1      51.4      53.4      52.6      58.3      65.1      68.9      78.4
  slope                      0.04      0.05     -0.02      0.15      0.16      0.08      0.19
brute_force        19.2      21.3      25.6      33.1      49.4      79.3     148.9     326.9
  slope                      0.15      0.26      0.37      0.58      0.68      0.91      1.13
morton_sorted      82.0     154.0     302.9     649.6    1339.5    2891.4    6080.6   13249.2
  slope                      0.91      0.98      1.10      1.04      1.11      1.07      1.12
axis_sorted        87.4     175.3     368.6     805.3    1717.8    3705.8    8007.7   17745.6
  slope                      1.00      1.07      1.13      1.09      1.11      1.11      1.15
```

## `spatial_move` · fixed_world

The cost of keeping the structure current under a fixed number of moves per tick. One radius query per tick is included on purpose: a candidate that rebuilds lazily does its work on the first observation, so with no query at all its rebuild would never happen and the family would measure an array write.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `grid_undo_log` | 0.094 | 0.7928 | 0.243 | 0.347 | 51.6 us | 83.4 us | 1.6x |
| `spatial_hash` | 0.094 | 0.8806 | 0.168 | 0.652 | 64.3 us | 100.9 us | 1.6x |
| `uniform_grid` | 0.101 | 0.8323 | 0.257 | 0.347 | 49.0 us | 83.8 us | 1.7x |
| `brute_force` | 0.564 | 0.9239 | 1.021 | 0.999 | 19.2 us | 316.2 us | 16.4x |
| `morton_sorted` | 1.061 | 0.9996 | 1.093 | 1.0 | 78.1 us | 13070.6 us | 167.3x |
| `axis_sorted` | 1.098 | 0.9998 | 1.126 | 1.0 | 85.9 us | 17486.0 us | 203.6x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
grid_undo_log      51.6      50.4      52.0      53.0      55.1      59.6      70.1      83.4
  slope                     -0.03      0.05      0.03      0.06      0.11      0.24      0.25
spatial_hash       64.3      63.7      66.0      66.7      72.0      80.0      88.4     100.9
  slope                     -0.01      0.05      0.02      0.11      0.15      0.14      0.19
uniform_grid       49.0      49.9      51.2      52.9      55.2      58.6      69.5      83.8
  slope                      0.03      0.04      0.05      0.06      0.09      0.25      0.27
brute_force        19.2      21.8      25.6      33.1      49.0      76.8     146.3     316.2
  slope                      0.18      0.23      0.37      0.57      0.65      0.93      1.11
morton_sorted      78.1     149.9     306.0     642.5    1341.4    2874.1    6055.3   13070.6
  slope                      0.94      1.03      1.07      1.06      1.10      1.08      1.11
axis_sorted        85.9     174.9     375.6     791.7    1682.0    3670.3    8010.0   17486.0
  slope                      1.03      1.10      1.08      1.09      1.13      1.13      1.13
```

## `spatial_query_radius` · fixed_density

One radius query against a settled index. Nothing moves after the load tick, so no candidate is paying to keep itself current and this is the query path alone.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `spatial_hash` | 0.04 | 0.4428 | 0.144 | 1.0 | 81.9 us | 106.6 us | 1.3x |
| `morton_sorted` | 0.073 | 0.9511 | 0.055 | 1.0 | 161.1 us | 234.1 us | 1.5x |
| `grid_undo_log` | 0.078 | 0.55 | 0.204 | 0.994 | 11.7 us | 19.6 us | 1.7x |
| `uniform_grid` | 0.083 | 0.7894 | 0.192 | 0.994 | 11.6 us | 18.8 us | 1.6x |
| `axis_sorted` | 0.538 | 0.9472 | 0.973 | 1.0 | 25.9 us | 454.5 us | 17.6x |
| `brute_force` | 1.01 | 0.9999 | 0.988 | 0.999 | 186.0 us | 24766.3 us | 133.1x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
spatial_hash       81.9      93.7      79.7      81.6      83.9      87.3      97.4     106.6
  slope                      0.19     -0.23      0.03      0.04      0.06      0.16      0.13
morton_sorted     161.1     178.4     193.3     201.6     212.5     216.9     228.7     234.1
  slope                      0.15      0.12      0.06      0.08      0.03      0.08      0.03
grid_undo_log      11.7      15.9      12.0      12.8      13.5      14.8      16.6      19.6
  slope                      0.45     -0.40      0.09      0.08      0.13      0.16      0.24
uniform_grid       11.6      12.1      13.3      12.0      13.1      14.4      15.3      18.8
  slope                      0.05      0.14     -0.15      0.13      0.14      0.09      0.30
axis_sorted        25.9      34.3      44.9      58.8      81.2     118.0     172.2     454.5
  slope                      0.41      0.39      0.39      0.47      0.54      0.55      1.40
brute_force       186.0     367.8     736.6    1487.3    2996.0    6294.5   12037.9   24766.3
  slope                      0.98      1.00      1.01      1.01      1.07      0.94      1.04
```

## `spatial_query_radius` · fixed_world

One radius query against a settled index. Nothing moves after the load tick, so no candidate is paying to keep itself current and this is the query path alone.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `spatial_hash` | 0.164 | 0.7747 | 0.487 | 0.955 | 72.1 us | 182.7 us | 2.5x |
| `morton_sorted` | 0.229 | 0.9944 | 0.177 | 1.0 | 120.1 us | 365.8 us | 3.0x |
| `grid_undo_log` | 0.455 | 0.9358 | 0.696 | 0.347 | 8.1 us | 75.0 us | 9.2x |
| `uniform_grid` | 0.459 | 0.9174 | 0.815 | 0.347 | 8.2 us | 77.2 us | 9.4x |
| `axis_sorted` | 0.694 | 0.9803 | 0.917 | 1.0 | 21.4 us | 606.3 us | 28.3x |
| `brute_force` | 0.939 | 0.9896 | 0.999 | 0.999 | 332.8 us | 25070.7 us | 75.3x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
spatial_hash       72.1      76.1      78.2      79.9      86.2      93.0     124.7     182.7
  slope                      0.08      0.04      0.03      0.11      0.11      0.42      0.55
morton_sorted     120.1     145.7     172.9     200.8     242.7     286.4     313.4     365.8
  slope                      0.28      0.25      0.22      0.27      0.24      0.13      0.22
grid_undo_log       8.1       9.0      10.4      13.0      16.7      28.6      41.3      75.0
  slope                      0.15      0.21      0.33      0.36      0.78      0.53      0.86
uniform_grid        8.2       8.8       9.8      12.3      16.6      24.9      42.9      77.2
  slope                      0.09      0.16      0.34      0.42      0.59      0.78      0.85
axis_sorted        21.4      28.2      40.1      59.4      95.9     170.0     322.9     606.3
  slope                      0.40      0.51      0.57      0.69      0.83      0.93      0.91
brute_force       332.8     369.3     773.0    1733.5    3034.9    6277.3   12407.7   25070.7
  slope                      0.15      1.07      1.17      0.81      1.05      0.98      1.01
```

## Cost of a move, with the forcing query removed

The move family carries one radius query per tick so that a candidate which rebuilds lazily actually does its rebuild. For a candidate whose query is itself expensive that query dominates the tick at large populations, and the family would report its query cost as its move cost. Subtracting one query's worth of the query family's measurement, at the same population and regime, separates them. It is an estimate: the two families place entities differently, so the per-query cost is close but not identical.

### fixed_world

| candidate | move-family n^ | moves-only n^ | ns per move at smallest | at largest |
|---|---:|---:|---:|---:|
| `brute_force` | 0.564 | 0.343 | 8.3 | 60.2 |
| `uniform_grid` | 0.101 | 0.1 | 24.5 | 41.6 |
| `grid_undo_log` | 0.094 | 0.093 | 25.7 | 41.4 |
| `spatial_hash` | 0.094 | 0.093 | 31.9 | 49.8 |
| `morton_sorted` | 1.061 | 1.063 | 38.6 | 6533.9 |
| `axis_sorted` | 1.098 | 1.098 | 42.9 | 8740.6 |

### fixed_density

| candidate | move-family n^ | moves-only n^ | ns per move at smallest | at largest |
|---|---:|---:|---:|---:|
| `brute_force` | 0.573 | 0.361 | 8.9 | 66.7 |
| `uniform_grid` | 0.08 | 0.08 | 24.8 | 36.2 |
| `grid_undo_log` | 0.091 | 0.091 | 25.0 | 39.1 |
| `spatial_hash` | 0.086 | 0.086 | 31.8 | 52.6 |
| `morton_sorted` | 1.056 | 1.059 | 40.4 | 6623.7 |
| `axis_sorted` | 1.099 | 1.099 | 43.6 | 8871.0 |

## Declared complexity against measured growth

The `complexity:` field of each manifest is a claim written by hand. Until this report existed nothing read it. Most claims are free text describing what the cost depends on, and those cannot be turned into a number without guessing what the author meant, so they are placed beside the measurement for a person to judge. Only `O(1)` and `O(n)` are checked automatically, against a tolerance of 0.15 in the exponent.

**A disagreement here has two possible causes and the table cannot tell them apart.** A complexity claim counts operations; the measurement is time. When they part company it means either that the claim is wrong about the operations, or that the claim is right and the machine does not behave the way the model assumes — most often because the working set has outgrown a level of cache, so a fixed number of memory accesses stops costing a fixed amount of time. Both are findings. Neither is a reason to edit the claim to match the number.

| candidate | claim | field | measured (family · regime) | verdict |
|---|---|---|---:|---|
| `aos` | `O(slots)` | `query` | n^0.794 | not machine-checkable |
| `aos` | `O(1)` | `get_set` | n^0.114 | agrees (expected 0.0) |
| `archetype` | `O(matching entities)` | `query` | n^0.696 | not machine-checkable |
| `archetype` | `O(1)` | `get_set` | n^0.088 | agrees (expected 0.0) |
| `axis_sorted` | `O(1), plus a deferred O(n log n)` | `insert_remove_move` | n^1.099 | not machine-checkable |
| `axis_sorted` | `O(log n + entities in the slab)` | `query_radius` | n^0.538 | not machine-checkable |
| `brute_force` | `O(entities)` | `query_knn` | n^0.948 | agrees (expected 1.0) |
| `brute_force` | `O(1)` | `insert_remove_move` | n^0.573 | **disagrees** (expected 0.0) |
| `brute_force` | `O(entities)` | `query_radius` | n^1.01 | agrees (expected 1.0) |
| `morton_sorted` | `O(1), plus a deferred O(n log n)` | `insert_remove_move` | n^1.056 | not machine-checkable |
| `morton_sorted` | `O(cells touched * log n + entities in them)` | `query_radius` | n^0.073 | not machine-checkable |
| `reference` | `O(entities), in scattered memory` | `query` | n^0.795 | not machine-checkable |
| `reference` | `O(1) expected, with a dependent chain of loads` | `get_set` | n^0.129 | not machine-checkable |
| `soa` | `O(slots)` | `query` | n^0.776 | not machine-checkable |
| `soa` | `O(1)` | `get_set` | n^0.059 | agrees (expected 0.0) |
| `sparse_set` | `O(size of the smallest matching set)` | `query` | n^0.805 | not machine-checkable |
| `sparse_set` | `O(1) with two dependent loads` | `get_set` | n^0.115 | not machine-checkable |
| `spatial_hash` | `O(1) expected` | `insert_remove_move` | n^0.086 | not machine-checkable |
| `spatial_hash` | `O(cells touched probes + entities in them)` | `query_radius` | n^0.04 | not machine-checkable |
| `uniform_grid` | `O(entities within the smallest sufficient box)` | `query_knn` | n^0.04 | not machine-checkable |
| `uniform_grid` | `O(1)` | `insert_remove_move` | n^0.08 | agrees (expected 0.0) |
| `uniform_grid` | `O(cells touched + entities in them)` | `query_radius` | n^0.083 | not machine-checkable |

