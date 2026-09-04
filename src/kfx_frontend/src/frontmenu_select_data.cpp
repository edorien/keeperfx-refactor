/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file frontmenu_select_data.cpp
 *     GUI menus for level and campaign select screens.
 * @par Purpose:
 *     Structures to show and maintain menus used for level and campaign list screens.
 * @par Comment:
 *     None.
 * @author   KeeperFX Team
 * @date     07 Dec 2012 - 11 Aug 2014
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "frontmenu_select.h"
#include "globals.h"
#include "bflib_basics.h"

#include "bflib_guibtns.h"
#include "bflib_sprite.h"
#include "bflib_sprfnt.h"
#include "bflib_vidraw.h"

#include "gui_frontbtns.h"
#include "gui_draw.h"
#include "frontend.h"
#include "frontmenu_saves.h"
#include "frontmenu_landpreview.h"
#include "config_settings.h"
#include "config_strings.h"
#include "game_legacy.h"
#include "post_inc.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
// GCC's -Wmissing-field-initializers fires on a partially-designated
// GuiButtonInit aggregate in this C++ translation unit even though the
// omitted fields are the struct's own zero defaults; not a real risk here
// since every field is still named where it matters.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
struct GuiButtonInit frontend_select_level_buttons[] = {
  { .gbtype = LbBtnT_NormalBtn, .id_num = BID_MENU_TITLE, .scr_pos_x = 999, .scr_pos_y = 30, .pos_x = 999, .pos_y = 30, .width = 371, .height = 46, .draw_call = frontend_draw_large_menu_button, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuFreePlayLevels_107 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 82, .scr_pos_y = 128, .pos_x = 82, .pos_y = 128, .width = 220, .height = 26, .draw_call = frontend_draw_scroll_box_tab, .tooltip_stridx = GUIStr_Empty, .content = { 28 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 82, .scr_pos_y = 154, .pos_x = 82, .pos_y = 154, .width = 450, .height = 180, .draw_call = frontend_draw_scroll_box, .tooltip_stridx = GUIStr_Empty, .content = { 26 } },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_level_select_up, .ptover_event = frontend_over_button, .scr_pos_x = 532, .scr_pos_y = 153, .pos_x = 532, .pos_y = 153, .width = 26, .height = 14, .draw_call = frontend_draw_slider_button, .tooltip_stridx = GUIStr_Empty, .content = { 17 }, .maintain_call = frontend_level_select_up_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_level_select_down, .ptover_event = frontend_over_button, .scr_pos_x = 532, .scr_pos_y = 321, .pos_x = 532, .pos_y = 321, .width = 26, .height = 14, .draw_call = frontend_draw_slider_button, .tooltip_stridx = GUIStr_Empty, .content = { 18 }, .maintain_call = frontend_level_select_down_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_level_select_scroll, .scr_pos_x = 536, .scr_pos_y = 167, .pos_x = 536, .pos_y = 167, .width = 20, .height = 154, .draw_call = frontend_draw_levels_scroll_tab, .tooltip_stridx = GUIStr_Empty, .content = { 40 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 102, .scr_pos_y = 129, .pos_x = 102, .pos_y = 129, .width = 220, .height = 26, .draw_call = frontend_draw_level_select_mappack, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuLevels } },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 0), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 0), .width = 424, .height = 22, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 0 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 1), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 1), .width = 424, .height = 22, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 1 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 2), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 2), .width = 424, .height = 22, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 2 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 3), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 3), .width = 424, .height = 22, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 3 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 4), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 4), .width = 424, .height = 22, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 4 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 5), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 5), .width = 424, .height = 22, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 5 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 6), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 6), .width = 424, .height = 22, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 6 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_change_state, .ptover_event = frontend_over_button, .btype_value = FeSt_MAPPACK_SELECT, .scr_pos_x = 999, .scr_pos_y = 404, .pos_x = 999, .pos_y = 404, .width = 371, .height = 46, .draw_call = frontend_draw_variable_mappack_exit_button, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuReturnToFreePlay } },
  { .gbtype = -1, .tooltip_stridx = GUIStr_Empty },
};

