# Optimization trials — 2026-10-24

Experiments ran on `bigmachine` against a frozen copy of the lower-memory submission at
`0dd7e387c76fb20902ad1e5ad86cf83fd8196a29`. Only the input-buffer and cursor-storage changes
were retained from the first four trials. The concurrent WAL-checkpoint work is separate
and was not included in these comparisons.

| Trial | Decision | Evidence |
|---|---|---|
| Release drained input buffers | Keep | For 10,000 idle connections after fragmented requests, added anonymous memory fell from 40.7 MiB to 1.6 MiB. Every connection remained usable. |
| Reuse cursor storage | Keep | Median application CPU fell 2.8% for individual mixed reads and 3.9% for batches of 32. Network throughput was within run-to-run variation. |
| Open only the required cursors | Reject | Mixed reads were slightly slower; endpoint-specific gains were inconsistent. |
| Size escaped JSON exactly | Reject | A 500-character ASCII post used a 1,024-byte allocation instead of 3,479 bytes, but individual mixed reads cost 2–5% more CPU across two implementations. |

## Methods and limits

Each variant passed all 42 official checks and the additional `edge_cases.py` checks:
fragmented and pipelined writes/reads, long strings, JSON escaping, UTF-8, response key order,
and 100 pipelined health requests. Candidate tests used disposable copies of the seed.

Cursor timings use the real application batch executor and serializers, excluding networking
and JWT verification. Each comparison interleaves A/B/B/A/A/B, with three samples per
variant, and measures thread CPU time. These are application-cost measurements, not an
end-to-end capacity claim.

| Mixed read workload | Before cursor reuse | After cursor reuse |
|---|---:|---:|
| One request per batch | 4,214.63 ns/request | 4,095.52 ns/request |
| 32 requests per batch | 3,211.82 ns/request | 3,086.17 ns/request |

The memory test keeps 10,000 connections open after sending fragmented requests with roughly
3 KiB of headers. It measures the change in server-cgroup anonymous memory, excluding the
sockets' kernel memory. It models fragmented traffic, not every idle connection.

Network trials use one server CPU, a 2 GiB cgroup limit, no swap, and a 65,535 descriptor
limit; clients run on the other cores over loopback. Source code does not set CPU affinity or
change host settings. The native load generator reproduces the challenge traffic model.
These short shared-host trials are not official scores. Runs wait for quiet windows;
measurements overlapping another load generator are excluded.

## Batching sweep

At 50,000 virtual users (10 s ramp, 20 s hold), all runs had zero errors. CPU variability
was large enough that the repeated 250 µs baseline is essential:

| Coalescing delay | Hold CPU, µs/request | Hold p95, ms | Hold p99, ms |
|---|---:|---:|---:|
| 250 µs, first control | 39.55 | 1.901 | 4.104 |
| 0 µs | 51.62 | 0.689 | 301.567 |
| 50 µs | 25.88 | 0.459 | 3.163 |
| 100 µs | 33.05 | 1.010 | 3.268 |
| Skip sleep when no requests were served | 50.39 | 11.344 | 310.784 |
| 250 µs, repeated control | 25.42 | 1.149 | 1.754 |

The shorter delay does not establish a CPU win at this load. Higher-load control and compiler
results are recorded below when their queued runs finish.
