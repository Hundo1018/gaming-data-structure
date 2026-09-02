# sparse_set

## Hypothesis

If each component keeps a packed array with no holes, iteration cost stops
depending on how many entities exist and starts depending on how many entities
hold that component. A query over several components can then be driven from
the rarest one.

## Falsifiable prediction

On a workload where one queried component is held by a small fraction of
entities, `sparse_set` should beat every layout that scans the whole index
space, and the margin should grow with the ratio between entity count and
component population.

## What would falsify it

Losing the sparse-component workload. That would mean the cost of probing the
non-driving sets in dense order — which is random with respect to how those
sets are stored — exceeds what is saved by not scanning holes.
