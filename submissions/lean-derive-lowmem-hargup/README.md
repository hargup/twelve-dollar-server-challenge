# Lean → C: a derived feed server (lower-memory variant)

This is the same server as `lean-derive-hargup`, from the same Lean specification and with the
same proofs, tuned to keep less memory resident:

* **Rings on demand.** io_uring rings are opened on demand behind one shared listener: one ring
  accepts at a time and hands over before its table of direct descriptors fills. 300k
  connections need 5 rings instead of 10, so there are fewer ring buffers and fewer
  `io_uring_enter` calls per loop.
* **Receive buffers.** 512 per ring instead of 1,024.
* **Response buffers.** Two size classes, 1 KiB and 8 KiB. The generator picks the class from
  each response's JSON shape (a list of rows starts big). Free lists are capped, so bursts do
  not stay resident.
* **SQLite page cache.** 1 MiB instead of 4 MiB; reads come from the mmap.

The kernel's memory per open connection is the same for both variants, and so is most of the
total: on the server side, about 2.4 KB for the TCP socket, 1.3 KB for its inode, file and dentry,
and 0.35 KB for the armed receive.

The API is specified in **Lean 4**: the domain, a global invariant over the stored data, the
read plans (with proofs that they compute the specified results), and the wire contract.
A Lean program then **generates the C application layer** from that specification. It is
compiled with SQLite and a small io_uring runtime into one binary. The build runs on the
benchmark box: Lean's kernel checks the theorems, and the generator writes the C that gets
compiled.

The premise: a framework commits to generality before it knows the application, and every
request pays for it. Here the application is known completely, so routing, validation,
serialization and even query execution are specialized to it.

| | |
|---|---|
| Language | Lean 4.35.0 (specification, proofs, generator) → C (gcc, `-O2 -march=native`) |
| Framework | none: the application layer is generated |
| Database | SQLite 3.54.1 amalgamation, compiled into the binary (`THREADSAFE=0`) |
| Transport | io_uring (Linux ≥ 6.1), one thread, HTTP/1.1 keep-alive |
| **Nginx** | **no: serves the internet directly** on `HOST:PORT` (`0.0.0.0:80`) |
| Dependencies | Lean toolchain and SQLite, pinned by version and SHA-256; nothing else |

## What is derived

`spec/` is a Lean package with no dependencies beyond Lean's core library.

| Lean (`spec/Feed/…`) | What it contains | What it becomes in `gen/app_gen.c` |
|---|---|---|
| `Model.lean` | users, posts, likes; `feed`, `getPost`, `newPost`, `like` as plain functions | the meaning of every endpoint; also the test oracle |
| `Invariant.lean` | `Db.Inv`, the startup check `checkInv`, the post-insert check `insertOk` | `app_check_inv`, the check after `INSERT … RETURNING` |
| `Plan.lean` | `feedPlan`, `postPlan` over B-tree primitives (tail scan, rowid seek, one index range scan) | `feed_tail`, `post_by_id`, `range_counts`, `user_name` |
| `Contract.lean` | routes, check order, statuses, error messages, JSON shapes | route matcher; every error response as complete constant bytes (Content-Length computed in Lean); serializers with all constant JSON merged |
| `Schema.lean` | `schema.sql` as data | record field positions and type checks; SQL text; a startup check of `PRAGMA table_info` |
| `Emit.lean` | the generator | `app_gen.c` (filled into `Feed/templates/app.c`) |

`build.sh` regenerates `app_gen.c` and refuses to build if it differs from the checked-in
`gen/app_gen.c`. The checked-in copy is there to be read; the compiled one always comes from
the specification.

### The invariant, and what it buys

`Db.Inv` says three things about the stored data that the schema does not enforce:

* `dense`: post ids are exactly 1…n. Posts are only appended, and SQLite gives a new row
  `max(rowid)+1`.
* `mono`: `created_at` never decreases in rowid order. New posts get `now`.
* `authors`: every post's author row exists. Users are never written.

Under it, the feed order (`created_at DESC, id DESC`) is reverse rowid order. The feed is
then: read the last 20 rows of the posts B-tree, plus **one** range scan of
`likes_post_id_idx` for all 20 like counts. The reference plan instead walks the
`created_at` index, seeks each row, and counts each post's likes separately.

Proved in Lean (kernel-checked on every build; no `sorry`, only the standard axioms):

| Theorem | Statement |
|---|---|
| `Db.feed_eq_tail` | `Inv db → db.feed = (db.posts.reverse.take 20).filterMap db.view` |
| `Db.feedPlan_eq` | `Inv db → db.feedPlan = db.feed`: the generated feed plan computes the feed |
| `Db.postPlan_eq` | `db.postPlan id = db.getPost id`, with no invariant needed |
| `Db.checkInv_sound` | `db.checkInv = true → Inv db` (the startup scan) |
| `Db.inv_addPost` | `Inv db → db.insertOk p = true → Inv (db.addPost p)` (the per-insert check) |
| `Db.inv_like` | likes preserve `Inv` |

At startup the server runs `checkInv` once, a single pass over posts. After every insert it
runs `insertOk` on the new row. Exclusive locking makes it the only writer, so these checks
cover every write. If the stored layout differs from `Schema.lean`, or a check ever fails
(say the clock steps back), the feed falls back to the specification's SQL
(`ORDER BY created_at DESC, id DESC LIMIT 20`) for good. The plans are proved against the
model; the C emitted for them is tested, not proved (see Tests).

