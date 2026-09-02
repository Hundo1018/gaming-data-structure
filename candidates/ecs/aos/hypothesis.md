# aos

## Hypothesis

Storing every component of an entity together in one record makes access to
several components of one entity cost a single cache line, and makes iteration
cost proportional to the number of slots ever allocated rather than to the
number of entities that match the query.

## Falsifiable prediction

On a workload dominated by point access to whole entities, `aos` is within
noise of any layout that splits components apart. On a workload dominated by
narrow iteration, `aos` moves strictly more bytes per matching entity than a
layout that stores each component separately, and its frame time is
correspondingly worse.

## What would falsify it

A query-heavy workload where `aos` matches or beats `soa` on frame time would
mean the extra bytes are free, and therefore that the layout argument does not
hold at this scale.
