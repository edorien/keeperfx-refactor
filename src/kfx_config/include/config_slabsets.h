/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file config_trapdoor.h
 *     Header file for config_trapdoor.c.
 * @par Purpose:
 *     Traps and doors configuration loading functions.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     25 May 2009 - 21 Dec 2010
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef DK_CFGSLABS_H
#define DK_CFGSLABS_H

#include "globals.h"
#include "bflib_basics.h"

#include "config.h"
#include "config_terrain.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
extern const struct ConfigFileData keeper_slabset_file_data;
extern const struct ConfigFileData keeper_columns_file_data;

// Moved from kfx_sim's map_columns.h (stage 13.3, docs/refactor/
// stage-13-enforce-and-document.md) -- embedded by value in
// struct ColumnConfig below; kfx_config is the lowest-ranked of its real
// consumers (kfx_sim/kfx_render also read it). enum ColumnFlags and
// COLUMN_WALL_HEIGHT stay in map_columns.h since kfx_config doesn't need
// them.
#pragma pack(1)
#define COLUMNS_COUNT          16384
#define COLUMN_STACK_HEIGHT        8
struct Column { // sizeof=0x18
    short use;
    unsigned char bitfields;
    unsigned short solidmask;
    unsigned short floor_texture;
    unsigned char orient;
    unsigned short cubes[COLUMN_STACK_HEIGHT];
};
#pragma pack()

struct ColumnConfig {
    long columns_count;
    struct Column cols[COLUMNS_COUNT];
};

// Moved from kfx_sim's slab_data.h (stage 13.3, docs/refactor/
// stage-13-enforce-and-document.md) -- config_slabsets.c writes these
// by value into kfx_sim's live slabset[]/slabobjs[] arrays while
// parsing slabset.toml/columns.toml; kfx_config is the lowest-ranked
// of their real by-value consumers. The arrays themselves stay
// kfx_sim-owned (kfx_sim_state.h) since map_blocks.c reads them
// pervasively at runtime; config_slabsets.c reaches them via
// ConfigReloadCallbacks pointer accessors.
#define SLABSET_COUNT (TERRAIN_ITEMS_MAX * SLABSETS_PER_SLAB)
#define SLABOBJS_COUNT 1024

#pragma pack(1)
struct SlabSet { // sizeof = 18
  ColumnIndex col_idx[9];
};

struct SlabObj {
  TbBool isLight;
  short slabset_id;
  unsigned char stl_id;
  short offset_x; // position within the subtile
  short offset_y;
  short offset_z;
  ThingClass class_id;
  ThingModel model; //for lights this is intencity
  unsigned char range; //radius for lights / range for effect generators
};
#pragma pack()

void clear_slabsets(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
