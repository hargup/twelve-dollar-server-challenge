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

### T1 design and implementation

`Wire.PlainText` carries a per-character no-escape predicate. `Shape.plainStr` requires
a reference of that type; the common post-shape constructor instantiates either generic
text or plain text. The emitter selects exact `n+2` capacity and a quoted memcpy for
the latter. The C trust boundary is the existing JSON scanner: only its validated
`escaped == false` branch constructs plain text. Trimming takes a substring, preserving
the property; no extra scan or sizing pass is added. A flag occupies existing padding
in the creation payload. General stored body/author/timestamp fields retain escaping.

Do not add a global timestamp invariant: its setup/invalidation complexity adds an extra
stored-data property for a small field; T1 directly tests the request-local opportunity.
ASAN/UBSAN compares the copy and general serializer for every substring of representative
plain UTF-8 bytes. Existing edge/claim cases exercise escaped and unescaped creation.
