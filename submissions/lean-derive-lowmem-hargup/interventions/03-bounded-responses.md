# Response representations selected by proven bounds

Status: first lowering implemented; awaiting benchmark. Source baseline: `100234f`.

## Work to eliminate

Health and like responses allocate a 1 KiB block plus an obuf descriptor, even though their maximum wire sizes are small and computable.

## Type and correctness design

Derive a body bound from the typed wire shape and leaf bounds. A bounded-response representation owns bytes inline with its queue node; general responses retain growable storage; constants retain static spans. Include header space and SIMD write padding in every capacity obligation.

## Implementation plan

Generate bounded out_begin calls for health/like success paths. Add a separate pooled inline node representation rather than inflating every large-response descriptor. Preserve short-send pointer advancement, errors, abort, queue cleanup and release-after-completion ownership.

## Validation and decision gate

Test extreme integer widths, degraded health fallback, fragmented sends, pipelines, resets and sanitizer checks. Measure health/like application CPU, pool allocation sizes, and mixed HTTP. Avoid claiming gains from a benchmark that excludes response release.

## Experiments

No measurements yet. Each entry will record source SHA, control SHA, commands, fixture, raw result location, correctness result, performance result and keep/reject decision. Multiple interventions may share one coherent compiler change; attribute that experiment to all affected files.

### Experiment R1: shape-derived inline ownership

`Shape.fixedBound` computes 57 bytes for health and 67 for like bodies; strings/fragments
produce no fixed bound. `Bounded shape capacity` requires the computed bound and evidence
that header reserve plus body fit. Lean checks these witnesses, and generated C static
assertions tie them to runtime allocation constants. The bound relies on documented C
primitive contracts, not a claim that the entire C serializer has been formally verified.

A separate inline-node pool owns 192 bytes with its 32-byte descriptor (224 bytes versus
32 + 1024 previously, excluding allocator overhead). Large/static responses keep their
original representation. A growing inline response is promoted to a normal descriptor before
publication. Health failure uses general storage directly. The i64 primitive now handles
INT64_MIN without signed-negation overflow, matching the declared wire type. Sanitizer
tests passed signed extrema, pool reuse, simulated partial sends, promotion and abort.

### P3/C2/R1 measured together (`99730e2`, control `e735916`)

All 42 official cases, 20,000 SQL-oracle post lookups, fragmentation/pipeline edges
and claim validation passed as UID 65534. Paired thread-CPU medians (ns/request):
health batch 1 2779.09 → 2409.05, health batch 32 127.97 → 121.76; mixed batch 32
4370.35 → 3060.16, with large control variability; decode-like 1239.40 → 1270.50.
Raw: `results/pair-e735916-99730e2-1793082571431849072`. No attribution of the noisy
mixed delta to an individual intervention. R1 reduces each retained inline allocation
from a 32-byte descriptor plus 1024-byte block to a single 224-byte allocation.

Long escaped claims against foundation `93ab6eb`: like decode 4012.44 → 3877.22;
create decode 4013.53 → 4156.19. Raw:
`results/pair-93ab6eb-99730e2-longclaims-1793082657017962536`. The forced-inline
claim specialization does not show a general decode speedup. Keep provisional pending
end-to-end comparisons; final decisions must not cite these samples as a clear win.
