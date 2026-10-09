# Edge-case workloads from the C port's review

Written by the agents that reviewed the C11 port (`benchmarks/language_port.md`)
to reach paths that `workloads/public` and `workloads/hidden` do not. Each
file's comment header says which path it aims at.

They are not part of the measured suite. `runner/orchestrate.py`, `verify.py`
and `equivalence.py` read only `public/` and `hidden/`; pass these with
`--extra` to run them:

    python3 runner/verify.py gds_spatial_uniform_grid --extra workloads/port_review/*.workload

Both the C and the C++ builds fail three of them in the same way, and these
failures are open defects of the C++ suite:

| workload | what happens | candidates |
|---|---|---|
| `xb4_overflow_world` | world 3e38 across; squared distances overflow to infinity; verify fails at `tick 1, op 230` on a radius query | every spatial candidate except `morton_lbvh` and the oracle |
| `dgr01_nan_typical_radius` | `query_radius_min: nan` is accepted by the parser; the k-nearest box loop never terminates | `uniform_grid`, `grid_undo_log`, `cell_sorted` |
The two ECS negative controls fail most of the ECS files here, as they
should: `broken_recycle` all 12 but `xa02_block_boundary` (which never
destroys an entity), and `query_memo` all but `xa02_block_boundary` and
`xa01_integrate_no_query` (which asks no query to memoise). Every other
candidate passes verify on every other file. The bench runs of
`xb4_overflow_world` take more than 15 minutes for the grid-based candidates.
