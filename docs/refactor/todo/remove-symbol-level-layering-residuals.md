# Plan: removing the symbol-level layering residuals (prerequisite for real static libraries)

Status: **implemented and verified, 2026-08-31.** `ACCEPTED_SYMBOL_VIOLATIONS`
in `scripts/check_layering_symbols.py` now holds exactly one entry —
`kfxmain` — down from seven (three already dead before this pass, four
fixed by it). `check_layering_symbols.py --strict` reports zero *new*
violations.

**What landed, exactly as planned:**
- `packets[PACKETS_COUNT]`/`bad_packet`/`PACKETS_COUNT` moved from
  `kfx_net_state.h`/`packets_misc.c`/`net_game.h` down into `kfx_sim`'s
  `packet_data.h` (renamed `sim_packets[]` to avoid a naming clash with an
  unrelated local `packets[]` field already used in
  `net_exchange_gameplay.c`). `get_packet`/`get_packet_direct`/
  `set_packet_action`/`set_players_packet_action` — all confirmed genuinely
  trivial, no netcode logic inside — now live in a new
  `src/kfx_sim/src/packet_data.c`. `kfx_net`'s `packets.c`/`packets_misc.c`
  keep writing into `sim_packets[]` directly for their own real
  packet-exchange logic (checksums, wire buffer packing) — a legitimate
  downward touch. Two more direct-reference call sites turned up beyond the
  plan's own file list (`kfx_apploop/src/game_session_loop.cpp`,
  `kfx_frontend/src/front_torture.c`) — both already-legitimate upward
  callers, just needed the rename, no new violation.
- `creature_table_add[KEEPERSPRITE_ADD_NUM]` moved from `kfx_render`'s
  `custom_sprites.c` into `kfx_sim`'s `creature_graphics.c`, right next to
  its sibling `creature_table` — exactly the precedent this plan predicted
  would work. `SIM_KEEPERSPRITE_ADD_OFFSET`/`_NUM` (previously duplicated
  as file-local constants in `creature_graphics.c`) moved up into
  `creature_graphics.h` alongside it, since the `extern` declaration needs
  a *complete* array type (a concrete size) for `sizeof()` to still work at
  every `#include` site — an incomplete `extern struct KeeperSprite
  creature_table_add[];` was the one thing the plan hadn't anticipated,
  caught immediately by the build (`custom_sprites.c`'s own
  `sizeof(creature_table_add)` call).
- Two now-dead Catch2 test-stub scaffolds removed entirely:
  `kfx_sim/tests/kfx_sim_test_stubs.cpp` (its fake `creature_table_add[1]`
  now conflicts with the real array, always linked) and
  `kfx_sim/tests/packet_test_stubs.cpp` + the `kfx_packet_test_stubs`
  CMake target that built it (its fake `get_packet`/etc. are now
  redundant — the real ones are always linked wherever `kfx_sim` is).
  `kfx_net`'s and `kfx_sim`'s own Catch2 test files
  (`packets_misc_test.cpp`, `net_checksums_test.cpp`) updated for the
  rename; no test *behavior* changed, only which symbol they poke.

**Verified**: clean two-variant build; `check_layering.py --strict` and
`check_layering_symbols.py --strict` both clean; all ten `kfx_*_utest`
Catch2 suites pass (`kfx_sim_utest` 1864 assertions, `kfx_platform_utest`
669, `kfx_config_utest` 1562, and seven more, all green); full 16-test
`-ftests` sweep passes including `net_resync_fake_multiplayer`;
`scripts/run_ftest_net_enet_loopback.sh` (the real two-process ENet
session, which exercises exactly the packet-exchange path this plan
touched) still passes clean.

**Not done as part of this pass** (see "Next steps"): actually attempting
a `kfx_*` `STATIC` library conversion as a proof of concept — this plan
only removed the known blockers for it.

## Motivation

