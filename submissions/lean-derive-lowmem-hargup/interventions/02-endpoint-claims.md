# Endpoint-specific authentication materialization

Status: planned. Source baseline: `100234f`.

## Work to eliminate

Every authenticated request decodes username into claims_t.name and copies it into the batch arena. Likes consume only uid; creations consume username as well.

## Type and correctness design

An endpoint-indexed demand describes which verified claims are materialized. Validation is distinct from materialization: both routes still validate signature, algorithm, expiration, sub and username, in the original error order.

## Implementation plan

Generate a uid-only verifier call for likes and a uid-plus-name call for creation. Constant specialization must erase unused username decoding/copies; a runtime flag that costs more than it saves is not sufficient. Reuse the demand definition later for job payload layout.

## Validation and decision gate

Exercise escaped/Unicode names, missing/wrong-type claims, invalid signatures, expiration, and auth-before-id behavior. Benchmark decode-only CPU with actual signed tokens, like traffic, and mixed HTTP. Confirm the successful creation response still uses the token username.

## Experiments

No measurements yet. Each entry will record source SHA, control SHA, commands, fixture, raw result location, correctness result, performance result and keep/reject decision. Multiple interventions may share one coherent compiler change; attribute that experiment to all affected files.
