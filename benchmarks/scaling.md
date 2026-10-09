# Scaling report

- run: `sweep-20261009T044632Z`
- commit: `ed47f82bf736c04a277502050e885d27d0b1fad9`
- cpu: Intel(R) Xeon(R) Processor @ 2.10GHz
- compiler: c++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
- build flags: `-O2 -DNDEBUG -fno-omit-frame-pointer -ffp-contract=off -Wall -Wextra -march=native`
- populations: 1000, 2000, 4000, 8000, 16000, 32000, 64000, 128000 (base 8000)
- 3 repetitions per point plus 1 warmup, median repetition

`brute_force` is the positive control: it looks at every entity for every query, so its query exponent must come out at 1. It measures 0.937 with an r2 of 0.9996. An exponent elsewhere in this report is only worth reading because that one came out right.

Every exponent below is the least-squares slope of log(cost) against log(population). A cost that does not depend on the population gives 0; one that looks at everything gives 1. `r2` is the fit quality: below about 0.9 the points are not following a single power law over this range and the exponent should not be quoted.

The number of operations per step is held constant while the population grows. Without that, every candidate would measure linear regardless of what it does.

## Regimes

- **fixed_world** — The world stays the same size, so density grows with the population and a query of fixed radius returns proportionally more entities. The exponent measured here includes the growth of the answer, not just the growth of the search.
- **fixed_density** — The world grows as the cube root of the population, so density and the size of a query's answer stay constant. The exponent measured here is the cost of finding the answer, which is what a complexity claim is about.
- **population** — Population grows with the operation count per frame held fixed. The only regime for a track with no world.

## `spatial_query_radius` · fixed_world

One radius query against a settled index. Nothing moves after the load tick, so no candidate is paying to keep itself current and this is the query path alone.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `spatial_hash` | 0.137 | 0.6729 | 0.41 | 0.955 | 71.1 us | 170.6 us | 2.4x |
| `morton_sorted` | 0.221 | 0.9958 | 0.189 | 1.0 | 141.1 us | 402.5 us | 2.9x |
| `delta_grid` | 0.309 | 0.8179 | 0.558 | 0.35 | 16.2 us | 65.0 us | 4.0x |
| `cell_rows` | 0.331 | 0.872 | 0.35 | 0.396 | 9.7 us | 34.1 us | 3.5x |
| `morton_lbvh` | 0.334 | 0.9985 | 0.322 | 0.999 | 25.8 us | 132.5 us | 5.1x |
| `grid_ring_knn` | 0.424 | 0.9194 | 0.754 | 0.347 | 9.4 us | 74.3 us | 7.9x |
| `grid_undo_log` | 0.432 | 0.9204 | 0.735 | 0.347 | 8.8 us | 73.6 us | 8.3x |
| `uniform_grid` | 0.434 | 0.9323 | 0.737 | 0.347 | 8.8 us | 73.0 us | 8.3x |
| `cell_sorted` | 0.452 | 0.8778 | 0.586 | 0.396 | 10.0 us | 93.0 us | 9.3x |
| `axis_sorted` | 0.639 | 0.967 | 0.936 | 1.0 | 20.4 us | 576.2 us | 28.2x |
| `brute_force` | 0.949 | 0.9994 | 1.004 | 0.999 | 298.9 us | 29306.8 us | 98.0x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
spatial_hash       71.1     100.7      78.4      80.4     116.0      96.6     119.8     170.6
  slope                      0.50     -0.36      0.04      0.53     -0.26      0.31      0.51
morton_sorted     141.1     161.2     185.5     222.8     269.5     309.9     344.0     402.5
  slope                      0.19      0.20      0.26      0.27      0.20      0.15      0.23
delta_grid         16.2      11.4      19.3      15.5      20.8      30.0      43.1      65.0
  slope                     -0.52      0.76     -0.32      0.43      0.53      0.52      0.59
cell_rows           9.7       7.2      12.3      10.6      20.2      21.0      36.7      34.1
  slope                     -0.44      0.78     -0.21      0.93      0.06      0.80     -0.10
morton_lbvh        25.8      32.5      40.6      51.1      65.5      84.8      99.1     132.5
  slope                      0.33      0.32      0.33      0.36      0.37      0.23      0.42
grid_ring_knn       9.4       9.9      11.3      13.6      18.0      26.1      43.2      74.3
  slope                      0.08      0.20      0.27      0.40      0.54      0.73      0.78
grid_undo_log       8.8      10.0      10.8      13.0      17.3      26.6      43.1      73.6
  slope                      0.17      0.11      0.27      0.41      0.62      0.70      0.77
uniform_grid        8.8       9.6      11.0      13.4      18.1      26.3      43.4      73.0
  slope                      0.13      0.20      0.28      0.44      0.54      0.72      0.75
cell_sorted        10.0      15.8      11.8      14.4      19.9      41.3      59.2      93.0
  slope                      0.65     -0.42      0.29      0.47      1.05      0.52      0.65
axis_sorted        20.4      37.6      55.6      57.4      92.0     157.5     292.0     576.2
  slope                      0.88      0.56      0.05      0.68      0.78      0.89      0.98
brute_force       298.9     512.5    1045.9    2039.6    3791.4    7290.1   14492.3   29306.8
  slope                      0.78      1.03      0.96      0.89      0.94      0.99      1.02
```

## `spatial_query_radius` · fixed_density

One radius query against a settled index. Nothing moves after the load tick, so no candidate is paying to keep itself current and this is the query path alone.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `grid_undo_log` | 0.067 | 0.398 | 0.233 | 0.994 | 16.7 us | 21.1 us | 1.3x |
| `grid_ring_knn` | 0.068 | 0.2557 | 0.139 | 0.994 | 17.2 us | 27.6 us | 1.6x |
| `spatial_hash` | 0.077 | 0.9053 | 0.17 | 1.0 | 73.9 us | 111.8 us | 1.5x |
| `cell_rows` | 0.08 | 0.4924 | 0.237 | 0.994 | 9.9 us | 17.4 us | 1.8x |
| `morton_sorted` | 0.082 | 0.815 | -0.067 | 1.0 | 181.4 us | 264.4 us | 1.5x |
| `uniform_grid` | 0.102 | 0.8292 | 0.245 | 0.994 | 12.3 us | 21.7 us | 1.8x |
| `cell_sorted` | 0.117 | 0.7764 | 0.31 | 0.994 | 13.8 us | 25.9 us | 1.9x |
| `delta_grid` | 0.162 | 0.6931 | 0.558 | 0.994 | 14.6 us | 39.0 us | 2.7x |
| `morton_lbvh` | 0.182 | 0.888 | 0.13 | 0.999 | 30.7 us | 83.3 us | 2.7x |
| `axis_sorted` | 0.459 | 0.9919 | 0.571 | 1.0 | 26.0 us | 240.5 us | 9.3x |
| `brute_force` | 0.937 | 0.9996 | 0.95 | 0.999 | 262.2 us | 25808.9 us | 98.4x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
grid_undo_log      16.7      13.1      13.2      13.2      18.8      15.2      17.7      21.1
  slope                     -0.35      0.02     -0.01      0.51     -0.30      0.21      0.25
grid_ring_knn      17.2      20.2      13.6      18.0      14.7      22.8      17.6      27.6
  slope                      0.23     -0.57      0.41     -0.30      0.64     -0.37      0.65
spatial_hash       73.9      76.5      80.5      80.4      86.2      88.3      98.2     111.8
  slope                      0.05      0.07     -0.00      0.10      0.03      0.15      0.19
cell_rows           9.9      13.7      10.2      10.6      11.1      12.5      13.6      17.4
  slope                      0.47     -0.43      0.05      0.07      0.17      0.12      0.35
morton_sorted     181.4     196.7     214.4     226.6     232.0     290.2     250.6     264.4
  slope                      0.12      0.12      0.08      0.03      0.32     -0.21      0.08
uniform_grid       12.3      13.1      13.1      13.2      14.1      15.4      17.2      21.7
  slope                      0.09      0.00      0.01      0.10      0.13      0.16      0.33
cell_sorted        13.8      13.8      14.3      14.4      14.9      16.8      20.1      25.9
  slope                      0.00      0.04      0.01      0.05      0.18      0.25      0.37
delta_grid         14.6      14.8      15.1      15.7      16.9      18.0      21.9      39.0
  slope                      0.02      0.03      0.06      0.11      0.09      0.28      0.83
morton_lbvh        30.7      46.8      43.9      63.1      59.5      69.6      73.7      83.3
  slope                      0.61     -0.09      0.52     -0.09      0.23      0.08      0.18
axis_sorted        26.0      32.3      42.6      56.6      78.4     109.0     159.7     240.5
  slope                      0.31      0.40      0.41      0.47      0.48      0.55      0.59
brute_force       262.2     527.6    1013.6    1991.1    3849.0    6912.7   12997.6   25808.9
  slope                      1.01      0.94      0.97      0.95      0.84      0.91      0.99
```

## `spatial_knn` · fixed_world

One k-nearest query against a settled index. Separate from the radius family because a k-nearest query has to widen its search until it can prove it has the k closest, and that loop is where two candidates lost to a linear scan in the main suite.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `spatial_hash` | -0.697 | 0.964 | -0.495 | 0.955 | 6643.8 us | 266.5 us | /24.9 |
| `morton_sorted` | -0.467 | 0.9623 | -0.606 | 0.99 | 3935.8 us | 451.7 us | /8.7 |
| `delta_grid` | -0.195 | 0.7462 | -0.148 | 0.35 | 355.7 us | 140.9 us | /2.5 |
| `grid_ring_knn` | -0.177 | 0.6157 | 0.086 | 0.347 | 178.3 us | 85.3 us | /2.1 |
| `grid_undo_log` | -0.167 | 0.6748 | -0.102 | 0.346 | 302.7 us | 141.9 us | /2.1 |
| `cell_sorted` | -0.16 | 0.6906 | -0.136 | 0.396 | 286.3 us | 136.7 us | /2.1 |
| `uniform_grid` | -0.154 | 0.6513 | -0.087 | 0.346 | 287.5 us | 149.3 us | /1.9 |
| `cell_rows` | -0.008 | 0.0129 | -0.076 | 0.396 | 95.8 us | 100.2 us | 1.0x |
| `morton_lbvh` | 0.195 | 0.9227 | -0.028 | 0.994 | 85.5 us | 213.6 us | 2.5x |
| `axis_sorted` | 0.392 | 0.9826 | 0.574 | 0.968 | 243.3 us | 1653.1 us | 6.8x |
| `brute_force` | 0.964 | 0.997 | 1.09 | 0.999 | 181.4 us | 19153.6 us | 105.6x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
spatial_hash     6643.8    3271.5    2611.0     987.0     653.0     529.1     248.3     266.5
  slope                     -1.02     -0.33     -1.40     -0.60     -0.30     -1.09      0.10
