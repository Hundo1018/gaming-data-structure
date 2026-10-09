# Agents

Empty on purpose. Nothing in this repository calls a model yet.

The roles are specified in `PROJECT.md` and their prompts are in `prompts/`:
Explorer, Mutator, Assumption Breaker, Adversary, Judge, Historian.

What already exists for them to plug into:

- **Candidate generation** writes a directory under `candidates/<track>/<name>/`.
  CMake picks it up on the next configure, and the contract in
  `substrate/include/gds/api.hpp` is enforced at compile time by a concept, so a
  generated structure that does not satisfy it fails to build with a message
  saying which requirement it missed.
- **Genealogy** is already in the schema: `manifest.yaml` carries `id`,
  `parents`, `island`, `origin` and `novelty_status`, and the archive stores all
  of them per run.
- **Adversarial workloads** are files in `workloads/`. Producing one requires no
  code change.
- **The Judge's job is already mechanical.** Correctness is decided against the
  oracle, and measurement is shared harness code, so neither is something a
  model is asked to assert.
- **The Historian's stage** has a field to write into (`novelty_status`) and is
  gated by design: it runs only against candidates that already have
  experimental evidence in the archive.

What is missing is the loop itself — island populations, selection from the
Pareto archive, mutation scheduling, and the model calls.

## How the latest generation was produced

The loop is still not in this repository, but one was run against it from a
coordinating Claude Code session, and its shape is recorded here because the
evidence it produced is.

1. **Specify.** The coordinator chose each candidate from an open question in
   the existing notes or from a gap in `benchmarks/floor.md`, and wrote its
   mechanism and falsifiable predictions, with thresholds, before any code
   existed. Those predictions are in each manifest's `predictions:`.
2. **Implement.** One agent per candidate, in its own build directory, under
   rules that kept it from editing anything but its own files, from
   benchmarking held-out workloads, and from editing its predictions after
   seeing a number. It verified against every workload and its own adversarial
   ones.
3. **Attack.** A second agent reviewed the code line by line for contract and
   measurement-integrity violations and wrote adversarial workloads and
   differential fuzzers against it. Every reported finding carried a severity.
4. **Fix.** A third agent fixed blockers and majors, or wrote down why not.
5. **Measure and judge.** One serial run of `runner/run_all.py`, then
   `runner/predictions.py`.
6. **Historian.** After measurement, one agent per surviving candidate compared
   it with the literature and classified its novelty, and a skeptic searched for
   closer prior work; the classification is in `novelty_status`.

The attack stage found a wrong-answer defect (an overflowing bound in
`grid_ring_knn`), an unrequested second change that confounded a comparison
(`cell_sorted`'s row walk, now its own candidate `cell_rows`), a code-shape
change that would have been credited to a mechanism (`fused_archetype`), and
three latent defects in `uniform_grid` that no workload can reach.

## The C port, and its reversal

The same kind of loop ported the whole suite to C11. The return to C++20
restored the sources of `83a58a6` rather than porting back. The coordinating
session wrote the C substrate and five candidates itself. Paired agents then
ported the other sixteen, a porter and then an adversarial reviewer for each
group. Each porter's work was accepted only once `runner/equivalence.py` showed
the C++ build's checksums on every workload; every recorded equivalence run of
the porters showed no difference. The reviewer read the C against the C++
function by function and wrote edge-case workloads. Those, together with six
that one porter wrote, are now in `workloads/port_review/`.

A second round of six agents then gathered the evidence for keeping C or not.
Five measured, and a skeptic re-ran their headline numbers and corrected seven
claims. The user chose C++ on that evidence. `benchmarks/language_port.md` has
the method, the numbers and the defects this found.
