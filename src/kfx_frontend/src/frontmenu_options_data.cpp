/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file frontmenu_options_data.cpp
 *     GUI menus for game options.
 * @par Purpose:
 *     Structures to show and maintain options screens.
 * @par Comment:
 *     None.
 * @author   KeeperFX Team
 * @date     05 Dec 2012 - 11 Feb 2013
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "frontmenu_ingame_opts.h"
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
struct GuiButtonInit frontend_define_keys_buttons[] = {
  { .gbtype = LbBtnT_NormalBtn, .id_num = BID_MENU_TITLE, .scr_pos_x = 999, .scr_pos_y = 30, .pos_x = 999, .pos_y = 30, .width = 371, .height = 46, .draw_call = frontend_draw_large_menu_button, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_DefineKeys } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 82, .scr_pos_y = 117, .pos_x = 82, .pos_y = 117, .width = 450, .height = 246, .draw_call = frontend_draw_scroll_box, .tooltip_stridx = GUIStr_Empty, .content = { 94 } },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_define_key_up, .ptover_event = frontend_over_button, .scr_pos_x = 532, .scr_pos_y = 116, .pos_x = 532, .pos_y = 116, .width = 26, .height = 14, .draw_call = frontend_draw_slider_button, .tooltip_stridx = GUIStr_Empty, .content = { 17 }, .maintain_call = frontend_define_key_up_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_define_key_down, .ptover_event = frontend_over_button, .scr_pos_x = 532, .scr_pos_y = 350, .pos_x = 532, .pos_y = 350, .width = 26, .height = 14, .draw_call = frontend_draw_slider_button, .tooltip_stridx = GUIStr_Empty, .content = { 18 }, .maintain_call = frontend_define_key_down_maintain },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = frontend_define_key_scroll, .scr_pos_x = 536, .scr_pos_y = 130, .pos_x = 536, .pos_y = 130, .width = 22, .height = 220, .draw_call = frontend_draw_define_key_scroll_tab, .tooltip_stridx = GUIStr_Empty, .content = { 40 } },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_define_key, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(130, 22, 0), .pos_x = 95, .pos_y = FE_ROW_Y(130, 22, 0), .width = 424, .height = 22, .draw_call = frontend_draw_define_key, .tooltip_stridx = GUIStr_Empty, .content = { -1 }, .maintain_call = frontend_define_key_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_define_key, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(130, 22, 1), .pos_x = 95, .pos_y = FE_ROW_Y(130, 22, 1), .width = 424, .height = 22, .draw_call = frontend_draw_define_key, .tooltip_stridx = GUIStr_Empty, .content = { -2 }, .maintain_call = frontend_define_key_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_define_key, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(130, 22, 2), .pos_x = 95, .pos_y = FE_ROW_Y(130, 22, 2), .width = 424, .height = 22, .draw_call = frontend_draw_define_key, .tooltip_stridx = GUIStr_Empty, .content = { -3 }, .maintain_call = frontend_define_key_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_define_key, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(130, 22, 3), .pos_x = 95, .pos_y = FE_ROW_Y(130, 22, 3), .width = 424, .height = 22, .draw_call = frontend_draw_define_key, .tooltip_stridx = GUIStr_Empty, .content = { -4 }, .maintain_call = frontend_define_key_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_define_key, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(130, 22, 4), .pos_x = 95, .pos_y = FE_ROW_Y(130, 22, 4), .width = 424, .height = 22, .draw_call = frontend_draw_define_key, .tooltip_stridx = GUIStr_Empty, .content = { -5 }, .maintain_call = frontend_define_key_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_define_key, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(130, 22, 5), .pos_x = 95, .pos_y = FE_ROW_Y(130, 22, 5), .width = 424, .height = 22, .draw_call = frontend_draw_define_key, .tooltip_stridx = GUIStr_Empty, .content = { -6 }, .maintain_call = frontend_define_key_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_define_key, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(130, 22, 6), .pos_x = 95, .pos_y = FE_ROW_Y(130, 22, 6), .width = 424, .height = 22, .draw_call = frontend_draw_define_key, .tooltip_stridx = GUIStr_Empty, .content = { -7 }, .maintain_call = frontend_define_key_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_define_key, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(130, 22, 7), .pos_x = 95, .pos_y = FE_ROW_Y(130, 22, 7), .width = 424, .height = 22, .draw_call = frontend_draw_define_key, .tooltip_stridx = GUIStr_Empty, .content = { -8 }, .maintain_call = frontend_define_key_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_define_key, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(130, 22, 8), .pos_x = 95, .pos_y = FE_ROW_Y(130, 22, 8), .width = 424, .height = 22, .draw_call = frontend_draw_define_key, .tooltip_stridx = GUIStr_Empty, .content = { -9 }, .maintain_call = frontend_define_key_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_define_key, .ptover_event = frontend_over_button, .scr_pos_x = 95, .scr_pos_y = FE_ROW_Y(130, 22, 9), .pos_x = 95, .pos_y = FE_ROW_Y(130, 22, 9), .width = 424, .height = 22, .draw_call = frontend_draw_define_key, .tooltip_stridx = GUIStr_Empty, .content = { -10 }, .maintain_call = frontend_define_key_maintain },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_change_state, .ptover_event = frontend_over_button, .btype_value = 27, .scr_pos_x = 999, .scr_pos_y = 404, .pos_x = 999, .pos_y = 404, .width = 371, .height = 46, .draw_call = frontend_draw_large_menu_button, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuRetToOptions } },
  { .gbtype = -1 },
};

