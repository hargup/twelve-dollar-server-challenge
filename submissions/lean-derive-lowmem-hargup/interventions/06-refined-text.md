# Refined strings through parsing and serialization

Status: planned. Source baseline: `100234f`.

## Work to eliminate

All output text uses the general escaping serializer. Input validation already discovers escaping and UTF-8 facts, but later stages forget them. Timestamp output is repeatedly scanned as arbitrary text.

## Type and correctness design

Distinguish arbitrary text, validated unescaped JSON content, bounded decoded text and fixed-format timestamps. Establish facts at a checked boundary and preserve them. Stored text refinements require actual validation and invalidation on foreign writes, not assumptions about the seed.

## Implementation plan

First preserve request-local no-escape information through trimming and create-response serialization without another counting pass. Consider a validated global timestamp-format invariant only if its preservation and fallback are complete. Generate safe-copy and general-escape serializers from the same wire description.

## Validation and decision gate

Test ASCII, escapes, control characters, BMP/astral UTF-8, trim behavior, timestamps outside the refinement and foreign writes. Use long-body and normal fixtures. Include initialization/validation overhead in the report. Exact-sizing double scans were already rejected; do not silently repeat them.

## Experiments

No measurements yet. Each entry will record source SHA, control SHA, commands, fixture, raw result location, correctness result, performance result and keep/reject decision. Multiple interventions may share one coherent compiler change; attribute that experiment to all affected files.
