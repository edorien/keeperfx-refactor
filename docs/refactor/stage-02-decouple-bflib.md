# Stage 2 — Decouple `bflib_*` (the platform layer)

See [00-overview.md](00-overview.md) for the full plan context.

## Goal

`bflib_*` (69 files, 31,281 LOC) is meant to be the lowest layer — SDL/OS
abstraction, math, string, file I/O — but a full per-header, per-symbol
audit found it reaching upward into config, frontend, net, and the
`struct Game` god-object. This stage cuts every one of those edges. It is
the highest-value, must-happen-early stage; risk is moderate (69 files) but
the fixes are largely mechanical once classified, which the audit below
already did.

## Already clean — no work needed

20 of 69 `bflib_*` files have **zero** upward (non-`bflib_`, non-external)
includes today: `bflib_basics.h`, `bflib_coroutine.c/.h`, `bflib_cpu.c`,
`bflib_crash.c`, `bflib_dernc.h`, `bflib_enet.h`, `bflib_fileio.c/.h`,
`bflib_filelst.h`, `bflib_fmvids.h`, `bflib_main.cpp`, `bflib_mshandler.hpp`,
`bflib_netconfig.hpp`, `bflib_netsession.c/.h`, `bflib_netsp.cpp`,
`bflib_text.h`, `bflib_vidraw.h`, `bflib_vidsurface.h`. (Their paired
`.c`/`.h` counterpart is often *not* clean — check per translation unit,
not per module.)

