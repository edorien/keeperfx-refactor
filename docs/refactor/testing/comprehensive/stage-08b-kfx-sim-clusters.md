# Stage 8b — `kfx_sim`'s five clusters: first pass across all of them

See [stage-08 §3](stage-08-comprehensive-library-passes.md#3-kfx_sims-per-cluster-breakdown)
for the cluster breakdown this document executes against, and
[stage-04c](../stage-04c-kfx-sim.md) for the original single-function
pilot this extends. That document's own table proposed five follow-up
documents, "each becoming its own... once actually started, not written
speculatively here." They've now all been started together, in one pass,
each getting a small set of foundational accessor tests rather than a
deep individual effort — worth one consolidated document reflecting that
shape, not five thin ones pretending to be five separate deep dives.

## What landed, per cluster

### map (`map_utils_test.cpp`, +1 test — 2 pre-existing from stage-04c)

`small_around_index_towards_destination` — the sibling function to
`small_around_index_in_direction` (stage-04c's original pilot) that
handles the exact-45°-multiple special case. Confirmed empirically before
asserting (a throwaway probe, per this whole plan's standing "verify,
don't guess" discipline): all four cardinal directions hit that special
case (each is a multiple of `DEGREES_45`), and the hand-plausible cardinal
values happened to match on the first try.

### thing (`thing_data_test.cpp`, 5 tests, new file)

`thing_is_invalid`/`thing_exists`/`thing_get` — the highest
fan-in-per-effort target stage-08 flagged, previously only exercised
*indirectly* through `kfx_net`'s `get_thing_checksum` tests
([stage-04e](../stage-04e-kfx-net.md)). Direct tests now cover the
reserved index-0 sentinel, in-range/out-of-range `thing_get`, and
`thing_exists`'s `TAlF_Exists` flag gate.

### creature (`creature_control_test.cpp`, 6 tests; `creature_states_test.cpp`, 9 tests, both new files)

`creature_control_get`/`creature_control_invalid`/
`creature_control_exists`/`creature_control_get_from_thing` — the
foundational accessors. Found and tested a genuine asymmetry by reading
the body rather than assuming symmetry with `thing_is_invalid`:
`creature_control_invalid()` only checks the *lower* bound
(`cctrl <= &kfx_sim_state.cctrl_data[0]`) — no upper-bound check against
`CREATURES_COUNT` at all, unlike `thing_is_invalid`. Tested as the
function's actual, current behavior (a dedicated `TEST_CASE` asserting a
one-past-the-end pointer is still reported "valid"), not silently
corrected — the same "test what's there" call
[stage-04-kfx-config.md](../stage-04-kfx-config.md) made for
`parameter_is_number`'s lone-`"-"` quirk.

**Depth increment, landed separately once this foundational pass was
in place**: `creature_states.c::can_change_from_state_to()` — the actual
state-transition *permission* logic, not just the accessor layer
beneath it. This is the function stage-08 §3's fan-in/risk argument was
really about (a silent regression here changes which state transitions
are legal, codebase-wide). Driven entirely by
`kfx_config_state.conf.crtr_conf.states[]` (pattern A) plus a handful of
`struct Thing` fields — no live simulation tick, no `CreatureControl`
needed, since the function never dereferences one (confirmed by reading
the body: `get_creature_state_besides_interruptions`, a neighboring
function, is the one that reaches for `creature_control_get_from_thing`,
not this one). 9 tests total: the `state_info_invalid`/
`get_thing_state_info_num` accessor pair (the same index-sentinel family,
here over `kfx_config_state.conf.crtr_conf.states[]` instead of a
`kfx_sim_state` array), plus 6 `can_change_from_state_to` cases covering
each independent gate the function checks in sequence — the
`TAlF_IsControlled` restriction to idle-type states, the
`transition`/`override_transition` pair, the `captive`/`override_captive`
pair, the `state_type`-keyed switch dispatching to the matching
`override_*` flag (using `CrStTyp_Sleep` as the representative case), and
confirming an unmatched `state_type` (the zero-valued `CrStTyp_Idle`
default, which has no explicit `case` in the switch) falls through to
`default: return true;`.

**Second depth increment**: the first *individual*
`creature_states_*.c` file — `creature_states_tresr.c` (`creature_states_tresr_test.cpp`,
3 tests), picked for being the smallest (57 lines), the same "cheapest
candidate first" discipline every stage in this plan has used. Its one
function, `creature_able_to_get_salary()`, turned out to need pattern B,
not just A: it calls `creature_stats_get_from_thing()`, which resolves
the creature's *effective* model through
`config_reload_callbacks->get_thing_model(thing)` — a real
`ConfigReloadCallbacks` indirection (architecture.md §5.1), not a direct
`thing->model` read. The default no-op implementation always returns `0`,
which would make every test resolve to the same reserved model-0
sentinel regardless of what's actually configured — a fake provider
(registered via `set_config_reload_callbacks()`) is what makes the
function's real pay-based-eligibility behavior observable at all, the
same "the default stub makes the interesting branches unreachable"
problem `kfx_apploop`'s `GetGameTurnFunc` fake solved
([stage-07](../stage-07-kfx-apploop-game-process.md)). First
`ConfigReloadCallbacks` pattern-B test in the whole plan (`GetGameTurnFunc`
and `EmulateIntegerOverflowFunc`, both in `kfx_platform`, were the only
callback structs exercised from a test before this).

### room (`room_data_test.cpp`, 6 tests, new file)

`room_get`/`room_is_invalid`/`room_exists` (the same accessor family
again) plus `compute_room_max_health()` — a genuinely different shape:
reads `kfx_config_state.conf.rules[0].workers.hits_per_slab` (pattern A
on `kfx_config_state`, a first for this cluster) and calls
`saturate_set_unsigned()` — the exact `EmulateIntegerOverflowFunc`
pattern-B target already tested directly in
`kfx_platform/tests/bflib_basics_test.cpp`. Here it's exercised
*indirectly*, through its real caller, with the default (non-emulating)
provider in effect — both the normal-multiply and 16-bit-saturation
cases asserted.

**Third depth increment**: `roomspace_test.cpp` (originally 1 test) —
`get_dungeon_sell_user_roomspace()`'s `single_subtile_mode` branch, the
simplest of its four `roomspace_mode` branches. This is where `kfx_sim`'s
`get_packet` accepted-residual stub (`kfx_sim/tests/packet_test_stubs.cpp`)
actually gets *called* for the first time — every prior test only linked
against it. The `single_subtile_mode` branch calls `get_packet_direct()`
but doesn't read any of the returned packet's fields, so this exercises
the stub's *call path* (confirming the link/stub wiring survives an
actual invocation, not just a successful link) without yet exercising a
branch that depends on the packet's *contents*. Also surfaced a real, if
minor, header-dependency gap: `roomspace.h` (via `roomspace_detection.h`)
doesn't transitively pull in `globals.h` through every include path — a
plain `#include "globals.h"` before `#include "roomspace.h"` in the test
file resolved it, the same kind of fix production `.c` files never
notice because they always happen to include `globals.h` earlier via
some other header first.

**Tenth depth increment**, closing that exact gap, per the user's
explicit request to focus on code around the accepted layering
residuals: `roomspace_test.cpp` grew three more tests covering
`box_placement_mode` (geometry-only, verified safe against a fully-zeroed
map by reading `subtile_is_sellable_room`/`_door_or_trap`'s bodies —
both short-circuit to `false` via `map_block_invalid()`, no crash risk)
and, at last, `drag_placement_mode` — the one branch that actually reads
`pckt->control_flags`. Directly poking the shared stub packet's
`control_flags` field (via `get_packet_direct(0)`, the same shared
`static struct Packet` `packet_test_stubs.cpp` always returns) proves a
real behavioral difference driven by the packet's *contents*: with no
button held, the drag box collapses to 1×1 at the current slab; with
`PCtr_LBtnHeld` set, it spans from `player->render_roomspace`'s stored
drag-start slab to the current one (asserted via `left`/`top`/`right`/
`bottom`/`width`/`height`, not just a boolean). Also found, by reading
the body before writing the fixture, that `player->render_roomspace.drag_mode`
must be `true` going in — otherwise an earlier, unconditional block in
the same function resets `drag_start_x`/`_y` to the current slab before
the mode branch even runs, silently defeating the whole test setup.
`roomspace_test.cpp` is now 4 tests total, confirmed stable across
several `--order rand` runs.

### player (`player_data_test.cpp`, 7 tests, new file)

`player_invalid`/`player_exists` plus `players_are_enemies`/
`players_are_mutual_allies`'s pure short-circuit branches (self, and the
neutral player, resolved before any real player-state lookup). The
biggest surprise of this whole pass, again found by reading the body
rather than assuming family resemblance: `player_data.c`'s accessors are
a genuinely different shape from the other four clusters' "index 0
reserved, sentinel is `&array[0]`" convention. Here, **index 0 is a real,
valid player** (`get_player_f()` returns `&kfx_sim_state.players[0]`
directly for a valid index, no reservation), and the invalid sentinel
(`INVALID_PLAYER`) is a **wholly separate global**, `&bad_player`, not
`&kfx_sim_state.players[0]`. Getting this wrong would have meant testing
a convention that doesn't hold for this cluster — caught by reading
`player_invalid()`'s actual body before writing assertions, not
generalizing from the other four clusters' pattern.

