# bitset_soa

## Hypothesis

`soa`'s query and `integrate` decide membership one slot at a time: for every
slot ever created they load its liveness byte and its mask byte and branch on
`alive_[i] && (mask_[i] & required) == required`. The work of a query is
therefore the number of slots, whatever the number of matches. A query naming
a component that 2% of entities hold still visits all of them, and on
`h05_sparse_component` `soa` measured 1604.3 us against `archetype`'s 562.2 us,
where `archetype` walks only the groups that match.

If that per-slot test is the cost, it can be removed without touching anything
else in `soa`. Hold membership a second time, as bit planes: one bit per slot
for liveness and one per slot per component, packed into 64-bit words. A query
ANDs, for each run of 64 slots, the liveness word with the word of every
required component, skips the word if the result is zero, and otherwise visits
its set bits with `countr_zero`. Its work becomes O(slots/64 + matches) instead
of O(slots). On a sparse mask almost every word ANDs to zero and costs one load
per plane; on a dense mask almost every word is non-zero and the scan does what
`soa`'s does.

## Mechanism

**What is unchanged.** `soa`'s index space, handles, free list, generation
counters and its seven parallel arrays (generation, mask, liveness byte,
Position, Velocity, Health, Tag), with the same growth. `get`, `set`, `mask` and
`alive` are `soa`'s code and never read the planes. Once a query or `integrate`
has found a matching slot it does exactly what `soa` does with it: gathers the
named components from their arrays into a value block and calls `digest_entity`,
or applies `p += v*dt` with the same float operations in the same order.

**The planes.** Five vectors of 64-bit words: liveness, Position, Velocity,
Health, Tag. Bit `i % 64` of word `i / 64` of a plane describes slot `i`. The
invariant, after every operation:

- the liveness bit of slot `i` equals `alive_[i]`;
- the component bit of slot `i` equals that bit of `mask_[i]`;
- every bit past the last slot is zero.

**Keeping it exact.** Each mutation writes the planes where it writes the bytes
they duplicate:

- `create` of a new slot appends one zero word to every plane when the slot is
  the first of its run of 64, which is what keeps the bits past the last slot
  zero. Whether the slot is new or recycled, all five of its bits are then
  written from the mask, set or cleared, so a recycled slot's bits do not depend
  on what they were before.
- `destroy` clears all five bits of the slot, as it clears `mask_[i]` and
  `alive_[i]`.
- `add` sets the component's bit and `remove` clears it, after the same
  handle check as `soa`, so neither touches a plane on a dead handle.
- `get` and `set` do not change membership and do not touch a plane.

**query.** The planes of the required components are collected once per call,
then for each word index `w` the liveness word is ANDed with each of them. A
zero result moves to the next word. A non-zero one is consumed lowest bit
first: slot `64w + countr_zero(bits)`, then `bits &= bits - 1`. Since `destroy`
clears a dead slot's component bits, the liveness word is redundant for a
non-empty mask; it is what makes a query over the empty mask, which the
contract allows and which matches every live entity, correct, and it costs one
word load per 64 slots. A mask is a byte, and its bits 4 to 7 name no component
and have no plane; `soa` and the oracle keep them in an entity's mask and match
on them, so a query naming one takes `soa`'s per-slot test unchanged. No
workload can build such a mask.

**integrate.** The same walk over liveness, Position and Velocity.

**Why this is the only difference.** On `w04_random_access`, whose op stream
holds only `get` and `set` and which does not integrate, every point operation
runs `soa`'s code unchanged, so any difference there is the one query per
frame.

## Constants

- **64 slots per word.** The width of a general-purpose register, and of the
  `countr_zero` and clear-lowest-bit instructions that consume a word. It is not
  tuned: a narrower word multiplies the word loop for no gain, and a wider
  vector unit would still have to hand set bits to a scalar loop.
- **One plane per component, not one interleaved record per 64 slots.** A query
  then loads only the planes of the components it names and liveness, which is
  `soa`'s own principle applied to membership. An interleaved block would load
  40 bytes per 64 slots whatever the mask.
- **No reservation.** Planes grow by `push_back`, one word per 64 new slots, and
  their size is not a parameter. From 64 slots up they reallocate at the same
  slot counts as `soa`'s arrays: a plane's word count passes a power of two at
  slot 64·2^j, which is where `soa`'s arrays double. Below 64 slots each plane
  is already one whole word, allocated with slot 0.

There are no thresholds. Nothing in the design was chosen from a workload.

## Falsifiable predictions

All against `soa` measured in the same serial run. Median frame time unless
stated.

- **P1.** On `h05_sparse_component`, the median frame time is at most 0.6x
  `soa`'s.
- **P2.** On `w01_steady_uniform`, `w02_query_heavy` and `w04_random_access`,
  each, the median frame time is within 10% of `soa`'s either way, between 0.9
  and 1.1 of it. With dense masks almost every word is non-zero and the scan
  does the same work: the per-slot test is replaced by a per-match bit step,
  and the gather and the digest, which are most of a matching slot's cost, are
  identical. On `w04` the point operations are identical code.
- **P3.** Peak allocated bytes are within 5% above `soa`'s on every ECS
  workload, public and hidden: at most 1.05x. From 64 slots up, a plane's word
  capacity is exactly `soa`'s slot capacity divided by 64, because both double
  at the same slot, so the five planes add 40 bytes per 64 slots of capacity,
  0.625 bytes per slot beside `soa`'s 42: 1.5%, with the same reallocation
  transient. Below 64 slots each plane is one whole word whatever the slot
  count, a fixed 40 bytes, which is more than 5% of `soa`'s arrays while they
  hold fewer than 20 slots. P3 therefore rests on every workload of the track
  growing past that; the public ones start at 2000 entities or more.

A control rather than a prediction: `reported_bytes` exceeds `soa`'s by exactly
the five planes' capacity in bytes.

## What would falsify it

Failing P1. Then the cost of a sparse query on `soa` is not the slots it visits
but the matches it loads: the planes remove the visit to every non-matching
slot, and if the frame does not fall to 0.6x, what remains — the gathers, the
digests and whatever else the frame does — was the cost all along.

Failing P2 on the slow side would mean that walking set bits costs more than
`soa`'s per-slot test when nearly every slot matches, so the planes buy the
sparse case at a price paid by the dense one. Failing it on the fast side would
mean the per-slot branch was a cost even with dense masks, which P2 does not
expect. Failing P3 would mean the planes' growth adds a transient that the
arithmetic above does not see.
