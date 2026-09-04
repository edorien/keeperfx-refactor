/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file frontmenu_select.h
 *     Header file for frontmenu_select.c.
 * @par Purpose:
 *     GUI menus for level and campaign select screens.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   KeeperFX Team
 * @date     07 Dec 2012 - 11 Aug 2014
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef DK_FRONTMENU_SELECT_H
#define DK_FRONTMENU_SELECT_H

#include "globals.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

struct GuiMenu;
struct GuiButton;

#pragma pack()
/******************************************************************************/
// FeSt_LEVEL_SELECT/GMnu_FELEVEL_SELECT (this menu) are no longer reachable:
// every entry point now goes to the merged Free play screen
// (FeSt_MAPPACK_SELECT/GMnu_MAPPACK_SELECT, frontend_select_mappack_menu
// below) instead. Left in place rather than removed -- level_select_list
// and its row functions are still used by that merged screen -- but
// frontend_select_level_buttons/frontend_select_level_menu themselves are
// dead code now.
#define frontend_select_level_items_max_visible  7
extern struct GuiMenu frontend_select_level_menu;
// Land selection's list column has more vertical room than the other
// select screens' 480px-tall single-purpose layout (it doesn't need to
// leave room for a fixed OK/Cancel row directly under the list) --
// see frontmenu_select_data.cpp's row count for how this maps to pixels.
#define frontend_select_campaign_items_max_visible  12
extern struct GuiMenu frontend_select_campaign_menu;
// Land selection layout constants shared between frontmenu_select_data.cpp
// (button layout) and frontmenu_select.c (frontend_land_selection_return_to_main_maintain,
// which right-anchors "Return to Main" using the same left margin/menu
// width the rest of the screen is laid out against).
#define FE_LANDSEL_COL_X 24
#define FE_LANDSEL_LIST_W 190
#define FE_LANDSEL_ROW_Y0 92
// PANEL_X starts clear of the list's scroll-arrow/thumb column
// (COL_X+LIST_W, flush against the list's own right edge, 20px wide --
// see frontmenu_select_data.cpp's up/down/scroll-tab buttons), not right
// against it.
#define FE_LANDSEL_PANEL_X 248
// Merged Free play screen (mappack list + level list + land preview):
// mappack list gets fewer visible rows than level list -- map packs are
// typically few, levels within one can be many -- to fit both stacked in
// the same left column height budget as Land selection's single list.
// See frontmenu_select_data.cpp for the pixel layout.
#define frontend_select_mappack_items_max_visible  5
extern struct GuiMenu frontend_select_mappack_menu;
// level_select_list's own row_base (frontmenu_selectlist.h's struct
// FrontendSelectList.row_base), distinct from FE_SELECTLIST_ROW_BASE (45,
// used by mappack_select_list, which is visible on the same merged screen)
// -- row buttons' content.lval doubles as a hover-tracking key
// (frontend_over_button, gui_frontbtns.c), so two simultaneously-visible
// lists sharing a content.lval range hover-highlight together.
#define FE_LEVEL_SELECTLIST_ROW_BASE 60
#define frontend_select_mp_mappack_items_max_visible  7
extern struct GuiMenu frontend_select_mp_mappack_menu;

/******************************************************************************/
// Level list selection screen
void frontend_draw_levels_scroll_tab(struct GuiButton *gbtn);
void frontend_draw_level_select_button(struct GuiButton *gbtn);
void frontend_level_select(struct GuiButton *gbtn);
void frontend_level_select_up(struct GuiButton *gbtn);
void frontend_level_select_down(struct GuiButton *gbtn);
void frontend_level_select_scroll(struct GuiButton *gbtn);
void frontend_level_select_up_maintain(struct GuiButton *gbtn);
void frontend_level_select_down_maintain(struct GuiButton *gbtn);
void frontend_level_select_maintain(struct GuiButton *gbtn);
void frontend_level_list_load(void);
void frontend_level_list_unload(void);
void frontend_level_select_update(void);
void frontend_draw_level_select_mappack(struct GuiButton *gbtn);