**Fourth depth increment**, `player` cluster: `player_utils_test.cpp`
(8 tests) — `player_has_lost` (a two-line `victory_state` read) and
`player_cannot_win`, covering every one of its four early-return gates
(neutral player, nonexistent player, already-lost, no dungeon heart) plus
its one "actually still winnable" branch (heart exists and isn't
`ObSt_BeingDestroyed`). `player_cannot_win` reaches through
`get_player_soul_container()` into `kfx_sim_state.dungeon[]` and a real
`Thing` slot — pattern A throughout, no `sim_feedback` fake needed, since
neither function is the pay/scoring family (`compute_player_final_score`,
`take_money_from_dungeon_f`) that does reach through `sim_feedback`.

**Fifth depth increment**, `thing` cluster: `thing_stats_test.cpp`
(14 tests) — `get_radially_decaying_value`/`get_radially_growing_value`
(pure integer arithmetic, no state at all: linear decay through a
region, the "never overshoot the epicenter" pull-back branch, and the
"push distance doesn't overshoot" fallthrough all traced by hand and
confirmed against the actual run rather than assumed),
`compute_controlled_speed_increase`/`_decrease` (the `speed_limit < 4`
step-by-1 special case plus the `[-speed_limit, speed_limit]` clamp,
both directions), `compute_creature_max_health` (pattern A on
`kfx_config_state`, including the `exp_level >= CREATURE_MAX_LEVEL`
clamp), and `is_neutral_thing`/`is_hero_thing` (pattern A on
`kfx_sim_state`, the latter reaching through `player_data.c`'s
`player_is_roaming`). `creature_states_barck.c` (76 lines, the next-
smallest `creature_states_*.c` file after `_tresr`) was considered as
this increment's creature-cluster target instead, but its two functions
turned out to need a fuller `Room`+`CreatureControl`+job context right
away — confirming stage-08b's own prediction that `_tresr` was the
cheapest possible foothold, not representative of the rest — so
`thing_stats.c` was a better return on this increment.

