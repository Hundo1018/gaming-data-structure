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
