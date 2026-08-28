/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file kfx_frontend_state.h
 *     Header file for kfx_frontend_state.c.
 * @par Purpose:
 *     Holds struct Game's kfx_frontend-owned field group and the
 *     frontend-owned globals migrated out of keeperfx.hpp, per
 *     docs/refactor/stage-10-kfx-frontend.md (mirrors stage 6.7/7.2/
 *     8.3/9.3's kfx_sim_state.h/kfx_net_state.h/kfx_game_state.h
 *     approach). `struct Game` is raw-serialized wholesale (see
 *     src/net_resync.cpp, src/game_saves.c, src/main_game.c's
 *     clear_complete_game()); this struct is synced the same way,
 *     alongside kfx_sim_state/kfx_net_state/kfx_game_state, at all
 *     three call sites.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 */
/******************************************************************************/

#ifndef DK_KFX_FRONTEND_STATE_H
#define DK_KFX_FRONTEND_STATE_H

#include "bflib_basics.h"
#include "globals.h"
#include "player_data.h"
#include "dungeon_data.h"
#include "game_merge.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

struct Thing;
struct Map;
struct GuiBox;

// struct TimerTime moved to kfx_sim_state.h (stage 13.2, docs/refactor/
// stage-13-enforce-and-document.md).

// GUI_MESSAGES_COUNT/GUI_MESSAGES_DELAY, struct GuiMessage, struct
// TextScrollWindow, and the messages/quick_messages/evntbox_*/
// box_tooltip fields below moved to kfx_sim_state.h (stage 13.2,
// docs/refactor/stage-13-enforce-and-document.md) -- kfx_sim is the
// lowest-ranked of all real consumers.

struct KfxFrontendState {
    // active_panel_mnu_idx moved to kfx_sim_state.h; comp_player_*
    // moved to kfx_net_state.h; creatures_tend_imprison/
    // creatures_tend_flee moved to kfx_sim_state.h; gui_cheat_box_2
    // moved to kfx_game_state.h (stage 13.2, docs/refactor/
    // stage-13-enforce-and-document.md) -- each moved to the
    // lowest-ranked of its real consumers.
    int32_t flash_button_index; /**< GUI Button Designation ID of a button which is supposed to flash, as part of tutorial. */
    float flash_button_time;

    // Moved here from keeperfx.hpp (stage 10).
    struct GuiBox *gui_cheat_box_1;
    struct GuiBox *gui_cheat_box_3;
    struct GuiBox *gui_cheat_box_4;

    int32_t last_mouse_x;
    int32_t last_mouse_y;
    // pointer_x/pointer_y/block_pointed_at_*/pointed_at_frac_*/
    // top_pointed_at_*/thing_pointed_at/me_pointed_at moved to
    // kfx_render_state.h; my_mouse_x/my_mouse_y moved to
    // kfx_game_state.h (stage 13.2, docs/refactor/
    // stage-13-enforce-and-document.md) -- real owner/writer of the
    // pointer cluster is engine_render.c, and my_mouse_x/y is read by
    // kfx_game's console_cmd.c (lower-ranked than kfx_frontend).

    int continue_game_option_available;
    int32_t define_key_scroll_offset;
    uint32_t time_last_played_demo;
    short drag_menu_x;
    short drag_menu_y;
    unsigned short tool_tip_time;
    unsigned short help_tip_time;
    char top_of_breed_list;
    /** Amount of different creature kinds the local player has. Used for creatures tab in panel menu. */
    char no_of_breeds_owned;
    char *level_names_data;
    char *end_level_names_data;

    // timerstarttime/Timer/TimerGame/TimerNoReset/TimerFreeze moved to
    // kfx_sim_state.h (stage 13.2, docs/refactor/
    // stage-13-enforce-and-document.md) -- kfx_sim is the lowest-ranked
    // of all real consumers.

    /* Moved from struct Game (stage 13, docs/refactor/
       stage-13-enforce-and-document.md) -- land_map_start/eastegg01_cntr/
       eastegg02_cntr are kfx_frontend-only; save_game_slot/time_delta are
       read by kfx_apploop too, but kfx_frontend is the lower-ranked of
       the two.
       land_map_start was a single byte in the original monolithic struct
       Game, immediately followed there by megabytes of other fields
       (lish, cctrl_data[], things_data[], navigation_map[256*256],
       map[256*256], ...) -- front_landview.c's load_map_and_window()
       relies on &land_map_start as the start of a real
       LANDVIEW_MAP_WIDTH*LANDVIEW_MAP_HEIGHT-ish buffer (bounds-checked
       there against 1228997 bytes), deliberately borrowing that
       trailing space as scratch rather than allocating its own. Every
       one of those neighboring fields has since been migrated out to
       other structs across this refactor (lish in Tier 3, the rest in
       stage 6.7), so by the time land_map_start landed here alone in
       KfxFrontendState it had nothing left to borrow -- the campaign
       intro's land view load was actually corrupting/overrunning
       whatever memory follows this struct. Given a real buffer instead
       of continuing to depend on neighboring-field placement. */
    unsigned char land_map_start[1228997];
    unsigned char eastegg01_cntr;
    unsigned char eastegg02_cntr;
    char save_game_slot;
    uint32_t time_delta;
};

extern struct KfxFrontendState kfx_frontend_state;

// Registered on GameCallbacks (src/kfx_config/include/game_callbacks.h);
// see kfx_frontend_state.c.
TbBool save_frontend_state(TbFileHandle fhandle);
TbBool load_frontend_state(TbFileHandle fhandle);
void reset_frontend_state(void);
size_t get_frontend_state_size(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
