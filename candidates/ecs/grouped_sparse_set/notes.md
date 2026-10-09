# grouped_sparse_set — observed

Run `20261009T042136Z`, Intel Xeon @ 2.10GHz, GCC 13.3.0,
`-O2 -DNDEBUG -ffp-contract=off -march=native`, 5 repetitions. Verdicts are
`benchmarks/predictions.md`'s; step times there are the median across
repetitions.

## The test sparse_set asked for: grouping closes the gap

| | prediction | measured | verdict |
|---|---|---:|---|
| P1 | closes at least half the `h05` gap from `sparse_set` to `archetype` | 0.61 of it | held |
| P2 | `w01` and `w02` at least 15% faster than `sparse_set` | 0.760x, **0.879x** | **falsified** |
| P3 | `w03` within 15% of `sparse_set` either way | **0.612x** | **falsified** |

`sparse_set/notes.md` ended on a question: was its loss on
`h05_sparse_component` about packed arrays, or about the missing alignment that
EnTT-style owning groups add? With an owning Position+Velocity group and nothing
else changed, the median frame on `h05` went from 996.7 us to 665.6 us, against
`archetype`'s 452.6 us: 61% of the gap closed (medians across repetitions). P1 was the falsifying case and it
held, so most of what `sparse_set` lost there was alignment.

## The two falsified predictions went in opposite directions

P2 held on `w01` by a wide margin (0.76x) and missed on `w02` (0.88x against
0.85). `w02`'s queries are Position, Position+Velocity, and
Position+Velocity+Health. The group can only serve the second and integrate: the
first names one component and has nothing to align, and the third, under the
smallest-set rule, can drive from Health and probe the rest. That is a reading of
the code against the mix, not a per-query measurement.

P3 was falsified by being much faster than predicted: 296.9 us on
`w03_structural_churn` against `sparse_set`'s 487.8 us. The prediction expected
the swaps into and out of the group prefix to cost something on a workload
dominated by add and remove. They cost less than what the group saves: `w03`
integrates and queries Position+Velocity every frame, and those now walk two
aligned prefixes. A churn workload that never iterates would separate the two
effects; none exists.

## Where it stands

On the Pareto front of `w03_structural_churn` and `w05_small_world`. Ahead of
`sparse_set` on every ECS workload, behind `archetype` and `bitset_soa` on most.
Its generalization gap is -1.71 ranks, the largest in the ECS track in the good
direction: it does better on what it could not see.

## Historian

Classified **EXACT_REDISCOVERY** after measurement, by a Historian agent with
literature search and a skeptic agent told to find closer prior work and to
check every citation. The Historian said EXACT_REDISCOVERY (high confidence);
the skeptic upheld it.

An EnTT full-owning group (EnTT v3.0, 2019), as the task that specified it said.

Closest known work, as cited (sources the agents report having read):

- **EnTT full-owning group (registry.group<position, velocity>()).** Michele
  Caini (skypjack), EnTT, open-source C++ ECS library,
  https://github.com/skypjack/entt. Groups came in with v3.0.0 (2019), whose
  release notes say the persistent view was removed in favour of full-owning,
  partial-owning and non-owning groups. Documented in docs/md/entity.md under
  'Views and Groups' / 'Full-owning groups', and implemented in
  src/entt/entity/group.hpp as class group_handler (checked on master at commit
  e0f4c376).
- **'ECS back and forth' Part 2: 'Where are my entities?' (grouping of
  independent sparse sets).** Michele Caini (skypjack), blog post, March 7,
  2019, https://skypjack.github.io/2019-03-07-ecs-baf-part-2/
- **Sparsey component groups.** Tudor Lechintan, Sparsey, open-source Rust ECS,
  https://github.com/LechintanTudor/sparsey (README; it lists EnTT among its
  inspirations)
- **Shipyard 'packs'.** leudz (Dylan Ancel), Shipyard, open-source Rust
  sparse-set ECS, https://github.com/leudz/shipyard. The 0.5 release
  announcement (users.rust-lang.org, March 20, 2021) says 'Packs are removed
  temporarily, this implementation had too many limitations.'
- **EnTT full-owning group (group_handler in src/entt/entity/group.hpp)** (found
  by the skeptic). Michele Caini (skypjack), EnTT,
  https://github.com/skypjack/entt. Groups were introduced in v3.0.0; its
  release notes say 'Introduced new grouping functionalities', 'Removed
  persistent view' and 'Full-owning groups: they allow what is called perfect
  SoA'. Documented in docs/md/entity.md under '### Full-owning groups'.
- **Shipyard 0.3/0.4 tight packs (TightPack; SparseSet::pack and
  SparseSet::actual_remove)** (found by the skeptic). leudz (Dylan Ancel),
  Shipyard Rust ECS crate. Versions 0.3.0 (crates.io, 2020-02-29) and 0.4.1
  (2020-06-05) contain src/pack/tight.rs and src/sparse_set/pack_info.rs. I
  checked the code in shipyard-0.4.1/src/sparse_set/mod.rs. The packs were
  removed in 0.5.0, announced by leudz on users.rust-lang.org topic 57203 on
  2021-03-20.

Citations were checked by the second agent, not by the coordinating session; a
reader relying on one should read it.
