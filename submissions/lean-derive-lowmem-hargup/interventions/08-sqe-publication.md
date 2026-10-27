# Typed SQE construction and batch publication

Status: planned. Source baseline: `100234f`.

## Work to eliminate

sqe_get clears 64 bytes and publishes the shared SQ tail for every entry, before callers fill opcode-specific fields. Construction and publication can be separated.

## Type and correctness design

Model reserved, initialized and published entries plus queue capacity. Only fully initialized entries become visible. Preserve release/acquire ordering and partial submission accounting. Zeroing removal requires proving every kernel-consumed field initialized; batching publication is the first experiment.

## Implementation plan

Maintain a private construction tail and publish it immediately before every io_uring_enter, including capacity-triggered flushes. Retain full zeroing initially. Consider opcode-specific initialization only after inspecting the kernel contract and generated assembly.

## Validation and decision gate

Test SQ wraparound, forced small queues, saturation, short sends, cancellation, rearm and shutdown. Measure enters/job, SQ tail stores and mixed network CPU, with unprivileged high-connection tests. Do not change host tuning or use busy kernel polling.

## Experiments

No measurements yet. Each entry will record source SHA, control SHA, commands, fixture, raw result location, correctness result, performance result and keep/reject decision. Multiple interventions may share one coherent compiler change; attribute that experiment to all affected files.
