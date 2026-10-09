# bitset_soa — observed

Run `20261009T042136Z` (suite) and `sweep-20261009T044632Z` (sweeps), Intel
Xeon @ 2.10GHz, GCC 13.3.0, `-O2 -DNDEBUG -ffp-contract=off -march=native`, 5
repetitions. Verdicts are `benchmarks/predictions.md`'s; step times there are the
median across repetitions.

## Two of three held, and the falsified one was too cautious

| | prediction | measured | verdict |
|---|---|---:|---|
| P1 | `h05_sparse_component` at most 0.6x `soa` | 0.562x | held |
| P2 | `w01`, `w02`, `w04` within 10% of `soa` either way | 0.807x, 0.873x, 0.901x | **falsified** |
| P3 | peak memory within 5% above `soa` everywhere | 1.007x | held |

P2 assumed that with dense masks almost every word is non-zero and the bitset
scan does the same work as `soa`'s mask test. It is faster than that on two of
the three: 19% on `w01` and 13% on `w02`. Few words are skipped there, so the
saving is not skipped work. The likelier cause is a branch: `soa` tests each slot
with a data-dependent branch, and with `p_velocity` at 0.8 about one slot in five
fails a Position+Velocity test at random, which a predictor cannot learn; walking
set bits with countr_zero has no branch per slot. That is a hypothesis about the
cause, not a measurement of it: the branch-miss counter is exactly what
`perf_event_open` would give and this container does not expose it.

## The generalist of the ECS track

On the Pareto front of 11 of the 12 ECS workloads, more than any other candidate
in either track; `soa` is on 10. On `w03_structural_churn` it has the lowest
median of every candidate, 268.1 us against `archetype`'s 343.9 us: structural
change on a flat index space is setting a byte and a bit, where `archetype` copies
the entity into another group. On the iteration-heavy workloads it is level with
`archetype` (1.00x on `w02`, 1.03x on `w01`) at `soa`'s footprint.

It is not where `archetype` is strongest: `h05_sparse_component` is 1.48x
`archetype`. A query naming a component 2% of entities hold still walks 200000
slots' worth of words, 3125 of them, and visits each set bit; `archetype` walks
only the groups that hold the component.

## Scaling

Point operations measure n^0.084 and the realistic frame n^0.681, the lowest
realistic-frame exponent of the ECS track in this run (`archetype` n^0.717). Its
memory exponent is `soa`'s, n^0.856 and n^0.821: five bits a slot do not show.

## Historian

Classified **EXACT_REDISCOVERY** after measurement, by a Historian agent with
literature search and a skeptic agent told to find closer prior work and to
check every citation. The Historian said EXACT_REDISCOVERY (high confidence);
the skeptic upheld it.

A bit-sliced index with an existence bitmap kept beside the column (O'Neil and
Quass 1997), and in game engines the bitset ECS of specs and hibitset.

Closest known work, as cited (sources the agents report having read):

- **Bit-Sliced index with an Existence Bitmap, kept beside a Projection index on
  the same column.** Patrick O'Neil and Dallan Quass, 'Improved Query
  Performance with Variant Indexes', ACM SIGMOD 1997, DOI 10.1145/253260.253268,
  https://www.cs.umb.edu/~poneil/varindexabs.html (full text checked: Definition
  2.1, and the EBM passage in Section 2). The paper says these ideas began in
  MODEL 204 and are fully built in Sybase IQ.
- **Bitset-based ECS, hierarchical flavour: Specs MaskedStorage + VecStorage +
  hibitset, joined through BitSetAnd.** Amethyst project, specs (Rust ECS),
  https://github.com/amethyst/specs, and hibitset,
  https://github.com/amethyst/hibitset (crate author listed as 'csheratt',
  v0.6.4, 2023). Classified as 'Bitset based ECS' in Sander Mertens, ECS FAQ,
  https://github.com/SanderMertens/ecs-faq. Source checked: MaskedStorage {
  mask: BitSet, inner }, VecStorage(Vec<..MaybeUninit<T>>) indexed by entity id,
  Allocator { generations, alive: BitSet, cache }, and BitIter using
  trailing_zeros.
- **Bitset-based ECS, flat flavour: entity_component (used by planck_ecs).**
  Joel Lupien (jojolepro), entity_component crate v1.1.2,
  https://github.com/jojolepro/entity_component (source checked: Components<T> {
  bitset: BitSetVec, components: Vec<Option<T>> }, an Entities alive bitset plus
  a generation Vec, and join! ANDs the bitsets with bit_and / bit_andnot /
  bit_or). Matches the ECS FAQ's 'array for each component with an accompanying
  bitset' flavour, https://github.com/SanderMertens/ecs-faq.
- **Per-entity component mask over entity-indexed pools (the other bitset-ECS
  flavour, equal to the parent soa).** Alec Thomas, EntityX (C++),
  https://github.com/alecthomas/entityx (source checked: entityx/Entity.h has
  `typedef std::bitset<MAX_COMPONENTS> ComponentMask`,
  `entity_component_mask_[]`, and a ViewIterator::next that tests
  `(entity_component_mask_[i_] & mask_) == mask_` on every slot up to capacity).
  The ECS FAQ lists it as a bitset ECS example.
- **Bit-Sliced Signature File (BSSF) kept beside a Sequential Signature File
  (SSF), used as an index on set-valued attributes** (found by the skeptic). C.
  S. Roberts, 'Partial-match retrieval via the method of superimposed codes',
  Proc. IEEE 67(12):1624-1642, 1979, DOI 10.1109/PROC.1979.11543 (origin of the
  bit-sliced organisation). Y. Ishikawa, H. Kitagawa, N. Ohbo, 'Evaluation of
  signature files as set access facilities in OODBs', ACM SIGMOD 1993, pp.
  247-256, DOI 10.1145/170035.170076 (compares SSF and BSSF for set-valued
  attributes; Crossref confirms the metadata, but the PDF host returned 503 and
  a self-signed certificate, so I did not read the body). S. Kocberber and F.
  Can, 'Partial evaluation of queries for bit-sliced signature files',
  Information Processing Letters 60(6):305-311, 1996, DOI
  10.1016/S0020-0190(96)00176-7 (full text read: 'Storing a signature file in
  column-wise order is called bit-sliced signature file (BSSF) method [Roberts].
  The BSSF method requires retrieval of the bit slices corresponding to the 1s
  of the query signature'; each slice 'must be read and ANDed with the result of
  the processed bit slices', at T_bitop per memory word).
- **Massive ECS (C#, bitset-based ECS with rollbacks)** (found by the skeptic).
  nilpunch, massive-ecs, https://github.com/nilpunch/massive-ecs (README: 'Based
  on bitsets. Inspired by EnTT'). Source checked: Runtime/BitSet/BitSetBase.cs
  holds ulong[] Bits plus NonEmptyBlocks and SaturatedBlocks summaries, and
  Runtime/Query/QueryCache.cs ANDs the included sets' NonEmptyBlocks and
  isolates bits with x & -x.

Citations were checked by the second agent, not by the coordinating session; a
reader relying on one should read it.
