# Metal optimization interventions

Branch: `lean-derive-metal-hargup`. Baseline: `100234f` (includes WAL bound).
One unified source tree. Git commits identify experiments; no source-variant directories.
The remote tree is a mirror of the committed source. Executables and logs may be retained
by SHA for interleaved comparisons without maintaining divergent source implementations.

## Stack rank

Ranking weighs frequency, removable work, implementation cost and correctness risk. It is
not a claimed speedup. Design the shared type system before optimizing individual consumers.

| Rank | Intervention | Reason | Status |
|---:|---|---|---|
| 0 | [Shared type system](00-type-system.md) | Establish facts once and drive all lowerings | foundation implemented |
| 1 | [Projected records](01-projected-records.md) | Up to 20 redundant decodes per feed | planned |
| 2 | [Endpoint claims](02-endpoint-claims.md) | Eliminate unused materialization with a narrow change | planned |
| 3 | [Bounded responses](03-bounded-responses.md) | Remove a separate allocation and select exact storage | planned |
| 4 | [Job layouts](04-job-layouts.md) | Shrink the common representation across all requests | planned |
| 5 | [Execution streams](05-execution-streams.md) | Let endpoint effects determine dispatch structure | planned |
| 6 | [Refined text](06-refined-text.md) | Preserve facts across input/output stages | planned |
| 7 | [Incremental framing](07-incremental-framing.md) | Eliminate repeated work on fragmented input | planned |
| 8 | [SQE publication](08-sqe-publication.md) | Reduce shared queue updates; higher transport risk | planned |

## Experiment discipline

- Write and review the shared type design before implementation. Prefer one coherent change
  when several interventions follow from one type-level fact; record the grouping explicitly.
- Commit source before measurements. Record the full SHA and executable hash in results.
  Rejected implementations stay in history and are reverted in the unified source.
- Compare against the last retained executable with interleaved runs. Application CPU and
  targeted microbenchmarks explain causes; mixed HTTP checks end-to-end effects.
- Use fresh database copies, one server CPU, 2 GiB memory, no swap, and 65,535 descriptors.
  Wait for quiet windows on bigmachine; never stop somebody else's benchmark. Record and
  exclude overlapping performance runs. Do not change system-wide settings.
- Preserve runtime input validation, SQLite reads per request, authentication per request,
  transaction/commit ordering, foreign-write fallback, and the exact wire contract.
- Rebuild Lean and check generated C. Run all 42 official tests, meaningful edge/differential
  tests, and appropriate sanitizer checks for memory/lifetime changes. Finish with clean
  Ubuntu CI and an unprivileged high-connection smoke test of the final retained source.
- Keep a change only with correctness evidence and a demonstrated performance/space benefit
  appropriate to its purpose. Record noisy or negative results honestly. No official-score
  or maximum-capacity claims from short local tests.

## Progress

2026-10-26: inspected the clean branch and existing harness. Recorded all interventions before
implementation. Source-transfer authorization is explicit. No current-round benchmarks yet.