// Campaign selection screen
void frontend_campaign_select_up(struct GuiButton *gbtn);
void frontend_campaign_select_down(struct GuiButton *gbtn);
void frontend_campaign_select_scroll(struct GuiButton *gbtn);
void frontend_campaign_select_up_maintain(struct GuiButton *gbtn);
void frontend_campaign_select_down_maintain(struct GuiButton *gbtn);
void frontend_campaign_select_maintain(struct GuiButton *gbtn);
void frontend_draw_campaign_select_button(struct GuiButton *gbtn);
// frontend_campaign_select is the list row click_event -- it *highlights*
// the row's campaign (loads its land preview, resets the detail panel),
// it does not commit/start it. Committing (starting the campaign, or the
// highlighted level within it) is frontend_land_selection_enter, the
// "Enter this land" button's click_event.
void frontend_campaign_select(struct GuiButton *gbtn);
void frontend_campaign_select_update(void);
void frontend_draw_simple_scroll_track(struct GuiButton *gbtn);
void frontend_draw_campaign_scroll_tab(struct GuiButton *gbtn);
void frontend_land_selection_enter(struct GuiButton *gbtn);
void frontend_land_selection_enter_maintain(struct GuiButton *gbtn);
void frontend_land_selection_return_to_main(struct GuiButton *gbtn);
void frontend_land_selection_return_to_main_maintain(struct GuiButton *gbtn);
void frontend_draw_land_selection_panel_bg(struct GuiButton *gbtn);
void frontend_draw_land_selection_detail(struct GuiButton *gbtn);
void frontend_campaign_list_load(void);

// Map pack selection screen
void frontend_mappack_select_up(struct GuiButton *gbtn);
void frontend_mappack_select_down(struct GuiButton *gbtn);
void frontend_mappack_select_scroll(struct GuiButton *gbtn);
void frontend_mappack_select_up_maintain(struct GuiButton *gbtn);
void frontend_mappack_select_down_maintain(struct GuiButton *gbtn);
void frontend_mappack_select_maintain(struct GuiButton *gbtn);
void frontend_draw_mappack_select_button(struct GuiButton *gbtn);
void frontend_mappack_select(struct GuiButton *gbtn);
void frontend_mappack_select_update(void);
void frontend_draw_mappack_scroll_tab(struct GuiButton *gbtn);
void frontend_draw_mp_mappack_scroll_tab(struct GuiButton *gbtn);
void frontend_mappack_list_load(void);
void frontend_draw_variable_mappack_exit_button(struct GuiButton *gbtn);
// frontend_mappack_select/frontend_level_select (above) are the two list
// rows' click_events, both *highlighting* now (repurposed the same way
// frontend_campaign_select was for Land selection) -- mappack highlight
// switches the level list's contents and auto-highlights its first level;
// level highlight loads that level's own land preview. Committing is
// frontend_freeplay_enter, the "Play"/"Enter this land" button's
// click_event -- same highlight/commit split as Land selection.
void frontend_freeplay_enter(struct GuiButton *gbtn);
void frontend_freeplay_enter_maintain(struct GuiButton *gbtn);
void frontend_freeplay_return_to_main(struct GuiButton *gbtn);
void frontend_freeplay_return_to_main_maintain(struct GuiButton *gbtn);
void frontend_draw_freeplay_detail(struct GuiButton *gbtn);

// Multiplayer Map pack selection screen
void frontend_mp_mappack_select_up(struct GuiButton *gbtn);
void frontend_mp_mappack_select_down(struct GuiButton *gbtn);
void frontend_mp_mappack_select_scroll(struct GuiButton *gbtn);
void frontend_mp_mappack_select_up_maintain(struct GuiButton *gbtn);
void frontend_mp_mappack_select_down_maintain(struct GuiButton *gbtn);
void frontend_mp_mappack_select_maintain(struct GuiButton *gbtn);
void frontend_draw_mp_mappack_select_button(struct GuiButton *gbtn);
void frontend_mp_mappack_select(struct GuiButton *gbtn);
void frontend_mp_mappack_select_update(void);
void frontend_mp_draw_mappack_scroll_tab(struct GuiButton *gbtn);
void frontend_mp_mappack_list_load(void);
void frontend_mp_draw_variable_mappack_exit_button(struct GuiButton *gbtn);
void frontend_back_from_mp_mappack_list(struct GuiButton *gbtn);
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
