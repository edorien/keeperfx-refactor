/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file frontmenu_ingame_tabs.h
 *     Header file for frontmenu_ingame_tabs.c.
 * @par Purpose:
 *     Main in-game GUI, visible during gameplay.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   KeeperFX Team
 * @date     05 Jan 2009 - 03 Jan 2011
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef DK_FRONTMENU_INGAMETABS_H
#define DK_FRONTMENU_INGAMETABS_H

#include "globals.h"

#include "bflib_guibtns.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

struct GuiMenu;
struct GuiButton;

// enum IngameButtonGroupIDs moved to globals.h (stage 9,
// docs/refactor/stage-09-kfx-game.md).

/******************************************************************************/
extern int32_t activity_list[24];
// gui_room_type_highlighted/gui_door_type_highlighted moved to
// gui_draw.h (stage 10, docs/refactor/stage-10-kfx-frontend.md) --
// gui_parchment.c (kfx_frontend's lower internal sub-layer) needed them
// without depending on frontmenu_ingame_tabs.h.
extern char gui_trap_type_highlighted;
extern char gui_creature_type_highlighted;
extern unsigned long first_person_instance_top_half_selected;

#pragma pack()
/******************************************************************************/
extern struct GuiMenu main_menu;
extern struct GuiMenu room_menu;
extern struct GuiMenu spell_menu;
extern struct GuiMenu spell_lost_menu;
extern struct GuiMenu trap_menu;
extern struct GuiMenu creature_menu;
extern struct GuiMenu event_menu;
extern struct GuiMenu query_menu;
extern struct GuiMenu creature_query_menu1;
extern struct GuiMenu creature_query_menu2;
extern struct GuiMenu creature_query_menu3;
extern struct GuiMenu creature_query_menu4;
extern struct TiledSprite status_panel;
extern struct GuiMenu spell_menu2;
extern struct GuiMenu room_menu2;
extern struct GuiMenu trap_menu2;

// AROUND_2x2_PIXEL..AROUND_6x6_PIXEL,
// ONE_PIXEL/TWO_PIXELS/THREE_PIXELS/FOUR_PIXELS, pixels_needed[],
// scale_pixel(), get_pixels_scaled_and_zoomed(), draw_square[] moved to
// gui_draw.h (stage 10, docs/refactor/stage-10-kfx-frontend.md) --
// gui_parchment.c (kfx_frontend's lower internal sub-layer) needed them
// without depending on frontmenu_ingame_tabs.h.

/******************************************************************************/
void gui_zoom_in(struct GuiButton *gbtn);
void gui_zoom_out(struct GuiButton *gbtn);
void draw_whole_status_panel(void);
void gui_set_button_flashing(long btn_idx, long gameturns);
short button_designation_to_tab_designation(short btn_designt_id);
short get_button_designation(short btn_group, short btn_item);
void draw_placefiller(long scr_x, long scr_y, long units_per_px);

void gui_over_creature_button(struct GuiButton* gbtn);

void update_room_tab_to_config(void);
void update_trap_tab_to_config(void);
void update_powers_tab_to_config(void);

void go_to_my_next_room_of_type_and_select(RoomKind rkind);
void go_to_my_next_room_of_type(RoomKind rkind);
RoomIndex find_my_next_room_of_type(RoomKind rkind);
RoomIndex find_next_room_of_type(PlayerNumber plyr_idx, RoomKind rkind);

void gui_query_next_creature_of_owner_and_model(struct GuiButton *gbtn);
void gui_query_next_creature_of_owner(struct GuiButton *gbtn);

void maintain_spell_next_page_button(struct GuiButton *gbtn);
void maintain_room_next_page_button(struct GuiButton *gbtn);
void maintain_trap_next_page_button(struct GuiButton *gbtn);
void gui_switch_players_visible(struct GuiButton* gbtn);

void go_to_adjacent_menu_tab(int direction);

void update_creatr_model_activities_list(TbBool forced);
void instant_instance_selected(CrInstance check_inst_id);
void draw_gold_total(PlayerNumber plyr_idx, int32_t scr_x, int32_t scr_y, int32_t units_per_px, long long value);
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
