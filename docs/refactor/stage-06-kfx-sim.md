# Stage 6 — Extract `kfx_sim`

See [00-overview.md](00-overview.md) for the full plan context. See also
the appendix [stage-06a-ariadne-pathfinding-interface.md](stage-06a-ariadne-pathfinding-interface.md)
for the pathfinding-specific sub-decision,
[stage-06b-sim-frontend-leaks.md](stage-06b-sim-frontend-leaks.md) for
`roomspace.c`/`map_events.c` upward includes into `kfx_frontend` found
later (during stage 10) but scoped to this stage's `sim_feedback.h`
mechanism, and
[stage-06c-roomspace-prediction-tangle.md](stage-06c-roomspace-prediction-tangle.md)
for `roomspace_prediction.c`'s three-cluster tangle (`kfx_frontend`,
`kfx_render`, `kfx_net`) — partly the same mechanism, partly a real
open decision about where `struct Packet` should live.

## Goal

The largest and riskiest stage: `creature_*`, `thing_*`, `room_*`, `map_*`,
`player_*`, `roomspace*`, `spdigger_*`, `dungeon_data.*`, `dungeon_stats.*`,
`power_*`, `slab_data.*`, `magic_powers.*`, `actionpt.*`, `tasks_list.*`,
and (bundled — see appendix) `ariadne*` — roughly 210 files / 140k LOC, the
actual game simulation. Don't try to sub-split this further in this pass;
the goal is pulling it out as *one* library with a clean lower boundary
(`kfx_config`, `kfx_platform`) and a clean upper boundary (nothing above
included from inside it).

## Prerequisite from stage 4

`config_magic.c`'s `add_power_to_player()`/`remove_power_from_player()`
(runtime power grant/revoke, not parsing) move here, alongside
`magic_powers.c` which already owns the runtime implementation of every
other power's effect.

## Prerequisite from stage 5

Migrate the `kfx_sim`-owned field group from `struct Game`
(`players[]`, `columns_data`, `slabset*`, `cctrl_data[]`, `things_data[]`,
`navigation_map[]`, `map[]`, `computer_task[]`/`computer[]`, `slabmap[]`,
`rooms[]`, `dungeon[]`, `thing_lists[]`, `gold_lookup[]`, `battles[]`,
`creature_scores[]`, `pool`, `chosen_room/spell/manufactr_*`,
`hand_over_subtile_*`, `around_*`, `block_health[]`, `entrance_room_id`,
`entrances_count`, `map_subtiles_x/y`, `map_tiles_x/y`, the random seeds)
per the strategy agreed in [stage-05-god-headers.md](stage-05-god-headers.md).
Without this, `kfx_sim` cannot have a clean lower boundary — it would need
to `#include game_legacy.h` for its own state, which drags in every other
library's fields too.

## Sequencing — sub-PRs by existing prefix cluster

Land one cluster at a time, each once its includes are clean. Internal
cross-includes *within* this set are fine and expected (creature ↔ thing ↔
room ↔ map is why they're one library, not five) — only includes reaching
*outside* the set are violations.

1. `map_*` + `slab_data.*` (lowest-level world data, 7,463 + ~1k LOC)
2. `thing_*` (27,365 LOC)
3. `room_*` + `roomspace*` (9,773 + 2,899 LOC)
4. `creature_*` + `spdigger_*` (27,909 + 3,665 LOC)
5. `player_*` + `dungeon_data.*` + `dungeon_stats.*` (14,345 LOC + ~2k)
6. the rest: `power_*`, `magic_powers.*`, `actionpt.*`, `tasks_list.*`
7. `ariadne*` — see appendix; may land bundled with cluster 4 (its heaviest
   consumer) or separately once the interface work is scoped.

## Exit criterion

`libs/kfx_sim` exists; nothing in it includes headers from `engine_*`,
`front_*`/`frontmenu_*`/`gui_*`, `net_*`/`packets*`, `lua_*`, `game_*`,
`lvl_script*`. `struct Game`'s sim field group is owned by `kfx_sim`, not
`game_legacy.h`.
