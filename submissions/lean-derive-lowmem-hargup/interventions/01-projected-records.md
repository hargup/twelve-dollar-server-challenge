# Schema-projected SQLite record kernels

Status: planned. Source baseline: `100234f`.

## Work to eliminate

user_name decodes three columns and materializes col_t[3], although only username is consumed; the loop remains in the compiled binary. Feed requests repeat this for up to 20 authors.

## Type and correctness design

Represent a projection by fields that belong to a particular schema, with an exact requested storage type. Lower to bounded prefix decoding and named outputs. Retain checks on untrusted serial types and lengths: SQLite declarations are not runtime typing guarantees.

## Implementation plan

Generate a username projection first, then a post projection and integer index prefix where justified. Skip unused trailing columns and avoid intermediate column arrays. Keep overflow-page handling and ensure a borrowed result cannot be overwritten by the shared spill buffer before use.

## Validation and decision gate

Compare native reads against SQL for 20,000 seeded posts and feeds, plus long/overflow rows and unexpected storage types. Measure feed/post/mixed application CPU in interleaved pairs, then network traffic. Keep only reproducible improvements without regressions.

## Experiments

No measurements yet. Each entry will record source SHA, control SHA, commands, fixture, raw result location, correctness result, performance result and keep/reject decision. Multiple interventions may share one coherent compiler change; attribute that experiment to all affected files.