`check_layering.py --strict` (the `#include`-graph checker) now reports zero
violations, accepted or otherwise
([remove-remaining-layering-violations.md](remove-remaining-layering-violations.md)).
`scripts/check_layering_symbols.py` — the post-build checker that catches
the blind spot `#include` scanning can't see (a lower-ranked library
reaching a higher-ranked one via a bare `extern` forward declaration
instead of an `#include`) — still has 7 accepted entries in
`ACCEPTED_SYMBOL_VIOLATIONS`, 3 of which are already dead (see below) and 4
real:

```
kfx_platform -> app_entry:  kfxmain
kfx_render   -> kfx_net:    get_packet_direct
kfx_sim      -> kfx_net:    get_packet, get_packet_direct, set_packet_action, set_players_packet_action
kfx_sim      -> kfx_render: creature_table_add
```

This came up while answering "could the `kfx_*` libraries become real
`STATIC` libraries instead of `OBJECT` libraries" — they can't, yet,
specifically *because* of these residuals. `OBJECT` libraries flatten every
constituent `.o` into one list handed to the final link in a single step,
so `ld.bfd`'s single-pass, left-to-right symbol resolution (already noted
in `CLAUDE.md`) never has to care which "library" a `.o` came from.
`STATIC` archives (`.a`) are scanned once, in declared order
(`kfx_platform`, `kfx_config`, `kfx_sim`, `kfx_render`, `kfx_net`, ...) —
`kfx_sim.a` calling into `kfx_net.a` (three ranks *later* in that list)
would leave those symbols unresolved unless every consumer of the archive
already needed something from it in the right order, which is exactly what
these residuals prevent for `kfx_sim`/`kfx_render`.

**Dead entries found while re-checking, should be removed regardless of
whether the rest of this plan proceeds**: `("kfx_net", "kfx_frontend_state",
"kfx_frontend")`, `("kfx_net", "kfx_game_state", "kfx_game")`, `("kfx_net",
"game", "kfx_game")` no longer occur — `net_resync.cpp` stopped referencing
those symbols directly once
[remove-remaining-layering-violations.md](remove-remaining-layering-violations.md)'s
Violation 2 landed (it goes through `NetCallbacks` function pointers now,
not direct symbol references). Confirmed via a fresh
`check_layering_symbols.py --build-dir out/coverage-ftest --strict` run:
these three don't appear in the report at all anymore.

## `kfxmain` (`kfx_platform` → `app_entry`) — genuinely irreducible, not a static-link blocker either

`PlatformLinux.cpp`'s real, OS-callable `main(argc, argv)` calls
`kfxmain(argc, argv)` (declared in `kfx_platform`'s own `platform.h`,
defined in `main.cpp`) after doing platform-level setup — the standard
"OS calls the lowest layer first, which hands off to the composition root"
bootstrap shape. No fix proposed; already correctly documented as
intentional.

