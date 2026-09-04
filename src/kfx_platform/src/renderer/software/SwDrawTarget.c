/******************************************************************************/
// Dungeon Keeper - Renderer Abstraction Layer
/******************************************************************************/
/** @file SwDrawTarget.c
 *     Where the software raster draws.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "renderer/software/SwDrawTarget.h"
#include "bflib_vidraw.h"   /* vec_screen, poly_screen, vec_map, vec_screen_width, vec_window_* */
#include "post_inc.h"

/******************************************************************************/

TbPixel* SwTargetWScreen(void)           { return lbDisplay.WScreen; }
TbPixel* SwTargetGraphicsWindowPtr(void) { return lbDisplay.GraphicsWindowPtr; }
int32_t SwTargetScanline(void)              { return (int32_t)lbDisplay.GraphicsScreenWidth; }
int32_t SwTargetScreenHeight(void)          { return (int32_t)lbDisplay.GraphicsScreenHeight; }

int32_t SwTargetWindowX(void)      { return (int32_t)lbDisplay.GraphicsWindowX; }
int32_t SwTargetWindowY(void)      { return (int32_t)lbDisplay.GraphicsWindowY; }
int32_t SwTargetWindowWidth(void)  { return (int32_t)lbDisplay.GraphicsWindowWidth; }
int32_t SwTargetWindowHeight(void) { return (int32_t)lbDisplay.GraphicsWindowHeight; }

TbPixel* SwTargetVecScreen(void)        { return vec_screen; }
TbPixel* SwTargetPolyScreen(void)       { return poly_screen; }
const unsigned char* SwTargetVecMap(void) { return vec_map; }
unsigned long SwTargetVecScreenWidth(void) { return vec_screen_width; }
long SwTargetVecWindowWidth(void)       { return vec_window_width; }
long SwTargetVecWindowHeight(void)      { return vec_window_height; }