**Sixth depth increment**, outside the original five clusters:
`dungeon_stats_test.cpp` (16 tests) — `dungeon_stats.c` doesn't match
any of §3's five file-prefix groupings (it's dungeon-level score
computation, closest in spirit to `player_utils.c`'s still-open scoring
functions but its own file), so it's recorded here rather than force-fit
into "room" or "player". `dungeon_stats.c`'s `compute_dungeon_*_score`
family, the
clamp-then-arithmetic point functions behind the end-of-game score
screen (`_rooms_attraction_score`, `_creature_tactics_score`,
`_rooms_variety_score`, `_train_research_manufctr_wealth_score`,
`_creature_amount_score`, `_creature_mood_score`). All six had no header
declaration anywhere (only ever called from within `dungeon_stats.c`
itself) — added to `dungeon_stats.h`, the same "add the missing
declaration" fix used for `lvl_script_conditions.h`/`value_util.h`
earlier in this plan. Every clamp boundary got its own assertion rather
than just a "normal case" test — e.g.
`compute_dungeon_creature_mood_score`'s division-by-zero guard
(`survived_creatrs <= 0` short-circuits to 0 before the `(annoyed<<8)/
survived` divide), and its "annoyed can never exceed survived" rule
(raises `survived` to match rather than clamping `annoyed` down),
verified by hand-tracing the arithmetic before asserting, the same
discipline every numeric test in this plan has used.

