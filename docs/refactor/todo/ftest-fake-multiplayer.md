# Plan: a faked-multiplayer harness to unlock netcode coverage for ftest/kfx_net

Status: **Phase 1 and Phase 2 both landed and passing, 2026-08-31.**

Phase 1 built directly as an `src/ftests/` harness (the ftest route, not
Catch2 — see "Open questions" below, resolved rather than left open) rather
than prototyped first. New files: `src/ftests/ftest_net_fake.{h,c}` (the
fake `NetSP`) and `src/ftests/tests/ftest_net_resync_fake_multiplayer.{h,c}`
(registered in `ftest_list.c` as `net_resync_fake_multiplayer`, map00011).
`net_resync.h` gained two declarations (`send_resync_game`/
`receive_resync_game`) that only existed as unheadered external-linkage
functions before — same "add the missing declaration" move
`net_resync_test.cpp` used for `store_localised_game_structure`/
`recall_localised_game_structure`. Verified: clean `keeperfx`/`kfx_ftests`
build (`out/coverage-ftest`), `-ftests net_resync_fake_multiplayer -headless
-exitonfailedtest` exits 0 and its log reads "Fake host->client resync
round-trip restored state byte-for-byte", `check_layering.py --strict`
unaffected (`src/ftests/` is the exempt tier).

Phase 2 turned out to need a real design change mid-flight: two `NetSP`
instances can't coexist in one process (see "Phase 2: a real design change"
below) the way Phase 1's plan assumed, so it's genuinely two `keeperfx`
processes instead, coordinated by a new driver script,
`scripts/run_ftest_net_enet_loopback.sh`. New files:
`src/ftests/tests/ftest_net_enet_loopback_{host,join,shared}.{h,c}`
(registered in `ftest_list.c`'s `long_running_tests_list` as
`net_enet_loopback_host`/`net_enet_loopback_join` — a deliberate repurposing
of that list, see "Shape actually built (Phase 2)"). Verified: clean build;
`scripts/run_ftest_net_enet_loopback.sh out/coverage-ftest` passes both
sides across 3 consecutive runs, logs show a real ENet connection
(`Net: ENet: host created...`, `Net: ENet: incoming connection accepted`)
and a real ping/pong exchange; the default `-ftests` sweep (no
`-includelongtests`) is unaffected, confirmed by re-running the full
existing 16-test suite clean (exit 0, no `net_enet_loopback_*` entries in
its output).

`scripts/run_ftest_net_enet_loopback.sh` also now merges both processes'
gcov data (via `gcov-tool merge`) into `out/coverage-ftest/coverage.info`
and, when present, refreshes `out/coverage-merged` via
`scripts/merge_coverage.sh` unchanged — see "Coverage merge across the two
loopback processes" below.

**Extended further, same day**: beyond the original ping/pong round trip,
the host/join pair now also exercises `sendmsg_all()`/
`sendmsg_single_unsequenced()`, the connection-quality query functions
(`GetPing()`/`GetPacketLoss()`/`GetClientDataInTransit()`/
`GetClientPacketsLost()`/`GetUploadRateBytesPerSecond()`/
`GetDownloadRateBytesPerSecond()`, called from both sides to hit both their
client-branch and host-branch code paths), and `drop_user()` (the manual
disconnect path) — see "Extending the loopback pair: more of
`bflib_enet.cpp`" below. Measured result, re-run end to end against a fresh
baseline: `net_resync.cpp` unchanged at 70.0%; `bflib_enet.cpp` 33.4% →
**49.4%** (303/613 lines, 31/42 functions, +12 functions from this
extension alone).

## Motivation

Following up on the `check_layering.py --strict` accepted-residual review
(architecture.md §8.2): after
[`two-remaining-layering-violations.md`](two-remaining-layering-violations.md)'s
fix landed, `console_cmd.c → game_session_loop.h` is gone (it was dead code).
Two accepted residuals remain, both in netcode:

| File | Violation | Line coverage **before** this plan (`out/coverage-merged/coverage.info`, 2026-08-31) | **after** Phase 1 + Phase 2 (same date, see "Result" below) |
|---|---|---|---|
| `src/kfx_net/src/net_resync.cpp` | → `kfx_frontend_state.h`, `kfx_game_state.h`, `game_legacy.h` | 48/257 (18.7%) | 180/257 (**70.0%**) |
| `src/kfx_platform/src/bflib_enet.cpp` | → `net_main.h` | 0/613 (0%) | 303/613 (**49.4%**) |