`mutex.hpp` and `thread.hpp` were flagged by the coarse naming-convention
grep only because they aren't prefixed `bflib_` — both are pure MinGW32
`std::mutex`/`std::this_thread` polyfills with zero game-domain content.
No action needed; not real coupling. (Optional: rename to `bflib_mutex.hpp`
/`bflib_thread.hpp` so future greps don't re-flag them.)

## The single biggest win: delete dead includes first

A large fraction of the "coupling" found by the original coarse grep turns
out to be **unused includes** — the header is pulled in, but the file
doesn't reference any symbol from it. These can be deleted today, with
zero interface work, zero risk:

| File | Dead include(s) to delete |
|---|---|
| `bflib_input_joyst.cpp` | `config_keeperfx.h`, `config_settings.h`, `frontend.h`, `front_input.h`, `frontmenu_ingame_tabs.h`, `game_legacy.h`, `kjm_input.h` — **7 of its 8 upward includes are dead**; only `config.h` (for `prepare_file_path()`) is real. |
| `bflib_inputctrl.cpp` | `config_settings.h`, `front_input.h` |
| `bflib_sound.c` | `config_settings.h`... *(real — see table below)*; `game_legacy.h`, `gui_soundmsgs.h` are dead |
| `bflib_enet.cpp` | `game_legacy.h` (only textual hits are in comments) |
| `bflib_datetm.h` | `game_legacy.h`, `keeperfx.hpp` — **the header itself** declares nothing that uses either; since `bflib_datetm.h` is transitively included by 37 other files, this single deletion de-pollutes all 37 from seeing the entire `struct Game` god-object and the whole game-loop API for zero benefit. |
| `bflib_mshandler.cpp`, `bflib_mspointer.cpp` | `keeperfx.hpp` (zero symbols used in either) |

Do this deletion pass **first**, as its own small PR, before any of the
real-coupling fixes below — it shrinks the problem for free and makes the
remaining real edges easier to see in review.

## Real coupling, by header, with fix suggestions

Classification key: **(b)** = calls into an upper-layer function, needs
callback/function-pointer inversion. **(c)** = reads/writes upper-layer
global state directly, needs restructuring (not just indirection).

| Header | File(s) | What's used | Class | Fix |
|---|---|---|---|---|
| ~~`cdrom.h`~~ | ~~`bflib_sndlib.cpp`~~ | **Resolved by reclassification, not a code fix.** `cdrom.cpp`/`linux.cpp` (the implementers) were reclassified into `kfx_platform` itself during the stage 4/6 investigation (they're OS-integration code, same bucket as `windows.cpp`/`steam_api.cpp` — see [00-overview.md §4](00-overview.md#4-target-architecture)). `bflib_sndlib.cpp → cdrom.h` is therefore a same-library edge; `scripts/check_layering.py` confirms it isn't flagged. No callback injection needed. | — | — |
| `config.h` | `bflib_input_joyst.cpp` | `prepare_file_path()` (impl. in `config.c`), to locate `gamecontrollerdb.txt` | (b) | Pass the resolved path into `init_controller_input()` as a parameter. |
| `config_keeperfx.h` | `bflib_sprfnt.c` | `install_info.lang_id` (global), `get_language_lwrstr()` | (c) | Move `install_info`/language state behind an accessor or inject a small "runtime language" struct at init. |
| `config_keeperfx.h` | `bflib_sound.c` | `AtmosStart`/`AtmosEnd`/`AtmosRepeat` externs (direct read) | (c) | Same — inject rather than read the global. |
| `config_keeperfx.h` | `bflib_inputctrl.cpp` | `freeze_game_on_focus_lost()`, `mute_audio_on_focus_lost()`, `unlock_cursor_when_game_paused()`, `lock_cursor_in_possession()` | (b) | Pass these four predicates in as a callback/query object at input-system init. |
| `config_keeperfx.h` | `bflib_sndlib.cpp` | `install_info.lang_id`, `features_enabled` (vs `Ft_NoCdMusic`), `get_language_lwrstr()` | (c) | Same pattern as `bflib_sprfnt.c` above. |
| `config_settings.h` | `bflib_sound.c` | `settings.sound_volume`, `settings.mentor_volume` (direct read) | (c) | Pass volumes as explicit parameters into the sound-playing functions. |
| `config_sounds.h` | `bflib_guibtns.c` | `snd_button_click`, `snd_tab_click` (cached IDs) | (c), mild | Pass the two sound IDs in as parameters, or a small "UI sound ids" struct populated once by config. |
| `custom_sprites.h` | `bflib_vidraw.c` | `get_panel_sprite()` (impl. in `custom_sprites.c`) | (b) | Panel-drawing routines accept a `const struct TbSprite *` (or lookup callback) from the caller. |
| `front_credits.h` | `bflib_sprfnt.c` | `frontstory_font` global, compared against `lbFontPtr` | (c) | Replace the `lbFontPtr == <specific global>` special-casing with an enum/callback the front-end registers. |
| `frontend.h` | `bflib_sprfnt.c` | `frontend_font[0..3]`, `winfont`, `font_sprites` globals | (c) | Same fix as `front_credits.h` above — one combined "font role" callback covers both. |
| `front_network.h` | `bflib_enet.cpp` | `display_attempting_to_join_message()`, `attempting_to_join_cancel_requested()` (impl. in `front_network.c`) | (b) | Inject a "join UI" callback interface (progress + cancel-requested) from `front_network.c`. |
| `game_legacy.h` | `bflib_inputctrl.cpp` | `game.operation_flags`, `game.view_mode_flags`, `game.packet_load_enable`, `get_my_player()` | (c) | Needs a "frame/pause state" snapshot struct passed in each frame instead of a raw reference into `game`. |
| `game_legacy.h` | `bflib_datetm.cpp` | `game.process_turn_time`, `game.delta_time`, `turns_per_second` | (c) | Same — pass timing state explicitly rather than reading `game` globally. |
| `kjm_input.h` | `bflib_fmvids.cpp` | `poll_inputs()`, `clear_key_pressed()` (impl. in `kjm_input.c`) | (b) | Pass an input-poll callback pair into the movie-playback loop. |
| `net_holepunch.h` | `bflib_enet.cpp` | `holepunch_stun_query()`, `holepunch_punch_to()` | (b) | Bundle with the two below into one injected "connectivity services" interface. |
| `net_main.h` | `bflib_enet.cpp` | mostly pure types/constants (fine as-is); but `OnNewUser` is referenced by name directly (assigned to `new_user_callback`) | (b), narrow | The existing `drop_callback` parameter on `bf_enet_init` already does this correctly for `OnDroppedUser` — add the same parameter for `new_user_callback` so `OnNewUser` stops being referenced by name. |
| `net_matchmaking.h` | `bflib_enet.cpp` | `matchmaking_punch()`, `matchmaking_poll_punch()` | (b) | Same "connectivity services" interface as `net_holepunch.h`. |
| `net_portforward.h` | `bflib_enet.cpp` | `port_forward_add_mapping()`, `port_forward_remove_mapping()` | (b) | Same "connectivity services" interface. |
| `player_data.h` | `bflib_enet.cpp` | `my_player_number` (direct read) | (c) | Pass the local player's ID into `IsLocalPeer()` as a parameter (or via the connectivity-services struct). |
| `vidmode.h` | `bflib_render_trig.c` | `pixmap` (`struct TbColorTables`), 24+ call sites across every blit/shade routine | (c), largest single item | `pixmap` is really a rendering *resource*, not a "video mode" concept — belongs owned by bflib itself, or threaded through the blit functions as an explicit `const struct TbColorTables *` parameter. This is the most invasive single fix in this stage. |

`sounds.h` (used by `bflib_sndlib.h` only for the `struct SoundSettings`
parameter type) and `globals.h`'s two escape hatches
(`get_gameturn()`/`detailed_multiplayer_logging`, reached only via the
`*LOG` debug macros in 9 files) are low-priority — see
[00-overview.md §3](00-overview.md#3-current-state--headline-evidence) for
why `globals.h` itself needs no work.

## Net ⇄ bflib bidirectional edge (informational, fixed in stage 8)

`bflib_enet.cpp` depends on `net_portforward.h`/`net_holepunch.h`/
`net_matchmaking.h`, and — confirmed by checking the reverse direction —
`net_portforward.cpp`/`net_holepunch.c`/`net_matchmaking.c` themselves
`#include bflib_basics.h`/`bflib_netsession.h` back. This is not a literal
`#include` cycle (neither `bflib_basics.h` nor `bflib_netsession.h`
references anything net-related), but it is a real *architectural* cycle.
The "connectivity services" callback interface proposed above for the
three `net_*.h` includes resolves bflib's side of it; the net side is
addressed in [stage-08-kfx-net.md](stage-08-kfx-net.md).

## Exit criterion

The `bflib_*` OBJECT target (from stage 1) compiles using only headers
from itself, external deps (SDL2/enet/zlib), and `globals.h`/`pre_inc.h`/
`post_inc.h`. The stage 0 dependency-graph script enforces this going
forward (advisory at this point, mandatory from stage 13).
