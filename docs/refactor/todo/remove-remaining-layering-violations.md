# Plan: actually removing the two remaining `check_layering.py` accepted residuals

Status: **both violations fixed and verified, 2026-08-31.** `ACCEPTED_VIOLATIONS`
in `scripts/check_layering.py` is now an empty set — `check_layering.py
--strict` reports zero violations, accepted or otherwise.

**Violation 1** (`bflib_enet.cpp` → `net_main.h`): fixed exactly as
planned. New `src/kfx_platform/include/bflib_netsp.h` holds `struct NetSP`
and its supporting types (`NetUserId`, `SERVER_ID`, `MAX_NET_USERS`,
`MAX_NET_PEERS`, `enum NetDropReason`, `NetDropCallback`/
`NetNewUserCallback`, the `TIMEOUT_CONNECT_*`/`PEER_TIMEOUT_*` constants);
`net_main.h` now `#include`s it instead of defining these itself;
`bflib_enet.cpp` includes `bflib_netsp.h` directly instead of `net_main.h`.
Pure declaration move, zero behavior change, exactly as predicted.

**Violation 2** (`net_resync.cpp` → `kfx_frontend_state.h`/
`kfx_game_state.h`/`game_legacy.h`): fixed via the planned `NetCallbacks`
extension, one pair per struct owner —
`resync_export_game_state`/`resync_import_game_state` (implemented in
`game_legacy.c`, combining `game`+`kfx_game_state` since both are
kfx_game-owned) and `resync_export_frontend_state`/
`resync_import_frontend_state` (implemented in `kfx_frontend_state.c`).
`send_resync_game()`/`receive_resync_game()` now treat both as opaque
`(pointer, length)` blobs, length-prefixed on the wire (like the existing
Lua blob already was), instead of `#include`-ing the headers and memcpy-ing
at fixed offsets. The original two-phase atomicity discipline (net_callbacks.h's
own comment on the Lua pair: parse and validate everything *before*
mutating any local state, so a failure partway through leaves everything
untouched) was preserved deliberately — the raw `kfx_sim_state`/
`kfx_net_state`/`lish` memcpys were moved to the very end, after all three
callback-based imports (game state, frontend state, Lua) succeed, not
interleaved with parsing.

