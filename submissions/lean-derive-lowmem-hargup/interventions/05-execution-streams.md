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

### Coupled J1/S1 design before implementation

Use three read streams (feed/post/health) and one ordered write stream. Their intrusive `next`
indices fit unused padding in the new 32-byte job envelope; heads/tails/counts are the only
additional state. Generated successful route constructors append to the appropriate stream.
Error-only jobs remain in delivery order but need no database execution entry.

Split exec_read into endpoint-specific functions so each read stream calls its own kernel
without a switch. Reads share the existing batch snapshot and may execute grouped by kind;
responses still occupy their original job slots and deliver in original order. Keep one
ordered write stream and its like/create selection; reordering writes is not justified.
Transaction begin, foreign-write invalidation, cursor lifetime, commit-before-response and
rollback response replacement remain unchanged. On commit failure only the write stream's
responses are replaced. Reset all stream state and the arena at batch completion.

A decode-only benchmark must reset the same construction state between rounds; otherwise it
would accumulate links to reused stack slots. Its driver must measure the actual generated
entry points with correct slot indices, preserving comparability with the archived executable.
Test mixed pipelines, creates followed by reads/likes, disconnects, errors and zero-read/write
batches before accepting any timings. Compare small and large batches, then mixed HTTP.
