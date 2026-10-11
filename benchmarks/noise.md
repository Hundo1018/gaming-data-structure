# Where the timing noise comes from, and what it allows

The C port's paired timing (`language_port.md`) found a median run-to-run
spread, (max − min) / median over five rounds, of 26% for `total_ns` and 29%
for `step_ns_p50`. `predictions.md` marked about a third of its verdicts
*within noise*. This file records what was tested to find the source of that
spread, what was found, and what changed in how verdicts are reported.

Machine: 4 vCPU, Intel Xeon @ 2.80GHz, a cloud guest. `perf_event_open`
returns `ENOENT`, so there are no hardware counters. Raw data is in
`benchmarks/noise/`.

**This is not the machine `results.json` was measured on.** That run reports
an Intel Xeon @ 2.10GHz. On the 163 candidate-workload cells both runs
measured, the coefficient of variation of `step_ns_p50` across repetitions has
a median of 2.0% in `results.json` and 15.5% here, about 8 times higher. The
noise figures below describe this machine. The section *Replication* shows
what that means for `results.json`.

## Question and competing explanations

Why does the same binary on the same workload vary by 20–35%? Each explanation
predicts something different:

| explanation | prediction |
|---|---|
| H1. Address layout (ASLR, heap placement) differs per process | spread between processes larger than within one; `setarch -R` reduces it |
| H2. The scheduler moves the process between CPUs | `taskset` to one CPU reduces the spread |
| H3. Page faults: each repetition maps fresh zeroed memory | many faults, a large system-time share; keeping freed memory in glibc reduces the spread |
| H4. The host: other guests and the hypervisor take CPU time, cache and memory bandwidth | a control with no candidate in it also varies; memory-bound work varies more than pure arithmetic; pinning and layout do not help |

## Method

`runner/noise_probe.py` ran 5 binaries × 2 workloads each, plus
`runner/noise_control.cpp` in three forms:
- `compute`: an integer multiply-add chain.
- `memory`: a dependent random walk over 64 MiB.
- `cache`: the same walk over 6 MiB.

Each cell was run 8 times under each of 4 conditions:
- `plain`
- `pinned`: `taskset -c 3`
- `noaslr`: `setarch -R`
- `pinned_noaslr`: both

Each invocation used 5 repetitions after 1 warmup. The conditions were
interleaved round by round, in a rotating order. A second run compared `plain`
with `retain`, where glibc's mmap and trim thresholds are set to 4 GiB so freed
memory is reused.

The probe recorded, per invocation:
- every repetition's `total_ns` and `step_ns_p50`
- guest steal time from `/proc/stat`
- the child's minor faults
- user and system time

`runner/noise_stats.py` computed, per cell:
- the lag-1 autocorrelation of consecutive repetitions
- the coefficient of variation
- a bootstrap 95% interval of the median of n repetitions

## Results

Spread of the repetitions inside one invocation, median over 8 invocations,
`total_ns`. Full table: `noise/probe_conditions.txt`.

| cell | plain | pinned | noaslr | pinned + noaslr |
|---|---:|---:|---:|---:|
| control `compute` | 3.9% | 3.9% | 3.9% | 3.5% |
| control `cache` (6 MiB) | 12.8% | 10.8% | 10.0% | 14.9% |
| control `memory` (64 MiB) | 10.4% | 10.8% | 14.0% | 15.6% |
| `aos` w01 | 22.3% | 18.8% | 29.0% | 23.1% |
| `aos` w04 | 15.3% | 17.1% | 18.8% | 22.3% |
| `archetype` w01 | 21.9% | 35.1% | 31.5% | 23.4% |
| `archetype` w04 | 29.1% | 26.4% | 18.4% | 24.4% |
| `uniform_grid` s01 | 30.9% | 18.3% | 25.7% | 32.8% |
| `uniform_grid` hs03 | 19.8% | 18.5% | 20.9% | 16.7% |
| `morton_lbvh` s01 | 29.8% | 21.3% | 21.4% | 23.6% |
| `morton_lbvh` hs03 | 20.2% | 24.0% | 19.1% | 22.4% |

- **H1 and H2 are rejected.** The spread inside one process, where layout and
  placement do not change, is already 15–35%. Neither pinning nor disabling
  ASLR lowers it, or the spread between processes, consistently: in the table above, a condition lowers the spread
  in some cells and raises it in others.
