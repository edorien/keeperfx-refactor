/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file frontmenu_saves_data.cpp
 *     GUI menus for saved games support (save and load screens).
 * @par Purpose:
 *     Structures to show and maintain menus used for saving and loading.
 * @par Comment:
 *     None.
 * @author   KeeperFX Team
 * @date     07 Dec 2012 - 11 Feb 2013
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "frontmenu_options.h"
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
#include "gui_vscroll.h"
#include "config_settings.h"
#include "frontmenu_options.h"
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
struct GuiButtonInit load_menu_buttons[] = {
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 999, .scr_pos_y = 10, .pos_x = 999, .pos_y = 10, .width = 155, .height = 32, .draw_call = gui_area_text, .sprite_idx = 1, .tooltip_stridx = GUIStr_MnuLoad },
  { .gbtype = LbBtnT_NormalBtn, .button_flags = 1, .click_event = gui_load_game, .scr_pos_x = 999, .scr_pos_y = 58, .pos_x = 999, .pos_y = 58, .width = 300, .height = 32, .draw_call = draw_load_button, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[0] }, .maintain_call = gui_load_game_maintain },
  { .gbtype = LbBtnT_NormalBtn, .button_flags = 1, .click_event = gui_load_game, .btype_value = 1, .scr_pos_x = 999, .scr_pos_y = 90, .pos_x = 999, .pos_y = 90, .width = 300, .height = 32, .draw_call = draw_load_button, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[1] }, .maintain_call = gui_load_game_maintain },
  { .gbtype = LbBtnT_NormalBtn, .button_flags = 1, .click_event = gui_load_game, .btype_value = 2, .scr_pos_x = 999, .scr_pos_y = 122, .pos_x = 999, .pos_y = 122, .width = 300, .height = 32, .draw_call = draw_load_button, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[2] }, .maintain_call = gui_load_game_maintain },
  { .gbtype = LbBtnT_NormalBtn, .button_flags = 1, .click_event = gui_load_game, .btype_value = 3, .scr_pos_x = 999, .scr_pos_y = 154, .pos_x = 999, .pos_y = 154, .width = 300, .height = 32, .draw_call = draw_load_button, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[3] }, .maintain_call = gui_load_game_maintain },
  { .gbtype = LbBtnT_NormalBtn, .button_flags = 1, .click_event = gui_load_game, .btype_value = 4, .scr_pos_x = 999, .scr_pos_y = 186, .pos_x = 999, .pos_y = 186, .width = 300, .height = 32, .draw_call = draw_load_button, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[4] }, .maintain_call = gui_load_game_maintain },
  { .gbtype = LbBtnT_NormalBtn, .button_flags = 1, .click_event = gui_load_game, .btype_value = 5, .scr_pos_x = 999, .scr_pos_y = 218, .pos_x = 999, .pos_y = 218, .width = 300, .height = 32, .draw_call = draw_load_button, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[5] }, .maintain_call = gui_load_game_maintain },
  { .gbtype = LbBtnT_NormalBtn, .button_flags = 1, .click_event = gui_load_game, .btype_value = 6, .scr_pos_x = 999, .scr_pos_y = 250, .pos_x = 999, .pos_y = 250, .width = 300, .height = 32, .draw_call = draw_load_button, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[6] }, .maintain_call = gui_load_game_maintain },
  { .gbtype = LbBtnT_NormalBtn, .button_flags = 1, .click_event = gui_load_game, .btype_value = 7, .scr_pos_x = 999, .scr_pos_y = 282, .pos_x = 999, .pos_y = 282, .width = 300, .height = 32, .draw_call = draw_load_button, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[7] }, .maintain_call = gui_load_game_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = gui_vscroll_input, .scr_pos_x = 368, .scr_pos_y = 58, .pos_x = 368, .pos_y = 58, .width = 33, .height = 254, .draw_call = gui_vscroll_draw, .tooltip_stridx = GUIStr_Empty, .maintain_call = gui_vscroll_maintain },
  { .gbtype = -1 },
};

