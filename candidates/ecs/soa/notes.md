# soa — observed

Run `20260904T064132Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 3 repetitions, median repetition
reported. All figures are median frame time unless stated.

## The footprint prediction held, firmly

Against `aos`, on the same index space and the same algorithm, the footprint is
about half everywhere:

| workload | soa peak | aos peak |
|---|---:|---:|
| `w02_query_heavy` | 5.50 MB | 10.50 MB |
| `w04_random_access` | 11.00 MB | 21.00 MB |
| `h01_zipf_hotspot` | 11.00 MB | 21.00 MB |
| `h02_bursty_spawn` | 22.00 MB | 42.00 MB |

This is structural, not incidental, and memory is measured rather than timed so
there is no noise in it. `aos` reserves 48 bytes of component space per slot
whether or not the entity holds those components; `soa` stores `Health` in 8
bytes and `Tag` in 4.

## The speed prediction is unresolved, and an earlier claim here was wrong

An earlier run of this suite had `soa` ahead of `aos` on
`w02_query_heavy` by 21%, and these notes recorded the narrow-query prediction
as confirmed. This run has `aos` ahead by 6.8% (5596.9 us against 5979.9 us).

The two runs are two days apart on a shared machine and absolute times moved by
about 40% across the whole population between them, so the machine is the
obvious suspect and the reversal cannot be attributed to the code. Within this
run, repetition spread for these two candidates on that workload is 1.2% to
3.9%, which is close enough to a 6.8% margin that the ordering is not something
this run establishes either.

What can be said: the layout difference does not produce a speed effect large
enough to survive a change of machine at these populations. The prediction is
not confirmed and not falsified; it is untested, and calling it confirmed
earlier was reading a single run too hard.

The experiment that would settle it needs many more repetitions, a machine that
is not shared, and a query narrow enough that the bytes not loaded are most of
the record — the current queries name two components of four.

## Where it stands

On the Pareto front of six of the ten ECS workloads, almost always because it is
the smallest of the fast candidates rather than the fastest.

## Scaling (run `sweep-20260904T081902Z`)

Lowest point-operation exponent of the five, **n^0.059** with the per-frame
passes switched off, against `aos`'s n^0.114 and the hash-map oracle's n^0.129.
Its `O(1)` claim holds.

This is the one place the layout difference between `soa` and `aos` appears as
something stable rather than something that flips between runs: a point access
touches one narrow array instead of a 56-byte record, so the working set per
operation grows more slowly with the population. The effect is small in absolute
terms and the two remain within noise of each other on frame time — see the
machine-change correction above — but the growth rates separate where the levels
did not.
