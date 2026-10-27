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
