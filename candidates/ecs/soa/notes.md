# soa — observed

Run `20260902T092528Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -march=native`, 5 repetitions, median repetition reported.

## The prediction held

Against `aos`, on the same index space and the same algorithm:

| workload | soa p50 | aos p50 | soa peak | aos peak |
|---|---:|---:|---:|---:|
| `w02_query_heavy` | 4004.7 us | 4845.1 us | 5.50 MB | 10.50 MB |
| `w01_steady_uniform` | 743.2 us | 882.3 us | 2.75 MB | 5.25 MB |
| `h05_sparse_component` | 1143.6 us | 1545.6 us | 11.00 MB | 21.00 MB |
| `w05_small_world` | 76.5 us | **65.7 us** | 0.17 MB | 0.33 MB |

Narrow queries are faster, the footprint is consistently about half, and the
one workload `aos` wins is the one small enough to sit in cache, where moving
fewer bytes buys nothing and the extra streams are pure overhead. That is the
predicted crossover and it appears where predicted.

The footprint difference is structural, not incidental: `aos` reserves 48 bytes
of component space per slot whether or not the entity holds those components,
while `soa` stores `Health` in 8 bytes and `Tag` in 4.

## On the Pareto front nine times out of ten

`soa` is non-dominated on every workload except `h01_zipf_hotspot`, almost
always because it is the smallest of the fast candidates rather than the
fastest. `archetype` beats it on time nearly everywhere; it stays on the front
because beating it on time costs memory.