// Land selection: merged campaign list (left) + interactive land preview
// panel + detail text + commit button (right), replacing the old
// campaign-select-then-separate-land-view-cutscene flow. Narrow list
// column follows Phase 1/4's left-anchored convention
// (FE_MAINMENU_COL_X-equivalent margin); the list engine itself
// (campaign_select_list) is unchanged, only repositioned/narrowed.
// FE_LANDSEL_COL_X/_LIST_W/_ROW_Y0 are shared via frontmenu_select.h -- see there.
// Width and height are independent here: FE_LANDSEL_ROW_STEP is a fixed,
// readable row height (matching the original campaign-select screen's own
// row height, not derived from box width the way frontend_draw_scroll_box's
// is -- see frontend_draw_land_selection_panel_bg's comment for why that
// control doesn't fit this screen's narrower column). Available vertical
// room is a layout choice (frontend_select_campaign_items_max_visible,
// frontmenu_select.h), not the other way around -- more rows visible at
// once, not smaller rows, is how "the box is taller here" should look.
#define FE_LANDSEL_ROW_STEP   22
// FE_LANDSEL_PANEL_X is shared via frontmenu_select.h -- see there. Right
// edge (PANEL_X + PANEL_W) is 616, clear of the 640px menu width.
#define FE_LANDSEL_PANEL_W    368
#define FE_LANDSEL_PREVIEW_Y  64
#define FE_LANDSEL_PREVIEW_H  230
#define FE_LANDSEL_DETAIL_Y   304
#define FE_LANDSEL_DETAIL_H   66
struct GuiButtonInit frontend_select_campaign_buttons[] = {
  { .gbtype = LbBtnT_NormalBtn, .id_num = BID_MENU_TITLE, .scr_pos_x = FE_LANDSEL_COL_X, .scr_pos_y = 20, .pos_x = FE_LANDSEL_COL_X, .pos_y = 20, .width = 300, .height = 36, .draw_call = frontend_draw_button_icon, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuLandSelection } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = FE_LANDSEL_COL_X, .scr_pos_y = 64, .pos_x = FE_LANDSEL_COL_X, .pos_y = 64, .width = FE_LANDSEL_LIST_W, .height = 26, .draw_call = frontend_draw_text, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuCampaigns } },
  // Flat background (frontend_draw_land_selection_panel_bg, see its own
  // comment) sized to the list's real footprint at the fixed row height --
  // genuinely independent width/height, unlike frontend_draw_scroll_box.
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = FE_LANDSEL_COL_X, .scr_pos_y = FE_LANDSEL_ROW_Y0 - 4, .pos_x = FE_LANDSEL_COL_X, .pos_y = FE_LANDSEL_ROW_Y0 - 4, .width = FE_LANDSEL_LIST_W, .height = FE_LANDSEL_ROW_STEP * frontend_select_campaign_items_max_visible + 8, .draw_call = frontend_draw_land_selection_panel_bg, .tooltip_stridx = GUIStr_Empty },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_campaign_select_up, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .scr_pos_y = FE_LANDSEL_ROW_Y0, .pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .pos_y = FE_LANDSEL_ROW_Y0, .width = 20, .height = 14, .tooltip_stridx = GUIStr_Empty, .content = { 17 }, .maintain_call = frontend_campaign_select_up_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_campaign_select_down, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .scr_pos_y = FE_LANDSEL_ROW_Y0 + FE_LANDSEL_ROW_STEP * frontend_select_campaign_items_max_visible - 14, .pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .pos_y = FE_LANDSEL_ROW_Y0 + FE_LANDSEL_ROW_STEP * frontend_select_campaign_items_max_visible - 14, .width = 20, .height = 14, .tooltip_stridx = GUIStr_Empty, .content = { 18 }, .maintain_call = frontend_campaign_select_down_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_campaign_select_scroll, .scr_pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .scr_pos_y = FE_LANDSEL_ROW_Y0 + 16, .pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .pos_y = FE_LANDSEL_ROW_Y0 + 16, .width = 16, .height = FE_LANDSEL_ROW_STEP * frontend_select_campaign_items_max_visible - 32, .draw_call = frontend_draw_campaign_scroll_tab, .tooltip_stridx = GUIStr_Empty, .content = { 40 } },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 0), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 0), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 45 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 1), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 1), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 46 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 2), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 2), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 47 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 3), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 3), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 48 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 4), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 4), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 49 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 5), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 5), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 50 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 6), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 6), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 51 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 7), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 7), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 52 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 8), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 8), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 53 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 9), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 9), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 54 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 10), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 10), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 55 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 11), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_LANDSEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 11), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 56 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = FE_LANDSEL_PANEL_X, .scr_pos_y = FE_LANDSEL_PREVIEW_Y, .pos_x = FE_LANDSEL_PANEL_X, .pos_y = FE_LANDSEL_PREVIEW_Y, .width = FE_LANDSEL_PANEL_W, .height = FE_LANDSEL_PREVIEW_H, .draw_call = land_preview_draw, .tooltip_stridx = GUIStr_Empty, .maintain_call = land_preview_maintain },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = FE_LANDSEL_PANEL_X, .scr_pos_y = FE_LANDSEL_DETAIL_Y, .pos_x = FE_LANDSEL_PANEL_X, .pos_y = FE_LANDSEL_DETAIL_Y, .width = FE_LANDSEL_PANEL_W, .height = FE_LANDSEL_DETAIL_H, .draw_call = frontend_draw_land_selection_detail, .tooltip_stridx = GUIStr_Empty },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_land_selection_enter, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X, .scr_pos_y = 404, .pos_x = FE_LANDSEL_COL_X, .pos_y = 404, .width = 180, .height = 42, .draw_call = frontend_draw_button_icon, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuEnterLand }, .maintain_call = frontend_land_selection_enter_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_land_selection_return_to_main, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 190, .scr_pos_y = 404, .pos_x = FE_LANDSEL_COL_X + 190, .pos_y = 404, .width = 180, .height = 42, .draw_call = frontend_draw_button_icon, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuReturnToMain }, .maintain_call = frontend_land_selection_return_to_main_maintain },
  { .gbtype = -1, .tooltip_stridx = GUIStr_Empty },
};

