# Edge-case workloads from the C port

These files were written while the suite was ported to C11
(`benchmarks/language_port.md`). They exercise paths that `workloads/public`
and `workloads/hidden` do not reach. The port's reviewers wrote 27 of them,
and each of those has a comment header saying which path it aims at. The
porter of the cell-based candidates wrote `x1` to `x6`, which have no header.
The header of `dgr01_nan_typical_radius` describes the C build before
`a6804f7`; both builds now behave as the table below says.

They are not part of the measured suite. `runner/orchestrate.py`, `verify.py`
and `equivalence.py` read only `public/` and `hidden/`. `verify.py` and
`equivalence.py` take these files with `--extra`. `verify.py` does not filter
`--extra` by track, so pass only the files of the binary's track. Also leave
out `dgr01_nan_typical_radius`, or set a short `--timeout`, for the candidates
it hangs:

    python3 runner/verify.py gds_spatial_morton_lbvh --timeout 60 \
        --extra $(grep -l '^track: spatial' workloads/port_review/*.workload)

Both the C and the C++ builds fail two of these files in the same way. These
failures are open defects of the C++ suite:

| workload | what happens | candidates |
|---|---|---|
| `xb4_overflow_world` | a world 3e38 across; squared distances overflow to infinity; verify fails at `tick 1, op 230` on a radius query | every spatial candidate except `morton_lbvh` and the oracle |
| `dgr01_nan_typical_radius` | `query_radius_min: nan` is accepted by the parser; the k-nearest query never returns | `uniform_grid`, `grid_undo_log`, `cell_sorted`, `cell_rows`, `morton_sorted`, `spatial_hash` |

The two ECS negative controls fail most of the ECS files here, as they should:
- `broken_recycle` fails all 12 except `xa02_block_boundary`, which never
  destroys an entity.
- `query_memo` fails all except `xa02_block_boundary` and
  `xa01_integrate_no_query`, which asks no query to memoise.

Every other candidate passes verify on every other file. `dgr01` was checked on
all eleven spatial binaries.

On `xb4_overflow_world`, the bench runs of seven candidates did not finish
within 15 minutes (with three jobs in parallel): `cell_rows`, `cell_sorted`,
`grid_ring_knn`, `grid_undo_log`, `morton_sorted`, `spatial_hash` and
`uniform_grid`.