**Seventh depth increment**, also outside the original five clusters:
`magic_powers_test.cpp` (5 tests) — `magic_powers.c`'s power-price
functions, `compute_power_price_scaled_with_amount`/`compute_power_price`
(`Cost_Default` branch only)/`compute_lowest_power_price`. Pattern A on
`kfx_config_state.conf.magic_conf.power_cfgstats[]`. `compute_power_price`'s
`Cost_Digger`/`Cost_Dwarf` branches additionally reach into a real
dungeon's creature counts and a `ConfigReloadCallbacks` provider
(`get_players_special_digger_model`) — a bigger increment, left open.
`compute_power_price_scaled_with_amount` had no header declaration
anywhere (only ever called from within `magic_powers.c` itself) — added
to `magic_powers.h`, the same "add the missing declaration" fix used
repeatedly across this plan (`lvl_script_conditions.h`,
`dungeon_stats.h`, `value_util.h`).

**Eighth depth increment**, `room` cluster: `room_capacity_test.cpp`
(7 tests) — `room_data.c`'s `count_slabs_*_wth_effcncy` family
(`_all_only`, `_all_wth_effcncy`, `_no_min_wth_effcncy`,
`_div2_wth_effcncy`, `_div2_nomin_effcncy`, `_mul2_wth_effcncy`,
`_pow2_wth_effcncy`), the `terrain_room_total_capacity_func_list`
dispatch table's entries. Each is a pure function of `room->slabs_count`/
`efficiency` → `room->total_capacity`, no other state needed — a
self-contained sibling group in the `compute_room_max_health` vein, just
varying the rounding/clamp rule. Confirmed a real pairwise asymmetry
directly (not assumed from the near-identical bodies): each
`_wth_effcncy` sibling has a matching `_nomin`/`_no_min` variant that
computes the exact same scaled value but skips the "clamp up to 1"
floor, allowing a true zero result instead — tested for both members of
each pair, not just one. None of the seven had a header declaration
outside `room_data.c`'s own same-file forward declarations — added to
`room_data.h`, the same "add the missing declaration" fix used
repeatedly across this plan.

**Ninth depth increment**, outside the original five clusters, and
targeted specifically at this library's one currently-*unaccepted*
`check_layering.py --strict` violation: `power_specials_test.cpp`
(6 tests) — see
[`docs/refactor/todo/two-remaining-layering-violations.md`](../../todo/two-remaining-layering-violations.md)
for the full root-cause writeup (`power_specials.c`'s `#include "api.h"`
is only for `script_hooks`, which is really declared in `kfx_config`'s
own `script_hooks.h`, already a legitimate dependency). The one
`script_hooks`-calling function, `activate_dungeon_special()`, is too
large/state-coupled to attempt directly (same call as
`creature_states_barck.c` above); instead `box_thing_to_special()`
(pattern A) and `activate_bonus_level()` — **`kfx_sim`'s first
`SimFeedbackCallbacks` pattern-B test in this whole plan**, despite
`sim_feedback` being called through pervasively across this library's
source.

**Eleventh depth increment**, `creature` cluster, and the last of the
symbol-level accepted residuals this pass set out to cover:
`creature_graphics_test.cpp` (4 tests) — `creature_table_add[]`
(`architecture.md` §8.2, `kfx_sim -> kfx_render`), tested via the
`keepersprite_frames`/`_rotable`/`_array`/`_index` dispatch functions
that read it. `kfx_sim_utest` already links `kfx_sim_test_stubs.cpp`'s
`creature_table_add[1]` — real, writable, just sized 1 instead of
production's 16383 — so every test here deliberately touches only index
0 (`n == SIM_KEEPERSPRITE_ADD_OFFSET`, 16384), never a second index, to
stay inside the stub's actual bounds. Also covers the genuinely-out-of-
range fallback (`ERRORLOG` + zero/`NULL`) and the `creature_list[]`-range
branch's own "not loaded yet" case (`creature_table_length` defaults to
0, so the real production creature-table pointer is never dereferenced).
`creature_table_length` had no header declaration anywhere — added to
`creature_graphics.h`, the same "add the missing declaration" fix used
repeatedly across this plan.

