/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file config_players.h
 *     Header file for config_players.c.
 * @par Purpose:
 *     Players configuration loading functions.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   KeeperFX Team
 * @date     17 Sep 2012 - 06 Mar 2015
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef DK_CFGPLAYERS_H
#define DK_CFGPLAYERS_H

#include "globals.h"
#include "bflib_basics.h"

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

#define PLAYER_STATES_COUNT_MAX    255

struct PlayerStateConfigStats {
    char code_name[COMMAND_WORD_LEN];
    unsigned char pointer_group;
    TbBool stop_own_units;
};

struct PlayerStateConfig {
    struct PlayerStateConfigStats plrst_cfg_stats[PLAYER_STATES_COUNT_MAX];
};

enum PlayerStates {
    PSt_None = 0,
    PSt_CtrlDungeon,
    PSt_BuildRoom,
    PSt_MkDigger,
    PSt_MkGoodCreatr,
    PSt_HoldInHand, // 5
    PSt_CallToArms,
    PSt_CastPowerOnSubtile,
    PSt_SightOfEvil,
    PSt_Slap,
    PSt_CtrlPassngr, // 10
    PSt_CtrlDirect,
    PSt_CreatrQuery,
    PSt_OrderCreatr,
    PSt_MkBadCreatr,
    PSt_CreatrInfo, // 15
    PSt_PlaceTrap,
    PSt_PlaceDoor,
    PST_CastPowerOnTarget,
    PSt_Sell,
    PSt_MkGoldPot, // 20
    PSt_FreeDestroyWalls,
    PSt_FreeCastDisease,
    PSt_FreeTurnChicken,
    PSt_FreeCtrlPassngr,
    PSt_FreeCtrlDirect, // 25
    PSt_StealRoom,
    PSt_DestroyRoom,
    PSt_KillCreatr,
    PSt_ConvertCreatr,
    PSt_StealSlab, // 30
    PSt_LevelCreatureUp,
    PSt_LevelCreatureDown,
    PSt_KillPlayer,
    PSt_HeartHealth,
    PSt_QueryAll, // 35
    PSt_MkHappy,
    PSt_MkAngry,
    PSt_PlaceTerrain,
    PSt_DestroyThing,
    PSt_CreatrInfoAll, // 40
    PSt_CreateDigger,
    PST_CastGenericLevelPower,
    // docs/refactor/editor/02-editing-toolbox.md §2.3 -- appended at the
    // end, not inserted among the numbered original entries above: this
    // enum's values are the same ones saved in savegames
    // (PlayerInfo::work_state) and packet-replay (.pck) files, so a new
    // entry has to get a new number, never renumber an existing one.
    PSt_EditorFill,
    // §2.6 -- placement itself is triggered directly by kfx_editor, not
    // this per-frame work-state switch (F17: object model has no
    // CheatSelection field to read back from), so this case exists mainly
    // to keep a stray click from falling through to whatever PSt_* was
    // active before, and to give the same cursor-highlight feedback other
    // placement tools have.
    PSt_EditorPlaceObject,
    // §2.7 -- unlike Objects, Traps/Doors reuse the *existing* UserState
    // fields the classic workshop-based PSt_PlaceTrap/PSt_PlaceDoor already
    // has (chosen_trap_kind/chosen_door_kind) -- no F17 gap here, since
    // those fields exist independently of CheatSelection. What's genuinely
    // new is the editor's *free* placement (no workshop stock check, no
    // resource cost), so these get their own work states rather than
    // reusing PSt_PlaceTrap/PSt_PlaceDoor themselves, whose dispatch
    // (packets_input.c) is wired to the resource-checked placement path.
    PSt_EditorPlaceTrap,
    PSt_EditorPlaceDoor,
    PSt_ListEnd
};

enum PlayerStatePointerGroup {
    PsPg_None,
    PsPg_CtrlDungeon,
    PsPg_BuildRoom,
    PsPg_Invisible,
    PsPg_Spell,
    PsPg_Query,
    PsPg_PlaceTrap,
    PsPg_PlaceDoor,
    PsPg_Sell,
    PsPg_PlaceTerrain,
    PsPg_MkDigger,
    PsPg_MkCreatr,
    PsPg_OrderCreatr
};

/******************************************************************************/
extern struct NamedCommand player_state_commands[];
// Moved from kfx_game's lvl_script_commands.c -- pure PLAYER*/
// ALL_PLAYERS name<->ID table with no functional coupling to script
// command parsing; kfx_config is its lowest-ranked real consumer
// (config_rules.c). See docs/refactor/stage-13-enforce-and-document.md.
extern const struct NamedCommand player_desc[];
extern const struct ConfigFileData keeper_playerstates_file_data;

/******************************************************************************/
const char *player_state_code_name(int wrkstate);
struct PlayerStateConfigStats *get_player_state_stats(PlayerState plr_state);
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