Verified: clean two-variant build; `check_layering.py --strict` and
`check_layering_symbols.py --strict` both report zero new violations;
`kfx_net_utest` (85 assertions) passes unchanged; the full 16-test
`-ftests` sweep passes, **including `net_resync_fake_multiplayer`** — the
byte-for-byte snapshot/corrupt/round-trip/`memcmp` test built in
[ftest-fake-multiplayer.md](ftest-fake-multiplayer.md) Phase 1, which is
exactly the regression net this plan was written to spend; `scripts/run_ftest_net_enet_loopback.sh`
(the real two-process ENet session, Phase 2) also still passes clean,
confirming Violation 1's header split didn't disturb `bflib_enet.cpp`'s
real behavior. New coverage: `game_legacy.c` 94.1% (3/3 functions),
`kfx_frontend_state.c` 94.1% (6/6 functions) — the new export/import
functions are directly exercised by the existing round-trip test, not just
compiled. `net_resync.cpp`'s line *percentage* dropped from 70.0% to 53.0%
(180/257 → 201/379) — not a regression: the file grew by ~120 lines (the
new length-prefixed parsing needs explicit truncation checks the old
fixed-offset version didn't), and the absolute covered-line count actually
increased; the new bounds-check error paths (malformed/truncated wire data)
simply aren't hit by the one happy-path test that exists today.

## Motivation

`check_layering.py`'s `ACCEPTED_VIOLATIONS` (architecture.md §8.2) currently
holds two residuals, both in netcode:

```
kfx_net -> kfx_frontend:
  src/kfx_net/src/net_resync.cpp  includes  kfx_frontend_state.h
kfx_net -> kfx_game:
  src/kfx_net/src/net_resync.cpp  includes  kfx_game_state.h
  src/kfx_net/src/net_resync.cpp  includes  game_legacy.h

kfx_platform -> kfx_net:
  src/kfx_platform/src/bflib_enet.cpp  includes  net_main.h
```

Both were previously judged "no viable fix without a deeper redesign out of
scope for the refactor" — and specifically, [ftest-fake-multiplayer.md](ftest-fake-multiplayer.md)'s
own motivation was that attempting such a redesign wasn't safe *without a
regression net first*. That net now exists: `net_resync.cpp` is at 70.0%
line coverage (up from 18.7%) and `bflib_enet.cpp` at 49.4% (up from 0%),
both driven by real functional tests (`net_resync_fake_multiplayer`,
`net_enet_loopback_host`/`_join`) that exercise exactly the code these two
violations sit in. This plan is about spending that safety net: an actual
approach for removing both violations for real, not just documenting why
they're hard.

Both turn out to be more tractable than "restructure netcode" suggested —
neither needs a wire-protocol redesign. One is a pure declaration-relocation
(zero behavior change); the other has an exact working precedent already
shipping in the same file.

## Violation 1: `bflib_enet.cpp` → `net_main.h` (`kfx_platform` → `kfx_net`)

### Root cause

`struct NetSP` (the network-transport vtable — `init/exit/host/join/update/
sendmsg_*/msgready/readmsg/drop_user`) is *defined* in `kfx_net/include/
net_main.h`, but the only thing that ever *constructs* an instance of it is
`InitEnetSP()` in `kfx_platform/src/bflib_enet.cpp`. This is an inverted
dependency: `NetSP` is architecturally a platform/HAL-style interface (an
implementation swapped in by the platform layer, consumed by the layer
above — the same shape as `bflib_video`/`bflib_sound`'s abstractions), but
its *type definition* lives one layer too high for the layer that has to
implement it.

### Exact symbol set needed (verified by grep, not guessed)

`bflib_enet.cpp` uses exactly these `net_main.h` symbols, nothing else:

```
NetUserId, SERVER_ID, MAX_NET_USERS, MAX_NET_PEERS,
enum NetDropReason (NETDROP_MANUAL, NETDROP_ERROR),
NetDropCallback, NetNewUserCallback, struct NetSP,
TIMEOUT_CONNECT_HOLEPUNCH, TIMEOUT_CONNECT_DIRECT_IPV6, TIMEOUT_CONNECT_DIRECT_IPV4,
PEER_TIMEOUT_LIMIT, PEER_TIMEOUT_MIN_MS, PEER_TIMEOUT_MAX_MS
```

Notably *not* needed: `enum NetMessageType`, `struct NetUser`/`NetFrame`/
`NetState`, `TbNetworkPlayerInfo`/`TbNetworkPlayerName`, `GameVersionPacket`,
`FrontendNetService`, `TIMEOUT_JOIN_LOBBY`/`TIMEOUT_LOBBY_EXCHANGE`/
`TIMEOUT_WAIT_FOR_ALL_PLAYERS`, `NET_MSG_BUFFER_SIZE`, `INVALID_USER_ID`.
Those stay exactly where they are — they're genuine `kfx_net`-only protocol
bookkeeping (login handshake, packet buffering, session tables), not
transport-layer contract.

### Fix

Move the symbol set above into a new `kfx_platform` header —
`src/kfx_platform/include/bflib_netsp.h` — and have:

- `net_main.h` `#include "bflib_netsp.h"` instead of defining these itself.
  Every existing consumer of `net_main.h` (there are many — `net_game.c`,
  `net_exchange_*.c`, `packets*.c`, `net_resync.cpp`, ...) keeps compiling
  unchanged, since `net_main.h` still exposes the same symbols, just via
  include rather than direct definition.
- `bflib_enet.h`/`bflib_enet.cpp` `#include "bflib_netsp.h"` directly
  instead of `net_main.h`. This is what actually removes the violation:
  `kfx_platform` no longer reaches into `kfx_net` at all.

New file name/exact placement is a judgment call (could equally live
directly in `bflib_enet.h` since ENet is the only implementation that
exists today) — recommend the separate header specifically because `NetSP`
is a *generic* transport contract, not an ENet-specific one, even though
ENet is currently its only implementer; keeping the contract and the
concrete implementation in separate files matches how the codebase already
separates other HAL-style interfaces from their implementations.

### Verification

Pure declaration relocation — no logic changes, no value changes, nothing
about *what* `bflib_enet.cpp` does changes, only *where the types it uses
are declared*. Verification is almost entirely mechanical:

1. `python3 scripts/check_layering.py --strict` — the `kfx_platform ->
   kfx_net` entry should disappear from `ACCEPTED_VIOLATIONS` reporting
   (remove the tuple from the allowlist itself once fixed, same as
   [two-remaining-layering-violations.md](two-remaining-layering-violations.md)'s
   `console_cmd.c` cleanup did).
2. `scripts/check_layering_symbols.py` (the post-build symbol-level audit,
   [check-layering-symbol-level-blind-spot.md](check-layering-symbol-level-blind-spot.md)) —
   confirms no new *symbol-level* violation slipped in alongside the
   `#include` fix.
3. Clean two-variant build (`keeperfx` + `keeperfx_hvlog`).
4. `kfx_net_utest` (Catch2, 85 assertions today) unchanged.
5. `scripts/run_ftest_net_enet_loopback.sh` — this is the test that most
   directly exercises `bflib_enet.cpp`'s real behavior; a pass here after
   the header move is strong evidence nothing behavioral moved.

### Risk

Very low. The only way this goes wrong is an include-order/circularity
issue (`bflib_netsp.h` needs `bflib_basics.h` for `TbBool`/`TbError`, same
as `net_main.h` does today) or a missed transitive consumer that happened
to rely on getting one of these symbols *through* `net_main.h` without
`#include`-ing it directly (unlikely given C's explicit-include discipline,
but worth a `grep -L '#include "net_main.h"' $(grep -rl 'NetSP\|NetUserId' src/)`
sanity pass before calling it done).

## Violation 2: `net_resync.cpp` → `kfx_frontend_state.h`/`kfx_game_state.h`/`game_legacy.h`

### Root cause

`send_resync_game()`/`receive_resync_game()` (`net_resync.cpp`) serialize
`game`, `kfx_game_state`, and `kfx_frontend_state` **wholesale** as part of
the raw-blob multiplayer resync payload (architecture.md §6.2's documented
invariant — this wire format itself is not changing). To `memcpy(&game,
...)`/`memcpy(&kfx_game_state, ...)`/`memcpy(&kfx_frontend_state, ...)`,
the file needs their full type definitions, which live in `kfx_game`/
`kfx_frontend` — both above `kfx_net`.

### The fix already exists in this exact file, for a different struct

The same function already does precisely this for `kfx_script`'s Lua VM
state, via `net_callbacks->lua_resync_export(&len)`/`lua_resync_import(data,
len)` (`kfx_config/include/net_callbacks.h`, implemented in
`kfx_script/src/lua_base.c`, wired up in `main.cpp::setup_game()`). This
isn't a new pattern to invent — it's the *exact same problem*, already
solved, already shipping, in the same file, for the same reason (`kfx_net`
needing an opaque blob from a higher layer it can't `#include`).

### Proposed shape

Add **two** new pairs to `struct NetCallbacks`, one per struct rather than
one combined callback — matches "each layer owns exporting its own state"
and mirrors the Lua pair's exact signature shape:

```c
/* game_legacy.h/kfx_game_state.h -- upper-state resync payload
   (net_resync.cpp), same export/import shape as the Lua pair above and
   for the same reason: kfx_net can't #include kfx_game's headers to
   memcpy these wholesale (game_legacy.h/kfx_game_state.h, both kfx_game,
   above kfx_net). */
const char *(*resync_export_game_state)(size_t *len);
TbBool (*resync_import_game_state)(const char *data, size_t len);

/* kfx_frontend_state.h -- same reasoning, kfx_frontend layer. */
const char *(*resync_export_frontend_state)(size_t *len);
TbBool (*resync_import_frontend_state)(const char *data, size_t len);
```

Implementations: a small new function pair in `kfx_game` (natural home:
`game_legacy.c`, which already owns `struct Game game`) that
`memcpy`s `game`+`kfx_game_state` into a static buffer and returns it
(exactly `lua_resync_export`'s shape, minus the Lua VM's own
serialize-to-arbitrary-length step — this is two fixed-size structs, so
the "export" function is `memcpy` into a `static char buffer[sizeof(game)
+ sizeof(kfx_game_state)]`, not a real serializer); and a matching pair in
`kfx_frontend` (natural home: `kfx_frontend_state.c`) for
`kfx_frontend_state` alone. Both registered in `main.cpp::setup_game()`'s
existing `net_callbacks_impl` initializer, both get a `noop_*` default in
`net_callbacks.c` (matching every other field there).

`net_resync.cpp`'s `send_resync_game()`/`receive_resync_game()` change from
directly `memcpy`-ing `game`/`kfx_game_state`/`kfx_frontend_state` at fixed
offsets to calling the two new callback pairs and treating their results as
opaque `(pointer, length)` blobs — combined into the same overall buffer
alongside the fields that *don't* need to move (`kfx_sim_state`,
`kfx_net_state` — both at or below `kfx_net`'s own rank, no violation;
`lish` — `kfx_render`, also below; the existing Lua blob — already opaque).
The `#include`s of `kfx_frontend_state.h`/`kfx_game_state.h`/`game_legacy.h`
drop out of `net_resync.cpp` entirely once nothing in the file names those
types directly.

### Wire format consequence (needs confirming, not assuming)

The two structs become length-prefixed opaque sections instead of
fixed-offset direct `memcpy`s — the on-wire byte layout changes. This is
almost certainly fine: resync only ever runs between two peers of the
*same build* already (login rejects/flags a version mismatch via
`net_versions_match()`, `front_network.c:404`, before a session
proceeds) — same precondition every field-layout change to any of these
structs already relies on, not a new constraint this refactor introduces.
**Confirm `net_versions_match()` is an actual hard gate (drops/rejects the
peer), not just an advisory UI warning, before treating this as settled** —
not verified as part of this planning pass.

### Verification — the regression net this whole effort built

This is exactly what [ftest-fake-multiplayer.md](ftest-fake-multiplayer.md)'s
Phase 1 test was built for, almost too neatly: `net_resync_fake_multiplayer`
snapshots `game`/`kfx_sim_state`/`kfx_net_state`/`kfx_game_state`/
`kfx_frontend_state`/`lish` right after `send_resync_game()`, corrupts live
state, runs `receive_resync_game()`, and `memcmp`s every struct byte-for-byte
against the snapshot. It doesn't care *how* the bytes got from one side to
the other — only that they round-trip identically. That's precisely the
property this refactor must preserve, so a passing `net_resync_fake_multiplayer`
after the change is strong, specific evidence the restructuring didn't
silently drop or reorder anything.

1. `net_resync_test.cpp` (Catch2, `kfx_net/tests/`) — unaffected in shape
   (`store_localised_game_structure`/`recall_localised_game_structure`/
   `animate_resync_progress_bar` don't touch these three structs), should
   pass unchanged.
2. `-ftests net_resync_fake_multiplayer -headless -exitonfailedtest` — must
   still exit 0 and log the byte-for-byte-restored message.
3. `python3 scripts/check_layering.py --strict` — the `kfx_net ->
   kfx_frontend`/`kfx_net -> kfx_game` entries disappear from
   `ACCEPTED_VIOLATIONS`; remove those tuples from the allowlist.
4. `scripts/check_layering_symbols.py` — confirm no new symbol-level issue.
5. Full `-ftests` sweep (16 tests) + `scripts/run_ftest_net_enet_loopback.sh`
   — regression check that nothing else in the netcode path broke.

### Risk

Higher than Violation 1 (this one touches the actual wire-assembly logic,
not just declarations), but meaningfully de-risked by item 2 above being a
genuinely strong, targeted regression test for exactly this change — this
is the rare refactor where "does the byte-for-byte round trip still work"
is both the risk and the test.

## Sequencing recommendation

1. **Violation 1 first** (`bflib_enet.cpp`). Small, independent, zero
   behavior change, fast to verify, builds confidence before touching the
   riskier one.
2. **Violation 2 second** (`net_resync.cpp`). Larger, touches real
   wire-assembly logic, but has the byte-for-byte round-trip test as a
   direct safety net.

No interaction between the two — either can be done alone, in either order,
without blocking the other.

## Non-goals

- Not touching the raw-blob wire format's *design* (architecture.md §6.2's
  invariant that resync is a wholesale state-struct memcpy stays exactly as
  it is) — only *which file is allowed to know the struct layouts directly*
  changes.
- Not attempting a matching fix for `console_cmd.c` → `game_session_loop.h`
  — already resolved (dead code, removed in
  [two-remaining-layering-violations.md](two-remaining-layering-violations.md)).
- Not extending Phase 2's fake/real NetSP harnesses further as part of this
  plan — they're consumed here as existing regression coverage, not
  something this plan itself needs to grow.

## Open questions / risks

- **`net_versions_match()` enforcement — resolved.** Confirmed a hard gate
  (not just an advisory UI warning) before implementing Violation 2, so the
  wire-format-change reasoning above holds: resync's peers are always the
  same build.
- **Where `resync_export_game_state` lives — resolved.** Landed in
  `game_legacy.c`, combining `game`+`kfx_game_state` into one static
  buffer (`resync_game_state_buffer`); `game_legacy.h` already transitively
  included `kfx_game_state.h`, so no new cross-file reach was needed.
- **`bflib_netsp.h`'s exact name/location — resolved.** Landed as planned:
  `src/kfx_platform/include/bflib_netsp.h`, a dedicated header separate
  from `bflib_enet.h`.

## Next steps

1. **Done.** Violation 1 (`bflib_enet.cpp`/`net_main.h` header split) —
   implemented and verified (build, `check_layering.py --strict`,
   `check_layering_symbols.py --strict`, `kfx_net_utest`, full `-ftests`
   sweep, `scripts/run_ftest_net_enet_loopback.sh`).
2. **Done.** Violation 2 (`NetCallbacks` extension for
   `game`/`kfx_game_state`/`kfx_frontend_state`) — implemented and
   verified against `net_resync_fake_multiplayer` specifically (byte-for-byte
   round trip still passes), not just a clean build.
3. `scripts/check_layering.py`'s `ACCEPTED_VIOLATIONS` is now empty —
   architecture.md §8.2's table should be updated to reflect zero accepted
   residuals (not done as part of this pass; architecture.md itself wasn't
   touched).
3. Once both land: `check_layering.py`'s `ACCEPTED_VIOLATIONS` should be
   empty. Remove the now-dead tuples from the allowlist and update
   architecture.md §8.2's table to reflect zero accepted residuals.