struct GuiButtonInit save_menu_buttons[] = {
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 999, .scr_pos_y = 10, .pos_x = 999, .pos_y = 10, .width = 155, .height = 32, .draw_call = gui_area_text, .sprite_idx = 1, .tooltip_stridx = GUIStr_MnuSave },
  { .gbtype = 5, .id_num = -2, .button_flags = 1, .click_event = gui_save_game, .scr_pos_x = 999, .scr_pos_y = 58, .pos_x = 999, .pos_y = 58, .width = 300, .height = 32, .draw_call = gui_area_text, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[0] }, .maxval = SAVE_TEXTNAME_LEN },
  { .gbtype = 5, .id_num = -2, .button_flags = 1, .click_event = gui_save_game, .btype_value = 1, .scr_pos_x = 999, .scr_pos_y = 90, .pos_x = 999, .pos_y = 90, .width = 300, .height = 32, .draw_call = gui_area_text, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[1] }, .maxval = SAVE_TEXTNAME_LEN },
  { .gbtype = 5, .id_num = -2, .button_flags = 1, .click_event = gui_save_game, .btype_value = 2, .scr_pos_x = 999, .scr_pos_y = 122, .pos_x = 999, .pos_y = 122, .width = 300, .height = 32, .draw_call = gui_area_text, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[2] }, .maxval = SAVE_TEXTNAME_LEN },
  { .gbtype = 5, .id_num = -2, .button_flags = 1, .click_event = gui_save_game, .btype_value = 3, .scr_pos_x = 999, .scr_pos_y = 154, .pos_x = 999, .pos_y = 154, .width = 300, .height = 32, .draw_call = gui_area_text, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[3] }, .maxval = SAVE_TEXTNAME_LEN },
  { .gbtype = 5, .id_num = -2, .button_flags = 1, .click_event = gui_save_game, .btype_value = 4, .scr_pos_x = 999, .scr_pos_y = 186, .pos_x = 999, .pos_y = 186, .width = 300, .height = 32, .draw_call = gui_area_text, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[4] }, .maxval = SAVE_TEXTNAME_LEN },
  { .gbtype = 5, .id_num = -2, .button_flags = 1, .click_event = gui_save_game, .btype_value = 5, .scr_pos_x = 999, .scr_pos_y = 218, .pos_x = 999, .pos_y = 218, .width = 300, .height = 32, .draw_call = gui_area_text, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[5] }, .maxval = SAVE_TEXTNAME_LEN },
  { .gbtype = 5, .id_num = -2, .button_flags = 1, .click_event = gui_save_game, .btype_value = 6, .scr_pos_x = 999, .scr_pos_y = 250, .pos_x = 999, .pos_y = 250, .width = 300, .height = 32, .draw_call = gui_area_text, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[6] }, .maxval = SAVE_TEXTNAME_LEN },
  { .gbtype = 5, .id_num = -2, .button_flags = 1, .click_event = gui_save_game, .btype_value = 7, .scr_pos_x = 999, .scr_pos_y = 282, .pos_x = 999, .pos_y = 282, .width = 300, .height = 32, .draw_call = gui_area_text, .sprite_idx = 1, .tooltip_stridx = GUIStr_Empty, .content = { .str = input_string[7] }, .maxval = SAVE_TEXTNAME_LEN },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = gui_vscroll_input, .scr_pos_x = 368, .scr_pos_y = 58, .pos_x = 368, .pos_y = 58, .width = 33, .height = 254, .draw_call = gui_vscroll_draw, .tooltip_stridx = GUIStr_Empty, .maintain_call = gui_vscroll_maintain },
  { .gbtype = -1 },
};

