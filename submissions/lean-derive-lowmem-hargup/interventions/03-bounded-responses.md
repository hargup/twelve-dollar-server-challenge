# Response representations selected by proven bounds

Status: planned. Source baseline: `100234f`.

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
