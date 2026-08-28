# Stage 6 appendix — `kfx_sim`'s remaining upward includes into `kfx_frontend`

See [stage-06-kfx-sim.md](stage-06-kfx-sim.md) and
[sim_feedback.h](../../libs/kfx_config/include/sim_feedback.h) (the existing
callback-registration mechanism this appendix extends). Discovered while
doing stage 10.0 ([stage-10-kfx-frontend.md](stage-10-kfx-frontend.md)'s
internal `gui_*` cleanup); out of stage 10's scope because the violation is
a `kfx_sim` → `kfx_frontend` edge, not anything internal to
`kfx_frontend`. Not required to land stage 10.0, but should land before
stage 13's dependency-graph check goes strict, since it fails that check.

## Why stage 6's audit missed this

Stage 6's cluster audit (`stage-06-kfx-sim.md`) doesn't mention
`roomspace.c` or `map_events.c` reaching into frontend headers, because the
symbols were reachable transitively: `gui_room_type_highlighted` was
declared in `frontmenu_ingame_tabs.h` (already a counted `roomspace.c`
include, so no *new* edge showed up when grepped) until stage 10.0 moved it
into `gui_draw.h`, and the direct `front_input.h`/`gui_frontmenu.h`/
`frontmenu_ingame_evnt.h` includes were simply not flagged by a
prefix-cluster-level pass. `scripts/check_layering.py`'s per-file
CLASSIFICATION table does catch all of these as `kfx_frontend`-owned
headers — the violation was real before stage 10.0, just under-counted.

## The two violations

**`libs/kfx_sim/src/roomspace.c`** includes `front_input.h` and
`gui_draw.h`:
- `is_game_key_pressed(...)` — 17 call sites, all reading UI key-binding
  state (`Gkey_BestRoomSpace`, `Gkey_SquareRoomSpace`,
  `Gkey_RoomSpaceIncSize`, `Gkey_RoomSpaceDecSize`, `Gkey_SellTrapOnSubtile`)
  to decide room-space drag/select behavior.
- `gui_room_type_highlighted` (1 write, [roomspace.c:877](../../libs/kfx_sim/src/roomspace.c)) —
  sets which room type the GUI should highlight after a room-space
  selection completes.

**`libs/kfx_sim/src/map_events.c`** includes `gui_frontmenu.h` and
`frontmenu_ingame_evnt.h`:
- `my_visible_event_idx` (3 sites) — read/cleared/set as part of map-event
  bookkeeping, but the variable itself is frontend-owned (which event
  banner is currently displayed).
- `my_event_button_state[]`/`EvBtnS_Read` (4 sites) — not mentioned in the
  original report, surfaced only once the build was attempted after
  removing the two includes: `frontmenu_ingame_evnt.h` also declares this
  frontend-owned per-event bitflag array, written from `event_initialise_all`,
  `event_initialise_event`, `activate_event_box`, and `clear_events`.

## Fix: extend `sim_feedback.h`, don't add a new struct

`sim_feedback.h` already exists for exactly this shape of problem —
`kfx_sim` needing to call upward into `kfx_frontend`/`kfx_game` without
including their headers, via a `struct SimFeedbackCallbacks` populated once
in `main.cpp` (mirrors [game_callbacks.h](../../libs/kfx_config/include/game_callbacks.h)'s
`kfx_game` → `kfx_frontend` pattern from stage 9). Add three fields:

```c
/* front_input.h -- one query per Gkey_* the code actually checks, each
   equivalent to is_game_key_pressed(Gkey_*, clear_pressed=false,
   ignore_mods=true) (confirmed identical across all 18 call sites) */
TbBool (*is_best_roomspace_key_pressed)(void);
TbBool (*is_square_roomspace_key_pressed)(void);
TbBool (*is_roomspace_incsize_key_pressed)(void);
TbBool (*is_roomspace_decsize_key_pressed)(void);
TbBool (*is_sell_trap_on_subtile_key_pressed)(void);

/* gui_draw.h */
void   (*set_room_type_highlighted)(char room_kind);

/* gui_frontmenu.h / frontmenu_ingame_evnt.h */
void   (*set_visible_event_idx)(EventIndex evidx);
void   (*clear_all_event_button_states)(void);
void   (*clear_event_button_state)(EventIndex evidx);
void   (*mark_event_button_read)(EventIndex evidx);
```