- **H3 is rejected.** `aos` on w01 takes about 10,000 minor faults per
  invocation and spends 1.0% of its CPU time in the kernel. With `retain`, its
  spread inside one process went from 32.7% to 22.7%, but the spread between
  processes went from 41.1% to 43.7%. `uniform_grid` on s01 went from 26.9% to
  20.8% inside one process and from 24.1% to 47.6% between processes
  (`noise/probe_retain.txt`). The 8 invocations of one condition do not
  separate these changes from the spread itself.
- **H4 fits.** The control varies with no candidate in it: 4% for arithmetic
  and 10–15% for memory-bound walks. Steal time over a whole condition was 15
  to 156 jiffies (0.15–1.6 s over several minutes), too small to account for
  the spread alone. So the host's effect on memory access is the remaining
  explanation. The candidates vary more than the memory control (15–35%), and
  this data does not say why. One possibility is that their working sets of a
  few MB are more exposed to other guests' use of the shared L3 than either
  control. That was not tested.

### The noise is fast

`noise/stats_total_ns.txt`: across the 8 candidate cells, the lag-1
autocorrelation of consecutive repetitions ranges from −0.11 to 0.03 for
`total_ns`, and from −0.18 to 0.05 for `step_ns_p50`. A slow repetition, about
0.3–0.5 s long, says nothing about the next one. Two consequences follow:

- Interleaving two candidates (`runner/ab.py`, `build_ab.py`) cancels drift
  that is slower than a round. It does not cancel this noise. Only more
  repetitions shrink it.
- Repetitions can be resampled as independent draws. That is what the
  bootstrap below assumes.

The distribution is close to symmetric: (mean − median) / sd is between −0.19
and 0.16 for `total_ns`, and between −0.44 and 0.12 for `step_ns_p50`. The
disturbances do not only slow runs down, so the fastest of n is not a steadier
estimate than the median. In the bootstrap, the minimum's interval was wider
than the median's in 6 of 8 cells at n = 5.

### What n repetitions buy

From the bootstrap 95% interval of the median, as ± a fraction of the median
(`total_ns`), over the 8 candidate cells:

| n | smallest | largest |
|---:|---:|---:|
| 1 | 20% | 32% |
| 5 | 14% | 19% |
| 15 | 6% | 13% |
| 45 | 3% | 8% |

These come from 40 repetitions per cell. The n = 45 row resamples more values
than were measured, so it is optimistic.

The suite measures with 5 repetitions. On this machine, a median of 5 is known
to roughly ±15%, and the ratio of two medians, which carries the noise of both, to roughly ±20%
(√2 × 15%, if the two are independent). Most bounds in the
predictions are 0.9x, 1.05x, 1.1x or 1.2x, which is closer than that.

## What changed

1. **`runner/predictions.py` reports a 95% interval** for every point computed
   from step percentiles. The interval resamples the repetitions of each
   measurement the point uses, 2000 times. A verdict is marked:
   - *resolved* when the intervals lie on the verdict's side of the bound;
   - *unresolved* when they cross it;
   - *no interval* when the points are sweeps or memory, which have no
     repetitions to resample here.

   Verdicts are unchanged: they are still the preregistered rule applied to the
   medians. On the run in `results.json` (5 repetitions), the 53 decided
   verdicts split as follows.

   | verdict | resolved | unresolved | no interval |
   |---|---:|---:|---:|
   | HELD | 16 | 10 | 12 |
   | FALSIFIED | 8 | 4 | 3 |

   The old *within noise* mark and the interval disagree on 9 verdicts:
   - 4 are marked within noise and resolved;
   - 5 are not marked and unresolved.

   The mark compares a point's distance from its bound with the range of 5
   repetitions. The interval asks the question directly.

   With 5 repetitions the bootstrap can only return values that were measured,
   so the interval is, if anything, too narrow.

2. **A replication with 30 repetitions** of every point the 14 unresolved
   verdicts use. See below.

`predictions.py` and the noise marks report the noise of the run they are
given, so they stay correct on either machine.

## Replication of the unresolved verdicts

### Forecast, written before the run

`noise/replication_prediction.json` was written before the run started. For
each of the 14 unresolved verdicts it records the distance of the deciding
point from its bound. It forecasts "resolves" when that distance exceeds 6%,
the interval half-width expected at n = 30 from the table above, and "stays
unresolved" otherwise: 7 of each.

### Run

