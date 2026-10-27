# Endpoint-specific authentication materialization

Status: first lowering under test. Source baseline: `100234f`.

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

### Experiment C1: materialization follows endpoint demand

`Endpoint.payload` determines `needsName`. The route generator emits a constant 0/1 into
authentication, allowing compiler specialization; both paths retain identical claim validation.
The like path skips username unescaping and both copies. The creation path is unchanged.
Additional tests cover missing/wrong claims, escaped Unicode usernames, signature/algorithm/
expiration failures, and auth-before-id behavior. This shares a commit with P2 record decoding;
benchmark each affected path and the combination.

C1 source `e735916` passed official, edge, SQL and claim checks. Seed-token decode CPU
medians: like 1235.01 → 1225.88 ns; create 1325.18 → 1307.83 ns, within noise. The
combined mixed-HTTP result was about +3.1% (see P2). Assembly retained a single
`authenticate.isra.0`, so the demand flag was not fully erased.

### Experiment C2: force constant specialization

Inline the verifier and authentication wrapper at the generated constant call sites. The
like and create paths now have the opportunity to eliminate unused materialization at
compile time. Measure ordinary and long escaped claim fixtures; the latter tests the work
this optimization actually removes. Shares the P3/R1 commit.

### P3/C2/R1 measured together (`99730e2`, control `e735916`)

All 42 official cases, 20,000 SQL-oracle post lookups, fragmentation/pipeline edges
and claim validation passed as UID 65534. Paired thread-CPU medians (ns/request):
health batch 1 2779.09 → 2409.05, health batch 32 127.97 → 121.76; mixed batch 32
4370.35 → 3060.16, with large control variability; decode-like 1239.40 → 1270.50.
Raw: `results/pair-e735916-99730e2-1793082571431849072`. No attribution of the noisy
mixed delta to an individual intervention. R1 reduces each retained inline allocation
from a 32-byte descriptor plus 1024-byte block to a single 224-byte allocation.

Long escaped claims against foundation `93ab6eb`: like decode 4012.44 → 3877.22;
create decode 4013.53 → 4156.19. Raw:
`results/pair-93ab6eb-99730e2-longclaims-1793082657017962536`. The forced-inline
claim specialization does not show a general decode speedup. Keep provisional pending
end-to-end comparisons; final decisions must not cite these samples as a clear win.