morton_sorted    3935.8    2751.5    2620.5    1593.8    1126.3    1046.6     459.3     451.7
  slope                     -0.52     -0.07     -0.72     -0.50     -0.11     -1.19     -0.02
delta_grid        355.7     255.7     242.7     138.8     147.9     172.9     116.6     140.9
  slope                     -0.48     -0.08     -0.81      0.09      0.23     -0.57      0.27
grid_ring_knn     178.3     116.1     115.5     102.9      55.8      75.7      60.1      85.3
  slope                     -0.62     -0.01     -0.17     -0.88      0.44     -0.33      0.50
grid_undo_log     302.7     221.6     227.4     128.0     138.9     163.4     110.1     141.9
  slope                     -0.45      0.04     -0.83      0.12      0.23     -0.57      0.37
cell_sorted       286.3     225.0     227.8     126.9     139.8     164.9     117.5     136.7
  slope                     -0.35      0.02     -0.84      0.14      0.24     -0.49      0.22
uniform_grid      287.5     225.4     223.3     131.7     144.0     168.4     109.1     149.3
  slope                     -0.35     -0.01     -0.76      0.13      0.23     -0.63      0.45
cell_rows          95.8      97.4     117.9      86.5     103.0     111.4      83.5     100.2
  slope                      0.02      0.28     -0.45      0.25      0.11     -0.42      0.26
morton_lbvh        85.5     105.0     119.1     140.1     156.0     221.9     191.0     213.6
  slope                      0.30      0.18      0.23      0.16      0.51     -0.22      0.16
axis_sorted       243.3     303.9     350.0     488.4     694.3     746.4    1176.9    1653.1
  slope                      0.32      0.20      0.48      0.51      0.10      0.66      0.49
brute_force       181.4     310.4     567.3    1068.3    2101.0    4227.8    8993.3   19153.6
  slope                      0.77      0.87      0.91      0.98      1.01      1.09      1.09
```

## `spatial_knn` · fixed_density

One k-nearest query against a settled index. Separate from the radius family because a k-nearest query has to widen its search until it can prove it has the k closest, and that loop is where two candidates lost to a linear scan in the main suite.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `spatial_hash` | 0.008 | 0.0299 | 0.15 | 1.0 | 1068.1 us | 1136.2 us | 1.1x |
| `grid_ring_knn` | 0.028 | 0.3179 | 0.07 | 0.994 | 73.7 us | 82.5 us | 1.1x |
| `delta_grid` | 0.031 | 0.1213 | 0.187 | 0.992 | 131.0 us | 185.3 us | 1.4x |
| `morton_sorted` | 0.038 | 0.7248 | -0.004 | 0.99 | 1098.1 us | 1339.8 us | 1.2x |
| `uniform_grid` | 0.041 | 0.757 | 0.076 | 0.989 | 129.5 us | 158.2 us | 1.2x |
| `grid_undo_log` | 0.05 | 0.7666 | 0.133 | 0.989 | 120.0 us | 158.1 us | 1.3x |
| `cell_rows` | 0.08 | 0.7549 | 0.127 | 0.99 | 81.8 us | 113.0 us | 1.4x |
| `cell_sorted` | 0.128 | 0.7259 | 0.201 | 0.99 | 110.0 us | 232.6 us | 2.1x |
| `morton_lbvh` | 0.187 | 0.9899 | 0.166 | 0.994 | 85.3 us | 218.2 us | 2.6x |
| `axis_sorted` | 0.397 | 0.9562 | 0.558 | 0.968 | 233.9 us | 1595.4 us | 6.8x |
| `brute_force` | 0.951 | 0.9925 | 1.042 | 0.999 | 181.8 us | 18435.3 us | 101.4x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
spatial_hash     1068.1     989.2    1019.8     978.3     909.1     923.1    1070.4    1136.2
  slope                     -0.11      0.04     -0.06     -0.11      0.02      0.21      0.09
grid_ring_knn      73.7      70.9      68.7      66.1      65.6      74.9      79.5      82.5
  slope                     -0.05     -0.05     -0.06     -0.01      0.19      0.09      0.05
delta_grid        131.0     191.7     131.8     137.9     139.4     143.1     160.6     185.3
  slope                      0.55     -0.54      0.06      0.02      0.04      0.17      0.21
morton_sorted    1098.1    1145.4    1237.9    1324.6    1262.7    1346.6    1288.4    1339.8
  slope                      0.06      0.11      0.10     -0.07      0.09     -0.06      0.06
uniform_grid      129.5     128.8     123.8     132.1     131.0     142.3     145.0     158.2
  slope                     -0.01     -0.06      0.09     -0.01      0.12      0.03      0.13
grid_undo_log     120.0     121.1     128.3     127.7     123.4     131.5     146.0     158.1
  slope                      0.01      0.08     -0.01     -0.05      0.09      0.15      0.11
cell_rows          81.8      83.0      83.4      87.9      87.2      94.7     124.3     113.0
  slope                      0.02      0.01      0.08     -0.01      0.12      0.39     -0.14
cell_sorted       110.0     117.4     117.6     122.5     126.4     176.0     143.1     232.6
  slope                      0.09      0.00      0.06      0.04      0.48     -0.30      0.70
morton_lbvh        85.3     103.2     123.6     133.7     153.6     173.5     194.7     218.2
  slope                      0.28      0.26      0.11      0.20      0.17      0.17      0.16
axis_sorted       233.9     300.4     352.0     487.7     918.4     736.1    1177.5    1595.4
  slope                      0.36      0.23      0.47      0.91     -0.32      0.68      0.44
brute_force       181.8     312.4     568.1    1660.4    2064.7    4349.6    8842.6   18435.3
  slope                      0.78      0.86      1.55      0.31      1.07      1.02      1.06
```

## `spatial_move` · fixed_world

The cost of keeping the structure current under a fixed number of moves per tick. One radius query per tick is included on purpose: a candidate that rebuilds lazily does its work on the first observation, so with no query at all its rebuild would never happen and the family would measure an array write.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `delta_grid` | -0.033 | 0.0277 | 0.262 | 0.35 | 101.1 us | 87.4 us | /1.2 |
| `spatial_hash` | 0.073 | 0.6293 | 0.193 | 0.652 | 64.8 us | 104.9 us | 1.6x |
| `grid_undo_log` | 0.101 | 0.8598 | 0.221 | 0.347 | 49.7 us | 81.9 us | 1.6x |
| `grid_ring_knn` | 0.103 | 0.811 | 0.27 | 0.347 | 51.3 us | 89.3 us | 1.7x |
| `uniform_grid` | 0.144 | 0.6994 | 0.479 | 0.347 | 50.8 us | 120.6 us | 2.4x |
| `cell_sorted` | 0.585 | 0.8689 | 1.169 | 0.396 | 156.1 us | 2610.7 us | 16.7x |
| `brute_force` | 0.607 | 0.9411 | 1.026 | 0.999 | 19.9 us | 386.1 us | 19.4x |
| `cell_rows` | 0.63 | 0.9015 | 1.116 | 0.396 | 95.2 us | 2555.9 us | 26.8x |
| `morton_lbvh` | 0.931 | 0.9921 | 1.061 | 0.999 | 28.7 us | 2410.7 us | 83.9x |
| `morton_sorted` | 1.063 | 0.9989 | 1.105 | 1.0 | 78.2 us | 13086.7 us | 167.4x |
| `axis_sorted` | 1.1 | 0.9997 | 1.116 | 1.0 | 90.0 us | 18284.4 us | 203.1x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
delta_grid        101.1     114.0      46.6      50.4      52.4      60.8      80.1      87.4
  slope                      0.17     -1.29      0.11      0.06      0.21      0.40      0.13
spatial_hash       64.8      82.5      67.0      84.8      75.3      80.3      90.9     104.9
  slope                      0.35     -0.30      0.34     -0.17      0.09      0.18      0.21
grid_undo_log      49.7      50.1      51.9      54.7      55.3      60.3      73.6      81.9
  slope                      0.01      0.05      0.07      0.02      0.13      0.29      0.15
grid_ring_knn      51.3      52.2      53.8      54.3      57.2      61.5      72.4      89.3
  slope                      0.03      0.04      0.01      0.07      0.10      0.24      0.30
uniform_grid       50.8      51.8      53.1      53.9      56.8      62.1      74.4     120.6
  slope                      0.03      0.03      0.02      0.07      0.13      0.26      0.70
cell_sorted       156.1     117.4     207.5     193.4     307.7     516.1    1095.5    2610.7
  slope                     -0.41      0.82     -0.10      0.67      0.75      1.09      1.25
brute_force        19.9      22.4      28.2      38.5      57.6      93.1     187.1     386.1
  slope                      0.17      0.33      0.45      0.58      0.69      1.01      1.05
cell_rows          95.2     173.9     132.8     188.4     291.2     544.4    1055.2    2555.9
  slope                      0.87     -0.39      0.50      0.63      0.90      0.95      1.28
morton_lbvh        28.7      44.6      70.1     138.1     269.4     553.8    1169.5    2410.7
  slope                      0.64      0.65      0.98      0.96      1.04      1.08      1.04
morton_sorted      78.2     151.2     308.1     646.0    1604.8    2830.4    6109.1   13086.7
  slope                      0.95      1.03      1.07      1.31      0.82      1.11      1.10
axis_sorted        90.0     180.2     380.9     821.0    1721.4    3889.8    8245.4   18284.4
  slope                      1.00      1.08      1.11      1.07      1.18      1.08      1.15
