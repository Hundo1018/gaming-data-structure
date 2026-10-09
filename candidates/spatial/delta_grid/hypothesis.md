# delta_grid

## Hypothesis

`grid_undo_log` settled that neither history strategy is the right default:
replaying a log beats snapshot-and-rebuild when 2% of entities move and loses
when all of them do, because unwinding a record costs a remove and an insert
into the index where a rebuild costs one insert per entity. Its notes end: "The
choice is set by the movement rate ... Neither strategy is the right default
without that number." This candidate does not need the number in advance. It
makes the choice itself, per tick and per query, from how many things changed,
and it keeps history out of the index so that unwinding never pays index
operations at all.

Two decisions are made at run time, each by comparing a count against a fixed
fraction:

- **How a tick is remembered.** As the records of what changed, or, when those
  records pass BETA of the id space, as a full pre-image of the world as the tick
  began.
- **How the index catches up.** By a full counting-sort rebuild, or, when the ids
  changed since the last query are at most ALPHA of the live population, by
  moving just those ids into a delta layer beside a compact base.

The bet is that both halves of the parents' trade can be had at once: the log's
cost when little moves, and the rebuild's when everything does.

## Mechanism

### History is data only

The authoritative state is two arrays indexed by id: position and liveness.
While a tick runs, the first change to each entity saves its id, previous
liveness and previous position (16 bytes; the liveness is packed into the top bit
of the id), as `UndoLogRewind` does. A stamp per id, compared against an epoch
that changes at every tick boundary and every rewind, makes the record happen
once per entity per tick.

If the records of the open tick pass BETA of the id space, the tick is turned
into a pre-image there and then: the arrays are copied and the records applied
backwards onto the copy, which leaves the positions and liveness exactly as the
tick began, together with the live count then. Records only grow within a tick,
so converting at the crossing produces the same frame that converting at
`end_tick` would, and nothing more is recorded for the rest of the tick because
the pre-image already holds every entity's starting state.

`end_tick` closes the open frame into a ring that grows by one slot per closed
tick until it holds `history_ticks + 1` slots, as the oracle's history grows, and
is circular from then on. A slot keeps its buffers, and closing a frame swaps
buffers rather than copying them, so once the ring is full no tick allocates.

Undoing a frame of either kind restores the state at the end of the frame before
it. `rewind_to(T)` therefore undoes the open tick and then every retained frame
newer than T, newest first; when any of them is a pre-image, it copies the oldest
such pre-image and undoes only the record frames older than it, because
everything newer is superseded by the copy. The frame of T itself is never
undone: it is kept so that T is known to lie inside the window, as the oracle's
snapshot of T does. Unwinding writes the two authoritative arrays and the live
count, and touches no part of the index.

### The index is derived, and maintained lazily by a cost choice

Every mutation, and every record unwound, lists its id in a deduplicated dirty
list. Copying a pre-image lists nothing and asks for a full rebuild instead. The
first query after any change brings the index up to date:

- **Rebuild**, if a rebuild was asked for or the listed ids exceed ALPHA of the
  live population: `cell_sorted`'s counting sort over the whole id space into
  per-cell contiguous ranges of ids, x, y and z, with a directory of cell+1
  offsets. A rebuild empties the delta.
- **Otherwise, apply the list to the delta.** The delta is `uniform_grid`'s
  intrusive doubly-linked per-cell lists, over the same cells, inherited from
  `uniform_grid` by path, holding exactly the entities changed since the last
  rebuild. A listed id that is in the base is tombstoned there; a live listed id
  is then linked into the list of its current cell, or relinked if it is already
  in the delta under another cell; a dead one is unlinked.

Every live entity is therefore in exactly one of base and delta, and the position
stored for it there is its current one. A radius query walks the cells of its box
in `uniform_grid`'s order, scans each cell's base range as `cell_sorted` does,
eight lanes at a time, and walks the cell's delta list. When the delta is empty
the list heads are not read at all. The box is `uniform_grid`'s widened on each
side by a millionth of `|c| + r` (see Changes after review), so that it holds
every point the shared test accepts.

Listing stops early when it can no longer change the outcome: once the list holds
more than ALPHA of every id slot, it holds more than ALPHA of the live
population whatever happens before the next query, so that query will rebuild and
further ids are not listed. The decision itself is made at the query, against the
live count then.

### Tombstones

