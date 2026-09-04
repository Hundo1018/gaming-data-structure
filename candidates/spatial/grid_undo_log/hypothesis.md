# grid_undo_log

## Hypothesis

Rollback does not need a copy of the world. Recording the previous state of
each entity the first time it changes within a tick, and replaying those records
backwards, costs a fraction of a snapshot whenever a fraction of the world moved.

## Falsifiable prediction

The index is its parent's, unchanged and included by path, so any difference
between the two is history and nothing else.

- On `hs02_rewind_few_moving`, where two entities in a hundred move per tick, it
  should beat `uniform_grid` under snapshot-and-rebuild on both memory and tail
  latency, and the memory gap should be roughly the movement rate.
- On `hs01_rewind_all_moving`, where every entity moves every tick, it should
  **lose**: the log becomes a snapshot with extra bookkeeping, and unwinding it
  costs two index operations per record where a rebuild costs one insert per
  entity.

## What would falsify it

Winning `hs01`. That would mean replaying a log beats rebuilding even when the
log is as large as the world, and the rebuild is not the right default for
rollback at all.
