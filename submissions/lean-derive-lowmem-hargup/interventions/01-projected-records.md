# Schema-projected SQLite record kernels

Status: first lowering under test. Source baseline: `100234f`.

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

### Experiment P1: username projection

The typed `usersName` reference supplies the ordinal. The lowering returns one checked
column, skips trailing serial types, and avoids the three-element intermediate array.
The generated ordinal remains static; untrusted lengths and serial types remain checked.
Control executable: `93ab6eb` (same C as typed foundation `1bf9f9b`). Candidate SHA and
measurements will be added after the build. Lean clean build passed all 20 targets.

P1 source `c420aa1`, control `93ab6eb`: official 42/42, edge cases, and SQL comparison
for 20,000 posts passed. Interleaved thread-CPU medians (ns/request): feed 8056.01 →
7888.08 (-2.1%); mixed batch 32: 3121.21 → 3198.46 (+2.5%); post 3366.97 →
3386.66 (+0.6%). No competing load generator was detected; substantial isolated control
outliers remain. Raw data: `metal/results/pair-93ab6eb-c420aa1-1793081628975389705`.
Decision: do not keep this generic lowering as the final optimization; it does not establish
a broad win.

### Experiment P2: schema-selected physical encoding

The schema identifies a text field immediately after a rowid alias. Emit a guarded one-byte
header/type path that returns a text span directly; retain the general checked projection
for longer encodings. This removes the intermediate column and varint loops on the common
encoding. The pure record codec was separated for direct sanitizer testing: 20,000 generated
valid/random records and truncated prefixes passed ASAN/UBSAN. This experiment shares a
commit with endpoint claim specialization; read-only microbenchmarks isolate the projection,
authentication microbenchmarks isolate claims, and mixed HTTP measures the combination.

P2/C1 source `e735916`, control `93ab6eb`: all correctness checks passed, including new
claim cases. Medians (ns/request): feed 8028.47 → 8083.09; mixed batch 32: 3155.58 →
3148.98; post 3309.80 → 3258.39. These do not establish an application-CPU gain. Mixed
HTTP ABBA runs were 174619, 182628, 179514, 176689 requests/s (about +3.1% for the
combination), all zero errors. Raw micro data: `pair-93ab6eb-e735916-1793082008107683415`;
HTTP summaries have timestamp suffixes 1793082188847693270 through 1793082223373738941.
Keep this as a promising control executable, not proof of an isolated projection speedup.

### Experiment P3: simpler prefix derived from membership

Replace the custom physical-encoding helper with the existing decoder instantiated at
`usersName.prefix = 2`, with a two-element array. `Field.prefix_le` proves that this is a
valid prefix of the schema. This directly removes the unused trailing field, adds no new
runtime parser and reduces code complexity. Shares a commit with C2 and R1; compare the
combined lowering with P2/C1. Sanitizer checks cover 20,000 valid records and truncations.
