/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file kfx_config_state.c
 *     Global instance for kfx_config_state.h.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "kfx_config_state.h"
#include "post_inc.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
// gui_blink_rate/neutral_flash_rate default to 1 here (matching
// keeperfx_ui_config's own defaults in config_keeperfx.c) rather than
// relying solely on main.cpp's one-time
// "kfx_config_state.gui_blink_rate = keeperfx_ui_config.gui_blink_rate"
// assignment: both are divisors in several kfx_frontend/kfx_render call
// sites (e.g. frontmenu_ingame_map.c's setup_panel_colors), so a 0 here
// before that assignment runs is a division-by-zero crash, not just a
// visual glitch.
struct KfxConfigState kfx_config_state = {
    .gui_blink_rate = 1,
    .neutral_flash_rate = 1,
};
/******************************************************************************/
#ifdef __cplusplus
}
#endif