Traced through *why* it doesn't block static linking either, despite being
`kfx_platform -> app_entry` (later in scan order than `kfx_platform`):
`main.cpp` isn't part of any `kfx_*` library (`CLAUDE.md`:
`KFX_SOURCES_REMAINING` is exactly `main.cpp` + `src/ftests/*`) — it's
compiled as a plain object and, per `add_executable()`'s normal argument
order, sits on the link line *before* the linked libraries. `main.cpp.o`
(with `kfxmain`'s definition) is therefore already in the linker's defined-
symbol pool by the time it scans `kfx_platform.a` to satisfy the C
runtime's own undefined reference to `main`, so `PlatformLinux.cpp.o`'s
reference to `kfxmain` resolves immediately. Confirmed this is genuinely
about relative position, not luck: `KFX_OBJECT_LIBS_STD`'s declared order
already has `kfx_platform` first among the libraries, and libraries are
appended *after* the executable's own sources — so this holds regardless
of the `kfx_platform` internal ordering. **No fix needed for this one,
kept out of scope below.**

## `get_packet`/`get_packet_direct`/`set_packet_action`/`set_players_packet_action` (`kfx_sim`/`kfx_render` → `kfx_net`)

### Root cause

`packet_data.h` (`kfx_sim`) already declares `struct Packet` and these four
accessors at `kfx_sim`'s layer — deliberately, since `kfx_sim`/`kfx_render`
dereference `struct Packet` fields directly and pervasively (the header's
own comment explains this, same shape as `camera_data.h`'s `struct
Camera`). But the *implementations* stay in `kfx_net`'s `packets.c`/
`packets_misc.c`, because the storage they read/write —
`kfx_net_state.packets[PACKETS_COUNT]` (`net_main.h`... actually
`kfx_net_state.h`; `PACKETS_COUNT` itself is defined in `kfx_net`'s
`net_game.h`) — lives in `kfx_net_state`, a `kfx_net`-owned global.

Verified all four are genuinely trivial (bounds check + array index, or a
plain field-setter with no side effects) — no netcode-specific logic
(checksums, sequencing, wire I/O) is mixed in:

```c
struct Packet *get_packet(long plyr_idx) {
    struct PlayerInfo* player = get_player(plyr_idx);        // kfx_sim
    if (player_invalid(player)) return INVALID_PACKET;
    if (player->packet_num >= PACKETS_COUNT) return INVALID_PACKET;
    return &kfx_net_state.packets[player->packet_num];        // the only kfx_net touch
}
struct Packet *get_packet_direct(long pckt_idx) {
    if ((pckt_idx < 0) || (pckt_idx >= PACKETS_COUNT)) return INVALID_PACKET;
    return &kfx_net_state.packets[pckt_idx];                  // ditto
}
void set_packet_action(struct Packet *pckt, ...) { /* pure field sets */ }
void set_players_packet_action(struct PlayerInfo *player, ...) {
    struct Packet* pckt = get_packet_direct(player->packet_num);
    /* pure field sets */
}
```

`get_player()` is already `kfx_sim` (`player_data.h`). The *only* thing
keeping these out of `kfx_sim` is `kfx_net_state.packets[]`'s location —
and `struct Packet` itself (`turn`/`checksum`/`action`/params/cursor
pos/control flags) is plain per-turn input data with no network-transport
machinery embedded in the type.

`bad_packet` (`INVALID_PACKET`'s target, `packets_misc.c`) is the same
shape one level down — declared `extern` in `kfx_sim`'s `packet_data.h`,
defined in `kfx_net`. Not currently flagged (nothing in `kfx_sim`/
`kfx_render`'s compiled objects references it directly — only `kfx_net`'s
own accessor bodies do), but it needs to move alongside the accessors it
backs, or the relocated implementations would themselves become a new
`kfx_sim -> kfx_net` violation.

### Fix

Move `packets[PACKETS_COUNT]` (and `bad_packet`) down to `kfx_sim` —
`kfx_sim_state` if it belongs alongside the rest of that struct's synced
fields, or a plain `kfx_sim`-owned global if it doesn't need to ride along
with `kfx_sim_state`'s wholesale-memcpy semantics (worth checking which
during implementation: `packets[]` is *not* one of §6.2's three raw-blob
sync points' payloads today since it's part of `kfx_net_state`, not one of
the structs `net_resync.cpp`/`game_saves.c` currently memcpy — moving it
into `kfx_sim_state` would silently add it to every future raw-blob sync;
moving it to a bare extern keeps today's scope exactly as narrow as it is
now). Move the four accessors' implementations (plus `bad_packet`) into a
new or existing `kfx_sim/src` file (`packet_data.c` alongside
`packet_data.h` is the obvious name). `PACKETS_COUNT` moves too (or gets
duplicated the way `PLAYERS_COUNT` already is between `kfx_config`/`kfx_sim`
per
[two-remaining-layering-violations.md](two-remaining-layering-violations.md) —
a call to make during implementation, not here).

`kfx_net` keeps *writing* into the relocated storage exactly as before —
`packets.c`, `packets_misc.c`, and `net_exchange_gameplay.c` (the three
files confirmed, by grep, to touch `kfx_net_state.packets[]` directly) just
write into the new `kfx_sim`-owned location instead. That's a legitimate
downward touch (a higher-ranked library writing into a lower-ranked
library's state), not a violation — same shape `net_resync.cpp`'s raw-blob
sync already relies on throughout §6.2, and the exact same reasoning that
already justifies `kfx_render`'s `custom_sprites.c` *writing* into `kfx_sim`'s
already-correctly-placed `creature_table` pointer (see below).

**Zero call-site changes needed.** All ~260 project-wide call sites (the
overwhelming majority in `kfx_frontend`/`kfx_game`/`kfx_net` itself, where
calling into `kfx_net` is already legitimate) keep calling `get_packet(x)`/
etc. by the same name with the same signature — only the *implementation's
physical location* and *which state struct it reads* change. Only the
~10 call sites actually in `kfx_sim`/`kfx_render` (`roomspace.c`,
`roomspace_prediction.c`, `creature_instances.c`, `thing_creature.c`,
`engine_redraw.c`) matter for the violation itself, and none of them need
editing either.

## `creature_table_add` (`kfx_sim` → `kfx_render`)

### Root cause

Same shape, and the sibling variable in the very same header
(`creature_graphics.h`) already shows the fix works: `creature_table`
(**without** `_add`) is a `struct KeeperSprite *` **defined in `kfx_sim`'s
own `creature_graphics.c`** (confirmed: `grep` finds exactly one
definition, `src/kfx_sim/src/creature_graphics.c:50`), with `kfx_render`
free to *assign* into it from a higher layer — no violation, already
working today. `creature_table_add[KEEPERSPRITE_ADD_NUM]`, by contrast, is
a fixed-size array **defined and populated in `kfx_render`'s
`custom_sprites.c`** (sprite decompression/loading — genuinely
`kfx_render`'s job), merely *declared* `extern` in `kfx_sim`'s header
because `kfx_sim`'s `creature_graphics.c` dereferences its fields directly
(frame count, rotable flag, sprite pointer lookups for creature animation).

### Fix

Move `struct KeeperSprite creature_table_add[KEEPERSPRITE_ADD_NUM]`'s
definition into `kfx_sim/src/creature_graphics.c`, right next to
`creature_table`'s own definition — matching the precedent already
established one line above it in the same header. `kfx_render`'s
`custom_sprites.c` keeps writing into it during sprite loading (`memset`,
the per-sprite `struct KeeperSprite *ksprite = &creature_table_add[...]`
population loop) exactly as today; that's the same legitimate
higher-writes-into-lower pattern as `creature_table` itself already uses.

**Call sites**: only `kfx_sim/src/creature_graphics.c` (4 references) reads
it outside `custom_sprites.c`; both stay unchanged in behavior, only the
storage's physical file moves.

## Verification plan

1. `python3 scripts/check_layering_symbols.py --build-dir <tree> --strict`
   — the `kfx_sim -> kfx_net`, `kfx_render -> kfx_net`, and
   `kfx_sim -> kfx_render` entries should disappear; remove those tuples
   (and the three already-dead `kfx_net -> kfx_frontend`/`kfx_game` ones)
   from `ACCEPTED_SYMBOL_VIOLATIONS`, leaving only `kfxmain`.
2. `python3 scripts/check_layering.py --strict` — should remain clean
   (this fix doesn't change any `#include` edges, only where symbols are
   *defined*, so the include-level checker was never going to see this
   class of issue either way).
3. Clean two-variant build.
4. `kfx_sim_utest`/`kfx_render_utest`/`kfx_net_utest` (Catch2) — the
   existing `kfx_sim_test_stubs.cpp`/`kfx_net`'s
   `kfx_game_state_test_stubs.cpp`-style stub files that currently provide
   fake `creature_table_add[]`/packet storage for isolated unit tests will
   need their own updates to match the new ownership (they already exist
   specifically *because* of these residuals — see
   `src/kfx_sim/tests/kfx_sim_test_stubs.cpp`'s and
   `src/kfx_net/tests/packets_misc_test.cpp`'s own comments — so this is
   expected churn, not a surprise).
5. Full `-ftests` sweep + `scripts/run_ftest_net_enet_loopback.sh` —
   regression check that packet handling and creature rendering still
   behave identically.
6. Only *after* all of the above: attempt converting one `kfx_*` OBJECT
   library to `STATIC` as a proof of concept (not all ten at once) and
   confirm it actually links — this plan removes the known blockers, but
   hasn't itself verified no *other*, not-yet-symbol-scanned issue exists
   (`check_layering_symbols.py`'s own docstring frames it as
   "for periodic/CI-post-build runs or local investigation", not an
   exhaustive guarantee).

## Non-goals

- Not converting any `kfx_*` library to `STATIC` as part of this plan —
  this only removes the known blockers; the conversion itself (and
  whatever `CMakeLists.txt` restructuring it needs — dropping the
  `KFX_OBJECT_LIBS_STD`/`_HVLOG` naming, `add_library(... STATIC ...)`
  instead of `OBJECT`, checking whether the two-variant std/hvlog split
  still works the same way) is a separate follow-up if this lands and the
  static-library question is still worth pursuing.
- Not touching `kfxmain` — confirmed both irreducible (OS bootstrap shape)
  and, separately, not actually a static-link blocker given current link
  ordering. **Revised**: turned out not to be irreducible after all —
  `kfx_platform` owned the *entry point itself*, not just a platform
  service; moving `main()`/`WinMain()` down into `app_entry` removed the
  reverse reference entirely. See
  [remove-kfxmain-symbol-residual.md](remove-kfxmain-symbol-residual.md).
- Not deciding `PACKETS_COUNT`'s final placement (own definition vs.
  duplicated `#define` like `PLAYERS_COUNT`) or whether `packets[]` joins
  `kfx_sim_state` vs. stays a bare extern — flagged as implementation-time
  calls, not resolved here.