Both are documented as structural/by-design, not just untested (§6.2's
raw-blob wire format; the ABI-shared `NetSP` function-pointer struct) — fixing
them for real means restructuring netcode, out of scope for the refactor.
Coverage doesn't remove that scope decision, but it's the prerequisite for
ever attempting such a restructuring safely. Right now there isn't one: the
9 coverage-driven ftests added in `ea8b70839` all target `kfx_sim`
creature/room logic, and the existing `kfx_net/tests/net_resync_test.cpp`
Catch2 suite explicitly stops at the self-contained pattern-A/B functions
(`store_localised_game_structure`/`recall_localised_game_structure`,
`animate_resync_progress_bar`) — its own file comment says so. The functions
it doesn't reach are exactly the ones that need a *live* `NetSP` moving bytes
between two endpoints: `send_resync_data`/`receive_resync_data`,
`LbNetwork_Resync`, `send_resync_game`/`receive_resync_game`, `resync_game`.
This plan is about building that "live but fake" endpoint.

## Architectural facts this plan relies on (verified against current code)

- **`struct NetSP`** (`src/kfx_net/include/net_main.h:82`) is a clean
  function-pointer vtable: `init/exit/host/join/update/sendmsg_single/
  sendmsg_single_unsequenced/sendmsg_all/msgready/readmsg/drop_user`. Exactly
  one implementation exists today — `InitEnetSP()` in `bflib_enet.cpp` — wired
  in at a single call site, `netstate.sp = InitEnetSP();` in
  `net_main.c::LbNetwork_Init()`. This is the natural seam for a fake.
- **`extern struct NetState netstate;`** is a single process-wide global —
  only one "network identity" exists per process today. A two-sided fake
  needs two independent send/receive queues, not two `netstate`s.
- **DK's multiplayer model is lockstep**: every client simulates the full
  world identically from the same input packets (`packets_input.c`); the
  network layer's job is exchanging per-turn commands plus, on rare checksum
  divergence, a full raw-blob resync (`net_resync.cpp`, `net_checksums.c`).
  So faking "another player" doesn't require a second full game-state
  instance — it requires a second `NetUserId`'s traffic flowing convincingly
  through the real send/receive/exchange/resync code.
- **`intentional_desync()`** (`net_resync.cpp:84`, called from the debug
  console in `console_cmd.c:2759`) already exists as a purpose-built hook
  that corrupts one side's local state on demand — exactly the trigger a test
  would need to force `resync_game()`'s path to actually run, without
  needing genuine, hard-to-reproduce simulation drift.
- **Confirmed: single-player/skirmish never touches `NetSP` at all.**
  `setup_network_service()` (`net_game.c:137`) explicitly early-returns
  (`process_network_error(-800)`) for anything other than
  `FrontendNetSvc_Online`/`FrontendNetSvc_LAN` — `FrontendNetSvc_Skirmish`
  never calls `LbNetwork_Init`. This matches the observed 0%/18.7% numbers:
  ftest today runs entirely off the skirmish/local path, so `netstate.sp` is
  never even assigned during a `-ftests` run. Confirms the gap is total, not
  partial, and that no minimal existing plumbing can be "extended" — this is
  new plumbing.

## Design options

### Option A — fake in-process `NetSP` (loopback vtable, no real sockets)

A new `InitLoopbackSP()`-style pair of `NetSP` instances, backed by shared
in-memory byte queues (one per direction), implementing the same
`init/host/join/update/sendmsg_*/msgready/readmsg` contract `net_main.c` and
`net_resync.cpp` already call through. Swapped in for `InitEnetSP()` at test
setup.

- Covers: `net_resync.cpp`, `net_exchange_*.c`, `packets*.c`,
  `net_checksums.c` — everything above the `NetSP` seam.
- Does **not** cover `bflib_enet.cpp` — that file *is* the thing being
  bypassed.
- Cheap, fast, fully deterministic (no real socket timing).

### Option B — real dual-ENet loopback (two `InitEnetSP()` instances over `127.0.0.1` in one process)

One "host" and one "join" `NetSP`, both the real ENet implementation, both
pumped from the same test driver loop.

