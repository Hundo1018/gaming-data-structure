# fused_archetype

## Hypothesis

In every ECS workload that integrates, a frame is point operations, then
`integrate` (`p += v*dt` over every entity holding Position and Velocity), then
one to three queries, then `sync` (`Replay::end_of_frame` in
`substrate/include/gds/harness.hpp`). `integrate` and each query are full
passes. `archetype` walks the Position and Velocity columns once for
`integrate` and again for every query that names them.

The contract allows deferring work as long as no observation is stale, and the
per-call salt rules out carrying an answer from one call to the next but not
fusing work inside one call. This candidate is `archetype` with `integrate`
deferred into the next query and applied in the walk that answers it. If the
second walk over the columns is a material share of the frame, removing it
shows in the frame time. If it does not show, the second walk was not what the
frame was spending.

## Mechanism

**Recording.** `integrate(dt)` sets a pending flag and stores `dt`. Nothing is
walked. If a step is already pending it is resolved first with a plain pass,
because two steps cannot be combined: `(p + v*a) + v*b` is not `p + v*(a + b)`
in float arithmetic. At most one step is ever pending.

**Resolving in a query.** With a step pending, `query(m, salt)` hands the call
to `query_and_step`, which walks every group once and treats each by two tests:
whether it holds both Position and Velocity (it is *stepped*), and whether its
mask contains `m` (it is *matched*).

- Stepped and matched: the group is walked in blocks of 256 rows. For each
  block the parent's `integrate` loop runs over those rows, and then the
  parent's query loop digests the same rows, reading the positions the step has
  just written.
- Stepped, not matched: the parent's `integrate` loop over the whole group.
- Matched, not stepped: the parent's query loop over the whole group.
- Neither: skipped.

The flag is cleared, so the remaining queries of the frame take the parent's
query unchanged: with no step pending, `query` is the parent's loop, character
for character. In `w01_steady_uniform` the one query is
`position+velocity`, and in `w02_query_heavy` the first is `position`; both
match every stepped group, so in both the whole step is fused into the first
query.

**Why blocks rather than rows.** Fusing row by row would put the step's scalar
float multiplies and adds into the digest loop and give up the vectorised
`integrate` loop. A slowdown from that would be a cost of the loop shape and
would be read as evidence about memory traffic, which it is not. Fusing by
blocks keeps both loops as the parent's and changes one thing: the digest's
loads of Position and Velocity hit lines that the step brought into L1 a few
thousand cycles earlier, instead of coming in again from L2 or L3.

**Bit identity.** Every position receives exactly one `p.x += v.x * dt`,
`p.y += v.y * dt`, `p.z += v.z * dt` per `integrate` call, with that call's
`dt` and the velocity the entity held when `integrate` was called, before
anything observes it. That is the parent's arithmetic in the parent's order
per entity, under the project's `-ffp-contract=off`, so every observation is
bit-identical to applying the step at `integrate` time. Applying a step whose
`dt` is zero is not skipped: where the velocity component is positive it turns
a position component of `-0` into `+0`, and so does the eager step.

**Which operations resolve first.** The step changes one thing: the Position of
an entity holding both Position and Velocity. An operation resolves with a
plain pass exactly when it could observe that value, change it, change the
velocity the step reads, or change which entities the step covers.

| operation | resolves | why |
|---|---|---|
| `create` | when the mask holds both | the new row would otherwise receive a step recorded before it existed |
| `destroy` | never | the swap-remove moves a row's Position and Velocity together, and the destroyed entity's step cannot be observed |
| `add` | Position or Velocity, when the entity holds both afterwards | an overwrite of Position would get the step on top of the new value; an overwrite of Velocity would change the step's input; gaining the second of the pair moves the entity into a stepped group it was not in at `integrate` |
| `remove` | Velocity from an entity holding Position | the position stays and needs the step computed from the velocity being discarded. Removing Position discards the only value the step changes, and no later operation can read it back |
| `get` | Position of an entity holding both | the one value the step changes |
| `set` | Position or Velocity of an entity holding both | as an overwriting `add` |
| `mask`, `alive`, `entity_count` | never | the step changes no mask, no liveness and no count |
| `sync` | always | so no step outlives the frame it was recorded in |

Adding or removing Health or Tag moves the entity between groups, carrying
Position and Velocity together into a group that is stepped exactly when the
old one was, so it needs no resolve.

The harness never calls a point operation between `integrate` and `sync`, so
`verify.py` cannot reach the conditional resolves above, and nothing in the
repository tests them. They were checked outside it, in a scratch directory,
by two independent differential tests: one against the oracle, one in lockstep
against the oracle and `archetype` together. Both interleave every operation
with `integrate` at random, including gets and sets while a step is pending,
two `integrate` calls in a row, queries with any mask, and `dt` of zero,
negative zero, negative, NaN, infinity, a denormal and 1e30. Each resolve was
then deleted, or its condition narrowed, one at a time, and both tests failed
on every such variant. A regression in these branches would pass the
repository's gate; catching one needs an interleaving test in the substrate.

**Const.** `query` and `get` are const in the contract and may have to apply
the step, so the group vector and the pending state are `mutable`. Resolving
changes no answer the structure gives, only how far its columns lag behind
those answers.

## What differs from the parent

The parent's code is reproduced rather than included by path because every
member of `Archetype` is private, and the fused query has to write a group's
Position column and read every column of a group by row, which neither a
derived class nor a wrapper around the public interface can do.

