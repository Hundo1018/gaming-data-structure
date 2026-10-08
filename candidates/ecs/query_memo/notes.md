# query_memo — observed

Measured with `runner/ab.py`, which runs the candidates in interleaved rounds so
that machine drift is shared between them. Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`. Median frame time across rounds.

## The hole is real

All three predictions held.

1. **The gate accepted it on all 10 ECS workloads.** Every answer it gives is
   correct, so per-operation comparison against the oracle, the periodic sweep
   and the benchmark checksum all agree with it.
2. **On `w04_random_access` it is the fastest thing in the population**, by a
   wide margin, five interleaved rounds:

   | candidate | median frame | ratio to `soa` |
   |---|---:|---:|
   | `soa` | 4446.3 us | 1.000 |
   | `archetype` | 4102.1 us | 0.923 |
   | `query_memo` | 1602.6 us | 0.360 |

3. **On `w02_query_heavy`, which integrates every frame, it gains nothing**:
   4894.4 us against `soa`'s 4866.6 us over three rounds, ratio 1.006.
   `integrate` marks every Position mask stale, and the recompute is the same
   pass `soa` does.

## What it says about the suite

A structure was 2.6x faster than the best honest candidate on a public workload
by never finding the answer. Under the contract as written that was legal.
Nothing about how the answer was reached was checked, only the answer's digest,
and that digest was a quantity a structure could maintain without ever
enumerating anything.

It also says something about `w04_random_access` itself. The prediction was
"faster by the share of the frame the query pass takes", and that share turned
out to be about 64%: the one full query over 200000 slots costs more than the
20000 random point operations the workload is named after. That is why
`aos/notes.md` found the four layouts within 2.7% of each other there. The
workload mostly measures one digest pass, which is the same work for every
layout.

## Fix

Next commit: every query carries a salt that changes between calls and is
folded into each entity's digest, so the answer to one call says nothing about
the next, and a running total cannot be kept. `query_memo` stays in the
population unchanged in logic as the control that the fix holds.