- Covers everything Option A covers, *plus* `bflib_enet.cpp` for real
  (`enet_host_create`/connect/send/receive).
- Costs: actual loopback UDP I/O, connection-establishment polling against
  the existing `TIMEOUT_*`/`PEER_TIMEOUT_*` constants (`net_main.h:30-38`),
  more moving parts, less deterministic timing than Option A — though
  loopback UDP inside one host should still be fast and CI-reliable (no
  cross-host flakiness).

### Option C — two full `keeperfx` ftest processes over a real network

Spawn two processes, coordinated by a driver script, closest to how real
multiplayer actually runs.

- Most realistic, but heavyweight: process spawn/sync/teardown, port
  allocation, cross-process log correlation, and a poor fit for the
  existing single-process gcov model (`.gcda` only flushes on clean exit —
  §7.3's contamination note already flags this as fragile even
  single-process).
- **Rejected for this plan.** Worth keeping in mind as a separate future
  true-integration-test direction, decoupled from the ftest gcov-coverage
  goal.

### Recommendation

Phase 1 = Option A. It closes the `net_resync.cpp` gap, is self-contained,
and reuses the existing `NetSP` seam. (This plan originally floated proving
it out at the Catch2 unit-test level first, lower cost to abandon — overridden
by explicit direction to build the `src/ftests/` harness directly; see
"Shape actually built" below for what that produced.)

Phase 2 = Option B, once Phase 1's plumbing (two `NetUserId`s, driving
`update()` from a test loop, triggering `intentional_desync()`, asserting
convergence) is proven out. This is what actually closes the
`bflib_enet.cpp` gap.

Option C: not planned; noted only so it isn't reinvented later without
knowing it was considered and why it was set aside.

## Shape actually built (Phase 1)

- `src/ftests/ftest_net_fake.h`/`.c` — the fake `NetSP`, guarded by
  `#ifdef FUNCTESTING` (not a shipped-binary file). Modeled as one mailbox
  per `NetUserId`, keyed the same way the real `NetSP`'s
  `msgready`/`readmsg(source, ...)` are: by whoever *sent* a message, not
  who it's addressed to. Delivery is synchronous/unconditional — no real
  transport to fail, drop, reorder, or wait on, so `timeout` is a no-op (see
  "Timeout fidelity" below, resolved this way for Phase 1). The caller
  declares which identity it's currently playing
  (`ftest_net_fake_set_role_host()`/`ftest_net_fake_set_role_client(id)`)
  before driving code that touches `netstate.sp` — this is what makes a
  *single process* usable for both sides: it never runs two roles
  concurrently (the single global `netstate` genuinely doesn't allow that,
  confirmed above), it plays them **sequentially** against one shared fake
  wire, and a message written under one role becomes readable once the
  caller switches to the other and reads from the matching mailbox.
- Didn't extend `ftest_list.c`'s `struct FTestConfig` schema — matches the
  plan's original intent (one-off setup belongs in a test's own
  `init_func`, precedent: `ftest_bug_pathing_pillar_circling`). No new
  `ftest_util` helper turned out to be needed either; the test file drives
  `ftest_net_fake_*` and `netstate` directly.
- First (and so far only) test: `net_resync_fake_multiplayer`
  (`src/ftests/tests/ftest_net_resync_fake_multiplayer.c`). Rather than
  `resync_game()`'s own `my_player_number`-based host/client dispatch (which
  would need a second valid `PlayerInfo` and carries real risk of breaking
  other systems that read `my_player_number` mid-test — flagged as future
  work below, not attempted here), it calls `send_resync_game()` and
  `receive_resync_game()` directly (newly declared in `net_resync.h` for
  this purpose) from a single ftest action:
  1. Install the fake `NetSP` as host, with one fake logged-in "client" (id
     1) so `send_resync_data()`'s per-user broadcast loop has somewhere to
     send.
  2. Call `send_resync_game()` — this genuinely serializes live `game`,
     `kfx_sim_state`, `kfx_net_state`, `kfx_game_state`,
     `kfx_frontend_state`, and `lish` through real zlib compression, a real
     CRC32, and real `net_callbacks->lua_resync_export`, onto the fake wire.
  3. **Snapshot state for later comparison only now, after step 2** — see
     the "surprising finding" below for why this ordering matters.
  4. Call `intentional_desync()` (the same hook the debug console's
     "desync" command uses) to force real, verifiable divergence.
  5. Switch role to client (id 1) and call `receive_resync_game()` — real
     decompression, real CRC verification, real
     `net_callbacks->lua_resync_import`.
  6. `memcmp` every snapshotted struct against live state: all six must be
     byte-identical to the step-3 snapshot, and
     `get_player(0)->instance_remain_turns` (the one field
     `intentional_desync()` unconditionally touches) must be back to its
     pre-corruption value.