A tombstone writes NaN into the slot's x and `kNoEntity` into its id. The radius
scan relies on the first: `dist2` against a NaN coordinate is NaN, and the shared
accept test `dist2(p, c) <= r*r` is false for NaN whatever the radius, so a
tombstone is rejected in its lane exactly like a point outside the query. The
k-nearest gather relies on the second and skips the slot by its id before
computing anything: a NaN distance compares equal to every distance under
`nearer`, which would break the strict ordering `std::partial_sort` needs and
would also count towards the k gathered. Nothing else reads a base slot.

### k-nearest

`uniform_grid`'s box doubling: start at a half-width of one cell edge, double
until the kth gathered neighbour lies within the half-width, stop past twice the
largest extent. Candidates come from each cell's base range and delta list.
Another candidate tests the search strategy; this one does not change it. Two
details differ from `uniform_grid` since the review: the box is widened as for a
radius query, which adds a row of cells only when a box edge lies within a
millionth of `|c| + r` of a cell boundary, and the stop past twice the largest
extent also requires the box to span every cell, which for a centre inside the
world it already does, so the doublings made for such a centre are unchanged.

### What is inherited

From `uniform_grid`, by inclusion, held as a member: the cell geometry (edge,
floor, clamping, order), the linked lists and their link and unlink, and the
authoritative position and liveness arrays. None of its mutators are used. From
`cell_sorted`, reproduced because its members are private: the counting sort, the
directory, the eight-lane scan and its padding. From `grid_undo_log` (that is,
`UndoLogRewind`), reproduced because its members are private and its frames
cannot hold a pre-image: the first-change-per-tick record and its stamp.

## Constants

**ALPHA = 0.5**, the share of the live population above which the listed ids are
rebuilt rather than applied to the delta. The argument counts random cache lines,
which dominate both paths:

- A rebuild touches, per live entity, one random directory entry in the counting
  pass, the same entry again and four random slots (id, x, y, z) in the scatter:
  six random lines. Per cell it makes two sequential passes over a four-byte
  entry (clear and running total), eight streamed bytes; charged as if they were
  random lines, that is an eighth of a line per cell, so at the 5.4 to 8.1 cells
  per entity of the public uniform worlds (`s01`, `s05`, the rewind sweep
  template) the cell term adds at most 0.7 to 1.0 lines per entity. Total: six to
  seven lines per live entity.
- Applying a listed id touches its slot index, the tombstone's two slots (x and
  id), its liveness and position, its delta cell, the head of its new list, the
  back pointer of that list's previous head, and its own two link fields: ten
  random lines, twelve when it was already in the delta and changed cell, and
  several of them dependent (slot then tombstone, head then previous head).

The break-even is therefore near 6.5 / 11, about 0.6 of the live population.
It is rounded down to 0.5 because an incremental pass has a further cost the
count leaves out: every query until the next rebuild reads a list head per cell
of its box and chases the delta's members, and scans the tombstones left in the
base. The cell term grows with the world: at `s02`'s 31 cells per entity the
same count puts the break-even near 0.85, and ALPHA is not adjusted for it.

Under `move_fraction: 1.0` the generator draws movers with replacement, so
`1 - 1/e`, 63.2%, of entities change in a tick. With ALPHA at 0.5 those workloads
take the rebuild path on every tick, which P3 relies on.

**BETA = 0.125**, the share of the id space above which a tick is kept as a
pre-image. The id space, `max_entity_id + 1`, is the reference rather than the
live population because a pre-image is a copy of the arrays and covers every id
slot, live or not. The argument is the restore cost of records against a copy of
the population:

- Restoring a pre-image copies 13 bytes per id slot (12 of position, 1 of
  liveness) sequentially: 26 bytes read and written, 0.4 of a line.
- Restoring a record reads it (16 bytes, sequential), then reads and writes the
  liveness and writes the position of a random id, and lists the id: about three
  random lines, 3.25 with the share of 12-byte positions that straddle a line.

Charging streamed lines at the price of random ones, one frame's records cost as
much to restore as a pre-image at 0.4 / 3.25, about an eighth of the id space.
Above that a pre-image is cheaper to restore even for a rewind one frame deep,
and a deeper rewind skips every frame newer than it. Charging streamed lines
less, as hardware prefetch makes them, would lower the break-even further; it is
not lowered, because the other two costs favour records: a pre-image costs a copy
and the records applied backwards on every tick that takes one, where records
cost nothing more to keep, and copying a pre-image forces a rebuild where
unwinding records leaves the index repair incremental while the listed ids stay
under ALPHA. At 0.125 a tick never keeps a frame costlier than
`RebuildRewind`'s per-tick snapshot plus an eighth of the id space in records.

