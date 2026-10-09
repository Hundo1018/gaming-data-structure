# grouped_sparse_set

## Hypothesis

`sparse_set` lost `h05_sparse_component` to `archetype`, 1242.7 us against
562.2 us, and its notes name two costs the plain sparse set cannot avoid.
`integrate` walks Velocity's dense array and reaches each entity's Position
through Position's sparse array, a dependent load into an index the size of
the population. A query over several components probes the non-driving sets in
the driver's dense order, which is unrelated to how those sets are stored.

Both disappear if the entities holding both Position and Velocity are kept in
the same positions of both dense arrays. This candidate is `sparse_set` with
that one ordering invariant added: an owning group over {Position, Velocity}.
If the falsification of `sparse_set`'s prediction was about the missing
alignment, this closes most of the gap. If it was about packed arrays
themselves, it does not.

## Mechanism

**The invariant.** Let G be the number of live entities holding both Position
and Velocity. Those G entities occupy positions `[0, G)` of Position's dense
array and of Velocity's dense array, and each one sits at the same position in
both. An entity holding only one of the pair sits at position G or later in that
component's array. Health and Tag are not part of the group and their arrays are
ordered exactly as the parent orders them.

**Keeping it.** Appends and swap-erases are the parent's. Three operations are
added around them:

- *Joining.* When an entity comes to hold both of the pair, by `create` with
  both in the mask or by `add` of the second, its new component is appended as
  in the parent. It then sits at G or later in both arrays, and so does
  whatever sits at position G, which is not a member. One swap per array puts
  the entity at position G in both, and G grows by one.
- *Leaving.* Before an entity loses either of the pair, by `remove` or by
  `destroy`, G shrinks by one and the entity is swapped with the last member,
  at position G, in both arrays. Because the last member sits at G in both, the
  swap moves it to the same position in both and the alignment holds. The
  leaving entity is now at G, outside the prefix, in both.
- *Erasing.* After leaving, the parent's swap-erase runs unchanged. The entity
  and the last element of the array are both at or past G, so the element moved
  into the hole is not a member and the prefix is not disturbed.

Each join and each leave is two swaps, each swap rewriting two dense slots, two
values and two sparse entries. Nothing allocates: every vector's size changes
exactly when and by how much the parent's does.

A side effect worth stating: an entity created with both of the pair is swapped
into position G, which is the end of the prefix, so a population created in
order keeps its group prefix in creation order. Non-members are rotated towards
the tail instead.

**integrate.** Positions `[0, G)` of Position's values and of Velocity's
values belong to the same entities in the same order, and every entity holding
both is among them. `integrate` is a loop over two arrays of the group's length
with no dense-to-entity load, no mask test and no sparse lookup, which is what
`archetype` does over its matching groups.

**query.** The driver is the smallest of: the group, of size G, when the mask
contains both Position and Velocity; and each required component's set. Ties
go to the group, because it is the only driver that supplies two components
without a lookup; and since G never exceeds the size of either set of the pair,
a mask containing both is never driven from Position's or Velocity's set. The
driver's own components are read at the walk's position. Only the required
components it does not supply are probed: a mask test against those
components, then one sparse lookup each. When both of the pair are among the
probed components, a qualifying entity is a group member, so one lookup in
Position's sparse array gives the position of both values.

For the masks in the public workloads: `position+velocity` is driven from the
group and reads nothing but the two value arrays; `position` is driven from
Position's set with no probe at all; `position+velocity+health` is driven from
whichever of the group and Health's set is smaller, and from Health's set it
reaches both of the pair through one lookup.

## What differs from the parent besides the group

Two query-side changes follow from "probe only the remaining required sets"
and are not alignment as such. Both are stated so the measurement can be read:

1. The driver's component is read at its dense position. The parent reads it
   back through its own sparse array.
2. When the driver supplies every required component, there is no mask test.
   In the public workloads this affects only the `position` query of
   `w02_query_heavy`.

They make every query in this candidate slightly cheaper than the parent's for
reasons other than the group. On `h05_sparse_component`, where both queries are
driven from Health's small set, their effect is small next to `integrate`'s.
On `w02_query_heavy` the second applies to one of three queries over about
100000 entities, so part of any gain there is not attributable to the group.

The rest is the parent's code: registry, handle layout, `get`, `set`, `mask`,
appends and swap-erases. It is reproduced rather than included by path because
every member of `SparseSet` is private and the group changes what its insert
and erase do to the dense order, which neither a derived class nor a wrapper
can reach.

## Constants

There are no thresholds or sizes to tune. The two choices:

- **The group is {Position, Velocity}.** `integrate` is defined over exactly
  this pair, and every public ECS workload queries either `position+velocity`
  or a mask containing it, or `position` alone. No other pair is iterated
  together in the public set. A second group could not share either component,
  because one dense array has one order.
- **Ties go to the group.** At equal length the group reads two components
  without a lookup and any set reads at most one.

No vector is reserved in advance, as in the parent, so the two grow
identically.

## Falsifiable predictions

All are on median frame time, compared against `sparse_set` and `archetype`
measured in the same serial run. "x% faster" means a median at most (1 - x)
of the baseline's, the stricter of the two readings.

- **P1.** On `h05_sparse_component`, `grouped_sparse_set` closes at least half
  the gap between `sparse_set` and `archetype`: its median is at most
  `sparse_set - 0.5 * (sparse_set - archetype)`. On the run recorded in
  `sparse_set/notes.md` that bound would be 902.5 us; the bound that counts is
  the one computed from the measuring run. If `sparse_set` is not slower than
  `archetype` in that run, P1 is untested there, not confirmed.
- **P2.** On `w01_steady_uniform` and on `w02_query_heavy`, each, its median is
  at most 0.85 of `sparse_set`'s: `integrate` and the Position+Velocity queries
  become aligned walks. Both workloads must meet it.
- **P3.** On `w03_structural_churn`, its median is within 15% of `sparse_set`'s
  in either direction, between 0.85 and 1.15 of it: each add or remove of
  Position or Velocity that makes or breaks the pair now costs two extra swaps,
  and the cheaper `integrate` and query over a group of between a quarter and
  a half of the population should roughly pay for them.

A control rather than a prediction: peak allocated bytes and `reported_bytes`
equal `sparse_set`'s on every workload, because every vector changes size
exactly when the parent's does. A difference there would mean the pair differs
in more than order, and the predictions above would not be measuring the group.

## What would falsify it

Failing P1. Then `sparse_set`'s loss on `h05_sparse_component` is about packed
arrays themselves — the per-component split, the registry, the scattered
point access — and not about the missing alignment, because the alignment is
present here and the gap remains.

Failing P2 with P1 holding would mean the alignment pays where Health is rare
and `integrate` dominates, but not where the queries' digest work dominates a
frame. Failing P3 on the slow side would mean the swaps cost more than the walks
save at `w03`'s churn, about 2200 adds and removes per 3000 operations, half of
them naming Position or Velocity.