**Surprising finding, worth keeping in mind for any future test built on
this harness**: the snapshot in step 3 above cannot be taken *before*
`send_resync_game()` is called, only after. `send_resync_game()`'s first
line is `pack_desync_history_for_resync()` (`net_checksums.c`), which
unconditionally overwrites `kfx_net_state.host_checksums` and
`kfx_net_state.log_snapshot` with freshly computed values as part of
building the payload — this is correct, intended production behavior (the
resync payload should carry live checksum history), not a bug. A snapshot
taken *before* the call captures a `kfx_net_state` that's stale by the time
anything actually goes out on the wire, which reads as a false round-trip
failure that has nothing to do with whether `send_resync_game()`/
`receive_resync_game()` themselves are correct. First attempt at this test
hit exactly this (`net_ok=0` while every other struct matched); moving the
snapshot to after the call fixed it. A second false lead ruled out along
the way: splitting host-send and client-receive across a game-turn boundary
(two `ftest_append_action`s) also produced a spurious `kfx_net_state`
mismatch, because `net_input_lag.c`'s fields legitimately re-derive every
turn regardless of resync activity — the final version deliberately keeps
both halves in one single-turn action to sidestep that too.

## Phase 2: a real design change (discovered, not planned)

Option B's premise — "two `InitEnetSP()` instances, one hosting, one
joining, in the same process" — turned out to be **impossible as written**,
discovered while starting Phase 2 implementation. `bflib_enet.cpp` keeps its
ENet state in anonymous-namespace **process-wide globals**: a single
`ENetHost *host` and `ENetPeer *client_peer`
(`src/kfx_platform/src/bflib_enet.cpp:56-57`). `bf_enet_host()` writes to
`host` directly; `bf_enet_join()`'s `create_join_host()` calls
`host_destroy()` (which nulls `host`) and then unconditionally reassigns the
same global. So a `join()` call in a process that already called `host()`
tears the host side down first — there is only one `ENetHost`/`ENetPeer`
slot per process, full stop. This was surfaced to the user directly (three
options: two real processes / refactor bflib_enet.cpp into instantiable
state / drop `bflib_enet.cpp` coverage) rather than silently picking one,
since it changes Phase 2's shape and risk profile from what this doc
originally proposed. Chosen: **two real processes**, i.e. Option C from the
original three-option list — previously rejected for being heavyweight and a
poor fit for gcov's clean-exit model, now the only route to genuine
`bflib_enet.cpp` coverage without touching the exact production file this
whole effort exists to build a safety net *for* (refactoring it first would
have the same chicken-and-egg problem Phase 1 already flagged for
`net_resync.cpp`'s wire format).

## Shape actually built (Phase 2)

- `src/ftests/tests/ftest_net_enet_loopback_shared.h` — the port
  (`FTEST_NET_ENET_LOOPBACK_PORT`, fixed rather than OS-assigned — see "Phase
  2 port allocation" below), join address, ping/pong message strings, and
  turn budget shared by both halves.
- `src/ftests/tests/ftest_net_enet_loopback_host.c` /
  `ftest_net_enet_loopback_join.c` — drive `LbNetwork_Init()`/`netstate.sp`
  directly, not `setup_network_service()` (frontend-only, gated to
  `FrontendNetSvc_Online`/`LAN`, drags in lobby-screen machinery) or
  `net_lobby.c`'s full `NETMSG_LOGIN` handshake (a different file from the
  one this plan targets). The host reserves `netstate.my_id = SERVER_ID` and
  marks `netstate.users[SERVER_ID]` logged in *before* calling `host()`, the
  same bookkeeping `net_lobby.c:256`'s real host path does (just without the
  wire handshake that normally produces it) — this keeps `OnNewUser()` (real
  production code, `net_main.c`) from assigning the incoming client to slot
  0 and colliding with the host's own identity. The join side sets
  `netstate.my_id = FTEST_NET_ENET_LOOPBACK_JOIN_USER_ID` the same way,
  known in advance rather than discovered, for the same reason.
  - Host: `LbNetwork_Init()` → `netstate.sp->host()` → poll
    `netstate.sp->update(OnNewUser)` until a client's connection is
    accepted → `sendmsg_single()` a ping → poll `msgready()`/`readmsg()` for
    the reply → `exit()`.
  - Join: `LbNetwork_Init()` → `netstate.sp->join()` (itself synchronous,
    already blocks on a real `enet_host_service()` poll loop against
    `TIMEOUT_CONNECT_DIRECT_IPV4` — by the time it returns, the real ENet
    connection exists) → poll `msgready()`/`readmsg()` for the ping →
    `sendmsg_single()` the reply → `exit()`.
