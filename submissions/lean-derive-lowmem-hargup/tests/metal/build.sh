#!/usr/bin/env bash
# Single source mirror, with executable artifacts keyed by source SHA.
set -euo pipefail
sha=${1:?source commit SHA}
root=/root/twelve/opt-audit-1024/metal
cd "$root/source"
mkdir -p "$root/artifacts/$sha"
flags=(-O2 -march=native -fno-plt -pthread -DSQLITE_THREADSAFE=2 -DSQLITE_DEFAULT_MEMSTATUS=0
 -DSQLITE_OMIT_DEPRECATED -DSQLITE_OMIT_SHARED_CACHE -DSQLITE_USE_ALLOCA -DSQLITE_DQS=0
 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1
 -Iruntime -Igen -I/root/twelve/deps/sqlite-amalgamation-3540100)
gcc "${flags[@]}" -o "$root/artifacts/$sha/server" runtime/server.c -lm
gcc "${flags[@]}" -o "$root/artifacts/$sha/micro" tests/metal/micro.c -lm
sha256sum "$root/artifacts/$sha/server" "$root/artifacts/$sha/micro" > "$root/artifacts/$sha/binaries.sha256"
printf '%s\n' "$sha" > "$root/artifacts/$sha/source.sha"
gcc --version | head -1 > "$root/artifacts/$sha/compiler.txt"
cat "$root/artifacts/$sha/binaries.sha256"