`enum GameKeys` (the `Gkey_*` constants) is declared in `front_input.h`
itself — a large (70+ entry), genuinely frontend-owned keybinding config
enum, not something to relocate down just for these 5 values. Rather than
pass a raw key ID as `int` through the callback (which would need the enum
visible to `kfx_sim`, or magic numbers), each of the 5 distinct queries
`roomspace.c` actually makes gets its own named zero-argument boolean
callback — consistent with how `sim_feedback.h` already wraps specific
semantic queries (`report_error_stat`, `show_onscreen_msg`) rather than a
generic pass-through.

`gui_room_type_highlighted`, `my_visible_event_idx`, and
`my_event_button_state[]` are direct-access globals today, not function
calls — the callbacks wrap each write
(`set_room_type_highlighted`/`set_visible_event_idx`/
`clear_all_event_button_states`/`clear_event_button_state`/
`mark_event_button_read`) rather than exposing the variables themselves,
same reasoning.

## Status: implemented

Landed as a separate commit from stage 10.0. What actually happened,
mechanically:

1. Added the 9 fields above to `struct SimFeedbackCallbacks` in
   [sim_feedback.h](../../libs/kfx_config/include/sim_feedback.h), plus
   matching no-op defaults in
   [sim_feedback.c](../../libs/kfx_config/src/sim_feedback.c).
2. Implemented the 9 real wrappers in `src/main.cpp` next to the existing
   `sim_feedback_impl` static initializer, and wired them into it —
   `main.cpp` already `#include`s `front_input.h`/`gui_draw.h`/
   `gui_frontmenu.h`/`frontmenu_ingame_evnt.h` as the app layer, so no new
   includes were needed there.
3. Replaced all 18 `is_game_key_pressed(Gkey_*, false, true)` call sites in
   `roomspace.c` with the matching `sim_feedback->is_*_key_pressed()` call,
   and the 1 `gui_room_type_highlighted = ...` write with
   `sim_feedback->set_room_type_highlighted(...)`.
4. Replaced the 3 `my_visible_event_idx` writes and 4
   `my_event_button_state[]`/`EvBtnS_Read` writes in `map_events.c` with
   the matching `sim_feedback->` calls, and added `#include
   "sim_feedback.h"` (not previously included in that file).
5. Removed `#include "front_input.h"` / `#include "gui_draw.h"` from
   `roomspace.c`, and `#include "gui_frontmenu.h"` /
   `#include "frontmenu_ingame_evnt.h"` from `map_events.c`.
6. Verified with `scripts/check_layering.py` (all 6 targeted violations
   gone; the untouched, out-of-scope ones — `kjm_input.h`,
   `frontmenu_ingame_tabs.h`, `cursor_tag.h`, `game_legacy.h` on
   `roomspace.c`; `frontend.h`, `kfx_frontend_state.h` on `map_events.c` —
   remain, as expected) and a full `linux.mk` build
   (`PKG_CONFIG_PATH`/`LIBRARY_PATH` pointed at `third_party/install`),
   which compiled and linked `bin/keeperfx` cleanly.

The `my_event_button_state[]`/`EvBtnS_Read` violation (4 sites) wasn't in
the original report — it only surfaced once the build was attempted after
removing `frontmenu_ingame_evnt.h`, since that header declares both
`my_visible_event_idx` and this array. Fixed the same way, same header,
same commit.

Not touched, left for future findings: `roomspace.c`'s `kjm_input.h`,
`frontmenu_ingame_tabs.h` (dead include — no real symbol from it is used
in the file), `cursor_tag.h`, `game_legacy.h`; `map_events.c`'s
`frontend.h`, `kfx_frontend_state.h`; and everything in
`roomspace_prediction.c` (`frontmenu_ingame_evnt.h`, `game_legacy.h`,
`packets.h`, `net_exchange_gameplay.h`, `cursor_tag.h`,
`engine_render.h`) — a larger, separate cross-cluster (sim/net/render)
tangle than this appendix scoped to.