```

## `spatial_move` · fixed_density

The cost of keeping the structure current under a fixed number of moves per tick. One radius query per tick is included on purpose: a candidate that rebuilds lazily does its work on the first observation, so with no query at all its rebuild would never happen and the family would measure an array write.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `uniform_grid` | 0.064 | 0.5997 | 0.191 | 0.994 | 51.1 us | 75.8 us | 1.5x |
| `grid_ring_knn` | 0.069 | 0.483 | -0.002 | 0.994 | 49.9 us | 76.9 us | 1.5x |
| `delta_grid` | 0.086 | 0.1727 | 0.321 | 0.994 | 38.8 us | 90.1 us | 2.3x |
| `grid_undo_log` | 0.106 | 0.6256 | 0.184 | 0.994 | 50.0 us | 73.8 us | 1.5x |
| `spatial_hash` | 0.107 | 0.8875 | 0.211 | 0.677 | 62.3 us | 106.6 us | 1.7x |
| `brute_force` | 0.583 | 0.9545 | 0.872 | 0.999 | 19.8 us | 325.9 us | 16.4x |
| `morton_lbvh` | 0.92 | 0.989 | 1.072 | 0.999 | 31.7 us | 2398.3 us | 75.6x |
| `cell_sorted` | 1.011 | 0.9896 | 1.18 | 0.994 | 37.3 us | 4488.4 us | 120.4x |
| `morton_sorted` | 1.038 | 0.9979 | 1.079 | 1.0 | 79.2 us | 12621.0 us | 159.3x |
| `axis_sorted` | 1.041 | 0.9985 | 1.125 | 1.0 | 110.9 us | 17629.6 us | 159.0x |
| `cell_rows` | 1.043 | 0.9916 | 1.186 | 0.994 | 32.4 us | 4769.0 us | 147.4x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
uniform_grid       51.1      52.4      65.5      54.4      55.8      58.2      67.4      75.8
  slope                      0.04      0.32     -0.27      0.04      0.06      0.21      0.17
grid_ring_knn      49.9      67.3      53.0      70.9      56.5      77.1      68.6      76.9
  slope                      0.43     -0.35      0.42     -0.33      0.45     -0.17      0.17
delta_grid         38.8     104.3      47.0      47.9      50.1      57.8      76.5      90.1
  slope                      1.42     -1.15      0.03      0.06      0.20      0.41      0.24
grid_undo_log      50.0      50.9      51.7      52.5      54.7      57.2      95.0      73.8
  slope                      0.03      0.02      0.02      0.06      0.06      0.73     -0.36
spatial_hash       62.3      64.2      64.5      67.6      72.2      79.6      92.1     106.6
  slope                      0.04      0.01      0.07      0.09      0.14      0.21      0.21
brute_force        19.8      22.4      28.2      38.5      57.9      97.3     173.6     325.9
  slope                      0.18      0.33      0.44      0.59      0.75      0.84      0.91
morton_lbvh        31.7      42.3      70.8     137.2     268.6     542.5    1154.3    2398.3
  slope                      0.42      0.74      0.95      0.97      1.01      1.09      1.06
cell_sorted        37.3      55.3      98.9     194.3     600.1     873.8    1892.5    4488.4
  slope                      0.57      0.84      0.97      1.63      0.54      1.11      1.25
morton_sorted      79.2     152.6     399.9     647.2    1326.7    2828.9    5975.5   12621.0
  slope                      0.95      1.39      0.69      1.04      1.09      1.08      1.08
axis_sorted       110.9     216.0     460.7     811.8    1728.5    3703.9    8070.7   17629.6
  slope                      0.96      1.09      0.82      1.09      1.10      1.12      1.13
cell_rows          32.4      52.6      93.7     182.9     375.0     921.0    2001.5    4769.0
  slope                      0.70      0.83      0.97      1.04      1.30      1.12      1.25
```

## `ecs_point_ops` · population

A fixed number of point operations per frame — get, set, add, remove, create, destroy — with integrate and the per-frame query switched off. Both of those are a full pass over the population, so leaving either on would make every candidate measure the pass rather than the operation.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `soa` | 0.024 | 0.1678 | 0.157 | 0.821 | 57.9 us | 66.0 us | 1.1x |
| `fused_archetype` | 0.05 | 0.3292 | 0.223 | 0.887 | 81.3 us | 97.9 us | 1.2x |
| `aos` | 0.061 | 0.2819 | 0.341 | 0.821 | 59.9 us | 84.1 us | 1.4x |
| `grouped_sparse_set` | 0.075 | 0.4804 | 0.273 | 0.906 | 52.0 us | 90.4 us | 1.7x |
| `bitset_soa` | 0.084 | 0.7387 | -0.025 | 0.821 | 49.6 us | 68.9 us | 1.4x |
| `archetype` | 0.091 | 0.8265 | 0.226 | 0.887 | 62.4 us | 100.8 us | 1.6x |
| `sparse_set` | 0.1 | 0.7651 | 0.268 | 0.906 | 50.2 us | 86.4 us | 1.7x |
| `reference` | 0.117 | 0.7923 | 0.259 | 0.964 | 63.5 us | 120.9 us | 1.9x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
soa                57.9      48.9      58.5      49.8      51.0      53.1      57.2      66.0
  slope                     -0.24      0.26     -0.23      0.03      0.06      0.11      0.21
fused_archetype      81.3      64.5      65.2      67.1      68.1      71.8      84.0      97.9
  slope                     -0.33      0.02      0.04      0.02      0.08      0.23      0.22
aos                59.9      46.0      61.3      46.8      65.7      52.4      59.9      84.1
  slope                     -0.38      0.41     -0.39      0.49     -0.33      0.19      0.49
grouped_sparse_set      52.0      71.0      55.0      55.6      59.1      61.9      72.4      90.4
  slope                      0.45     -0.37      0.01      0.09      0.07      0.23      0.32
bitset_soa         49.6      50.0      50.2      50.5      51.7      71.4      67.4      68.9
  slope                      0.01      0.00      0.01      0.03      0.47     -0.08      0.03
archetype          62.4      63.4      65.0      66.1      68.6      73.7      86.3     100.8
  slope                      0.02      0.03      0.03      0.05      0.10      0.23      0.23
sparse_set         50.2      50.7      51.1      51.8      53.5      59.7      68.7      86.4
  slope                      0.01      0.01      0.02      0.05      0.16      0.20      0.33
reference          63.5      72.5      72.7      69.7      71.4      84.4     104.9     120.9
  slope                      0.19      0.00     -0.06      0.04      0.24      0.31      0.21
```

## `ecs_mixed` · population

The same operation mix with integrate and one query per frame left on: the realistic shape of a frame rather than an isolated operation. Its exponent is expected to sit between the point-operation family's and 1, because a frame is a fixed number of operations plus two linear passes.

| candidate | time n^ | r2 | top-end n^ | memory n^ | step p50 at 1000 | at 128000 | growth |
|---|---:|---:|---:|---:|---:|---:|---:|
| `bitset_soa` | 0.681 | 0.9381 | 0.99 | 0.856 | 87.3 us | 2180.0 us | 25.0x |
| `archetype` | 0.717 | 0.9645 | 0.988 | 0.909 | 74.3 us | 2237.0 us | 30.1x |
| `fused_archetype` | 0.733 | 0.9635 | 0.747 | 0.909 | 73.8 us | 2231.1 us | 30.2x |
| `soa` | 0.733 | 0.9653 | 1.002 | 0.856 | 88.4 us | 2848.9 us | 32.2x |
| `grouped_sparse_set` | 0.737 | 0.9722 | 1.012 | 0.919 | 68.9 us | 2316.5 us | 33.6x |
| `aos` | 0.772 | 0.9777 | 1.008 | 0.857 | 64.5 us | 2572.3 us | 39.9x |
| `reference` | 0.784 | 0.9781 | 0.914 | 0.996 | 100.5 us | 3862.5 us | 38.4x |
| `sparse_set` | 0.79 | 0.9762 | 1.05 | 0.919 | 70.0 us | 2993.0 us | 42.8x |

Population grew 128x across this table. `top-end n^` is the slope across the last three points: where it exceeds the fitted exponent, a fixed per-call cost is flattening the small end and the larger number is closer to the asymptotic behaviour.

Median step time in microseconds at each population, and the slope of each adjacent pair beneath it. A candidate whose pairwise slopes drift is not following one power law, and its fitted exponent is an average over a changing shape rather than a description of it.

```
population         1000      2000      4000      8000     16000     32000     64000    128000
bitset_soa         87.3     107.3     105.6     167.8     298.6     552.4    1085.1    2180.0
  slope                      0.30     -0.02      0.67      0.83      0.89      0.97      1.01
archetype          74.3      86.8     114.3     180.8     296.0     569.0    1079.6    2237.0
  slope                      0.23      0.40      0.66      0.71      0.94      0.92      1.05
fused_archetype      73.8      86.8     118.2     173.0     303.8     792.5    1067.3    2231.1
  slope                      0.23      0.45      0.55      0.81      1.38      0.43      1.06
soa                88.4      90.9     176.0     207.8     368.4     710.6    1376.3    2848.9
  slope                      0.04      0.95      0.24      0.83      0.95      0.95      1.05
grouped_sparse_set      68.9      82.2     116.8     180.2     317.4     569.6    1104.6    2316.5
  slope                      0.26      0.51      0.63      0.82      0.84      0.96      1.07
aos                64.5      83.3     119.5     188.6     333.8     636.2    1259.2    2572.3
  slope                      0.37      0.52      0.66      0.82      0.93      0.98      1.03
reference         100.5     123.1     181.5     293.4     511.0    1087.7    2085.6    3862.5
  slope                      0.29      0.56      0.69      0.80      1.09      0.94      0.89
sparse_set         70.0      86.6     128.1     208.2     366.1     697.9    1439.1    2993.0
  slope                      0.31      0.57      0.70      0.81      0.93      1.04      1.06