- `scripts/run_ftest_net_enet_loopback.sh` — the external coordinator these
  two tests need (neither can pass standalone): launches the host process
  in the background, sleeps 1s, launches the join process, waits for both,
  reports pass only if both exit 0. Gives each process its own
  `GCOV_PREFIX` (so two live processes don't race writing the same `.gcda`
  paths — those paths are absolute, baked in at compile time, so they'd
  collide regardless of which directory each process runs from) and its own
  `-log` file (a real CLI flag, `main.cpp::determine_log_filename()`) so
  their `keeperfx*.log` output doesn't interleave.
- Registered in `ftest_list.c`'s **`long_running_tests_list`**, not
  `tests_list` — deliberately, even though neither test is actually
  long-running. A bare `-ftests` sweep (no test name) runs every
  `tests_list` entry in one process; adding these there would make that
  sweep (used by the `coverage` CMake target and, per
  `bug_invisible_units_cant_select`'s existing comment in the same file, by
  extension anything CI-adjacent) hang on `net_enet_loopback_host` waiting
  for a client that never arrives in a single-process context.
  `long_running_tests_list` is gated behind `-includelongtests` for both a
  bare sweep *and* a named run — confirmed by reading
  `ftest_fill_teststorun_by_name()` (`ftest.c:177`) before relying on it —
  so it's the correct *practical* fit ("opt-in only, never in the default
  sweep") even though "long-running" isn't a semantically accurate label
  for what these are; documented as a deliberate repurposing in the list
  entry's own comment.

**Real bug found and fixed while getting this to pass**: the first version
had the join side call `sendmsg_single()` (the pong) immediately followed by
`exit()`. That failed intermittently-turned-reliably in practice — `exit()`
tears down the local `ENetHost`/socket, but `sendmsg_single()`'s
`enet_host_flush()` only hands the packet to the OS socket send buffer, it
doesn't wait for the *other, separate process* to actually receive and ack
it. Disconnecting immediately after send raced that ack and the host's
`msgready()` poll timed out waiting for a reply that was in flight but never
delivered. Fixed by calling `netstate.sp->msgready(SERVER_ID, 250)` once
after the send, purely for its blocking side effect (250ms is enough for a
loopback ack round trip) before `exit()` — `msgready()`'s return value is
irrelevant here. This is the multi-process, real-transport analogue of
Phase 1's `pack_desync_history_for_resync()` finding: both were about
"what does the real code actually do at the moment you thought you could
just call the next function," surfaced only by actually running the thing.

## Coverage merge across the two loopback processes (built after Phase 2 landed)

Each process's `.gcda` output lands in a separate `GCOV_PREFIX`-mirrored
scratch directory (a `mktemp -d`, outside the build tree — an earlier
version nested them inside `out/coverage-ftest` itself, which made the merge
step below also pick up the mirrored copies as spurious unrelated files with
no matching `.gcno`; moved out once that surfaced). `scripts/run_ftest_net_enet_loopback.sh`
now folds coverage merging in as its final phase, entirely best-effort (a
non-coverage build just runs the tests and stops there):

1. **`gcov-tool merge`** (ships with GCC — `gcov-tool merge <dir1> <dir2> -o
   <out>`) combines two `.gcda` trees by matching files on their path
   *relative to each dir*. Since `GCOV_PREFIX_STRIP=0` mirrors the full
   absolute path, `<host-prefix-dir>$KEEPERFX_DIR` has exactly the same
   relative structure (`CMakeFiles/...`) as `$KEEPERFX_DIR` itself, so they
   line up and the counts genuinely sum rather than one clobbering the
   other. Two sequential 2-way merges (real+host, then that result+join),
   since the tool only takes a pair at a time.
