/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file main_game.h
 *     Header file for main_game.c.
 * @par Purpose:
 *     Level/session lifecycle: starting, clearing, and resetting a game
 *     session. Created in stage 12 (docs/refactor/stage-12-slim-app-target.md)
 *     as functions get migrated out of src/main.cpp -- grows as later
 *     stage-12 sub-stages add more.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 */
/******************************************************************************/
#ifndef DK_MAIN_GAME_H
#define DK_MAIN_GAME_H

#include "bflib_basics.h"
#include "globals.h"
#include "bflib_coroutine.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
struct PlayerInfo;

extern short default_loc_player;

void clear_game_for_summary(void);
void clear_game(void);
void reinit_level_after_load(void);
void startup_network_game(CoroutineLoop *context, TbBool local);
void faststartup_network_game(CoroutineLoop *context);
void faststartup_saved_packet_game(void);
CoroutineLoopState set_not_has_quit(CoroutineLoop *context);
void set_general_information(int32_t msg_id, PlayerNumber plyr_idx, TbMapLocation target, MapSubtlCoord x, MapSubtlCoord y);
void set_general_information_with_icon(int32_t msg_id, PlayerNumber plyr_idx, TbMapLocation target, MapSubtlCoord x, MapSubtlCoord y, short icon_idx);
void set_quick_information(int32_t msg_id, PlayerNumber plyr_idx, TbMapLocation target, MapSubtlCoord x, MapSubtlCoord y);
void set_quick_information_with_icon(int32_t msg_id, PlayerNumber plyr_idx, TbMapLocation target, MapSubtlCoord x, MapSubtlCoord y, short icon_idx);
void process_objective(const char *msg_text, PlayerNumber plyr_idx, TbMapLocation target, MapSubtlCoord x, MapSubtlCoord y);
void process_objective_with_icon(const char *msg_text, PlayerNumber plyr_idx, TbMapLocation target, MapSubtlCoord x, MapSubtlCoord y, short icon_idx);
void set_general_objective(int32_t msg_id, PlayerNumber plyr_idx, TbMapLocation target, MapSubtlCoord x, MapSubtlCoord y);
void set_general_objective_with_icon(int32_t msg_id, PlayerNumber plyr_idx, TbMapLocation target, MapSubtlCoord x, MapSubtlCoord y, short icon_idx);
short winning_player_quitting(struct PlayerInfo *player, int32_t *plyr_count);
short lose_level(struct PlayerInfo *player);
short resign_level(struct PlayerInfo *player);
short complete_level(struct PlayerInfo *player);

void reset_script_timers_and_flags(void);
void clear_complete_game(void);
void init_seeds();
TbBool startup_saved_packet_game(void);
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