## Next steps

1. **Done.** Removed the three dead `ACCEPTED_SYMBOL_VIOLATIONS` entries.
2. **Done.** Relocated `sim_packets[]`/`bad_packet`/the four accessors
   from `kfx_net` to `kfx_sim` (`packet_data.c`).
3. **Done.** Relocated `creature_table_add[]` from `kfx_render` to
   `kfx_sim` (`creature_graphics.c`, next to `creature_table`).
4. **Done.** Removed the two now-dead Catch2 test stubs
   (`kfx_sim_test_stubs.cpp`, `packet_test_stubs.cpp` + its
   `kfx_packet_test_stubs` CMake target) and updated the two test files
   that referenced the renamed `sim_packets[]`.
5. **Done.** Full verification plan above all green.
6. **Done.** `check_layering_symbols.py`'s `ACCEPTED_SYMBOL_VIOLATIONS`
   updated (now just `kfxmain`). `architecture.md` wasn't found to
   reference any of these four symbols by name, so no update needed there
   — only the `check_layering.py`-level residuals (already handled in
   [remove-remaining-layering-violations.md](remove-remaining-layering-violations.md))
   were named in prose there.
7. **Done**, as a separate follow-up commit (`70ef4db20`, "Convert all
   kfx_* libraries from OBJECT to STATIC"): converted `kfx_platform`
   first as a proof of concept, confirmed it links on both native Linux
   and mingw-w64 Windows, then converted the remaining 9 `kfx_*`
   libraries the same way. Root `CMakeLists.txt` reverses
   `KFX_OBJECT_LIBS_STD`/`_HVLOG` (built lowest-ranked-first) before
   linking `keeperfx`/`keeperfx_hvlog`, since `ld.bfd`'s single
   left-to-right archive scan needs referencing libraries listed before
   the libraries they reference — the opposite ordering `OBJECT`
   libraries needed. Every `kfx_*_utest`'s existing hand-written link
   order already listed libraries highest-rank-first, so no test
   `CMakeLists.txt` needed changes. Verified: both native-Linux variants
   and the mingw-w64 cross-compile build and link; all 10 `kfx_*_utest`
   Catch2 suites pass; the full ftest sweep (including
   `net_resync_fake_multiplayer`) and the real two-process ENet loopback
   ftest both pass; `check_layering.py`/`check_layering_symbols.py
   --strict` remain clean.
