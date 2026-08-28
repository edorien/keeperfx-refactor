# Stage 6 appendix — `roomspace_prediction.c`'s upward includes

See [stage-06-kfx-sim.md](stage-06-kfx-sim.md) and
[stage-06b-sim-frontend-leaks.md](stage-06b-sim-frontend-leaks.md) (the
sibling appendix this one follows on from — same file, left explicitly
out of scope there). Not required to land any other stage; should land
before stage 13's dependency-graph check goes strict.

## Scope: what's specific to this file vs. pre-existing/systemic

`libs/kfx_sim/src/roomspace_prediction.c` (`libs/kfx_sim/src/roomspace_prediction.c`)
directly `#include`s six headers above `kfx_sim` in the dependency order
(`kfx_platform → kfx_config → kfx_sim → kfx_render → kfx_net → kfx_game →
kfx_frontend`, per `scripts/check_layering.py`'s `LIBRARY_ORDER`):
`game_legacy.h` (`kfx_game`), `packets.h`/`net_exchange_gameplay.h`
(`kfx_net`), `cursor_tag.h`/`engine_render.h` (`kfx_render`),
`frontmenu_ingame_evnt.h` (`kfx_frontend`).

**`game_legacy.h` is out of scope for this appendix.** It's included by
~80 of `kfx_sim`'s ~150 `.c` files (`scripts/check_layering.py`'s full
output for the cluster), including `roomspace.c` and `thing_creature.c` —
this is the pre-existing "god header" problem the overview already tracks
as an ongoing, incremental effort (see
[stage-05-god-headers.md](stage-05-god-headers.md) and §7's revision
history: "struct Game decomposition... incremental field migration, timed
to match stages 6–10"). Nothing here is specific to `roomspace_prediction.c`;
fixing it is a much larger, separately-tracked effort, not a one-file fix.
Notably, `game_legacy.h` transitively `#include`s `packets.h` — which
matters for the next section.

The other five headers *are* specific to this file (only 2 other `kfx_sim`
files touch `packets.h`/`cursor_tag.h`, and no others touch
`net_exchange_gameplay.h`/`engine_render.h`/this particular
`frontmenu_ingame_evnt.h` symbol) and split into three groups of very
different difficulty.

## Groups 1 and 2 — implemented

Landed together (Group 2's plan changed during implementation — see
below), as a separate commit. Both extend `sim_feedback.h`, same mechanism
as stage 06b:

- **`frontmenu_ingame_evnt.h`**: `battle_creature_over` (1 read,
  `prevent_local_dig_prediction`) → `is_battle_creature_over_active()`.
- **`engine_render.h`**: `map_volume_box.visible` (1 write),
  `box_lag_compensation_x`/`_y` (4 writes, always to `0`) →
  `hide_map_volume_box()` / `reset_box_lag_compensation()`.
- **`cursor_tag.h`**: `tag_cursor_blocks_dig(player, pckt, roomspace,
  stl_x, stl_y, full_slab)` → wrapped as a `sim_feedback` callback field
  with the identical signature, wired directly to the real function in
  `main.cpp` (no wrapper needed there).

The plan below for `cursor_tag.h` (move the call to its caller in
`packets.c`, since `packets_input.c` already calls the same function the
same way) turned out not to fit once traced fully: the `player` argument
`tag_cursor_blocks_dig` needs isn't the live player, it's
`predicted_player` — a local copy `update_local_dig_prediction_cursor_preview`
builds internally via `get_local_dig_prediction_roomspace()` (packet-context
tweaks to `roomspace_highlight_mode`, `one_click_lock_cursor`,
`render_roomspace.*`, etc.). The caller has no way to reconstruct that
without duplicating the prediction logic itself, so "just move the call
up" would have meant exposing a second piece of internal state (on top of
the existing `local_dig_render_roomspace`/`get_local_dig_prediction_render_roomspace()`
accessor) rather than a clean one-line move. A callback, matching Group
1's shape, turned out to be the better fit after all — kept here as a
record of why the original plan changed, same as stage 06b's
`my_event_button_state` discovery.

