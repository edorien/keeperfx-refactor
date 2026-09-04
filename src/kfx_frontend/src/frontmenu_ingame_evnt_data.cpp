/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file frontmenu_ingame_evnt_data.cpp
 *     In-game events GUI, visible during gameplay at bottom.
 * @par Purpose:
 *     Structures to show and maintain message menu appearing ingame.
 * @par Comment:
 *     None.
 * @author   KeeperFX Team
 * @date     05 Jan 2009 - 11 Feb 2013
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "frontmenu_ingame_evnt.h"
#include "frontmenu_ingame_tabs.h"
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
#include "sprites.h"
#include "kfx_frontend_state.h"
#include "post_inc.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
void gui_previous_battle(struct GuiButton *gbtn);
void gui_next_battle(struct GuiButton *gbtn);
void gui_get_creature_in_battle(struct GuiButton *gbtn);
void gui_go_to_person_in_battle(struct GuiButton *gbtn);
void gui_setup_friend_over(struct GuiButton *gbtn);
void gui_area_friendly_battlers(struct GuiButton *gbtn);
void gui_setup_enemy_over(struct GuiButton *gbtn);
void gui_area_enemy_battlers(struct GuiButton *gbtn);
/******************************************************************************/
// GCC's -Wmissing-field-initializers fires on a partially-designated
// GuiButtonInit aggregate in this C++ translation unit even though the
// omitted fields are the struct's own zero defaults; not a real risk here
// since every field is still named where it matters.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
struct GuiButtonInit text_info_buttons[] = {
  { .scr_pos_x = 999, .scr_pos_y = 4, .pos_x = 999, .pos_y = 4, .width = 400, .height = 78, .draw_call = gui_area_scroll_window, .tooltip_stridx = GUIStr_Empty, .content = { .ptr = &kfx_sim_state.evntbox_scroll_window } },
  { .gbtype = 1, .id_num = BID_EVENT_ZOOM, .click_event = gui_go_to_event, .scr_pos_x = 4, .scr_pos_y = 4, .pos_x = 4, .pos_y = 4, .width = 30, .height = 24, .draw_call = gui_area_new_normal_button, .sprite_idx = GPS_message_message_btn_show_act, .tooltip_stridx = GUIStr_ZoomToArea, .maintain_call = maintain_zoom_to_event },
  { .id_num = BID_OBJ_CLOSE, .button_flags = 1, .click_event = gui_close_objective, .rclick_event = gui_close_objective, .scr_pos_x = 4, .scr_pos_y = 56, .pos_x = 4, .pos_y = 56, .width = 30, .height = 24, .draw_call = gui_area_new_normal_button, .sprite_idx = GPS_message_message_btn_accept_act, .tooltip_stridx = GUIStr_CloseWindow },
  { .gbtype = 1, .id_num = BID_OBJ_SCRL_UP, .click_event = gui_scroll_text_up, .scr_pos_x = 446, .scr_pos_y = 4, .pos_x = 446, .pos_y = 4, .width = 30, .height = 24, .draw_call = gui_area_new_normal_button, .sprite_idx = GPS_message_message_btn_up_act, .tooltip_stridx = GUIStr_CtrlUp, .content = { .ptr = &kfx_sim_state.evntbox_scroll_window }, .maintain_call = maintain_scroll_up },
  { .gbtype = 1, .id_num = BID_OBJ_SCRL_DWN, .click_event = gui_scroll_text_down, .scr_pos_x = 446, .scr_pos_y = 56, .pos_x = 446, .pos_y = 56, .width = 30, .height = 24, .draw_call = gui_area_new_normal_button, .sprite_idx = GPS_message_message_btn_down_act, .tooltip_stridx = GUIStr_CtrlDown, .content = { .ptr = &kfx_sim_state.evntbox_scroll_window }, .maintain_call = maintain_scroll_down },
  { .gbtype = -1 },
};

