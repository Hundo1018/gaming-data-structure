# broken_recycle

## Claim under test

The claim is about the harness, not about the structure: **the correctness gate
rejects a structure that is fast because it answers incorrectly.**

`broken_recycle` is `aos` with the generation counter deleted. Resolving a
handle becomes one comparison cheaper and each entity is four bytes smaller.
The cost is that a recycled slot answers for a destroyed entity's handle.

## Falsifiable prediction

The gate rejects it on every workload that destroys entities, and rejects it
whether or not the workload deliberately touches stale handles: the periodic
oracle sweep checks every slot ever created, including destroyed ones.

A workload with no destroy operations cannot expose the bug and should pass.

## What would falsify it

Passing every workload. That would mean the gate does not check handle
invalidation, and every candidate that ever passed it is unverified in that
respect.
