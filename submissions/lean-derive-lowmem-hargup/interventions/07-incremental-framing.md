# Phase-indexed incremental HTTP framing

Status: planned. Source baseline: `100234f`.

## Work to eliminate

An incomplete body causes http_parse to rescan the header block on subsequent receives. Finding the header terminator also rescans prior fragments.

## Type and correctness design

A connection-local framing state distinguishes searching for headers from waiting for a known body length. Stored offsets remain valid across realloc/memmove; borrowed pointers must not survive those moves. The state resets exactly when one framed request is consumed.

## Implementation plan

Persist the search frontier and validated framing metadata only for partial requests. Keep the common complete-request path compact. Decide whether cold state belongs in the existing fragment allocation rather than enlarging every idle connection.

## Validation and decision gate

Split requests at every delimiter and body boundary, pipeline next requests, test conflicting Content-Length, close/error/reset paths, and run sanitizer checks. Benchmark fragmented large bodies and ordinary traffic; report cold-state memory and reject broad normal-path regressions.

## Experiments

No measurements yet. Each entry will record source SHA, control SHA, commands, fixture, raw result location, correctness result, performance result and keep/reject decision. Multiple interventions may share one coherent compiler change; attribute that experiment to all affected files.

### H1 implementation and validation design

`Feed.Frame.Phase.fields` determines a phase-specific C union: a header search frontier,
or validated method/close/auth flags and six 32-bit header-relative spans/lengths.
Membership-indexed field references generate stores. Body restoration reconstructs
pointers only from the current receive base. The existing C header parser is the trust
boundary; its successful header validation constructs the body-wait phase.

The 32-byte state is prepended to an allocated fragment buffer, adding no per-idle-
connection fields. Search resumes with three bytes of overlap. Body wait tests only
`n >= header_length + body_length`. Consuming a request, capacity hold/replay, idle
release and close/reset preserve or reset the matching phase. Header errors retain
the original status and connection-close behavior.

`framing_cases.c` compares each incremental prefix against stateless parsing across
all first-split positions, relocates storage between calls, exercises malformed/duplicate
headers, large fragmented bodies, pipelined prefixes and size limits. ASAN/UBSAN passed
locally. `transport_cases.c` exercises the actual cold allocation/growth/release and
write/capacity hold paths on Linux without opening a ring. Network tests and ordinary
traffic comparison remain required before keeping H1.
