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

struct GuiButtonInit frontend_select_campaign_buttons[] = {
  { .gbtype = LbBtnT_NormalBtn, .id_num = BID_MENU_TITLE, .scr_pos_x = 999, .scr_pos_y = 30, .pos_x = 999, .pos_y = 30, .width = 371, .height = 46, .draw_call = frontend_draw_large_menu_button, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuLandSelection } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 82, .scr_pos_y = 128, .pos_x = 82, .pos_y = 128, .width = 220, .height = 26, .draw_call = frontend_draw_scroll_box_tab, .tooltip_stridx = GUIStr_Empty, .content = { 28 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 82, .scr_pos_y = 154, .pos_x = 82, .pos_y = 154, .width = 450, .height = 180, .draw_call = frontend_draw_scroll_box, .tooltip_stridx = GUIStr_Empty, .content = { 26 } },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_campaign_select_up, .ptover_event = frontend_over_button, .scr_pos_x = 532, .scr_pos_y = 153, .pos_x = 532, .pos_y = 153, .width = 26, .height = 14, .draw_call = frontend_draw_slider_button, .tooltip_stridx = GUIStr_Empty, .content = { 17 }, .maintain_call = frontend_campaign_select_up_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_campaign_select_down, .ptover_event = frontend_over_button, .scr_pos_x = 532, .scr_pos_y = 321, .pos_x = 532, .pos_y = 321, .width = 26, .height = 14, .draw_call = frontend_draw_slider_button, .tooltip_stridx = GUIStr_Empty, .content = { 18 }, .maintain_call = frontend_campaign_select_down_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_campaign_select_scroll, .scr_pos_x = 536, .scr_pos_y = 167, .pos_x = 536, .pos_y = 167, .width = 20, .height = 154, .draw_call = frontend_draw_campaign_scroll_tab, .tooltip_stridx = GUIStr_Empty, .content = { 40 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 102, .scr_pos_y = 129, .pos_x = 102, .pos_y = 129, .width = 220, .height = 26, .draw_call = frontend_draw_text, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuCampaigns } },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 0), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 0), .width = 424, .height = 22, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 45 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 1), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 1), .width = 424, .height = 22, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 46 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 2), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 2), .width = 424, .height = 22, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 47 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 3), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 3), .width = 424, .height = 22, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 48 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 4), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 4), .width = 424, .height = 22, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 49 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 5), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 5), .width = 424, .height = 22, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 50 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_campaign_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(167, 22, 6), .pos_x = 95, .pos_y = FE_ROW_Y(169, 22, 6), .width = 424, .height = 22, .draw_call = frontend_draw_campaign_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 51 }, .maintain_call = frontend_campaign_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_change_state, .ptover_event = frontend_over_button, .btype_value = FeSt_MAIN_MENU, .scr_pos_x = 999, .scr_pos_y = 404, .pos_x = 999, .pos_y = 404, .width = 371, .height = 46, .draw_call = frontend_draw_large_menu_button, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuReturnToMain } },
  { .gbtype = -1, .tooltip_stridx = GUIStr_Empty },
};

