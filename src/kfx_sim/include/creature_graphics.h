/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file creature_graphics.h
 *     Header file for creature_graphics.c.
 * @par Purpose:
 *     Creature graphics support functions.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     11 Mar 2010 - 23 May 2010
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef DK_CRTRGRAPHICS_H
#define DK_CRTRGRAPHICS_H

#include "globals.h"
#include "bflib_basics.h"

#ifdef __cplusplus
extern "C" {
#endif

// note - this is temporary value; not correct
#define CREATURE_FRAMELIST_LENGTH    982
#define CREATURE_GRAPHICS_INSTANCES   25

// enum CreatureGraphicsInstances moved to globals.h (stage 13.3) -- see there.
/******************************************************************************/
#pragma pack(1)

struct Thing;

/**
 * Enhanced TbSprite structure, with additional fields for thing animation sprites.
 */
enum FrameFlags {
    FFL_NoShadows = 1,
};

struct KeeperSprite {
  uint32_t DataOffset;

  unsigned short SWidth;
  unsigned short SHeight;
  unsigned short FrameWidth;
  unsigned short FrameHeight;
  unsigned char Rotable;
  unsigned char FramesCount;
  short FrameOffsW;
  short FrameOffsH;

  short offset_x;
  short offset_y;

  short shadow_offset;
  short frame_flags;
};

struct KeeperSpriteDisk {
    uint32_t DataOffset;
    unsigned char SWidth;
    unsigned char SHeight;
    unsigned char FrameWidth;
    unsigned char FrameHeight;
    unsigned char Rotable;
    unsigned char FramesCount;
    unsigned char FrameOffsW;
    unsigned char FrameOffsH;
    short offset_x;
    short offset_y;
};

/******************************************************************************/
//extern unsigned short creature_graphics[][22];
extern struct KeeperSprite *creature_table;
extern struct KeeperSprite creature_table_add[];
/******************************************************************************/

#pragma pack()
/******************************************************************************/
struct PickedUpOffset *get_creature_picked_up_offset(struct Thing *thing);

unsigned long keepersprite_index(unsigned short n);
struct KeeperSprite * keepersprite_array(unsigned short n);
unsigned char keepersprite_frames(unsigned short n); // This returns number of frames in animation
unsigned char keepersprite_rotable(unsigned short n);
void get_keepsprite_unscaled_dimensions(long kspr_anim, long angle, long frame, short *orig_w, short *orig_h, short *unsc_w, short *unsc_h);
long get_lifespan_of_animation(long ani, long speed);
short get_creature_anim(struct Thing *thing, unsigned short frame);
short get_creature_model_graphics(long crmodel, unsigned short frame);
// set_creature_model_graphics moved to kfx_config's config_creature.h (stage 13.3).
void set_creature_graphic(struct Thing *thing);
void update_creature_rendering_flags(struct Thing *thing);

size_t creature_table_load_get_size(size_t disk_size);
void creature_table_load_unpack(unsigned char *src, size_t disk_size);

void init_censorship(void);
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