The predictions compare frame time against `archetype`, and a change in code
shape alone, such as factoring a loop into a helper, moves frame time on this
track by as much as the 5% thresholds. So the reproduction keeps the parent's
text wherever the mechanism does not need to change it: the registry, the
handle layout, the group table, append, swap-remove, the move between groups,
and `query` with no step pending. With comments stripped, and apart from the
class name, `#include <algorithm>` and the two constants `kPair` and
`kFuseRows`, the two headers differ in these places and no others:

- `integrate` records the step. The pass that applies a step on its own,
  `resolve_pending`, is the parent's `integrate` loop moved there unchanged.
- `query` with a step pending returns `query_and_step`, the one new walk.
- `create`, `add`, `remove`, `get`, `set` and `sync` each test the pending
  flag and may call `resolve_pending`; one statement is inserted into each, and
  the rest of each body is the parent's.
- `query_and_step` and `resolve_pending` are kept out of line
  (`[[gnu::noinline]]`). Inlined, the new walk would be compiled into the same
  function as the parent's query loop, and the step pass into every point
  operation, and the code around them would no longer be compiled as the
  parent's is.
- The group vector and the pending state are `mutable`, as above.
- `reported_bytes` also counts the group vector itself, a heap allocation the
  parent owns and leaves out. It exceeds the parent's by that vector's capacity
  times `sizeof(Group)`, a few kilobytes at most with sixteen possible groups.

One difference cannot be removed. `Replay::end_of_frame` calls `integrate` and
then the queries, and the compiler inlines both into it. In the parent the
integrate loop is compiled next to the query loop; here it is not, because the
mechanism moves the step out of `integrate`. That can change how the parent's
query loop is compiled even though its text is the same.

## Constants

- **256 rows per fused block.** Upper bound: the block must still be in L1 when
  the digest reads it. A row of all four columns is 36 bytes, so a block is at
  most 9 KB: under a fifth of the 48 KB L1d of the measuring machine (Xeon,
  family 6 model 207) and under a third of a 32 KB L1d, leaving room for the
  stack and for the lines the prefetcher brings in for the next block. Lower
  bound: per block the overhead is two loop entries and a `min`, tens of
  cycles, against 256 digests that each cost tens of cycles, under 1%. Within
  that range 256 is a power of two at which every column's block is a whole
  number of 64-byte lines (48, 48, 32 and 16), so block boundaries fall at the
  same offset within a line in every block. Nothing was tuned: no workload was
  run before this number was chosen.
- **At most one pending step.** Required by bit identity, as above.

No other constants. No vector is reserved, as in the parent, so the two
allocate identically.

## An estimate, made before any measurement

Recorded so the result can be read against it. It is arithmetic, not a
measurement, and it does not change the predictions below.

The digest is the dominant cost of a query walk. For one entity under
`position+velocity` it is eight `splitmix64` calls that depend on the row (the
salt's is loop-invariant): sixteen 64-bit multiplies, in two independent chains
of six that join into a chain of four, tens of cycles per entity. The step is three multiplies and three adds
per entity in a vectorised loop. What fusion removes is the step walk's own
loads and stores; the query's second read of the columns was already
overlapped with the digest's arithmetic.

- `w01_steady_uniform`: about 40000 entities hold both (`p_velocity` 0.8 of
  50000). Their Position and Velocity columns are about 0.96 MB, inside the
  2 MB L2 of the measuring machine. The step walk is about 1.4 MB of L2
  traffic, on the order of 10 to 30 us, against the 1115.7 us median recorded
  for `archetype` in `benchmarks/report.md`: one to three percent.
- `w02_query_heavy`: about 90000 entities hold both. The three queries read
  Position, Velocity and Health, about 2.8 MB together, more than L2, so the
  step walk's 3.2 MB of traffic comes mostly from L3, on the order of 130 to
  220 us against 4974.1 us: three to four percent.

By this estimate both P1 and P2 fail, P1 by less. If they hold, the estimate
has the cost of a memory walk at this size wrong, and that is itself the
finding.

## Falsifiable predictions

All are on median frame time, compared against `archetype` measured in the
same serial run. "At most 0.95x" means a median no greater than 0.95 times
`archetype`'s.

- **P1.** On `w02_query_heavy`, the median frame is at most 0.95x
  `archetype`'s.
- **P2.** On `w01_steady_uniform`, the median frame is at most 0.95x
  `archetype`'s.
- **P3.** On no ECS workload, public or hidden, is the median frame more than
  1.05x `archetype`'s.

A control rather than a prediction: peak allocated bytes equal `archetype`'s on
every workload, because no allocation is added and every vector grows when the
parent's does. A difference there would mean the pair differs in more than the
deferral.

A second control, added after review and not part of the preregistration: the
mechanism-off build. It is this header with one line added at the end of
`integrate`, a call to `resolve_pending()`, so the step is applied at
`integrate` time and the fused walk never runs, and every other difference
listed above stays. It changes no prediction. P1 to P3 are scored against
`archetype`, as registered. The control says how much of a ratio to `archetype`
is the mechanism's. Where the mechanism-off build itself differs from
`archetype`, that difference is code shape, and the candidate's ratio to
`archetype` on that workload is not the mechanism's alone.

## What would falsify it

Failing both P1 and P2. Then the second pass over the columns is not a material
share of the frame, and memory traffic is not what these frames are spending.

Holding P1 and failing P2 would fit the reading above: `w02`'s columns exceed
L2 and the walk removed came from L3, while `w01`'s fit in L2. Failing P3 would
mean the fused walk, or the flag tested by every point operation, costs
something the separate walks did not.