```

## Cost of a move, with the forcing query removed

The move family carries one radius query per tick so that a candidate which rebuilds lazily actually does its rebuild. For a candidate whose query is itself expensive that query dominates the tick at large populations, and the family would report its query cost as its move cost. Subtracting one query's worth of the query family's measurement, at the same population and regime, separates them. It is an estimate: the two families place entities differently, so the per-query cost is close but not identical.

### fixed_world

| candidate | move-family n^ | moves-only n^ | ns per move at smallest | at largest |
|---|---:|---:|---:|---:|
| `brute_force` | 0.607 | 0.417 | 8.8 | 78.6 |
| `morton_lbvh` | 0.931 | 0.933 | 14.3 | 1204.8 |
| `grid_undo_log` | 0.101 | 0.1 | 24.8 | 40.7 |
| `uniform_grid` | 0.144 | 0.143 | 25.4 | 60.0 |
| `grid_ring_knn` | 0.103 | 0.102 | 25.6 | 44.4 |
| `spatial_hash` | 0.073 | 0.073 | 32.1 | 51.8 |
| `morton_sorted` | 1.063 | 1.066 | 38.5 | 6541.8 |
| `axis_sorted` | 1.1 | 1.1 | 44.9 | 9140.0 |
| `cell_rows` | 0.63 | 0.631 | 47.6 | 1277.8 |
| `delta_grid` | -0.033 | -0.034 | 50.5 | 43.4 |
| `cell_sorted` | 0.585 | 0.585 | 78.0 | 1305.0 |

### fixed_density

| candidate | move-family n^ | moves-only n^ | ns per move at smallest | at largest |
|---|---:|---:|---:|---:|
| `brute_force` | 0.583 | 0.394 | 8.9 | 62.1 |
| `morton_lbvh` | 0.92 | 0.922 | 15.7 | 1198.8 |
| `cell_rows` | 1.043 | 1.043 | 16.1 | 2384.5 |
| `cell_sorted` | 1.011 | 1.012 | 18.6 | 2244.1 |
| `delta_grid` | 0.086 | 0.086 | 19.4 | 44.9 |
| `grid_ring_knn` | 0.069 | 0.069 | 24.9 | 38.4 |
| `grid_undo_log` | 0.106 | 0.106 | 24.9 | 36.8 |
| `uniform_grid` | 0.064 | 0.063 | 25.5 | 37.8 |
| `spatial_hash` | 0.107 | 0.108 | 30.9 | 52.8 |
| `morton_sorted` | 1.038 | 1.041 | 38.9 | 6309.5 |
| `axis_sorted` | 1.041 | 1.041 | 55.3 | 8813.9 |

## Declared complexity against measured growth

The `complexity:` field of each manifest is a claim written by hand. Until this report existed nothing read it. Most claims are free text describing what the cost depends on, and those cannot be turned into a number without guessing what the author meant, so they are placed beside the measurement for a person to judge. Only `O(1)` and `O(n)` are checked automatically, against a tolerance of 0.15 in the exponent.

**A disagreement here has two possible causes and the table cannot tell them apart.** A complexity claim counts operations; the measurement is time. When they part company it means either that the claim is wrong about the operations, or that the claim is right and the machine does not behave the way the model assumes — most often because the working set has outgrown a level of cache, so a fixed number of memory accesses stops costing a fixed amount of time. Both are findings. Neither is a reason to edit the claim to match the number.

| candidate | claim | field | measured (family · regime) | verdict |
|---|---|---|---:|---|
| `aos` | `O(1)` | `get_set` | n^0.061 | agrees (expected 0.0) |
| `aos` | `O(slots)` | `query` | n^0.772 | not machine-checkable |
| `archetype` | `O(1)` | `get_set` | n^0.091 | agrees (expected 0.0) |
| `archetype` | `O(matching entities)` | `query` | n^0.717 | not machine-checkable |
| `axis_sorted` | `O(log n + entities in the slab)` | `query_radius` | n^0.459 | not machine-checkable |
| `axis_sorted` | `O(1), plus a deferred O(n log n)` | `insert_remove_move` | n^1.041 | not machine-checkable |
| `bitset_soa` | `O(1)` | `get_set` | n^0.084 | agrees (expected 0.0) |
| `bitset_soa` | `O(n)` | `query` | n^0.681 | **disagrees** (expected 1.0) |
| `brute_force` | `O(entities)` | `query_radius` | n^0.937 | agrees (expected 1.0) |
| `brute_force` | `O(entities)` | `query_knn` | n^0.951 | agrees (expected 1.0) |
| `brute_force` | `O(1)` | `insert_remove_move` | n^0.583 | **disagrees** (expected 0.0) |
| `cell_rows` | `O(1)` | `query_radius` | n^0.08 | agrees (expected 0.0) |
| `cell_rows` | `O(entities within the smallest sufficient box)` | `query_knn` | n^0.08 | not machine-checkable |
| `cell_rows` | `O(1), plus a deferred O(ids + cells) rebuild` | `insert_remove_move` | n^1.043 | not machine-checkable |
| `cell_sorted` | `O(1)` | `query_radius` | n^0.117 | agrees (expected 0.0) |
| `cell_sorted` | `O(1)` | `query_knn` | n^0.128 | agrees (expected 0.0) |
| `cell_sorted` | `O(n)` | `insert_remove_move` | n^1.011 | agrees (expected 1.0) |
| `delta_grid` | `O(1)` | `query_radius` | n^0.162 | **disagrees** (expected 0.0) |
| `delta_grid` | `O(1)` | `query_knn` | n^0.031 | agrees (expected 0.0) |
| `delta_grid` | `O(1)` | `insert_remove_move` | n^0.086 | agrees (expected 0.0) |
| `fused_archetype` | `O(1)` | `get_set` | n^0.05 | agrees (expected 0.0) |
| `fused_archetype` | `O(n)` | `query` | n^0.733 | **disagrees** (expected 1.0) |
| `grid_ring_knn` | `O(1)` | `query_radius` | n^0.068 | agrees (expected 0.0) |
| `grid_ring_knn` | `O(1)` | `query_knn` | n^0.028 | agrees (expected 0.0) |
| `grid_ring_knn` | `O(1)` | `insert_remove_move` | n^0.069 | agrees (expected 0.0) |
| `grouped_sparse_set` | `O(1)` | `get_set` | n^0.075 | agrees (expected 0.0) |
| `grouped_sparse_set` | `O(n)` | `query` | n^0.737 | **disagrees** (expected 1.0) |
| `morton_lbvh` | `O(internal nodes whose box meets the sphere, one eight-box test each + leaves whose box meets the sphere, sixteen entity tests each); output-sensitive, with no worst-case bound below O(n)` | `query_radius` | n^0.182 | not machine-checkable |
| `morton_lbvh` | `O(internal nodes expanded, one eight-box test and up to eight frontier pushes of O(log frontier) each + leaves expanded, sixteen entity tests each and an O(log k) heap update per entity kept)` | `query_knn` | n^0.187 | not machine-checkable |
| `morton_lbvh` | `O(n)` | `insert_remove_move` | n^0.92 | agrees (expected 1.0) |
| `morton_sorted` | `O(cells touched * log n + entities in them)` | `query_radius` | n^0.082 | not machine-checkable |
| `morton_sorted` | `O(1), plus a deferred O(n log n)` | `insert_remove_move` | n^1.038 | not machine-checkable |
| `reference` | `O(1) expected, with a dependent chain of loads` | `get_set` | n^0.117 | not machine-checkable |
| `reference` | `O(entities), in scattered memory` | `query` | n^0.784 | not machine-checkable |
| `soa` | `O(1)` | `get_set` | n^0.024 | agrees (expected 0.0) |
| `soa` | `O(slots)` | `query` | n^0.733 | not machine-checkable |
| `sparse_set` | `O(1) with two dependent loads` | `get_set` | n^0.1 | not machine-checkable |
| `sparse_set` | `O(size of the smallest matching set)` | `query` | n^0.79 | not machine-checkable |
| `spatial_hash` | `O(cells touched probes + entities in them)` | `query_radius` | n^0.077 | not machine-checkable |
| `spatial_hash` | `O(1) expected` | `insert_remove_move` | n^0.107 | not machine-checkable |
| `uniform_grid` | `O(cells touched + entities in them)` | `query_radius` | n^0.102 | not machine-checkable |
| `uniform_grid` | `O(entities within the smallest sufficient box)` | `query_knn` | n^0.041 | not machine-checkable |
| `uniform_grid` | `O(1)` | `insert_remove_move` | n^0.064 | agrees (expected 0.0) |

## Parameter sweeps

A parameter sweep holds the population and the world fixed and gives one workload key each of a list of values. Nothing is fitted: the axis is not a size, and no complexity claim is about it. What it answers is which candidate is cheaper at each value, and between which two values that changes.

A change of order is stated as the interval between the nearest values on either side of it at which both candidates were measured and differ, and never as a point inside that interval: nothing between them was run. A value inside the interval at which the two were equal, or at which one was not measured, is named beside it. The last column is the first-named candidate's value over the second's at the two ends: below 1 at the smaller value, above 1 at the larger, and how far from 1 says how decisive the change is. A change whose ratios are both within a few per cent of 1 is within the noise of a shared machine, and a pair that changes order more than once on one objective is not separated by this sweep at all.

A p99 change of order is a change in the tail of the tick times, taken over every tick after the load tick. It is a change in the cost of a rewind only where both candidates' rewinds are costlier than their ordinary ticks; for the rewinds themselves read the median rewind tick, which is taken over the ticks that open with a rewind and nothing else. Each family's section says which tick of its run the p99 is.

`history` is the rewind strategy the binary reported: `native` for a candidate that keeps its own, `snapshot_rebuild` for one measured inside the substrate's snapshot-and-rebuild wrapper, `none` on a workload that never rewinds.

### `rewind_depth` · varies `rewind_depth`

Two history strategies against how far back a rewind goes, at 10% of entities moving each tick. The median tick does not rewind and should not move with depth for either strategy. The median rewind tick is the median of the 9 ticks that open with a rewind, and is where a log that unwinds tick by tick and a rebuild that does not are expected to part as the depth grows. The p99 is the fifth costliest of the 399 ticks after the load tick: where every rewind costs more than any ordinary tick it is about the middle of the rewinds, and where a strategy's rewind costs about what an ordinary tick does, as a log's may at shallow depth, it is the tick tail rather than the rewind.

Held fixed by `rewind_depth.template`: `seed` 4006, `world_size` 1024, `world_height` 256, `initial_entities` 30000, `ticks` 400, `inserts_per_tick` 20, `removes_per_tick` 15, `move_fraction` 0.1, `speed_min` 0.5, `speed_max` 4.0, `placement` uniform, `radius_queries_per_tick` 48, `entity_radius_queries_per_tick` 48, `knn_queries_per_tick` 12, `knn_k` 8, `query_radius_min` 8, `query_radius_max` 16, `query_focus` uniform, `rewind_every` 40, `history_ticks` 32.

Every candidate here passed verification against the oracle at every value of `rewind_depth` before any was measured. Rewinds scheduled in each run: 9 at every value.

The p99 column is the harness's 99th percentile over the ticks of the median repetition after the load tick: the fifth costliest of 399. Tick 0, which inserts the whole population, is left out of every percentile and reported on its own. Every rewind is among those ticks, and a p99 value is a rewind tick only where that candidate's rewinds cost more than its ordinary ticks; any tick slowed by the machine ranks among the costliest too. The median rewind tick table measures the rewinds alone.

Median tick, microseconds, by `rewind_depth`:

| candidate | history | 1 | 2 | 4 | 8 | 16 | 32 |
|---|---|---:|---:|---:|---:|---:|---:|
| `axis_sorted` | snapshot_rebuild | 4146.8 | 4142.9 | 4153.2 | 4181.2 | 4041.8 | 4044.3 |
| `brute_force` | native | 8928.6 | 9028.1 | 8698.3 | 8622.1 | 8183.1 | 7085.4 |
| `cell_rows` | snapshot_rebuild | 726.2 | 725.8 | 721.0 | 709.5 | 726.9 | 698.2 |
| `cell_sorted` | snapshot_rebuild | 757.5 | 758.5 | 775.5 | 763.4 | 777.2 | 725.3 |
| `delta_grid` | native | 254.9 | 252.5 | 257.6 | 254.6 | 253.8 | 252.1 |
| `grid_ring_knn` | snapshot_rebuild | 248.3 | 230.0 | 244.8 | 246.9 | 225.2 | 220.6 |
| `grid_undo_log` | native | 247.3 | 247.1 | 245.9 | 248.1 | 248.9 | 242.1 |
| `morton_lbvh` | snapshot_rebuild | 820.9 | 783.0 | 786.4 | 795.9 | 765.8 | 753.4 |
| `morton_sorted` | snapshot_rebuild | 3563.8 | 3546.0 | 3529.3 | 3544.8 | 3448.1 | 3425.7 |
| `spatial_hash` | snapshot_rebuild | 598.4 | 581.7 | 590.8 | 610.7 | 577.8 | 744.4 |
| `uniform_grid` | snapshot_rebuild | 270.4 | 277.6 | 266.9 | 267.9 | 271.4 | 315.7 |

p99 tick, microseconds, by `rewind_depth`:

| candidate | history | 1 | 2 | 4 | 8 | 16 | 32 |
|---|---|---:|---:|---:|---:|---:|---:|
| `axis_sorted` | snapshot_rebuild | 6084.8 | 4708.5 | 5865.2 | 5322.9 | 4637.1 | 5258.8 |
| `brute_force` | native | 14378.2 | 14476.8 | 10851.9 | 15219.0 | 11600.2 | 13847.2 |
| `cell_rows` | snapshot_rebuild | 1028.1 | 946.7 | 891.1 | 879.5 | 1439.2 | 880.8 |
| `cell_sorted` | snapshot_rebuild | 1360.3 | 1189.7 | 1091.2 | 1031.2 | 1000.6 | 963.0 |
| `delta_grid` | native | 329.8 | 381.6 | 520.1 | 1072.4 | 1138.3 | 1301.6 |
| `grid_ring_knn` | snapshot_rebuild | 965.1 | 665.4 | 693.2 | 726.3 | 646.9 | 684.1 |
| `grid_undo_log` | native | 470.2 | 367.4 | 470.7 | 709.5 | 1197.8 | 2036.3 |
| `morton_lbvh` | snapshot_rebuild | 1345.0 | 1078.5 | 976.8 | 1191.0 | 1004.6 | 1205.5 |
| `morton_sorted` | snapshot_rebuild | 4623.3 | 4143.3 | 4589.2 | 4529.1 | 4616.7 | 3743.9 |
| `spatial_hash` | snapshot_rebuild | 2326.5 | 2386.1 | 2317.8 | 2445.4 | 2253.2 | 2905.7 |
| `uniform_grid` | snapshot_rebuild | 719.5 | 746.7 | 700.0 | 700.2 | 727.5 | 804.7 |

Peak allocated bytes, MB, by `rewind_depth`:

| candidate | history | 1 | 2 | 4 | 8 | 16 | 32 |
|---|---|---:|---:|---:|---:|---:|---:|
| `axis_sorted` | snapshot_rebuild | 18.29 | 18.29 | 18.28 | 18.28 | 18.26 | 18.23 |
| `brute_force` | native | 16.48 | 16.48 | 16.48 | 16.48 | 16.48 | 16.48 |
| `cell_rows` | snapshot_rebuild | 18.98 | 18.69 | 18.69 | 18.68 | 18.67 | 18.65 |
| `cell_sorted` | snapshot_rebuild | 18.98 | 18.69 | 18.69 | 18.68 | 18.67 | 18.65 |
| `delta_grid` | native | 5.80 | 5.80 | 5.80 | 6.35 | 6.35 | 6.35 |
| `grid_ring_knn` | snapshot_rebuild | 18.60 | 18.12 | 18.01 | 18.01 | 18.01 | 18.01 |
| `grid_undo_log` | native | 5.53 | 5.53 | 5.53 | 5.53 | 5.53 | 5.53 |
| `morton_lbvh` | snapshot_rebuild | 19.00 | 19.00 | 18.99 | 18.98 | 18.96 | 18.91 |
| `morton_sorted` | snapshot_rebuild | 18.91 | 18.91 | 18.90 | 18.89 | 18.86 | 18.81 |
| `spatial_hash` | snapshot_rebuild | 20.06 | 20.06 | 20.06 | 20.06 | 20.06 | 20.06 |
| `uniform_grid` | snapshot_rebuild | 18.59 | 18.12 | 18.01 | 18.01 | 18.01 | 18.01 |

Median rewind tick, microseconds, by `rewind_depth`:

| candidate | history | 1 | 2 | 4 | 8 | 16 | 32 |
|---|---|---:|---:|---:|---:|---:|---:|
| `axis_sorted` | snapshot_rebuild | 4410.2 | 4354.9 | 4444.6 | 4383.2 | 4225.4 | 4301.5 |
| `brute_force` | native | 8948.0 | 9310.0 | 8706.3 | 8672.8 | 7860.4 | 7061.5 |
| `cell_rows` | snapshot_rebuild | 929.6 | 924.0 | 883.7 | 879.5 | 893.6 | 873.9 |
| `cell_sorted` | snapshot_rebuild | 932.8 | 981.5 | 965.7 | 967.4 | 949.2 | 862.3 |
| `delta_grid` | native | 319.5 | 375.7 | 505.7 | 1070.3 | 1138.1 | 1301.6 |
| `grid_ring_knn` | snapshot_rebuild | 707.9 | 664.9 | 673.9 | 725.7 | 646.4 | 642.2 |
| `grid_undo_log` | native | 298.2 | 364.6 | 470.2 | 709.0 | 1184.2 | 2035.4 |
| `morton_lbvh` | snapshot_rebuild | 1076.5 | 993.5 | 976.0 | 1008.9 | 943.9 | 927.0 |
| `morton_sorted` | snapshot_rebuild | 3819.6 | 3753.9 | 3761.3 | 3760.5 | 3720.3 | 3637.2 |
| `spatial_hash` | snapshot_rebuild | 2326.0 | 2357.2 | 2317.5 | 2441.8 | 2239.9 | 2904.9 |
| `uniform_grid` | snapshot_rebuild | 719.4 | 744.7 | 699.6 | 700.2 | 727.2 | 726.2 |

Where two candidates change order along `rewind_depth`:

| objective | lower at the smaller value | lower at the larger value | order changes between | first / second, at each end |
|---|---|---|---|---:|
| p50 | `spatial_hash` | `cell_rows` | 16 and 32 | 0.79, 1.07 |
| p50 | `cell_sorted` | `morton_lbvh` | 8 and 16 | 0.96, 1.01 |
| p50 | `morton_lbvh` | `cell_sorted` | 16 and 32 | 0.99, 1.04 |
| p50 | `spatial_hash` | `cell_sorted` | 16 and 32 | 0.74, 1.03 |
| p50 | `grid_undo_log` | `grid_ring_knn` | 1 and 2 | 1.00, 1.07 |
| p99 | `cell_rows` | `cell_sorted` | 8 and 16 | 0.85, 1.44 |
| p99 | `cell_sorted` | `cell_rows` | 16 and 32 | 0.70, 1.09 |
| p99 | `delta_grid` | `cell_rows` | 4 and 8 | 0.58, 1.22 |
| p99 | `cell_rows` | `delta_grid` | 8 and 16 | 0.82, 1.26 |
| p99 | `delta_grid` | `cell_rows` | 16 and 32 | 0.79, 1.48 |
| p99 | `grid_undo_log` | `cell_rows` | 16 and 32 | 0.83, 2.31 |
| p99 | `cell_rows` | `morton_lbvh` | 8 and 16 | 0.74, 1.43 |
| p99 | `morton_lbvh` | `cell_rows` | 16 and 32 | 0.70, 1.37 |
| p99 | `delta_grid` | `cell_sorted` | 4 and 8 | 0.48, 1.04 |
| p99 | `grid_undo_log` | `cell_sorted` | 8 and 16 | 0.69, 1.20 |
| p99 | `morton_lbvh` | `cell_sorted` | 4 and 8 | 0.90, 1.15 |
| p99 | `delta_grid` | `grid_ring_knn` | 4 and 8 | 0.75, 1.48 |
| p99 | `delta_grid` | `grid_undo_log` | 1 and 2 | 0.70, 1.04 |
| p99 | `grid_undo_log` | `delta_grid` | 8 and 16 | 0.66, 1.05 |
| p99 | `delta_grid` | `morton_lbvh` | 8 and 16 | 0.90, 1.13 |
| p99 | `delta_grid` | `uniform_grid` | 4 and 8 | 0.74, 1.53 |
| p99 | `grid_undo_log` | `grid_ring_knn` | 8 and 16 | 0.98, 1.85 |
| p99 | `uniform_grid` | `grid_ring_knn` | 1 and 2 | 0.75, 1.12 |
| p99 | `grid_ring_knn` | `uniform_grid` | 4 and 8 | 0.99, 1.04 |
| p99 | `uniform_grid` | `grid_ring_knn` | 8 and 16 | 0.96, 1.12 |
| p99 | `grid_undo_log` | `morton_lbvh` | 8 and 16 | 0.60, 1.19 |
| p99 | `grid_undo_log` | `uniform_grid` | 4 and 8 | 0.67, 1.01 |
| peak bytes | `axis_sorted` | `grid_ring_knn` | 1 and 2 | 0.98, 1.01 |
| peak bytes | `axis_sorted` | `uniform_grid` | 1 and 2 | 0.98, 1.01 |
| peak bytes | `morton_sorted` | `cell_rows` | 1 and 2 | 1.00, 1.01 |
| peak bytes | `morton_sorted` | `cell_sorted` | 1 and 2 | 1.00, 1.01 |
| rewind tick | `cell_rows` | `cell_sorted` | 16 and 32 | 0.94, 1.01 |
| rewind tick | `delta_grid` | `cell_rows` | 4 and 8 | 0.57, 1.22 |
| rewind tick | `grid_undo_log` | `cell_rows` | 8 and 16 | 0.81, 1.33 |
| rewind tick | `delta_grid` | `cell_sorted` | 4 and 8 | 0.52, 1.11 |
| rewind tick | `grid_undo_log` | `cell_sorted` | 8 and 16 | 0.73, 1.25 |
| rewind tick | `cell_sorted` | `morton_lbvh` | 8 and 16 | 0.96, 1.01 |
| rewind tick | `morton_lbvh` | `cell_sorted` | 16 and 32 | 0.99, 1.08 |
| rewind tick | `delta_grid` | `grid_ring_knn` | 4 and 8 | 0.75, 1.47 |
| rewind tick | `grid_undo_log` | `delta_grid` | 8 and 16 | 0.66, 1.04 |
| rewind tick | `delta_grid` | `morton_lbvh` | 4 and 8 | 0.52, 1.06 |
| rewind tick | `delta_grid` | `uniform_grid` | 4 and 8 | 0.72, 1.53 |
| rewind tick | `grid_undo_log` | `grid_ring_knn` | 8 and 16 | 0.98, 1.83 |
| rewind tick | `grid_ring_knn` | `uniform_grid` | 4 and 8 | 0.96, 1.04 |
| rewind tick | `uniform_grid` | `grid_ring_knn` | 8 and 16 | 0.96, 1.12 |
| rewind tick | `grid_undo_log` | `morton_lbvh` | 8 and 16 | 0.70, 1.25 |
| rewind tick | `grid_undo_log` | `uniform_grid` | 4 and 8 | 0.67, 1.01 |

- `cell_sorted` and `morton_lbvh` change order 2 times on p50: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `cell_rows` and `cell_sorted` change order 2 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `cell_rows` and `delta_grid` change order 3 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `cell_rows` and `morton_lbvh` change order 2 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `delta_grid` and `grid_undo_log` change order 2 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `grid_ring_knn` and `uniform_grid` change order 3 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `cell_sorted` and `morton_lbvh` change order 2 times on rewind tick: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `grid_ring_knn` and `uniform_grid` change order 2 times on rewind tick: over part of this range they are within noise of each other, and no one crossing should be read from it.

### `rewind_move_fraction` · varies `move_fraction`

Two history strategies against the share of entities that move each tick, with the rollback schedule fixed. The median tick does not rewind: it is the cost of keeping history while the world moves. The median rewind tick is taken over the 9 ticks that open with a rewind and nothing else, so it is the cost of putting the world back. The p99 tick is taken over the 99 ticks after the load tick, which the harness reports apart; it is the second costliest of them, so where every rewind costs more than any ordinary tick it is the second costliest rewind, and where a strategy's rewind costs about what an ordinary tick does, as a log's may when little moves, it is the tick tail and says nothing about the rewind.

Held fixed by `rewind_move_fraction.template`: `seed` 4006, `world_size` 1024, `world_height` 256, `initial_entities` 30000, `ticks` 100, `inserts_per_tick` 20, `removes_per_tick` 15, `speed_min` 0.5, `speed_max` 4.0, `placement` uniform, `radius_queries_per_tick` 48, `entity_radius_queries_per_tick` 48, `knn_queries_per_tick` 12, `knn_k` 8, `query_radius_min` 8, `query_radius_max` 16, `query_focus` uniform, `rewind_every` 10, `rewind_depth` 6, `history_ticks` 12.

Every candidate here passed verification against the oracle at every value of `move_fraction` before any was measured. Rewinds scheduled in each run: 9 at every value.

The p99 column is the harness's 99th percentile over the ticks of the median repetition after the load tick: the second costliest of 99. Tick 0, which inserts the whole population, is left out of every percentile and reported on its own. Every rewind is among those ticks, and a p99 value is a rewind tick only where that candidate's rewinds cost more than its ordinary ticks; any tick slowed by the machine ranks among the costliest too. The median rewind tick table measures the rewinds alone.

Median tick, microseconds, by `move_fraction`:

| candidate | history | 0.01 | 0.02 | 0.05 | 0.1 | 0.2 | 0.35 | 0.5 | 0.75 | 1.0 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `axis_sorted` | snapshot_rebuild | 3927.1 | 3923.6 | 4011.3 | 4065.2 | 4094.0 | 4156.0 | 4263.5 | 4428.4 | 4649.4 |
| `brute_force` | native | 6287.5 | 6401.7 | 6358.0 | 6261.0 | 6349.0 | 6448.6 | 6432.3 | 6593.7 | 6977.1 |
| `cell_rows` | snapshot_rebuild | 608.3 | 626.7 | 633.4 | 669.7 | 741.5 | 846.8 | 953.6 | 1092.6 | 1256.2 |
| `cell_sorted` | snapshot_rebuild | 637.3 | 858.6 | 664.8 | 716.6 | 776.2 | 868.3 | 965.4 | 1116.6 | 1277.5 |
| `delta_grid` | native | 141.7 | 152.8 | 191.6 | 248.7 | 396.4 | 542.7 | 670.3 | 1077.8 | 1135.2 |
| `grid_ring_knn` | snapshot_rebuild | 88.4 | 103.7 | 147.6 | 210.1 | 354.2 | 532.0 | 748.9 | 1054.3 | 1372.3 |
| `grid_undo_log` | native | 104.0 | 122.3 | 162.2 | 235.9 | 394.1 | 650.3 | 941.8 | 1253.6 | 1723.4 |
| `morton_lbvh` | snapshot_rebuild | 710.5 | 700.5 | 1091.5 | 731.5 | 852.1 | 956.7 | 1001.1 | 1156.5 | 1360.5 |
| `morton_sorted` | snapshot_rebuild | 3408.2 | 3356.7 | 3442.0 | 3441.3 | 3570.9 | 3556.7 | 3725.7 | 3969.8 | 3973.1 |
| `spatial_hash` | snapshot_rebuild | 380.1 | 414.8 | 451.9 | 544.7 | 783.4 | 997.6 | 1304.1 | 1648.0 | 2022.2 |
| `uniform_grid` | snapshot_rebuild | 136.7 | 149.7 | 194.4 | 260.4 | 389.2 | 576.9 | 799.4 | 1072.1 | 1381.3 |

p99 tick, microseconds, by `move_fraction`:

| candidate | history | 0.01 | 0.02 | 0.05 | 0.1 | 0.2 | 0.35 | 0.5 | 0.75 | 1.0 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `axis_sorted` | snapshot_rebuild | 4351.9 | 4335.9 | 4561.4 | 5584.6 | 5711.4 | 5309.1 | 5891.5 | 5805.2 | 6175.9 |
| `brute_force` | native | 6978.8 | 7470.6 | 10129.5 | 7141.2 | 6903.3 | 11141.5 | 7469.3 | 10642.5 | 11503.2 |
| `cell_rows` | snapshot_rebuild | 976.7 | 916.9 | 906.7 | 1063.2 | 914.9 | 1010.2 | 1360.8 | 1264.7 | 1439.0 |
| `cell_sorted` | snapshot_rebuild | 892.7 | 1176.8 | 874.4 | 1071.5 | 919.6 | 1120.3 | 1123.1 | 1301.6 | 1489.1 |
| `delta_grid` | native | 249.0 | 305.6 | 2322.9 | 1002.1 | 973.9 | 1702.7 | 1077.4 | 1227.5 | 1663.4 |
| `grid_ring_knn` | snapshot_rebuild | 507.4 | 511.7 | 550.0 | 634.1 | 760.9 | 910.1 | 1174.8 | 1509.3 | 1902.8 |
| `grid_undo_log` | native | 149.0 | 241.7 | 354.9 | 589.8 | 1083.4 | 1733.9 | 2420.4 | 3190.6 | 4126.6 |
| `morton_lbvh` | snapshot_rebuild | 1221.3 | 981.3 | 1553.0 | 893.3 | 1031.1 | 1248.1 | 1634.4 | 1411.9 | 2092.3 |
| `morton_sorted` | snapshot_rebuild | 3748.6 | 3813.7 | 4616.7 | 3978.5 | 4454.6 | 3835.9 | 4744.4 | 5250.9 | 5228.9 |
| `spatial_hash` | snapshot_rebuild | 2122.9 | 2144.4 | 2091.5 | 2248.0 | 2572.6 | 2684.6 | 3160.8 | 3369.5 | 3881.1 |
| `uniform_grid` | snapshot_rebuild | 565.2 | 592.0 | 608.7 | 722.4 | 801.4 | 1032.0 | 2032.7 | 1506.6 | 1820.0 |

Peak allocated bytes, MB, by `move_fraction`:

| candidate | history | 0.01 | 0.02 | 0.05 | 0.1 | 0.2 | 0.35 | 0.5 | 0.75 | 1.0 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `axis_sorted` | snapshot_rebuild | 7.61 | 7.61 | 7.61 | 7.61 | 7.61 | 7.61 | 7.61 | 7.61 | 7.61 |
| `brute_force` | native | 5.95 | 5.95 | 5.95 | 5.95 | 5.95 | 5.95 | 5.95 | 5.95 | 5.95 |
| `cell_rows` | snapshot_rebuild | 8.01 | 8.01 | 8.01 | 8.01 | 8.01 | 8.01 | 8.01 | 8.01 | 8.01 |
| `cell_sorted` | snapshot_rebuild | 8.01 | 8.01 | 8.01 | 8.01 | 8.01 | 8.01 | 8.01 | 8.01 | 8.01 |
| `delta_grid` | native | 3.45 | 3.56 | 3.77 | 4.65 | 8.96 | 8.96 | 8.96 | 8.96 | 8.96 |
| `grid_ring_knn` | snapshot_rebuild | 7.33 | 7.33 | 7.33 | 7.33 | 7.33 | 7.33 | 7.33 | 7.33 | 7.33 |
| `grid_undo_log` | native | 2.96 | 2.96 | 3.18 | 3.70 | 4.76 | 6.87 | 6.87 | 6.87 | 11.09 |
| `morton_lbvh` | snapshot_rebuild | 8.29 | 8.29 | 8.29 | 8.29 | 8.29 | 8.29 | 8.29 | 8.29 | 8.29 |
| `morton_sorted` | snapshot_rebuild | 8.25 | 8.25 | 8.25 | 8.25 | 8.25 | 8.25 | 8.25 | 8.25 | 8.25 |
| `spatial_hash` | snapshot_rebuild | 7.83 | 7.83 | 7.83 | 7.83 | 7.83 | 7.83 | 9.44 | 9.04 | 8.83 |
| `uniform_grid` | snapshot_rebuild | 7.33 | 7.33 | 7.33 | 7.33 | 7.33 | 7.33 | 7.33 | 7.33 | 7.33 |

Median rewind tick, microseconds, by `move_fraction`:

| candidate | history | 0.01 | 0.02 | 0.05 | 0.1 | 0.2 | 0.35 | 0.5 | 0.75 | 1.0 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `axis_sorted` | snapshot_rebuild | 4090.4 | 4105.3 | 4190.0 | 4280.6 | 4350.3 | 4406.4 | 4413.3 | 4565.8 | 4806.8 |
| `brute_force` | native | 6359.2 | 6396.2 | 6302.4 | 6195.2 | 6317.9 | 6523.2 | 6361.4 | 6419.8 | 7139.8 |
| `cell_rows` | snapshot_rebuild | 741.3 | 754.7 | 762.6 | 806.4 | 859.1 | 973.7 | 1078.2 | 1208.5 | 1385.7 |
| `cell_sorted` | snapshot_rebuild | 784.6 | 866.8 | 799.2 | 836.1 | 895.2 | 985.8 | 1088.4 | 1220.0 | 1405.1 |
| `delta_grid` | native | 185.3 | 232.6 | 376.1 | 969.6 | 880.0 | 1008.8 | 1059.8 | 975.5 | 1039.8 |
| `grid_ring_knn` | snapshot_rebuild | 483.5 | 499.7 | 530.3 | 604.4 | 737.5 | 899.0 | 1139.1 | 1447.5 | 1842.0 |
| `grid_undo_log` | native | 138.9 | 195.7 | 334.2 | 565.4 | 1010.7 | 1667.7 | 2276.3 | 3131.9 | 3919.0 |
| `morton_lbvh` | snapshot_rebuild | 871.7 | 857.5 | 1261.0 | 866.3 | 990.7 | 1103.6 | 1135.5 | 1351.4 | 1520.3 |
| `morton_sorted` | snapshot_rebuild | 3587.0 | 3520.6 | 3623.1 | 3636.9 | 3809.5 | 3771.3 | 3931.7 | 4105.5 | 4166.7 |
| `spatial_hash` | snapshot_rebuild | 2039.3 | 2078.0 | 2065.2 | 2176.0 | 2555.5 | 2613.8 | 3013.4 | 3283.7 | 3757.7 |
| `uniform_grid` | snapshot_rebuild | 534.6 | 534.3 | 582.6 | 652.7 | 788.0 | 970.0 | 1232.5 | 1473.8 | 1803.4 |

Where two candidates change order along `move_fraction`:

| objective | lower at the smaller value | lower at the larger value | order changes between | first / second, at each end |
|---|---|---|---|---:|
| p50 | `grid_ring_knn` | `cell_rows` | 0.75 and 1.0 | 0.97, 1.09 |
| p50 | `grid_undo_log` | `cell_rows` | 0.5 and 0.75 | 0.99, 1.15 |
| p50 | `spatial_hash` | `cell_rows` | 0.1 and 0.2 | 0.81, 1.06 |
| p50 | `uniform_grid` | `cell_rows` | 0.75 and 1.0 | 0.98, 1.10 |
| p50 | `grid_ring_knn` | `cell_sorted` | 0.75 and 1.0 | 0.94, 1.07 |
| p50 | `grid_undo_log` | `cell_sorted` | 0.5 and 0.75 | 0.98, 1.12 |
| p50 | `cell_sorted` | `morton_lbvh` | 0.01 and 0.02 | 0.90, 1.23 |
| p50 | `morton_lbvh` | `cell_sorted` | 0.02 and 0.05 | 0.82, 1.64 |
| p50 | `spatial_hash` | `cell_sorted` | 0.1 and 0.2 | 0.76, 1.01 |
| p50 | `uniform_grid` | `cell_sorted` | 0.75 and 1.0 | 0.96, 1.08 |
| p50 | `grid_ring_knn` | `delta_grid` | 0.35 and 0.5 | 0.98, 1.12 |
| p50 | `delta_grid` | `grid_ring_knn` | 0.5 and 0.75 | 0.89, 1.02 |
| p50 | `grid_ring_knn` | `delta_grid` | 0.75 and 1.0 | 0.98, 1.21 |
| p50 | `grid_undo_log` | `delta_grid` | 0.2 and 0.35 | 0.99, 1.20 |
| p50 | `uniform_grid` | `delta_grid` | 0.02 and 0.05 | 0.98, 1.01 |
| p50 | `delta_grid` | `uniform_grid` | 0.1 and 0.2 | 0.95, 1.02 |
| p50 | `uniform_grid` | `delta_grid` | 0.2 and 0.35 | 0.98, 1.06 |
| p50 | `delta_grid` | `uniform_grid` | 0.5 and 0.75 | 0.84, 1.01 |
| p50 | `uniform_grid` | `delta_grid` | 0.75 and 1.0 | 0.99, 1.22 |
| p50 | `grid_ring_knn` | `morton_lbvh` | 0.75 and 1.0 | 0.91, 1.01 |
| p50 | `grid_undo_log` | `morton_lbvh` | 0.5 and 0.75 | 0.94, 1.08 |
| p50 | `grid_undo_log` | `uniform_grid` | 0.1 and 0.2 | 0.91, 1.01 |
| p50 | `spatial_hash` | `morton_lbvh` | 0.2 and 0.35 | 0.92, 1.04 |
| p50 | `uniform_grid` | `morton_lbvh` | 0.75 and 1.0 | 0.93, 1.02 |
| p99 | `morton_sorted` | `axis_sorted` | 0.02 and 0.05 | 0.88, 1.01 |
| p99 | `axis_sorted` | `morton_sorted` | 0.05 and 0.1 | 0.99, 1.40 |
| p99 | `cell_sorted` | `cell_rows` | 0.01 and 0.02 | 0.91, 1.28 |
| p99 | `cell_rows` | `cell_sorted` | 0.02 and 0.05 | 0.78, 1.04 |
| p99 | `cell_sorted` | `cell_rows` | 0.05 and 0.1 | 0.96, 1.01 |
| p99 | `cell_rows` | `cell_sorted` | 0.35 and 0.5 | 0.90, 1.21 |
| p99 | `cell_sorted` | `cell_rows` | 0.5 and 0.75 | 0.83, 1.03 |
| p99 | `delta_grid` | `cell_rows` | 0.02 and 0.05 | 0.33, 2.56 |
| p99 | `cell_rows` | `delta_grid` | 0.05 and 0.1 | 0.39, 1.06 |
| p99 | `delta_grid` | `cell_rows` | 0.1 and 0.2 | 0.94, 1.06 |
| p99 | `cell_rows` | `delta_grid` | 0.35 and 0.5 | 0.59, 1.26 |
| p99 | `delta_grid` | `cell_rows` | 0.75 and 1.0 | 0.97, 1.16 |
| p99 | `grid_ring_knn` | `cell_rows` | 0.5 and 0.75 | 0.86, 1.19 |
| p99 | `grid_undo_log` | `cell_rows` | 0.1 and 0.2 | 0.55, 1.18 |
| p99 | `cell_rows` | `morton_lbvh` | 0.05 and 0.1 | 0.58, 1.19 |
| p99 | `morton_lbvh` | `cell_rows` | 0.1 and 0.2 | 0.84, 1.13 |
| p99 | `uniform_grid` | `cell_rows` | 0.2 and 0.35 | 0.88, 1.02 |
| p99 | `delta_grid` | `cell_sorted` | 0.02 and 0.05 | 0.26, 2.66 |
| p99 | `cell_sorted` | `delta_grid` | 0.05 and 0.1 | 0.38, 1.07 |
| p99 | `delta_grid` | `cell_sorted` | 0.1 and 0.2 | 0.94, 1.06 |
| p99 | `cell_sorted` | `delta_grid` | 0.35 and 0.5 | 0.66, 1.04 |
| p99 | `delta_grid` | `cell_sorted` | 0.75 and 1.0 | 0.94, 1.12 |
| p99 | `grid_ring_knn` | `cell_sorted` | 0.35 and 0.5 | 0.81, 1.05 |
| p99 | `grid_undo_log` | `cell_sorted` | 0.1 and 0.2 | 0.55, 1.18 |
| p99 | `cell_sorted` | `morton_lbvh` | 0.01 and 0.02 | 0.73, 1.20 |
| p99 | `morton_lbvh` | `cell_sorted` | 0.02 and 0.05 | 0.83, 1.78 |
| p99 | `cell_sorted` | `morton_lbvh` | 0.05 and 0.1 | 0.56, 1.20 |
| p99 | `morton_lbvh` | `cell_sorted` | 0.1 and 0.2 | 0.83, 1.12 |
| p99 | `uniform_grid` | `cell_sorted` | 0.35 and 0.5 | 0.92, 1.81 |
| p99 | `delta_grid` | `grid_ring_knn` | 0.02 and 0.05 | 0.60, 4.22 |
| p99 | `grid_ring_knn` | `delta_grid` | 0.35 and 0.5 | 0.53, 1.09 |
| p99 | `grid_undo_log` | `delta_grid` | 0.1 and 0.2 | 0.59, 1.11 |
| p99 | `delta_grid` | `morton_lbvh` | 0.02 and 0.05 | 0.31, 1.50 |
| p99 | `morton_lbvh` | `delta_grid` | 0.1 and 0.2 | 0.89, 1.06 |
| p99 | `delta_grid` | `morton_lbvh` | 0.2 and 0.35 | 0.94, 1.36 |
| p99 | `morton_lbvh` | `delta_grid` | 0.35 and 0.5 | 0.73, 1.52 |
| p99 | `delta_grid` | `spatial_hash` | 0.02 and 0.05 | 0.14, 1.11 |
| p99 | `spatial_hash` | `delta_grid` | 0.05 and 0.1 | 0.90, 2.24 |
| p99 | `delta_grid` | `uniform_grid` | 0.02 and 0.05 | 0.52, 3.82 |
| p99 | `uniform_grid` | `delta_grid` | 0.35 and 0.5 | 0.61, 1.89 |
| p99 | `grid_undo_log` | `grid_ring_knn` | 0.1 and 0.2 | 0.93, 1.42 |
| p99 | `grid_ring_knn` | `morton_lbvh` | 0.5 and 0.75 | 0.72, 1.07 |
| p99 | `morton_lbvh` | `grid_ring_knn` | 0.75 and 1.0 | 0.94, 1.10 |
| p99 | `grid_ring_knn` | `uniform_grid` | 0.5 and 0.75 | 0.58, 1.00 |
| p99 | `grid_undo_log` | `morton_lbvh` | 0.1 and 0.2 | 0.66, 1.05 |
| p99 | `grid_undo_log` | `spatial_hash` | 0.75 and 1.0 | 0.95, 1.06 |
| p99 | `grid_undo_log` | `uniform_grid` | 0.1 and 0.2 | 0.82, 1.35 |
| p99 | `uniform_grid` | `morton_lbvh` | 0.35 and 0.5 | 0.83, 1.24 |
| p99 | `morton_lbvh` | `uniform_grid` | 0.75 and 1.0 | 0.94, 1.15 |
| peak bytes | `delta_grid` | `axis_sorted` | 0.1 and 0.2 | 0.61, 1.18 |
| peak bytes | `grid_undo_log` | `axis_sorted` | 0.75 and 1.0 | 0.90, 1.46 |
| peak bytes | `delta_grid` | `brute_force` | 0.1 and 0.2 | 0.78, 1.51 |
| peak bytes | `grid_undo_log` | `brute_force` | 0.2 and 0.35 | 0.80, 1.15 |
| peak bytes | `delta_grid` | `cell_rows` | 0.1 and 0.2 | 0.58, 1.12 |
| peak bytes | `grid_undo_log` | `cell_rows` | 0.75 and 1.0 | 0.86, 1.38 |
| peak bytes | `spatial_hash` | `cell_rows` | 0.35 and 0.5 | 0.98, 1.18 |
| peak bytes | `delta_grid` | `cell_sorted` | 0.1 and 0.2 | 0.58, 1.12 |
| peak bytes | `grid_undo_log` | `cell_sorted` | 0.75 and 1.0 | 0.86, 1.38 |
| peak bytes | `spatial_hash` | `cell_sorted` | 0.35 and 0.5 | 0.98, 1.18 |
| peak bytes | `delta_grid` | `grid_ring_knn` | 0.1 and 0.2 | 0.63, 1.22 |
| peak bytes | `grid_undo_log` | `delta_grid` | 0.75 and 1.0 | 0.77, 1.24 |
| peak bytes | `delta_grid` | `morton_lbvh` | 0.1 and 0.2 | 0.56, 1.08 |
| peak bytes | `delta_grid` | `morton_sorted` | 0.1 and 0.2 | 0.56, 1.09 |
| peak bytes | `delta_grid` | `spatial_hash` | 0.1 and 0.2 | 0.59, 1.14 |
| peak bytes | `spatial_hash` | `delta_grid` | 0.35 and 0.5 | 0.87, 1.05 |
| peak bytes | `delta_grid` | `spatial_hash` | 0.75 and 1.0 | 0.99, 1.01 |
| peak bytes | `delta_grid` | `uniform_grid` | 0.1 and 0.2 | 0.63, 1.22 |
| peak bytes | `grid_undo_log` | `grid_ring_knn` | 0.75 and 1.0 | 0.94, 1.51 |
| peak bytes | `grid_undo_log` | `morton_lbvh` | 0.75 and 1.0 | 0.83, 1.34 |
| peak bytes | `grid_undo_log` | `morton_sorted` | 0.75 and 1.0 | 0.83, 1.34 |
| peak bytes | `grid_undo_log` | `spatial_hash` | 0.75 and 1.0 | 0.76, 1.26 |
| peak bytes | `grid_undo_log` | `uniform_grid` | 0.75 and 1.0 | 0.94, 1.51 |
| peak bytes | `spatial_hash` | `morton_lbvh` | 0.35 and 0.5 | 0.95, 1.14 |
| peak bytes | `spatial_hash` | `morton_sorted` | 0.35 and 0.5 | 0.95, 1.14 |
| rewind tick | `delta_grid` | `cell_rows` | 0.05 and 0.1 | 0.49, 1.20 |
| rewind tick | `cell_rows` | `delta_grid` | 0.35 and 0.5 | 0.97, 1.02 |
| rewind tick | `grid_ring_knn` | `cell_rows` | 0.35 and 0.5 | 0.92, 1.06 |
| rewind tick | `grid_undo_log` | `cell_rows` | 0.1 and 0.2 | 0.70, 1.18 |
| rewind tick | `uniform_grid` | `cell_rows` | 0.35 and 0.5 | 1.00, 1.14 |
| rewind tick | `delta_grid` | `cell_sorted` | 0.05 and 0.1 | 0.47, 1.16 |
| rewind tick | `cell_sorted` | `delta_grid` | 0.1 and 0.2 | 0.86, 1.02 |
| rewind tick | `delta_grid` | `cell_sorted` | 0.2 and 0.35 | 0.98, 1.02 |
| rewind tick | `cell_sorted` | `delta_grid` | 0.35 and 0.5 | 0.98, 1.03 |
| rewind tick | `grid_ring_knn` | `cell_sorted` | 0.35 and 0.5 | 0.91, 1.05 |
| rewind tick | `grid_undo_log` | `cell_sorted` | 0.1 and 0.2 | 0.68, 1.13 |
| rewind tick | `cell_sorted` | `morton_lbvh` | 0.01 and 0.02 | 0.90, 1.01 |
| rewind tick | `morton_lbvh` | `cell_sorted` | 0.02 and 0.05 | 0.99, 1.58 |
| rewind tick | `uniform_grid` | `cell_sorted` | 0.35 and 0.5 | 0.98, 1.13 |
| rewind tick | `delta_grid` | `grid_ring_knn` | 0.05 and 0.1 | 0.71, 1.60 |
| rewind tick | `grid_ring_knn` | `delta_grid` | 0.35 and 0.5 | 0.89, 1.07 |
| rewind tick | `grid_undo_log` | `delta_grid` | 0.1 and 0.2 | 0.58, 1.15 |
| rewind tick | `delta_grid` | `morton_lbvh` | 0.05 and 0.1 | 0.30, 1.12 |
| rewind tick | `morton_lbvh` | `delta_grid` | 0.1 and 0.2 | 0.89, 1.13 |
| rewind tick | `delta_grid` | `uniform_grid` | 0.05 and 0.1 | 0.65, 1.49 |
| rewind tick | `uniform_grid` | `delta_grid` | 0.35 and 0.5 | 0.96, 1.16 |
| rewind tick | `grid_undo_log` | `grid_ring_knn` | 0.1 and 0.2 | 0.94, 1.37 |
| rewind tick | `grid_ring_knn` | `morton_lbvh` | 0.35 and 0.5 | 0.81, 1.00 |
| rewind tick | `grid_ring_knn` | `uniform_grid` | 0.75 and 1.0 | 0.98, 1.02 |
| rewind tick | `grid_undo_log` | `morton_lbvh` | 0.1 and 0.2 | 0.65, 1.02 |
| rewind tick | `grid_undo_log` | `spatial_hash` | 0.75 and 1.0 | 0.95, 1.04 |
| rewind tick | `grid_undo_log` | `uniform_grid` | 0.1 and 0.2 | 0.87, 1.28 |
| rewind tick | `uniform_grid` | `morton_lbvh` | 0.35 and 0.5 | 0.88, 1.09 |

- `cell_sorted` and `morton_lbvh` change order 2 times on p50: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `delta_grid` and `grid_ring_knn` change order 3 times on p50: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `delta_grid` and `uniform_grid` change order 5 times on p50: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `axis_sorted` and `morton_sorted` change order 2 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `cell_rows` and `cell_sorted` change order 5 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `cell_rows` and `delta_grid` change order 5 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `cell_rows` and `morton_lbvh` change order 2 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `cell_sorted` and `delta_grid` change order 5 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `cell_sorted` and `morton_lbvh` change order 4 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `delta_grid` and `grid_ring_knn` change order 2 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `delta_grid` and `morton_lbvh` change order 4 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `delta_grid` and `spatial_hash` change order 2 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `delta_grid` and `uniform_grid` change order 2 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `grid_ring_knn` and `morton_lbvh` change order 2 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `morton_lbvh` and `uniform_grid` change order 2 times on p99: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `delta_grid` and `spatial_hash` change order 3 times on peak bytes: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `cell_rows` and `delta_grid` change order 2 times on rewind tick: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `cell_sorted` and `delta_grid` change order 4 times on rewind tick: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `cell_sorted` and `morton_lbvh` change order 2 times on rewind tick: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `delta_grid` and `grid_ring_knn` change order 2 times on rewind tick: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `delta_grid` and `morton_lbvh` change order 2 times on rewind tick: over part of this range they are within noise of each other, and no one crossing should be read from it.
- `delta_grid` and `uniform_grid` change order 2 times on rewind tick: over part of this range they are within noise of each other, and no one crossing should be read from it.

