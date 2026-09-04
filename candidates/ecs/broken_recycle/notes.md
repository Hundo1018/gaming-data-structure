# broken_recycle — observed

The gate rejected it on 9 of the 10 ECS workloads, in this run as in every run
since it was added.

It passed `w04_random_access`, which is correct and expected: that workload sets
`w_create: 0.0` and `w_destroy: 0.0`, so no slot is ever recycled and the
missing generation counter cannot be observed. A negative control that passes a
workload unable to expose its defect is evidence the workload is narrow, not
evidence the gate is broken.

Two detection paths fired, and the difference between them matters:

- On `h03_stale_handles`, which aims one access in five at a destroyed handle,
  the mismatch was caught mid-frame at the operation itself:
  `frame 1, op 28: observation mismatch on op kind 5 slot 28785 component 0`.
- On the other eight, including workloads with no deliberate stale access at
  all, the periodic oracle sweep caught it instead:
  `frame 31, op 0: alive() disagrees for slot 0`. The sweep re-checks every slot
  ever created, destroyed ones included, so handle invalidation is verified
  whether or not a workload thinks to test it.

The second path is the one worth keeping. It means correctness of handle
invalidation does not depend on an adversary having thought of it.