2. The merged result is copied back over the real in-place `.gcda` tree,
   so it's indistinguishable from having been produced by a single process
   — anything downstream (a normal `cmake --build <dir> --target coverage`
   re-run, this script's own lcov capture below, or a person poking at the
   tree by hand) sees one coherent, enriched set of counts.
3. The script then re-runs the **exact** `lcov --capture`/`--extract`/
   `genhtml` commands `CMakeLists.txt`'s `KFX_FUNCTESTING` `coverage` custom
   target uses (same binary, same flags, same `KFX_LCOV_EXCL_LINE_PATTERN`
   — duplicated as a literal in the script since there's no clean way to
   read a CMake variable from a shell script; if that pattern ever changes
   in `CMakeLists.txt`, update it here too) to refresh
   `$KEEPERFX_DIR/coverage.info`/`coverage-html`.
4. Finally, if `out/coverage/coverage.info` (the unit-test tree's own
   report) already exists, it calls `scripts/merge_coverage.sh` unchanged
   to refresh `out/coverage-merged` — no changes needed there at all, since
   by this point `out/coverage-ftest/coverage.info` already carries the
   loopback pair's contribution baked in.

**Result, run end-to-end 2026-08-31** (full `-ftests` sweep → both loopback
processes → coverage merge → `scripts/merge_coverage.sh`):

