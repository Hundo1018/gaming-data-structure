# Agents

Empty on purpose. Nothing in this repository calls a model yet.

The roles are specified in `PROJECT.md` and their prompts are in `prompts/`:
Explorer, Mutator, Assumption Breaker, Adversary, Judge, Historian.

What already exists for them to plug into:

- **Candidate generation** writes a directory under `candidates/<track>/<name>/`
  with a C11 `structure.h` and a three-line `structure.c`. CMake picks it up on
  the next configure, and the contract in `substrate/include/gds/api.h` (or
  `gds/spatial/api.h`) is enforced at compile time by redeclaring every
  operation with its required prototype, so a generated structure with a wrong
  signature fails to compile and one with a missing function fails to link,
  each with a message naming the function.
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

## How the C port was produced

The suite was written in C++20 and ported to C11 at the user's request. The
coordinating session wrote the substrate and five candidates itself (`soa`,
`uniform_grid`, `grid_undo_log` and both oracles) and accepted that design
only when `runner/equivalence.py` reported the same checksums as the C++
binaries on all 25 workloads, with reported bytes, peak bytes and allocation
counts also identical. The other sixteen candidates and the floor tool were
ported by two workflows of paired agents: a porter per group of candidates,
working in its own build directory and iterating until the equivalence check
passed, then an adversarial reviewer that read the C against the C++ function
by function, wrote edge-case workloads for paths the suite does not reach, and
fixed what it found. The acceptance rule was bit-level: every verify and bench
checksum, status, failure message and op count of the C build equal to the C++
build's on every workload.
