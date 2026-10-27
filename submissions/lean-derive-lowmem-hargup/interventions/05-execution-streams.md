# Specialized batch execution streams

Status: planned. Source baseline: `100234f`.

## Work to eliminate

app_batch scans the same generic array to count requests and dispatch reads and writes; deliver scans it again. Runtime switches recover endpoint facts already known while decoding.

## Type and correctness design

Endpoint effects determine scheduling, but write order and each connection's response order remain explicit invariants. Separate dispatch structure from delivery order. No response or database value may be shared across requests as a result cache.

## Implementation plan

Construct compact execution streams while decoding. Generate endpoint-specific read loops where independent; preserve a single ordered write stream and original delivery slots. Derive counts from construction. Do not reintroduce the previously rejected branch-heavy selective-cursor optimization.

## Validation and decision gate

Compare interleaved creates/likes/reads and failure responses with the existing ordering semantics. Measure one-request and large mixed batches, then HTTP pipelines. Keep only when both dispatch cost and full workload support the change.

## Experiments

No measurements yet. Each entry will record source SHA, control SHA, commands, fixture, raw result location, correctness result, performance result and keep/reject decision. Multiple interventions may share one coherent compiler change; attribute that experiment to all affected files.