// Merged Free play screen: mappack list + level list, both using the same
// shared listbox control (frontend_draw_scroll_box/_tab) as campaign
// select/load game, stacked in one screen instead of navigating between
// two -- picking a mappack refreshes the level list in place. Both boxes
// span the same ~450px width the control was designed for (scaling it
// wider ties row height to width -- see gui_draw_scroll_box's own
// comment -- so native width keeps rows a normal, readable size) and
// split the vertical space roughly 40/60 (3 lines for map packs, 4 for
// levels: frontend_select_mappack/level_items_max_visible,
// frontmenu_select.h) since map packs are typically few and levels
// within one can be many.
#define FE_FREEPLAY_MAPPACK_TAB_Y   110
#define FE_FREEPLAY_MAPPACK_BOX_Y   136
#define FE_FREEPLAY_MAPPACK_BOX_H   92
#define FE_FREEPLAY_LEVEL_TAB_Y     244
#define FE_FREEPLAY_LEVEL_BOX_Y     270
#define FE_FREEPLAY_LEVEL_BOX_H     114
struct GuiButtonInit frontend_select_mappack_buttons[] = {
  { .gbtype = LbBtnT_NormalBtn, .id_num = BID_MENU_TITLE, .scr_pos_x = 999, .scr_pos_y = 30, .pos_x = 999, .pos_y = 30, .width = 371, .height = 46, .draw_call = frontend_draw_large_menu_button, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuFreePlayLevels_107 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 82, .scr_pos_y = FE_FREEPLAY_MAPPACK_TAB_Y, .pos_x = 82, .pos_y = FE_FREEPLAY_MAPPACK_TAB_Y, .width = 220, .height = 26, .draw_call = frontend_draw_scroll_box_tab, .tooltip_stridx = GUIStr_Empty, .content = { 28 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 82, .scr_pos_y = FE_FREEPLAY_MAPPACK_BOX_Y, .pos_x = 82, .pos_y = FE_FREEPLAY_MAPPACK_BOX_Y, .width = 450, .height = FE_FREEPLAY_MAPPACK_BOX_H, .draw_call = frontend_draw_scroll_box, .tooltip_stridx = GUIStr_Empty, .content = { 25 } },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_mappack_select_up, .ptover_event = frontend_over_button, .scr_pos_x = 532, .scr_pos_y = FE_FREEPLAY_MAPPACK_BOX_Y - 1, .pos_x = 532, .pos_y = FE_FREEPLAY_MAPPACK_BOX_Y - 1, .width = 26, .height = 14, .draw_call = frontend_draw_slider_button, .tooltip_stridx = GUIStr_Empty, .content = { 17 }, .maintain_call = frontend_mappack_select_up_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_mappack_select_down, .ptover_event = frontend_over_button, .scr_pos_x = 532, .scr_pos_y = FE_FREEPLAY_MAPPACK_BOX_Y + FE_FREEPLAY_MAPPACK_BOX_H - 13, .pos_x = 532, .pos_y = FE_FREEPLAY_MAPPACK_BOX_Y + FE_FREEPLAY_MAPPACK_BOX_H - 13, .width = 26, .height = 14, .draw_call = frontend_draw_slider_button, .tooltip_stridx = GUIStr_Empty, .content = { 18 }, .maintain_call = frontend_mappack_select_down_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_mappack_select_scroll, .scr_pos_x = 536, .scr_pos_y = FE_FREEPLAY_MAPPACK_BOX_Y + 13, .pos_x = 536, .pos_y = FE_FREEPLAY_MAPPACK_BOX_Y + 13, .width = 20, .height = FE_FREEPLAY_MAPPACK_BOX_H - 26, .draw_call = frontend_draw_mappack_scroll_tab, .tooltip_stridx = GUIStr_Empty, .content = { 40 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 102, .scr_pos_y = FE_FREEPLAY_MAPPACK_TAB_Y + 1, .pos_x = 102, .pos_y = FE_FREEPLAY_MAPPACK_TAB_Y + 1, .width = 220, .height = 26, .draw_call = frontend_draw_text, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuMapPacks } },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_mappack_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_BOX_Y + 13, 22, 0), .pos_x = 95, .pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_BOX_Y + 15, 22, 0), .width = 424, .height = 22, .draw_call = frontend_draw_mappack_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 45 }, .maintain_call = frontend_mappack_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_mappack_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_BOX_Y + 13, 22, 1), .pos_x = 95, .pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_BOX_Y + 15, 22, 1), .width = 424, .height = 22, .draw_call = frontend_draw_mappack_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 46 }, .maintain_call = frontend_mappack_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_mappack_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_BOX_Y + 13, 22, 2), .pos_x = 95, .pos_y = FE_ROW_Y(FE_FREEPLAY_MAPPACK_BOX_Y + 15, 22, 2), .width = 424, .height = 22, .draw_call = frontend_draw_mappack_select_button, .tooltip_stridx = GUIStr_Empty, .content = { 47 }, .maintain_call = frontend_mappack_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 82, .scr_pos_y = FE_FREEPLAY_LEVEL_TAB_Y, .pos_x = 82, .pos_y = FE_FREEPLAY_LEVEL_TAB_Y, .width = 220, .height = 26, .draw_call = frontend_draw_scroll_box_tab, .tooltip_stridx = GUIStr_Empty, .content = { 28 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 82, .scr_pos_y = FE_FREEPLAY_LEVEL_BOX_Y, .pos_x = 82, .pos_y = FE_FREEPLAY_LEVEL_BOX_Y, .width = 450, .height = FE_FREEPLAY_LEVEL_BOX_H, .draw_call = frontend_draw_scroll_box, .tooltip_stridx = GUIStr_Empty, .content = { 91 } },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_level_select_up, .ptover_event = frontend_over_button, .scr_pos_x = 532, .scr_pos_y = FE_FREEPLAY_LEVEL_BOX_Y - 1, .pos_x = 532, .pos_y = FE_FREEPLAY_LEVEL_BOX_Y - 1, .width = 26, .height = 14, .draw_call = frontend_draw_slider_button, .tooltip_stridx = GUIStr_Empty, .content = { 17 }, .maintain_call = frontend_level_select_up_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_level_select_down, .ptover_event = frontend_over_button, .scr_pos_x = 532, .scr_pos_y = FE_FREEPLAY_LEVEL_BOX_Y + FE_FREEPLAY_LEVEL_BOX_H - 13, .pos_x = 532, .pos_y = FE_FREEPLAY_LEVEL_BOX_Y + FE_FREEPLAY_LEVEL_BOX_H - 13, .width = 26, .height = 14, .draw_call = frontend_draw_slider_button, .tooltip_stridx = GUIStr_Empty, .content = { 18 }, .maintain_call = frontend_level_select_down_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_level_select_scroll, .scr_pos_x = 536, .scr_pos_y = FE_FREEPLAY_LEVEL_BOX_Y + 13, .pos_x = 536, .pos_y = FE_FREEPLAY_LEVEL_BOX_Y + 13, .width = 20, .height = FE_FREEPLAY_LEVEL_BOX_H - 26, .draw_call = frontend_draw_levels_scroll_tab, .tooltip_stridx = GUIStr_Empty, .content = { 40 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 102, .scr_pos_y = FE_FREEPLAY_LEVEL_TAB_Y + 1, .pos_x = 102, .pos_y = FE_FREEPLAY_LEVEL_TAB_Y + 1, .width = 220, .height = 26, .draw_call = frontend_draw_level_select_mappack, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuLevels } },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_BOX_Y + 13, 22, 0), .pos_x = 95, .pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_BOX_Y + 15, 22, 0), .width = 424, .height = 22, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 0 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_BOX_Y + 13, 22, 1), .pos_x = 95, .pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_BOX_Y + 15, 22, 1), .width = 424, .height = 22, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 1 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_BOX_Y + 13, 22, 2), .pos_x = 95, .pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_BOX_Y + 15, 22, 2), .width = 424, .height = 22, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 2 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_level_select, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_BOX_Y + 13, 22, 3), .pos_x = 95, .pos_y = FE_ROW_Y(FE_FREEPLAY_LEVEL_BOX_Y + 15, 22, 3), .width = 424, .height = 22, .draw_call = frontend_draw_level_select_button, .tooltip_stridx = GUIStr_Empty, .content = { FE_LEVEL_SELECTLIST_ROW_BASE + 3 }, .maintain_call = frontend_level_select_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_change_state, .ptover_event = frontend_over_button, .btype_value = FeSt_MAIN_MENU, .scr_pos_x = 999, .scr_pos_y = 404, .pos_x = 999, .pos_y = 404, .width = 371, .height = 46, .draw_call = frontend_draw_large_menu_button, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuReturnToMain } },
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
