/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file gui_draw.h
 *     Header file for gui_draw.c.
 * @par Purpose:
 *     GUI elements drawing functions.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     20 Jan 2009 - 30 Jan 2009
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/

#ifndef DK_GUIDRAW_H
#define DK_GUIDRAW_H

#include "bflib_basics.h"
#include "bflib_video.h"
#include "bflib_sprite.h"
#include "globals.h"
#include "vidmode.h"

// Sprites
// Maybe "Count + 1"? there is no sprite#517
#define GUI_SLAB_DIMENSION 64
// Positioning constants for menus
#define POS_AUTO -9999
#define POS_MOUSMID -999
#define POS_MOUSPRV -998
#define POS_SCRCTR  -997
#define POS_SCRBTM  -996
#define POS_GAMECTR  999
#define ROUNDSLAB64K_LIGHT 0
#define ROUNDSLAB64K_DARK 1

// Moved here from frontmenu_ingame_tabs.h (stage 10,
// docs/refactor/stage-10-kfx-frontend.md) -- gui_parchment.c
// (kfx_frontend's lower internal sub-layer) needed the pixel-scaling
// helpers without depending on frontmenu_ingame_tabs.h.
#define AROUND_2x2_PIXEL      4
#define AROUND_3x3_PIXEL      9
#define AROUND_4x4_PIXEL      16
#define AROUND_5x5_PIXEL      25
#define AROUND_6x6_PIXEL      36

#define ONE_PIXEL       2048
#define TWO_PIXELS      1024
#define THREE_PIXELS     512
#define FOUR_PIXELS      256
#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

struct GuiButton;
/******************************************************************************/
// gui_panel_sprites/frontend_sprite/gui_slab moved to kfx_render's
// vidmode.h (stage 13.3, docs/refactor/stage-13-enforce-and-document.md).
extern unsigned char *frontend_background;
extern int gui_blink_rate;
extern int neutral_flash_rate;
// Moved here from frontmenu_ingame_tabs.h (stage 10,
// docs/refactor/stage-10-kfx-frontend.md).
extern char gui_room_type_highlighted;
extern char gui_door_type_highlighted;

#pragma pack()
/******************************************************************************/
extern char gui_textbuf[TEXT_BUFFER_LENGTH];
extern const short pixels_needed[];
// draw_square moved to kfx_sim's power_hand.h (stage 13.3, docs/refactor/
// stage-13-enforce-and-document.md).
/******************************************************************************/
short get_pixels_scaled_and_zoomed(long basic_zoom);
short scale_pixel(long basic_zoom);
int simple_button_sprite_height_units_per_px(const struct GuiButton *gbtn, long spridx, int fraction);
int simple_button_sprite_width_units_per_px(const struct GuiButton *gbtn, long spridx, int fraction);
int simple_frontend_sprite_height_units_per_px(const struct GuiButton *gbtn, long spridx, int fraction);
int simple_frontend_sprite_width_units_per_px(const struct GuiButton *gbtn, long spridx, int fraction);
int simple_gui_panel_sprite_height_units_per_px(const struct GuiButton *gbtn, long spridx, int fraction);
int simple_gui_panel_sprite_width_units_per_px(const struct GuiButton *gbtn, long spridx, int fraction);

// Moved here from front_simple.h (stage 10,
// docs/refactor/stage-10-kfx-frontend.md).
TbBool copy_raw8_image_buffer(TbPixel *dst_buf,const int scanline,const int nlines,const int dst_width,const int dst_height,
    const int spw,const int sph,const unsigned char *src_buf,const int src_width,const int src_height);

void draw_bar64k(long pos_x, long pos_y, int units_per_px, long width);
void draw_lit_bar64k(long pos_x, long pos_y, int units_per_px, long width);
void draw_slab64k_background(long pos_x, long pos_y, long width, long height);
/** The tiling itself; draw_slab64k_background routes through the renderer first. */
void draw_slab64k_background_immediate(long pos_x, long pos_y, long width, long height);
void draw_slab64k(long pos_x, long pos_y, int units_per_px, long width, long height);
void draw_ornate_slab64k(long pos_x, long pos_y, int units_per_px, long width, long height);
void draw_ornate_slab_outline64k(long pos_x, long pos_y, int units_per_px, long width, long height);
void draw_round_slab64k(long pos_x, long pos_y, int units_per_px, long width, long height, long style_type);
void draw_string64k(long x, long y, int units_per_px, const char * text);

void draw_button_string(struct GuiButton *gbtn, int base_width, const char *text);
TbBool draw_text_box(const char *text);
TbBool draw_text_box_top(const char* text, ushort drawflags);
void draw_scroll_box(struct GuiButton *gbtn, int units_per_px, int num_rows);
int scroll_box_get_units_per_px(struct GuiButton *gbtn);

#define draw_gui_panel_sprite_left(x, y, units_per_px, spridx) draw_gui_panel_sprite_left_player(x, y, units_per_px, spridx, my_player_number)
void draw_gui_panel_sprite_left_player(long x, long y, int units_per_px, long spridx, PlayerNumber plyr_idx);
#define draw_gui_panel_sprite_rmleft(x, y, units_per_px, spridx, remap) draw_gui_panel_sprite_rmleft_player(x, y, units_per_px, spridx, remap, my_player_number)
void draw_gui_panel_sprite_rmleft_player(long x, long y, int units_per_px, long spridx, unsigned long remap, PlayerNumber plyr_idx);
void draw_gui_panel_sprite_centered(long x, long y, int units_per_px, long spridx);
void draw_gui_panel_sprite_occentered(long x, long y, int units_per_px, long spridx, TbPixel color);
void draw_button_sprite_left(long x, long y, int units_per_px, long spridx);
void draw_button_sprite_rmleft(long x, long y, int units_per_px, long spridx, unsigned long remap);

void draw_frontend_sprite_left(long x, long y, int units_per_px, long spridx);

void draw_frontmenu_background(int rect_x,int rect_y,int rect_w,int rect_h);
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