// Title and back button left-anchored (x=24, matching the main menu's
// column margin) to match Phase 1's layout
// (docs/refactor/gui/02-menu-v2-mockup-gap-analysis.md Phase 4) -- the save
// list/scrollbar layout below is untouched (cosmetic repositioning only, no
// game data here to verify a full-cluster reflow doesn't misalign them).
struct GuiButtonInit frontend_load_menu_buttons[] = {
  { .gbtype = LbBtnT_NormalBtn, .id_num = BID_MENU_TITLE, .scr_pos_x = 24, .scr_pos_y = 30, .pos_x = 24, .pos_y = 30, .width = 300, .height = 46, .draw_call = frontend_draw_button_icon, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuLoadGame_7 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 82, .scr_pos_y = 124, .pos_x = 82, .pos_y = 124, .width = 220, .height = 26, .draw_call = frontend_draw_scroll_box_tab, .tooltip_stridx = GUIStr_Empty, .content = { 28 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 82, .scr_pos_y = 150, .pos_x = 82, .pos_y = 150, .width = 450, .height = 182, .draw_call = frontend_draw_scroll_box, .tooltip_stridx = GUIStr_Empty, .content = { 26 } },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_load_game_up, .ptover_event = frontend_over_button, .scr_pos_x = 532, .scr_pos_y = 149, .pos_x = 532, .pos_y = 149, .width = 26, .height = 14, .draw_call = frontend_draw_slider_button, .tooltip_stridx = GUIStr_Empty, .content = { 17 }, .maintain_call = frontend_load_game_up_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_load_game_down, .ptover_event = frontend_over_button, .scr_pos_x = 532, .scr_pos_y = 317, .pos_x = 532, .pos_y = 317, .width = 26, .height = 14, .draw_call = frontend_draw_slider_button, .tooltip_stridx = GUIStr_Empty, .content = { 18 }, .maintain_call = frontend_load_game_down_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_load_game_scroll, .scr_pos_x = 536, .scr_pos_y = 163, .pos_x = 534, .pos_y = 163, .width = 20, .height = 154, .draw_call = frontend_draw_games_scroll_tab, .tooltip_stridx = GUIStr_Empty, .content = { 40 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 102, .scr_pos_y = 125, .pos_x = 102, .pos_y = 125, .width = 220, .height = 26, .draw_call = frontend_draw_text, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuGames } },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_load_game, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = 157, .pos_x = 95, .pos_y = 157, .width = 424, .height = 22, .draw_call = frontend_draw_load_game_button, .tooltip_stridx = GUIStr_Empty, .content = { 45 }, .maintain_call = frontend_load_game_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_load_game, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = 185, .pos_x = 95, .pos_y = 185, .width = 424, .height = 22, .draw_call = frontend_draw_load_game_button, .tooltip_stridx = GUIStr_Empty, .content = { 46 }, .maintain_call = frontend_load_game_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_load_game, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = 213, .pos_x = 95, .pos_y = 213, .width = 424, .height = 22, .draw_call = frontend_draw_load_game_button, .tooltip_stridx = GUIStr_Empty, .content = { 47 }, .maintain_call = frontend_load_game_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_load_game, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = 241, .pos_x = 95, .pos_y = 241, .width = 424, .height = 22, .draw_call = frontend_draw_load_game_button, .tooltip_stridx = GUIStr_Empty, .content = { 48 }, .maintain_call = frontend_load_game_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_load_game, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = 269, .pos_x = 95, .pos_y = 269, .width = 424, .height = 22, .draw_call = frontend_draw_load_game_button, .tooltip_stridx = GUIStr_Empty, .content = { 49 }, .maintain_call = frontend_load_game_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_load_game, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = 297, .pos_x = 95, .pos_y = 297, .width = 424, .height = 22, .draw_call = frontend_draw_load_game_button, .tooltip_stridx = GUIStr_Empty, .content = { 50 }, .maintain_call = frontend_load_game_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_change_state, .ptover_event = frontend_over_button, .btype_value = 1, .scr_pos_x = 24, .scr_pos_y = 404, .pos_x = 24, .pos_y = 404, .width = 180, .height = 42, .draw_call = frontend_draw_button_icon, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuReturnToMain } },
  { .gbtype = -1 },
};
#pragma GCC diagnostic pop

struct GuiMenu load_menu =
 {   GMnu_LOAD, 0, 4, load_menu_buttons,          POS_GAMECTR,POS_GAMECTR, 436, 350, gui_pretty_background, 0, NULL,    init_load_menu,          0, 1, 0,};
struct GuiMenu save_menu =
 {   GMnu_SAVE, 0, 4, save_menu_buttons,          POS_GAMECTR,POS_GAMECTR, 436, 350, gui_pretty_background, 0, NULL,    init_save_menu,          0, 1, 0,};
struct GuiMenu frontend_load_menu =
 { GMnu_FELOAD, 0, 1, frontend_load_menu_buttons,  POS_SCRCTR, POS_SCRCTR, 640, 480, NULL,                  0, NULL,    NULL,                    0, 0, 0,};

/******************************************************************************/
#ifdef __cplusplus
}
#endif
/******************************************************************************/