With this addition, 2 of `check_layering_symbols.py`'s 9 accepted
symbol *names* (`get_packet_direct`, `creature_table_add`) now have real
test coverage, exercised across 2 of their combined 9 call sites
(`roomspace.c` and `engine_redraw.c` for `get_packet_direct`;
`creature_graphics.c` for `creature_table_add`) — checked precisely, not
assumed, after an earlier draft of this note overclaimed "all 15
covered": `get_packet` (the plain, non-`_direct` function — a different
symbol, called from `roomspace_prediction.c`), `set_packet_action`,
`set_players_packet_action`, and `net_resync.cpp`'s `game`/
`kfx_game_state`/`kfx_frontend_state` blob symbols are all still
untouched — `net_resync_test.cpp` covers functions *in the same file* as
the residual (`store_localised_game_structure`/
`animate_resync_progress_bar`), not the raw-blob resync functions that
actually reference those three symbols. See
[`docs/Architecture/testing-harness.md`](../../../Architecture/testing-harness.md)
§9 for the running list of what's covered where.

## Running total

`kfx_sim` currently has **102 tests** (verified via `ctest -N`, not
recomputed by hand), part of **302 across all ten libraries** — the five
clusters' foundational pass, the `can_change_from_state_to` and
`creature_states_tresr.c` depth increments, `roomspace_test.cpp` (now
4 tests, up from 1), `player_utils_test.cpp`, `thing_stats_test.cpp`,
`dungeon_stats_test.cpp`, `magic_powers_test.cpp`, `room_capacity_test.cpp`,
`power_specials_test.cpp`, and `creature_graphics_test.cpp`, combined.
`check_layering.py --strict` and `check_layering_symbols.py --strict`
both reconfirmed unaffected after every addition in this cluster, not
just checked once at the end.

## What's still open in each cluster

- **map**: the spiral-search/filter functions (`get_position_spiral_near_map_block_with_filter`
  and friends) — more involved, callback-parameter-driven.
- **thing**: `thing_stats.c`'s config-dependent `calculate_correct_*`
  family (reach through `creature_control`/`config_creature` for the
  creature's current stat-modifying state, a bigger increment than the
  `compute_*`/`get_radially_*` pure-computation functions covered here)
  and anything in `thing_creature.c`/`thing_factory.c`/`thing_physics.c`
  beyond the base accessors.
- **creature**: `can_change_from_state_to()` covers *permission* to
  transition, and `creature_states_tresr.c` (the smallest per-state file)
  has its one helper function tested — but 16 other `creature_states_*.c`
  files remain completely untouched, including every actual
  `process_state`/`cleanup_state` implementation and the `FuncIdx`
  dispatch table that calls into them. `creature_states_tresr.c`'s single
  function (`creature_able_to_get_salary`, a config lookup, not a state
  transition itself) was the easiest possible foothold in this file
  family, not representative of what the other 16 files will need —
  expect most of them to require a fuller `Thing`+`CreatureControl`+
  room/job context, genuinely closer to pattern B/fixture territory than
  anything landed in this cluster so far.
- **room**: `get_dungeon_sell_user_roomspace`'s remaining
  `roomspace_detection_mode` branch (calls `get_current_room_as_roomspace()`,
  needs a real room fixture, not attempted) — `box_placement_mode` and
  `drag_placement_mode` are now covered, including `drag_placement_mode`'s
  `pckt->control_flags`-driven behavior. `get_dungeon_build_user_roomspace`
  (the sibling function, not yet touched at all), `roomspace_prediction.c`,
  and every `room_<kind>.c` file (`room_garden`, `room_library`, …).
- **player**: `player_utils.c`'s scoring/payday functions
  (`compute_player_final_score`, `compute_and_update_player_payday_total`)
  — these reach through `sim_feedback` (`get_loaded_level_number()`) and
  dungeon gold bookkeeping, a bigger increment than `player_has_lost`/
  `player_cannot_win` turned out to be, deliberately not attempted here —
  and all of `player_computer*` (AI — orchestration-heavy, stage-08's own
  "lower priority" call).

Each of these is a reasonable next increment.