Under the generator's sampling with replacement, `hs02`'s 2% moving is about 2%
changed per tick, so its frames are records; `hs01`'s every-entity-moving is
63.2% changed, so its frames are pre-images.

Neither ALPHA nor BETA was chosen from a measurement: both are fixed here, before
any benchmark of this structure, from the counts above.

**Ring of up to `history_ticks + 1` frames plus the open one.** The oracle keeps
`history_ticks + 1` snapshots and a rewind succeeds exactly when it holds the
target, so the ring retains the same ticks; the open tick needs a buffer of its
own while it accumulates. The ring grows with the ticks closed, as the oracle's
history does, rather than being sized from `history_ticks` at construction. With
`history_ticks` of 0 no history is kept; a rewind to the tick just closed
succeeds when nothing has changed since it closed and fails otherwise (see Risks
and Changes after review).

**Eight lanes and seven slots of padding.** `cell_sorted`'s, for its reason:
GCC's `-O2` cost model vectorises the distance test only over a trip count that
is a known multiple of the vector width, and a block that starts at a range's
last entity reads seven slots past it. Lanes past the range are masked by index,
so neither padding nor tombstones there are ever accepted.

**Cell geometry and k-nearest bounds.** `uniform_grid`'s, inherited: cell edge at
the typical query radius, floored at 1/256 of the largest extent and at 1e-4;
search from one cell edge, stopping past twice the largest extent.

**32-bit epochs.** The record stamp's epoch advances once per tick and per
rewind, and the dirty list's pass counter once per query that found work, so
neither wraps before 2^32 ticks.

## Falsifiable predictions

Preregistered before any measurement of this structure. Each states median (p50)
and 99th-percentile (p99) tick time as the harness reports them.

- **P1.** On `hs01_rewind_all_moving`, lower p50 and lower p99 than both
  `uniform_grid` under snapshot-and-rebuild and `grid_undo_log`. Mechanism: each
  tick records an eighth of the id space, then copies a pre-image; listing stops
  at half the id space; the first query rebuilds by counting sort. A rewind
  copies the oldest pre-image in the window, which is the frame just after the
  target, and the next query rebuilds; nothing is unwound record by record and
  nothing is removed from or inserted into an index.
- **P2.** On `hs02_rewind_few_moving`, p50 within 1.3x of `grid_undo_log`'s, p99
  within 1.5x of `grid_undo_log`'s, and p99 lower than `uniform_grid`'s under
  snapshot-and-rebuild. Mechanism: frames are records; each query moves the few
  listed ids into the delta; a rewind unwinds the records of the discarded ticks
  onto the arrays and lists their ids, and the next query applies them to the
  delta without a rebuild, because they stay far under half the population.
- **P3.** On the workloads that never rewind and move every entity every tick
  (`s01`, `s03`, `s04`, `hs04`), p50 within 1.2x of `cell_sorted`'s. It takes the
  same rebuild path: 63.2% changed per tick exceeds ALPHA, no history is kept
  when the workload never rewinds, and the delta is empty, so a query reads no
  list heads. What it adds is listing half the id space per tick and writing a
  slot index per id in the rebuild.
- **P4.** In the `spatial_move` sweep (2000 moves per tick against populations up
  to 128000), its fitted time exponent is at most 0.3 in both regimes, unlike
  `cell_sorted`'s. 2000 draws with replacement change `n (1 - exp(-2000 / n))`
  entities, which exceeds half the population only below n = 2885, so 1000 and
  2000 take the rebuild path and the six larger populations stay on the
  incremental path, whose cost per tick is set by the 2000 moves rather than by
  n.
- **P5.** In the rewind crossover sweep (`rewind_move_fraction` in
  `workloads/sweep/sweeps.yaml`), its p99 is within 1.2x of the smaller of
  `uniform_grid`'s and `grid_undo_log`'s at every value of `move_fraction`.
  Mechanism: at small fractions it unwinds the same records `grid_undo_log` does
  without index operations and repairs the index incrementally. From about 0.13,
  where `1 - e^-f` passes BETA, its frames are pre-images and a rewind is one copy
  followed by a rebuild. From about 0.69, where `1 - e^-f` passes ALPHA, that
  rebuild is the one an ordinary query would have made at that movement rate
  anyway; between the two, ordinary queries repair the index incrementally and
  the rewind's rebuild is a cost they do not pay (the third risk below). This
  mechanism sentence was corrected after the review; the prediction is unchanged.

## What would falsify the argument

Failing P1. That would mean that even with index-free unwinding, frames stored as
whichever form is cheaper and a lazy cost-chosen rebuild, keeping history inside
the structure costs more than snapshotting outside it when everything moves.