// Merged Free play screen (mappack list + level list + land preview):
// same left-column-list(s) + right-column-preview/detail/buttons
// structure as Land selection, just two stacked lists on the left instead
// of one, sharing the left column's vertical budget
// (frontend_select_mappack_items_max_visible=5 + _level_=7 rows, both in
// frontmenu_select.h). Both list backgrounds use
// frontend_draw_land_selection_panel_bg (see its own comment) for the
// same reason Land selection's list does -- frontend_draw_scroll_box
// couples row height to width, wrong for this narrow column.
#define FE_FREEPLAY_MAPPACK_ROW_Y0 92
#define FE_FREEPLAY_LEVEL_ROW_Y0   240
struct GuiButtonInit frontend_select_mappack_buttons[] = {
  { .gbtype = LbBtnT_NormalBtn, .id_num = BID_MENU_TITLE, .scr_pos_x = FE_LANDSEL_COL_X, .scr_pos_y = 20, .pos_x = FE_LANDSEL_COL_X, .pos_y = 20, .width = 300, .height = 36, .draw_call = frontend_draw_button_icon, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuFreePlayLevels_107 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = FE_LANDSEL_COL_X, .scr_pos_y = 64, .pos_x = FE_LANDSEL_COL_X, .pos_y = 64, .width = FE_LANDSEL_LIST_W, .height = 26, .draw_call = frontend_draw_text, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuMapPacks } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = FE_LANDSEL_COL_X, .scr_pos_y = FE_FREEPLAY_MAPPACK_ROW_Y0 - 4, .pos_x = FE_LANDSEL_COL_X, .pos_y = FE_FREEPLAY_MAPPACK_ROW_Y0 - 4, .width = FE_LANDSEL_LIST_W, .height = FE_LANDSEL_ROW_STEP * frontend_select_mappack_items_max_visible + 8, .draw_call = frontend_draw_land_selection_panel_bg, .tooltip_stridx = GUIStr_Empty },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_mappack_select_up, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .scr_pos_y = FE_FREEPLAY_MAPPACK_ROW_Y0, .pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .pos_y = FE_FREEPLAY_MAPPACK_ROW_Y0, .width = 20, .height = 14, .tooltip_stridx = GUIStr_Empty, .content = { 17 }, .maintain_call = frontend_mappack_select_up_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_mappack_select_down, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .scr_pos_y = FE_FREEPLAY_MAPPACK_ROW_Y0 + FE_LANDSEL_ROW_STEP * frontend_select_mappack_items_max_visible - 14, .pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .pos_y = FE_FREEPLAY_MAPPACK_ROW_Y0 + FE_LANDSEL_ROW_STEP * frontend_select_mappack_items_max_visible - 14, .width = 20, .height = 14, .tooltip_stridx = GUIStr_Empty, .content = { 18 }, .maintain_call = frontend_mappack_select_down_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_mappack_select_scroll, .scr_pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .scr_pos_y = FE_FREEPLAY_MAPPACK_ROW_Y0 + 16, .pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .pos_y = FE_FREEPLAY_MAPPACK_ROW_Y0 + 16, .width = 16, .height = FE_LANDSEL_ROW_STEP * frontend_select_mappack_items_max_visible - 32, .draw_call = frontend_draw_mappack_scroll_tab, .tooltip_stridx = GUIStr_Empty, .content = { 40 } },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_mappack_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_ROW_Y0, FE_LANDSEL_ROW_STEP, 0), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_ROW_Y0, FE_LANDSEL_ROW_STEP, 0), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_mappack_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 45 }, .maintain_call = frontend_mappack_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_mappack_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_ROW_Y0, FE_LANDSEL_ROW_STEP, 1), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_ROW_Y0, FE_LANDSEL_ROW_STEP, 1), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_mappack_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 46 }, .maintain_call = frontend_mappack_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_mappack_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_ROW_Y0, FE_LANDSEL_ROW_STEP, 2), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_ROW_Y0, FE_LANDSEL_ROW_STEP, 2), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_mappack_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 47 }, .maintain_call = frontend_mappack_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_mappack_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_ROW_Y0, FE_LANDSEL_ROW_STEP, 3), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_ROW_Y0, FE_LANDSEL_ROW_STEP, 3), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_mappack_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 48 }, .maintain_call = frontend_mappack_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_mappack_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_ROW_Y0, FE_LANDSEL_ROW_STEP, 4), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_ROW_Y0, FE_LANDSEL_ROW_STEP, 4), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_mappack_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 49 }, .maintain_call = frontend_mappack_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = FE_LANDSEL_COL_X, .scr_pos_y = 212, .pos_x = FE_LANDSEL_COL_X, .pos_y = 212, .width = FE_LANDSEL_LIST_W, .height = 26, .draw_call = frontend_draw_text, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuLevels } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = FE_LANDSEL_COL_X, .scr_pos_y = FE_FREEPLAY_LEVEL_ROW_Y0 - 4, .pos_x = FE_LANDSEL_COL_X, .pos_y = FE_FREEPLAY_LEVEL_ROW_Y0 - 4, .width = FE_LANDSEL_LIST_W, .height = FE_LANDSEL_ROW_STEP * frontend_select_level_items_max_visible + 8, .draw_call = frontend_draw_land_selection_panel_bg, .tooltip_stridx = GUIStr_Empty },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_level_select_up, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .scr_pos_y = FE_FREEPLAY_LEVEL_ROW_Y0, .pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .pos_y = FE_FREEPLAY_LEVEL_ROW_Y0, .width = 20, .height = 14, .tooltip_stridx = GUIStr_Empty, .content = { 17 }, .maintain_call = frontend_level_select_up_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_level_select_down, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .scr_pos_y = FE_FREEPLAY_LEVEL_ROW_Y0 + FE_LANDSEL_ROW_STEP * frontend_select_level_items_max_visible - 14, .pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .pos_y = FE_FREEPLAY_LEVEL_ROW_Y0 + FE_LANDSEL_ROW_STEP * frontend_select_level_items_max_visible - 14, .width = 20, .height = 14, .tooltip_stridx = GUIStr_Empty, .content = { 18 }, .maintain_call = frontend_level_select_down_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_level_select_scroll, .scr_pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .scr_pos_y = FE_FREEPLAY_LEVEL_ROW_Y0 + 16, .pos_x = FE_LANDSEL_COL_X + FE_LANDSEL_LIST_W, .pos_y = FE_FREEPLAY_LEVEL_ROW_Y0 + 16, .width = 16, .height = FE_LANDSEL_ROW_STEP * frontend_select_level_items_max_visible - 32, .draw_call = frontend_draw_levels_scroll_tab, .tooltip_stridx = GUIStr_Empty, .content = { 40 } },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 0), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 0), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 0 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 1), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 1), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 1 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 2), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 2), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 2 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 3), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 3), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 3 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 4), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 4), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 4 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 5), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 5), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 5 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 6, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 6), .pos_x = FE_LANDSEL_COL_X + 6, .pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_ROW_Y0, FE_LANDSEL_ROW_STEP, 6), .width = FE_LANDSEL_LIST_W - 12, .height = FE_LANDSEL_ROW_STEP, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 6 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = FE_LANDSEL_PANEL_X, .scr_pos_y = FE_LANDSEL_PREVIEW_Y, .pos_x = FE_LANDSEL_PANEL_X, .pos_y = FE_LANDSEL_PREVIEW_Y, .width = FE_LANDSEL_PANEL_W, .height = FE_LANDSEL_PREVIEW_H, .draw_call = land_preview_draw, .tooltip_stridx = GUIStr_Empty, .maintain_call = land_preview_maintain },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = FE_LANDSEL_PANEL_X, .scr_pos_y = FE_LANDSEL_DETAIL_Y, .pos_x = FE_LANDSEL_PANEL_X, .pos_y = FE_LANDSEL_DETAIL_Y, .width = FE_LANDSEL_PANEL_W, .height = FE_LANDSEL_DETAIL_H, .draw_call = frontend_draw_freeplay_detail, .tooltip_stridx = GUIStr_Empty },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_freeplay_enter, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X, .scr_pos_y = 404, .pos_x = FE_LANDSEL_COL_X, .pos_y = 404, .width = 180, .height = 42, .draw_call = frontend_draw_button_icon, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuPlayLevel }, .maintain_call = frontend_freeplay_enter_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_freeplay_return_to_main, .ptover_event = frontend_over_button, .scr_pos_x = FE_LANDSEL_COL_X + 190, .scr_pos_y = 404, .pos_x = FE_LANDSEL_COL_X + 190, .pos_y = 404, .width = 180, .height = 42, .draw_call = frontend_draw_button_icon, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuReturnToMain }, .maintain_call = frontend_freeplay_return_to_main_maintain },
  { .gbtype = -1, .tooltip_stridx = GUIStr_Empty },
};

