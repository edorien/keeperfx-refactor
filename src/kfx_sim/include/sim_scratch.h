/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file sim_scratch.h
 *     Header file for sim_scratch.c.
 * @par Purpose:
 *     Shared scratch buffer used by simulation-layer algorithms (pathing/
 *     dig-area/treasure-vein scans etc.) as generic working memory.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef DK_SIM_SCRATCH_H
#define DK_SIM_SCRATCH_H

#include "bflib_basics.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
extern unsigned char *big_scratch; // 16 Mb
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