### Native read plans on SQLite's B-tree layer

The read plans run on SQLite's own B-tree API (`sqlite3BtreeTableMoveto`,
`sqlite3BtreeIndexMoveto`, `Next`/`Previous`, payload fetch). They are compiled in the same
translation unit as the amalgamation. Every request reads its rows from SQLite's pages through
SQLite's pager, WAL and locking, inside a SQLite transaction. What is skipped is the bytecode
interpreter, since each query is known ahead of time. Writes are ordinary SQL (prepared
statements). Measured on the seed with the same JSON output (byte-identical to the SQL
results): feed 16.7 µs with the reference SQL → 5.4 µs.

## The rest of the server

* **Transport.** Several io_uring instances, each with a `SO_REUSEPORT` listener and a table
  of direct descriptors:
  * multishot accept/receive, provided receive buffers, `DEFER_TASKRUN`;
  * direct descriptors do not use the process file table, so under `LimitNOFILE=65535` the
    server holds up to 10 × 65,471 connections;
  * idle keep-alive connections are never closed.
* **Batches.** All requests from one round of completions form one batch, which is one SQLite
  transaction:
  * reads run first, from the snapshot at batch start; then writes; then `COMMIT`;
  * no response leaves before its batch commits, so a 201 is always committed, and reads never
    see uncommitted rows;
  * after a light round the loop sleeps 250 µs so that the next round is a batch rather than a
    single request.
* **SQLite.** WAL, `synchronous=NORMAL`, `locking_mode=EXCLUSIVE`, 1 GiB mmap, small page cache.
* **JWT.** HS256 is verified on every request (signature, `alg`, `exp` required and in the
  future). The HMAC key pads are computed once from `JWT_SECRET`; that is the key schedule,
  not a verification result.
* **Requests.** HTTP/1.1 keep-alive, pipelining in order, `Connection: close`, partial
  requests. Input JSON is fully validated.

## Rules

1–3. SQLite file at `$SQLITE_PATH`, in-process. The schema is untouched; no tables, columns,
indexes or triggers are added.

4. `test/test.sh`: 42/42 (CI flow, run in a clean Ubuntu 24.04 container).

5. No caching across requests. Every response is built from SQLite pages read while the request is
being served. The only state kept between batches is whether the invariant still holds, which
records a property of the data, not data.

6. WAL + `synchronous=NORMAL`; group commit; each response waits for its commit.

7. Nothing depends on the load test. The invariant is checked, not assumed.

8–9. Direct on `0.0.0.0:80`. It needs no file descriptors beyond the listeners (see Transport).

10. Built from source on the box. Lean 4.35.0 and SQLite 3.54.1 are pinned by version and SHA-256.

11. Nothing on the box is tuned. The process raises its own soft `RLIMIT_NOFILE` to the hard
limit.

## Tests

* `test/test.sh` (42 checks) through `test/run.sh`, exactly as CI runs it.
* Native plans vs SQL: the feed and 20,000 random posts are byte-identical on the seed.
* Load: a native load generator reproducing `bench/load.js` (user model, think times, mix,
  `ramping-vus`, one keep-alive connection per user). It is not part of this submission.

## Local measurements (not the official score)

The harness is the same as for `lean-derive-hargup`, on a shared machine (Xeon E-2176G, Ubuntu
24.04, kernel 6.8):

* **Server:** a systemd scope on one core with `MemoryMax=2G` and a 65,535 file-descriptor
  limit, on a fresh copy of the seed.
* **Load:** a native generator replaying `bench/load.js` on the other cores, over loopback in a
  private network namespace.
* **Run:** 300,000 users, 60 s ramp, 90 s hold, 30 s ramp-down as in `load.js`.

Memory is the server cgroup's: its peak, and its value mid-hold.

| Server | Runs | CPU per request | Peak | Mid-hold total (process / kernel) | Errors |
|---|---:|---|---|---|---:|
| this variant | 3 | 16.3, 17.4, 18.1 µs | 560–575 MB | 524–527 MB (28–29 / 461 MB) | 0 |
| `lean-derive-hargup` | 1 | 16.5 µs | 653 MB | 603 MB (94 / 475 MB) | 0 |
| Rust io_uring, #82 | 1 | 23.8 µs | 616 MB | 566 MB (67 / 464 MB) | 0 |

The kernel's share is the open connections themselves, about the same for every server.

Caveats:
* **Loopback.** The client's CPU does part of the kernel's receive work.
* **Faster core.** This core is much faster than the benchmark's 2.4 GHz VPS.
* **Noise.** The machine is shared; repeated runs vary by about ±1 µs per request.
* **Checkpoint stalls.** On this machine's busy disk, WAL checkpoints (one every 4,000 pages,
  run inside the event loop) occasionally stall responses for up to a second.
* **Not a capacity search.** This is a same-harness comparison, not a search for the maximum
  number of users and not an official score.

## Layout

```
spec/         Lean specification, proofs and generator (lake exe derive <dir>)
gen/          app_gen.c as generated from spec/ (for reading; build.sh regenerates and compares)
runtime/      application-independent C: io_uring transport, HTTP framing, JSON input,
              HS256, output buffers, SQLite B-tree cursor primitives; server.c includes it all
tests/blast.c closed-loop throughput probe used during development
```