struct GuiButtonInit frontend_option_buttons[] = {
  { .gbtype = LbBtnT_NormalBtn, .id_num = BID_MENU_TITLE, .scr_pos_x = 999, .scr_pos_y = 30, .pos_x = 999, .pos_y = 30, .width = 371, .height = 46, .draw_call = frontend_draw_large_menu_button, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuOptions } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 95, .scr_pos_y = 107, .pos_x = 95, .pos_y = 107, .width = 220, .height = 26, .draw_call = frontend_draw_scroll_box_tab, .tooltip_stridx = GUIStr_Empty, .content = { 28 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 95, .scr_pos_y = 133, .pos_x = 95, .pos_y = 133, .width = 450, .height = 88, .draw_call = frontend_draw_scroll_box, .tooltip_stridx = GUIStr_Empty, .content = { 89 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 115, .scr_pos_y = 108, .pos_x = 115, .pos_y = 108, .width = 220, .height = 26, .draw_call = frontend_draw_text, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuSoundOptions } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 116, .scr_pos_y = 136, .pos_x = 116, .pos_y = 136, .width = 26, .height = 32, .draw_call = frontend_draw_icon, .sprite_idx = 90, .tooltip_stridx = GUIStr_Empty },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 116, .scr_pos_y = 176, .pos_x = 116, .pos_y = 176, .width = 26, .height = 32, .draw_call = frontend_draw_icon, .sprite_idx = 89, .tooltip_stridx = GUIStr_Empty },
  { .gbtype = LbBtnT_HorizSlider, .id_num = BID_SOUND_VOL, .click_event = gui_set_sound_volume, .scr_pos_x = 144, .scr_pos_y = 147, .pos_x = 144, .pos_y = 147, .width = 180, .height = 22, .draw_call = frontend_draw_slider, .tooltip_stridx = GUIStr_Empty, .maxval = 255 },
  { .gbtype = LbBtnT_HorizSlider, .id_num = BID_MUSIC_VOL, .click_event = gui_set_music_volume, .scr_pos_x = 144, .scr_pos_y = 187, .pos_x = 144, .pos_y = 187, .width = 180, .height = 22, .draw_call = frontend_draw_slider, .tooltip_stridx = GUIStr_Empty, .maxval = 255 },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 346, .scr_pos_y = 136, .pos_x = 346, .pos_y = 136, .width = 26, .height = 32, .draw_call = frontend_draw_icon, .sprite_idx = 95, .tooltip_stridx = GUIStr_Empty },
  { .gbtype = LbBtnT_HorizSlider, .id_num = BID_MENTOR_VOL, .click_event = gui_set_mentor_volume, .scr_pos_x = 364, .scr_pos_y = 147, .pos_x = 364, .pos_y = 147, .width = 180, .height = 22, .draw_call = frontend_draw_slider, .tooltip_stridx = GUIStr_Empty, .maxval = 255 },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 95, .scr_pos_y = 231, .pos_x = 95, .pos_y = 231, .width = 220, .height = 26, .draw_call = frontend_draw_scroll_box_tab, .tooltip_stridx = GUIStr_Empty, .content = { 28 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 95, .scr_pos_y = 257, .pos_x = 95, .pos_y = 257, .width = 450, .height = 88, .draw_call = frontend_draw_scroll_box, .tooltip_stridx = GUIStr_Empty, .content = { 89 } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 115, .scr_pos_y = 232, .pos_x = 115, .pos_y = 232, .width = 220, .height = 26, .draw_call = frontend_draw_text, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MouseOptions } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 102, .scr_pos_y = 271, .pos_x = 102, .pos_y = 271, .width = 190, .height = 26, .draw_call = frontend_draw_text, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_Sensitivity } },
  { .gbtype = LbBtnT_HorizSlider, .id_num = BID_MOUSE_MUL, .click_event = frontend_set_mouse_sensitivity, .scr_pos_x = 304, .scr_pos_y = 271, .pos_x = 304, .pos_y = 271, .width = 190, .height = 22, .draw_call = frontend_draw_small_slider, .tooltip_stridx = GUIStr_Empty, .maxval = 7 },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_invert_mouse, .ptover_event = frontend_over_button, .scr_pos_x = 102, .scr_pos_y = 303, .pos_x = 102, .pos_y = 303, .width = 380, .height = 26, .draw_call = frontend_draw_text, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuInvertMouse } },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 320, .scr_pos_y = 303, .width = 100, .height = 26, .draw_call = frontend_draw_invert_mouse, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuInvertMouse } },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_change_state, .ptover_event = frontend_over_button, .btype_value = 26, .scr_pos_x = 999, .scr_pos_y = 357, .pos_x = 999, .pos_y = 357, .width = 371, .height = 46, .draw_call = frontend_draw_large_menu_button, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_DefineKeys_95 } },
  { .gbtype = LbBtnT_NormalBtn, .click_event = frontend_change_state, .ptover_event = frontend_over_button, .btype_value = 1, .scr_pos_x = 999, .scr_pos_y = 404, .pos_x = 999, .pos_y = 404, .width = 371, .height = 46, .draw_call = frontend_draw_large_menu_button, .tooltip_stridx = GUIStr_Empty, .content = { FEBtn_MnuReturnToMain } },
  { .gbtype = -1 },
};
#pragma GCC diagnostic pop

struct GuiMenu frontend_define_keys_menu =
 {GMnu_FEDEFINE_KEYS, 0, 1, frontend_define_keys_buttons,POS_SCRCTR, POS_SCRCTR, 640, 480, NULL, 0, NULL,    NULL,                    0, 0, 0,};
struct GuiMenu frontend_option_menu =
 {     GMnu_FEOPTION, 0, 1, frontend_option_buttons,     POS_SCRCTR, POS_SCRCTR, 640, 480, NULL, 0, NULL,    frontend_init_options_menu,0,0,0,};

/******************************************************************************/
#ifdef __cplusplus
}
#endif
/******************************************************************************/