## Risks stated in advance

These are not predictions; they are where the mechanism above is most likely to
cost a prediction, written down before any measurement.

- **The delta is never trimmed except by a rebuild.** Under sustained low
  movement nothing crosses ALPHA after the load tick, so the delta accumulates
  the union of everything that has changed since, which after a few hundred ticks
  at 2% is most of the population, and the base fills with tombstones. A query
  then pays `uniform_grid`'s list walks plus a base scan of rejected lanes. This
  is the main risk to P2's p50.
- **P4 against the cache.** The incremental path touches more per-id arrays than
  `uniform_grid`'s move, and `brute_force`'s O(1) move measured n^0.358 in this
  family from cache misses alone; `uniform_grid`'s relink measured n^0.08.
- **P5 between the endpoints.** Around a fraction of 0.1 to 0.35 the rewind tick
  pays an unwind and a rebuild where `uniform_grid` under snapshot-and-rebuild
  pays a copy and a rebuild into linked lists, and its ordinary tick pays an
  incremental repair of up to half the population where `uniform_grid` relinks
  only the movers that changed cell.
- **No history when `history_ticks` is 0.** A workload that rewinds 0 deep with no
  retained history disagrees with the oracle here, as it does for both substrate
  history strategies; `runner/sweep.py` refuses such a workload. (Narrowed after
  the review: see Changes after review.)

## Changes after review

Made after an adversarial review and after the indicative timings reported with
the first build. None of them changes ALPHA, BETA, any prediction or the
falsification criterion. P5's mechanism sentence is the only prose under the
predictions that changed. It read: "from about 0.14 upwards its frames are
pre-images and a rewind is one copy and the rebuild a query would have made at
that movement rate anyway", which holds only above about 0.69, because below
that ordinary queries take the incremental path.

- **The ring grows with the ticks closed.** It was sized to `history_ticks + 1`
  frame headers at construction, so a workload declaring a history far longer
  than its run paid for frames that could never exist (96 MB of headers at
  `history_ticks` of 1e6) or failed to allocate them at all (4e9). It now gains
  one slot per closed tick until it holds `history_ticks + 1`, and then turns as
  before, so the frames retained, and the no-allocation property once it is full,
  are unchanged.
- **`history_ticks` of 0.** The harness passes 0 both for workloads that never
  rewind and for ones that rewind 0 deep without declaring a history, so keeping
  even the open tick's records would charge every workload that never rewinds,
  which P3 relies on not paying. Instead the structure remembers the tick last
  closed and whether anything has changed since. A rewind to that tick with
  nothing changed is honoured, because the state is already the one asked for;
  that is the rewind a depth-0 workload issues, as the first operation of its
  tick. A rewind after a change in the open tick still returns false where the
  oracle returns true; no workload file can produce one.
- **The query box is conservative and the k-nearest stop is exact.** Both were
  `uniform_grid`'s, byte for byte. Its box computed `c - r` in float, which can
  round onto a cell boundary and leave out a point one ulp below it that `dist2`
  accepts at exactly `r*r`; an infinite bound converted to an undefined integer
  and collapsed the box; and k-nearest stopped past twice the largest extent even
  when the box had not reached the far side of the world from a centre outside
  it. The box is now widened on each side by a millionth of `|c| + r` plus
  1e-20, clamped in float before conversion, and a k-nearest search stops early
  only once the box also spans every cell. The millionth is about seventeen float
  roundings of `|c| + r`: the bounds carry one rounding each, and the shared test
  can accept a point up to about three roundings of `r` beyond `r`; the 1e-20
  covers a squared difference that underflows to zero, below 2^-75. A bound moves
  across a cell boundary only for a query whose box edge lies within a millionth
  of `|c| + r` of it, so the work of a query is unchanged in practice. None of the
  three cases is reachable from a workload file, and the search strategy is the
  same.
- **`uniform_grid` is held as a mutable member, not inherited.** The first query
  after a change repairs the delta lists, and queries are const; an inherited
  grid's lists could be repaired only by casting const away. Holding it as a
  mutable member, as `cell_sorted` holds its index, removes the cast.
- **The cell walk is compiled out of line.** With the box computed inside it,
  the compiler inlined the walk into each query, and the queries of a tick of
  `s01` took about 8 us longer than before the review in a scratch phase timing;
  `[[gnu::noinline]]` on the walk restored the earlier shape, in which the
  compiler had kept it out of line by its own choice. This is an implementation
  detail chosen from a public workload, not a design constant.