Verified with `scripts/check_layering.py` (all three headers gone from
`roomspace_prediction.c`'s violation list; `game_legacy.h`/`packets.h`/
`net_exchange_gameplay.h` remain, as expected — Group 3, below) and a full
`linux.mk` build/link.

## Group 3 — real design decision, not a mechanical fix (needs a maintainer call)

- **`packets.h`** (12 sites: `struct Packet` used *by value* — copied,
  swapped, and field-accessed throughout, not just held by pointer — plus
  `get_packet`, `get_packet_direct`) and **`net_exchange_gameplay.h`**
  (`get_history_packet`, 1 site).

  `struct Packet` (`libs/kfx_net/include/packets.h:280`) is a small, plain
  12-field POD struct — no pointers, no methods, no ownership semantics.
  It's already a de facto shared vocabulary type: `roomspace.h` and
  `thing_creature.h` (both `kfx_sim` *public* headers) forward-declare and
  take it by pointer, and `thing_creature.c` `#include`s `packets.h`
  directly. It's used across `kfx_sim`, `kfx_render`, `kfx_game`, and
  `kfx_frontend` — this is the same shape of problem as `struct Game`
  (stage 5) and the `Navigation`/`Ariadne` circular-ownership issue (stage
  06a), just smaller: a type genuinely needed by many layers, but
  currently owned by one of the highest (`kfx_net`).

  Unlike `battle_creature_over`/`map_volume_box`, this can't be wrapped as
  a handful of callback fields: `roomspace_prediction.c` doesn't just read
  a couple of fields, it does full struct copies and in-place swaps
  (`struct Packet saved_packet = *direct_packet; *direct_packet = *pckt;`
  in `update_predicted_build_or_sell_roomspace_preview`) — that requires
  the complete type, not accessor functions, and `->field` access on a
  pointer already requires the complete type too (a forward declaration
  isn't enough once you dereference a field, unlike the `roomspace.h`/
  `thing_creature.h` cases which only pass it through by pointer).

  Three real options, not a preferred one yet:

  1. **Relocate `struct Packet`'s definition down** (to `kfx_config`, or a
     new small shared header) — mirrors how `globals.h` already holds
     `Coord3d`/domain-ID typedefs as legitimate shared vocabulary.
     `packets.h` keeps the *functions* (`get_packet`, network exchange
     logic, `PckA_*`/`PCtr_*` constants can move too or stay — TBD) and
     becomes purely `kfx_net`'s interface layer above the now-shared type.
     Cleanest end state; touches every file that currently gets `struct
     Packet` from `packets.h` (need an actual count — not done here).
  2. **Opaque handle + field accessors**, ariadne-style
     (`world_get_packet_control_flags(handle)`, etc.) — avoids moving the
     type, but doesn't fit this file's struct-copy/swap pattern without
     also inventing copy/restore callbacks, which is more machinery than
     the problem warrants for a 12-field POD struct.
  3. **Push packet-field extraction into the (correctly-positioned)
     caller** — `packets.c`/`packets_input.c` already have full legitimate
     access to `struct Packet`; have them extract the handful of scalars
     `roomspace_prediction.c` actually needs (`control_flags` bits,
     `action`, `actn_par1/2`, `pos_x/y`, `turn`) and pass those down as
     plain parameters, so `kfx_sim` never touches `struct Packet` at all.
     No type relocation needed, but is the most invasive rewrite of this
     file's actual control flow (many internal functions take/return
     `struct Packet*` today) and duplicates packet-unpacking logic that
     currently lives in one place.

  Recommendation, not yet decided: **option 1** — the type is small,
  already leaking through `kfx_sim`'s own public headers, and matches the
  precedent `globals.h` already sets for exactly this kind of shared
  low-level struct. But this needs a maintainer call before implementing
  (same as stage 5's `struct Game` decomposition and stage 06a's ariadne
  pathfinding decision), not something to do unilaterally in a mechanical
  pass — a full audit of every current `packets.h` includer (not just the
  three `kfx_sim` files found here) is the right next step if this option
  is chosen.

## Suggested sequencing

Groups 1 and 2 landed independent of Group 3's decision — neither touches
`struct Packet`. Group 3 remains a separate, larger piece of work gated on
a decision, same as ariadne's stretch-goal framing in stage 06a.