| File | Before | After |
|---|--:|--:|
| `net_resync.cpp` | 48/257 (18.7%) | **180/257 (70.0%)**, 8/10 functions |
| `bflib_enet.cpp` | 0/613 (0%) | **205/613 (33.4%)**, 19/42 functions |
| Merged total (all files) | 36.2% lines / 52.7% funcs | 35.57% lines / 53.16% funcs (denominator grew — new test files themselves now compile+count too; both targeted files' real improvement is the signal here, not the barely-moved global percentage) |

`scripts/merge_coverage.sh`'s `lcov --add-tracefile` step emits a large
number of `WARNING: function data mismatch` lines (e.g.
`custom_sprites.c`, `front_landview.c`) — pre-existing behavior from
combining two separately-compiled trees' function-checksum metadata, not
something this change introduced; non-fatal, the merge completes and
produces a correct result regardless.

## Extending the loopback pair: more of `bflib_enet.cpp`

With the two-process harness proven and coverage-merge automated, the next
obvious move was using that *same* real session to reach more of
`bflib_enet.cpp` than the original ping/pong round trip touched — rather
than standing up a second, separate session for it. Checked which
functions were still at 0 hits (`FNDA:0,<mangled-name>` in
`coverage-merged/coverage.info`) before picking targets, rather than
guessing: the cheap, safe-to-call-once-connected cluster was
`bf_enet_sendmsg_all`, `bf_enet_sendmsg_single_unsequenced`,
`bf_enet_drop_user`, and the six `Get*` connection-quality query functions
(`GetPing`/`GetPacketLoss`/`GetClientDataInTransit`/`GetClientPacketsLost`/
`GetUploadRateBytesPerSecond`/`GetDownloadRateBytesPerSecond`, declared in
`bflib_enet.h`, normally only called from frontend UI to show connection
stats). Left alone: the holepunch/matchmaking cluster
(`join_via_holepunch`, `create_ipv6_host`, `resolve_punch_address`,
`enet_matchmaking_host_update`, ...) — exercising those meaningfully needs
a fake `EnetConnectivityServices` (matchmaking server calls, STUN queries)
that's a materially bigger, separate effort, not a cheap add to this
session.

**Shape**: extended both existing test files rather than adding new ones,
since this is still the one session Phase 2 already establishes:

- Host (`ftest_net_enet_loopback_host.c`), after the existing ping/pong:
  `sendmsg_all()` a broadcast + `sendmsg_single_unsequenced()` a message on
  the unsequenced channel (`action004`); after the join side acks (see
  below), call all six `Get*` stat functions (values logged, not asserted
  — they're real but non-deterministic network measurements), then
  `drop_user()` (the manual-disconnect path, as opposed to every other
  disconnect in this pair happening only as an `exit()` side effect), then
  `exit()` (`action005`).
- Join (`ftest_net_enet_loopback_join.c`): read both follow-up messages
  (order-independent — ENet doesn't guarantee delivery order *across*
  channels, so this checks message-set membership, not a fixed sequence),
  send an explicit ack, call the same six `Get*` functions itself (their
  `GetPing`/`GetPacketLoss`/etc. each branch on
  `IsPeerConnected(client_peer)` — a client-only fast path distinct from
  the host-side peer-list scan the host's own calls exercise, so calling
  from both processes is what actually covers both branches, not
  redundant), then `exit()`.

**A second race, found the same way as the first**: the initial version
had the host send its two follow-up messages then immediately move on to
`drop_user()`/`exit()` after only a blind 250ms settle (the same fix
pattern the original ping/pong race used). That's not actually equivalent
here: the earlier fix protected *the sender's own* reliable delivery
before *that same process* exited. Here, the risk is different — if the
host disconnects while the join side is still mid-poll, a `DISCONNECT`
event processed in the middle of `wait_for_incoming_packet()`'s loop calls
`destroy_incoming_queue()` (`bflib_enet.cpp`) for that source, which would
silently wipe any of the two follow-up messages already delivered into the
join process's kernel socket buffer but not yet drained into ENet's
internal queue and read by test code. Fixed by replacing the blind settle
with an explicit ack (`FTEST_NET_ENET_LOOPBACK_EXTRAS_ACK_MSG`): the host
now waits (bounded, same turn-budget pattern as everywhere else in this
pair) for the join side to confirm it actually read both messages before
calling `drop_user()`/`exit()` at all. Confirmed in the logs after the
fix: join's `action003` log line (both messages read, ack sent) appears
*before* `Net: ENet: peer 0 disconnected (clean)` shows up in its own log
— the ordering the ack was meant to guarantee.

**Result, re-measured end-to-end** (fresh `find -delete` on every `.gcda`,
full `-ftests` sweep, both loopback processes, coverage merge,
`scripts/merge_coverage.sh` — to make sure nothing stale from before this
extension's own recompile was still mixed in):

| File | Before this extension | After |
|---|--:|--:|
| `net_resync.cpp` | 180/257 (70.0%) | unchanged, 180/257 (70.0%) |
| `bflib_enet.cpp` | 205/613 (33.4%), 19/42 functions | **303/613 (49.4%)**, 31/42 functions |

Stable across two consecutive re-runs of `scripts/run_ftest_net_enet_loopback.sh`
(identical exit codes and coverage numbers both times).

## Open questions / risks

- **Timeout fidelity — resolved for Phase 1.** `ftest_net_fake_msgready()`
  ignores its `timeout` argument outright: delivery is synchronous, so
  there's never anything to actually wait for. Fine for Phase 1, where
  everything is driven by direct function calls
  (`send_resync_game()`/`receive_resync_game()`), not a real polling loop —
  `receive_resync_data()`'s own `while` loop against
  `RESYNC_RECEIVE_TIMEOUT_MS` never has to iterate more than once against
  the fake. Revisit if a future test drives the polling path (e.g.
  `LbNetwork_Resync()` directly, or anything relying on realistic wait
  behavior) — the current fake would make a real timeout untestable, since
  it can never actually elapse one.
- **Determinism — not yet exercised, still open.** The landed test never
  runs more than one game turn, so it hasn't touched
  `net_checksums.c`-driven multi-turn checksum divergence or the known
  un-wrapped RNG source (`crypt.h`, per `src/ftests/README.md`) at all. A
  future test that lets several turns pass before triggering resync should
  check this before trusting turn-by-turn determinism.
- **Phase 2 port allocation — landed with the known limitation, not solved.**
  `FTEST_NET_ENET_LOOPBACK_PORT` is a fixed port
  (`ftest_net_enet_loopback_shared.h`), not OS-assigned. Genuinely harder
  now than originally scoped: with two *separate processes* and no shared
  memory or IPC channel between them, "discover the ephemeral port the host
  was assigned and hand it to the join process" needs its own coordination
  mechanism (a file the host writes and the join process/driver script
  polls, most likely) — deferred rather than built, since the fixed-port
  approach works and the collision risk is low for a manually-invoked test.
  Revisit if this ever needs to run unattended/concurrently with itself in
  CI.
- **gcov coverage merging across two processes — resolved.** Was "not
  automated" as of Phase 2's initial landing; `scripts/run_ftest_net_enet_loopback.sh`
  now does this as a final best-effort phase (see "Coverage merge across the
  two loopback processes" above) via `gcov-tool merge`, refreshes
  `out/coverage-ftest/coverage.info`, and calls `scripts/merge_coverage.sh`
  unchanged for the combined report. One residual rough edge:
  `KFX_LCOV_EXCL_LINE_PATTERN` is duplicated as a literal string in the
  script (no clean way to read a CMake variable from shell) — if
  `CMakeLists.txt`'s copy ever changes, this script's copy needs a matching
  update, silently drifting out of sync otherwise.
- **`resync_game()`'s own host/client dispatch is still untested.** The
  landed test bypasses `resync_game()` itself (and its
  `store_localised_game_structure()`/`recall_localised_game_structure()`
  bracketing) by calling `send_resync_game()`/`receive_resync_game()`
  directly, specifically to avoid flipping the global `my_player_number`
  mid-test (risks other systems that read it, e.g. `get_my_player()`,
  resolving to an invalid or wrong player on a level that may not have a
  second valid `PlayerInfo`). Closing this gap needs either a level with a
  confirmed-valid second player, or a narrower way to fake the dispatch
  condition without touching the global.

## Non-goals

- Not attempting to fix the two `check_layering.py` violations themselves in
  this plan — per architecture.md §8.2 they're structural/by-design
  (raw-blob wire format, ABI-shared struct), independent of coverage. This
  plan only builds the regression net that would make attempting such a
  redesign safer later, if that's ever pursued.
- Not refactoring `bflib_enet.cpp`'s global ENet state into an instantiable
  form — considered for Phase 2 (see "Phase 2: a real design change"),
  explicitly not chosen: touching that file's structure is exactly the kind
  of change this whole effort is meant to have a safety net *before*
  attempting, not as a side effect of building the net.
- Not building a real login handshake (`net_lobby.c`'s `NETMSG_LOGIN`
  protocol) into either phase — both `netstate.my_id` assignments in Phase 2
  are hardcoded test bookkeeping, not discovered over the wire. `net_lobby.c`
  is a different file from the two this plan targets.

## Next steps

1. **Done.** Phase 1's fake `NetSP` (`ftest_net_fake.{h,c}`) and first test
   (`net_resync_fake_multiplayer`) landed directly as an `src/ftests/`
   harness and pass.
2. **Done.** Phase 2's two-process real ENet loopback
   (`ftest_net_enet_loopback_{host,join}`) landed and passes, run via
   `scripts/run_ftest_net_enet_loopback.sh`.
3. **Done.** Coverage merge across the two loopback processes implemented
   in `scripts/run_ftest_net_enet_loopback.sh` (`gcov-tool merge` +
   `scripts/merge_coverage.sh`) — see "Coverage merge across the two
   loopback processes" above for the full pipeline.
4. **Done.** Extended the loopback pair to cover more of `bflib_enet.cpp`:
   `sendmsg_all`/`sendmsg_single_unsequenced`/`drop_user`/the six `Get*`
   connection-quality functions. Final measured result: `net_resync.cpp`
   18.7% → 70.0% (unchanged since Phase 2 first landed), `bflib_enet.cpp`
   0% → **49.4%** — see "Extending the loopback pair" above.
5. Remaining `bflib_enet.cpp` gap is almost entirely the holepunch/
   matchmaking cluster (`join_via_holepunch`, `create_ipv6_host`,
   `resolve_punch_address`, `enet_matchmaking_host_update`, ...) —
   deliberately left alone (see "Extending the loopback pair"'s own
   reasoning): would need a fake `EnetConnectivityServices`, materially
   bigger than anything added so far. Worth its own pass if `bflib_enet.cpp`
   coverage becomes a priority again later.
6. Close the `resync_game()`-dispatch gap noted above (Phase 1), if a
   suitable second-player level is confirmed (or another way to avoid
   touching `my_player_number` mid-test is found).
7. If this ever needs to run unattended/in CI: solve the port-allocation
   open question above properly rather than living with the fixed-port
   limitation Phase 2 shipped with.
