/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file engine_redraw.h
 *     Header file for engine_redraw.c.
 * @par Purpose:
 *     Functions to redraw the engine screen.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     06 Nov 2010 - 03 Jul 2011
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/

#ifndef DK_ENGNRDRAW_H
#define DK_ENGNRDRAW_H

#include "bflib_basics.h"
#include "globals.h"
#include "bflib_video.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

struct PlayerInfo;
struct Camera;
struct Coord3d;

#pragma pack()
/******************************************************************************/
extern unsigned char smooth_on;
/******************************************************************************/
void setup_engine_window(long x1, long y1, long x2, long y2);
void store_engine_window(TbGraphicsWindow *ewnd,int divider);
void load_engine_window(TbGraphicsWindow *ewnd);

void set_engine_view(struct PlayerInfo *player, long val);

void draw_overlay_compass(long a1, long a2);

TbBool keeper_screen_redraw(void);
void smooth_screen_area(TbPixel *a1, long a2, long a3, long a4, long a5, long a6);

int get_place_terrain_pointer_graphics(SlabKind skind);

TbBool players_cursor_is_at_top_of_view(void);
TbBool engine_point_to_map(struct Camera *camera, long screen_x, long screen_y, int32_t *map_x, int32_t *map_y);
TbBool screen_to_map(struct Camera *camera, int32_t screen_x, int32_t screen_y, struct Coord3d *mappos);
void update_local_mouse_light(void);
void update_mouse_light(struct PlayerInfo *player);
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