```
python3 runner/orchestrate.py --skip-build --repeats 30 --only archetype,bitset_soa,\
  cell_rows,cell_sorted,delta_grid,fused_archetype,grid_ring_knn,grid_undo_log,\
  grouped_sparse_set,soa,sparse_set,spatial_hash,uniform_grid \
  --results benchmarks/noise/replication_results.json
python3 runner/predictions.py --results benchmarks/noise/replication_results.json \
  --out benchmarks/noise/replication_predictions.json \
  --report benchmarks/noise/replication_predictions.md
python3 runner/replication_compare.py benchmarks/predictions.json \
  benchmarks/noise/replication_predictions.json \
  --forecast benchmarks/noise/replication_prediction.json \
  --md benchmarks/noise/replication_compare.md
```

The run covered every workload of the 13 candidates that the 14 predictions
compare, at 30 repetitions after 1 warmup, serially. Timing took 57 minutes, after the verify gate. The build was this
branch's `7e3f30b`. Of the candidates involved, three changed in that commit:
- `spatial_hash`, in an integer conversion whose results are the same inside
  the clamp;
- `sparse_set` and `grouped_sparse_set`, in the branch for an empty query
  mask, which no measured workload takes.

Every cell passed verify.

### Result

The full table is in `noise/replication_compare.md`.

- **6 of 14 verdicts replicated.** The 8 that did not all went from HELD to
  FALSIFIED: `bitset_soa` P1, `cell_rows` P1, `cell_sorted` P2 and P5,
  `delta_grid` P2b and P3, `fused_archetype` P3, and `grid_ring_knn` P3.
- **6 of 14 are resolved in the replication.** The 6 are:
  - `delta_grid` P1b, P2a and P3;
  - `cell_rows` P1;
  - `grouped_sparse_set` P1;
  - `spatial_hash` P1.
- **The forecast came true for 5 of 14.** It assumed the intervals would shrink
  to about ±6%. They did not: across the replication's cells the CV of a
  repetition is 13–18% (p25 to p75), and the deciding intervals at n = 30 still reach 3–21% from the
  median on one side or the other. Example: `bitset_soa` P1 on
  h05_sparse_component, at most 0.6, measured 0.679 [0.583, 0.824]. The
  forecast took the n = 30 width from a table built on 40 pooled repetitions
  of 8 cells, and the pooled bootstrap understated the spread of the ratio of
  two medians.

### Why the replication cannot overturn the original

The replication changed two things at once: the noise and the machine.

- **Noise:** CV 2.0% → 15.5% (median over the 163 shared cells).
- **Machine:** the median `step_ns_p50` of a cell is 0.98 to 4.81 times its
  value in `results.json`, with a median of 1.59. The factor differs by
  candidate and workload. For example, `archetype`'s cells range from 1.38 to
  4.78, and `grid_undo_log`'s from 1.00 to 1.88.

A slowdown that differs by candidate changes the ratios that the predictions
bound. `delta_grid` P3 bounds its time at 1.2x `cell_sorted`'s on
s03_wide_radius. The replication measured 1.535, with an interval of
[1.33, 1.69]. That is not noise around a ratio below 1.2: on this machine the
ratio is higher. So a flipped verdict here says that the verdict depends on the
machine. It does not say that the original was wrong on its own machine.

What the data supports:

1. On the 2.10GHz machine the noise was low (CV 2%). Its 14 unresolved verdicts
   are those whose deciding points sit within a few percent of the bound.
2. On this machine the noise is 8 times higher. Five repetitions cannot settle
   a bound closer than about 20%, and 30 repetitions about 10–20%.
3. The ratios between candidates move between the two machines by more than
   either machine's noise. A prediction that bounds a ratio at 1.05x or 1.2x is
   therefore a claim about one machine. Of the 53 decided verdicts, the
   replication tested 14, and 8 of those changed.

## What this changes in practice

- Report the machine with every verdict. `results.json` already records the
  CPU string; `predictions.md` now carries the intervals.
- Do not compare a measurement on this machine with `results.json`. Measure
  every candidate a comparison needs in the same run, as `orchestrate.py`
  already does.
- On this machine, timing predictions need margins of 20% or more, or 30+
  repetitions and margins of 10–20%. Tighter claims need a quieter machine or
  a deterministic metric. With `perf_event_open` unavailable, the
  deterministic option here is instruction counts under cachegrind, which
  measure work, not time. The C port evidence used them, in
  `language_port.md`.
- Whether the machine dependence found here is general is the open question.
  It would take a third machine, measuring the same 13 candidates in one run.
