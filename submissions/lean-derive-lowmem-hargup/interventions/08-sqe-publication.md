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

### Q1 design before measurement

`Feed.Sqe.ReadyPlan op` contains typed references into the exact opcode argument context
and proof that all required dynamic fields are assigned. It emits six specialized
constructors: accept, recv, send, shutdown, fixed-file close and accept cancellation.
The shared `Wire.Ref` system now includes signed fd, u32 and u64 machine domains.
Constructors reserve/zero a slot, initialize constants and typed arguments, and only
then commit to the private tail. Full 64-byte zeroing remains; removing it is not
justified without a complete per-opcode kernel field model.

`ring_enter` is the single publication boundary for both normal submission and capacity
flushes. It release-stores the ready tail immediately before entry and subtracts only
the returned submission count. Reserve uses acquire-loaded head and preserves unsigned
wrap arithmetic. Shutdown reserves two slots before constructing its linked close,
preventing an application-induced capacity flush from splitting that pair.

The ABI ordering and link boundary follow the Linux manual:
[io_uring_enter(2)](https://man7.org/linux/man-pages/man2/io_uring_enter.2.html).
This is an explicit C/kernel trust boundary, not a Lean proof of kernel behavior.
`sqe_cases.c` mocks only the syscall and tests actual production functions with tiny
queues, unsigned tail wrap, interrupt retries, partial consumption, field zeroing and
close-pair capacity. Real small-queue API/pipeline tests and mixed network measurements
are required before keeping Q1.