struct GuiButtonInit battle_buttons[] = {
  { .gbtype = LbBtnT_NormalBtn, .button_flags = 1, .click_event = gui_close_objective, .scr_pos_x = 4, .scr_pos_y = 72, .pos_x = 4, .pos_y = 72, .width = 30, .height = 24, .draw_call = gui_area_new_normal_button, .sprite_idx = GPS_message_message_btn_accept_act, .tooltip_stridx = GUIStr_CloseWindow },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = gui_previous_battle, .scr_pos_x = 446, .scr_pos_y = 4, .pos_x = 446, .pos_y = 4, .width = 30, .height = 24, .draw_call = gui_area_new_normal_button, .sprite_idx = GPS_message_message_btn_up_act, .tooltip_stridx = GUIStr_KeyUp },
  { .gbtype = LbBtnT_HoldableBtn, .click_event = gui_next_battle, .scr_pos_x = 446, .scr_pos_y = 72, .pos_x = 446, .pos_y = 72, .width = 30, .height = 24, .draw_call = gui_area_new_normal_button, .sprite_idx = GPS_message_message_btn_down_act, .tooltip_stridx = GUIStr_KeyDown },
  { .gbtype = LbBtnT_NormalBtn, .click_event = gui_get_creature_in_battle, .rclick_event = gui_go_to_person_in_battle, .ptover_event = gui_setup_friend_over, .scr_pos_x = 42, .scr_pos_y = 12, .pos_x = 42, .pos_y = 12, .width = 160, .height = 24, .draw_call = gui_area_friendly_battlers, .tooltip_stridx = GUIStr_Empty },
  { .gbtype = LbBtnT_NormalBtn, .click_event = gui_get_creature_in_battle, .rclick_event = gui_go_to_person_in_battle, .ptover_event = gui_setup_enemy_over, .scr_pos_x = 260, .scr_pos_y = 12, .pos_x = 260, .pos_y = 12, .width = 160, .height = 24, .draw_call = gui_area_enemy_battlers, .tooltip_stridx = GUIStr_Empty },
  { .gbtype = LbBtnT_NormalBtn, .click_event = gui_get_creature_in_battle, .rclick_event = gui_go_to_person_in_battle, .ptover_event = gui_setup_friend_over, .btype_value = 1, .scr_pos_x = 42, .scr_pos_y = 42, .pos_x = 42, .pos_y = 42, .width = 160, .height = 24, .draw_call = gui_area_friendly_battlers, .tooltip_stridx = GUIStr_Empty },
  { .gbtype = LbBtnT_NormalBtn, .click_event = gui_get_creature_in_battle, .rclick_event = gui_go_to_person_in_battle, .ptover_event = gui_setup_enemy_over, .btype_value = 1, .scr_pos_x = 260, .scr_pos_y = 42, .pos_x = 260, .pos_y = 42, .width = 160, .height = 24, .draw_call = gui_area_enemy_battlers, .tooltip_stridx = GUIStr_Empty },
  { .gbtype = LbBtnT_NormalBtn, .click_event = gui_get_creature_in_battle, .rclick_event = gui_go_to_person_in_battle, .ptover_event = gui_setup_friend_over, .btype_value = 2, .scr_pos_x = 42, .scr_pos_y = 72, .pos_x = 42, .pos_y = 72, .width = 160, .height = 24, .draw_call = gui_area_friendly_battlers, .tooltip_stridx = GUIStr_Empty },
  { .gbtype = LbBtnT_NormalBtn, .click_event = gui_get_creature_in_battle, .rclick_event = gui_go_to_person_in_battle, .ptover_event = gui_setup_enemy_over, .btype_value = 2, .scr_pos_x = 260, .scr_pos_y = 72, .pos_x = 260, .pos_y = 72, .width = 160, .height = 24, .draw_call = gui_area_enemy_battlers, .tooltip_stridx = GUIStr_Empty },
  { .gbtype = LbBtnT_NormalBtn, .scr_pos_x = 214, .scr_pos_y = 34, .pos_x = 214, .pos_y = 34, .width = 32, .height = 32, .draw_call = gui_area_null, .sprite_idx = GBS_guisymbols_sym_fight, .tooltip_stridx = GUIStr_Empty },
  { .gbtype = -1 },
};
#pragma GCC diagnostic pop


struct GuiMenu text_info_menu =
// { 16, 0, 4, text_info_buttons,                        160, 316, 480,  86, gui_round_glass_background,  0, NULL,    reset_scroll_window,     0, 0, 0,};
 { GMnu_TEXT_INFO,   0, 4, text_info_buttons,                  160, POS_SCRBTM,480,  86, gui_round_glass_background,  0, NULL,    reset_scroll_window,     0, 0, 0,};
struct GuiMenu battle_menu =
// { 34, 0, 4, battle_buttons,                    160,        300, 480, 102, gui_round_glass_background,  0, NULL,    NULL,                    0, 0, 0,};
 { GMnu_BATTLE,      0, 4, battle_buttons,                    160, POS_SCRBTM, 480, 102, gui_round_glass_background,  0, NULL,    NULL,                    0, 0, 0,};

/******************************************************************************/
#ifdef __cplusplus
}
#endif
/******************************************************************************/