struct GuiButtonInit frontend_select_mp_mappack_buttons[] = {
  { .gbtype = LbBtnT_NormalBtn, .id_num = BID_MENU_TITLE, .scr_pos_x = 999, .scr_pos_y = 30, .pos_x = 999, .pos_y = 30, .width = 371, .height = 46, .draw_call = frontend_draw_large_menu_button, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuMpMapPacks } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 82, .scr_pos_y = 128, .pos_x = 82, .pos_y = 128, .width = 220, .height = 26, .draw_call = frontend_draw_scroll_box_tab, .tooltip_stridx = GUIStr_Empty, .content = { 28 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 82, .scr_pos_y = 154, .pos_x = 82, .pos_y = 154, .width = 450, .height = 180, .draw_call = frontend_draw_scroll_box, .tooltip_stridx = GUIStr_Empty, .content = { 26 } },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_mp_mappack_select_up, .ptover_event = frontend_over_button, .scr_pos_x = 532, .scr_pos_y = 153, .pos_x = 532, .pos_y = 153, .width = 26, .height = 14, .draw_call = frontend_draw_slider_button, .tooltip_stridx = GUIStr_Empty, .content = { 17 }, .maintain_call = frontend_mp_mappack_select_up_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_mp_mappack_select_down, .ptover_event = frontend_over_button, .scr_pos_x = 532, .scr_pos_y = 321, .pos_x = 532, .pos_y = 321, .width = 26, .height = 14, .draw_call = frontend_draw_slider_button, .tooltip_stridx = GUIStr_Empty, .content = { 18 }, .maintain_call = frontend_mp_mappack_select_down_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_mp_mappack_select_scroll, .scr_pos_x = 536, .scr_pos_y = 167, .pos_x = 536, .pos_y = 167, .width = 20, .height = 154, .draw_call = frontend_draw_mp_mappack_scroll_tab, .tooltip_stridx = GUIStr_Empty, .content = { 40 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 102, .scr_pos_y = 129, .pos_x = 102, .pos_y = 129, .width = 220, .height = 26, .draw_call = frontend_draw_text, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuMapPacks } },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_mp_mappack_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 0), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 0), .width = 424, .height = 22, .draw_call = frontend_draw_mp_mappack_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 45 }, .maintain_call = frontend_mp_mappack_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_mp_mappack_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 1), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 1), .width = 424, .height = 22, .draw_call = frontend_draw_mp_mappack_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 46 }, .maintain_call = frontend_mp_mappack_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_mp_mappack_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 2), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 2), .width = 424, .height = 22, .draw_call = frontend_draw_mp_mappack_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 47 }, .maintain_call = frontend_mp_mappack_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_mp_mappack_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 3), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 3), .width = 424, .height = 22, .draw_call = frontend_draw_mp_mappack_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 48 }, .maintain_call = frontend_mp_mappack_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_mp_mappack_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 4), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 4), .width = 424, .height = 22, .draw_call = frontend_draw_mp_mappack_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 49 }, .maintain_call = frontend_mp_mappack_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_mp_mappack_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 5), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 5), .width = 424, .height = 22, .draw_call = frontend_draw_mp_mappack_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 50 }, .maintain_call = frontend_mp_mappack_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_mp_mappack_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 6), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 6), .width = 424, .height = 22, .draw_call = frontend_draw_mp_mappack_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 51 }, .maintain_call = frontend_mp_mappack_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_back_from_mp_mappack_list, .ptover_event = frontend_over_button, .btype_value = FeSt_NET_START, .scr_pos_x = 999, .scr_pos_y = 404, .pos_x = 999, .pos_y = 404, .width = 371, .height = 46, .draw_call = frontend_draw_large_menu_button, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuReturnToLobby } },
  { .gbtype = -1, .tooltip_stridx = GUIStr_Empty },
};
#pragma GCC diagnostic pop

struct GuiMenu frontend_select_mappack_menu =
 { GMnu_MAPPACK_SELECT,     0, 1, frontend_select_mappack_buttons,   POS_SCRCTR, POS_SCRCTR, 640, 480, NULL, 0, NULL,    NULL,                    0, 0, 0,};
 struct GuiMenu frontend_select_level_menu =
 { GMnu_FELEVEL_SELECT,     0, 1, frontend_select_level_buttons,   POS_SCRCTR, POS_SCRCTR, 640, 480, NULL, 0, NULL,    NULL,                    0, 0, 0,};
struct GuiMenu frontend_select_campaign_menu =
 { GMnu_FECAMPAIGN_SELECT,  0, 1, frontend_select_campaign_buttons,POS_SCRCTR, POS_SCRCTR, 640, 480, NULL, 0, NULL,    NULL,                    0, 0, 0,};
struct GuiMenu frontend_select_mp_mappack_menu =
 { GMnu_MP_MAPPACK_SELECT,     0, 1, frontend_select_mp_mappack_buttons,   POS_SCRCTR, POS_SCRCTR, 640, 480, NULL, 0, NULL,    NULL,                    0, 0, 0,};



/******************************************************************************/
#ifdef __cplusplus
}
#endif
/******************************************************************************/
