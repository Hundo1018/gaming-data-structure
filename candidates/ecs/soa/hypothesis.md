# soa

## Hypothesis

Splitting one record per entity into one array per component means a query
loads only the components it names. Iteration cost should fall roughly in
proportion to the fraction of the record the query actually reads, and random
access to several components of one entity should get worse by roughly one
cache line per extra component.

## Falsifiable prediction

Against `aos`, on the same index space and the same algorithm:
- narrow queries are faster and move fewer bytes;
- multi-component point access is slower;
- the footprint per entity is smaller, because small components stop being
  padded out to the width of the largest.

## What would falsify it

`soa` losing to `aos` on a query-heavy workload, or matching it on footprint.
Either would mean the split is not buying what it costs.
