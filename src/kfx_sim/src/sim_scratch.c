/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file sim_scratch.c
 *     Shared scratch buffer used by simulation-layer algorithms.
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "sim_scratch.h"
#include "post_inc.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
static unsigned char big_scratch_data[1024*1024*16] = {0};
unsigned char *big_scratch = big_scratch_data;
/******************************************************************************/
#ifdef __cplusplus
}
#endif
