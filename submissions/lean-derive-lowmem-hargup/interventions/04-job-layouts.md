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

### Coupled J1/S1 design before implementation

Generate `job_gen.h` from `Endpoint.payload`: post has one ID; like has post/user IDs;
create has user ID, username span and body span; feed/health have no payload. A common
transport envelope carries kind/write/close flags, connection/generation, a cold payload
pointer and response pointer. The planned x86-64 envelope is 32 bytes versus 72 currently.
Payloads live in the request/batch arena and expire only after execution. Allocation follows
authentication/id validation so error precedence is preserved.

Introduce endpoint/argument-indexed references whose membership is checked by Lean; generate
both payload declarations and accessor expressions from this description. Authentication
returns validated claims to the generated constructor, and materializes username only for
creation. Body decoding fills the creation payload. Runtime consumers use the specific
payload type inside the matching endpoint implementation. The generator emits both the C
application and the job header; build.sh must regenerate/compare both.

The envelope has four alignment bytes after its generation field. Use them for a `next`
32-bit slot index for S1, so execution lists do not increase envelope size. Keep only stream
heads, tail pointers and counts; no MAX_JOBS-sized pointer arrays. Typed slot indices lower
to u32 with UINT32_MAX as the empty-list sentinel. The parser already bounds job slots.

`app_decode` receives the destination slot index as an explicit argument. Runtime parsing
passes `njobs-1`; the benchmark driver passes its local array index. A stream append writes
the previous tail's next index, and app_batch follows indices relative to its `js` array.
The envelope/streams are one coherent representation change and will be measured together.
