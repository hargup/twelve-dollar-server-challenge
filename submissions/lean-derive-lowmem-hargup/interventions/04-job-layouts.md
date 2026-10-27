# Endpoint-specific payload layouts

Status: planned. Source baseline: `100234f`.

## Work to eliminate

Every request initializes and carries fields for all endpoints in job_t. Reads drag authentication and write-body metadata through batch scans.

## Type and correctness design

Derive payload fields from endpoint demand, keeping a small common transport envelope and separate exact payloads. A typed reference must identify the payload kind and lifetime. An endpoint tag alone is not proof unless construction and consumption are both generated from the same description.

## Implementation plan

Generate payload structs and typed construction/accessors. Try a compact envelope plus cold payload arena or per-kind arrays; do not merely replace fields with an equally large union. Preserve all connection generation, close-after, response order and error handling semantics.

## Validation and decision gate

Measure sizeof/layout, bytes initialized per read, mixed decode/batch CPU and pipeline behavior. Benchmark memory at high concurrency separately from per-batch temporary memory. Reject extra indirection if it outweighs reduced cache traffic.

## Experiments

No measurements yet. Each entry will record source SHA, control SHA, commands, fixture, raw result location, correctness result, performance result and keep/reject decision. Multiple interventions may share one coherent compiler change; attribute that experiment to all affected files.
