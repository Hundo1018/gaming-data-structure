# Game Data Structure Discovery Agent

## Goal
Build an autonomous research system that explores novel data representations for game workloads. The system must not be constrained to ECS or to known data structures.

## Core principle
LLMs propose hypotheses and implementations. Code, correctness tests, benchmarks, adversarial workloads, and hidden workloads decide whether candidates survive.

## Research domains
1. ECS / entity management
2. Spatial queries
3. Temporal state / rollback / replay
4. Event streams
5. Graph / navigation
6. Streaming / world partition

ECS is only the first implementation target, not the research boundary.

## Architecture
Explorer -> Candidate -> Compile -> Correctness -> Benchmark -> Adversary -> Mutation -> Benchmark -> Pareto Archive -> Literature Verification

Use multiple islands to preserve diversity. Literature search is disabled during initial discovery and enabled only after experimental candidates exist.

## Candidate repository
Each candidate is a real compilable C++ research object:

candidate/
- manifest.yaml
- hypothesis.md
- structure.hpp
- structure.cpp
- tests.cpp
- benchmark.cpp
- notes.md

The candidate API must remain minimal and must not prescribe the internal representation.

## Candidate manifest
Required fields:
- id
- name
- track
- parents
- hypothesis
- representation
- operations
- assumptions
- expected advantages/disadvantages
- complexity (allowed to be unknown)
- novelty status
- origin

## Evaluation
Correctness is mandatory before performance measurement.

Metrics should include:
- latency / throughput
- p50 / p95 / p99 where meaningful
- memory bytes and bytes/entity
- cycles
- instructions
- cache references / misses
- branch instructions / misses
- allocations

Fitness is multi-objective / Pareto rather than a single weighted score.

## Workloads
A workload is a first-class research object. Important dimensions include:
- entity/object count
- operation distribution
- temporal locality
- spatial locality
- burstiness
- access distribution
- mutation frequency
- query shape

Hidden workloads are used to detect overfitting.

## Agent roles
### Explorer
Generate substantially different representations without literature search. Do not reject ideas because they appear familiar.

### Mutator
Change fundamental representation or assumptions, not cosmetic implementation details.

### Assumption Breaker
Ask why an assumed property is necessary and deliberately remove or weaken assumptions.

### Adversary
Construct workloads that expose the candidate's failure modes.

### Judge
Check correctness and benchmark quality; never invent performance results.

### Historian
Only after experiments, compare surviving candidates with known literature and classify novelty:
EXACT_REDISCOVERY, KNOWN_VARIANT, KNOWN_COMPONENTS_NEW_COMBINATION, NEW_APPLICATION, POSSIBLY_NEW, UNCERTAIN.

## Initial MVP
- Python orchestration
- C++ benchmark substrate
- SQLite archive
- Git candidate genealogy
- six domain schema, but ECS implementation first
- baseline candidates: AoS, SoA, Sparse Set, Archetype
- 10 exploration islands
- hidden workloads
- Pareto archive

## First success criterion
The first milestone is NOT beating an existing ECS implementation. It is demonstrating that the system can maintain diverse candidate families, discover representation changes beyond cosmetic variations, falsify them with adversarial workloads, and preserve useful research evidence.
