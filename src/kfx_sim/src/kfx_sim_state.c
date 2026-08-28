/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file kfx_sim_state.c
 *     Global instance for kfx_sim_state.h.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "kfx_sim_state.h"
#include "post_inc.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
struct KfxSimState kfx_sim_state;

// See the doc comment at these globals' declaration in kfx_sim_state.h --
// kept outside struct KfxSimState so clear_complete_game()'s per-session
// memset doesn't wipe them.
unsigned char *engine_palette;
unsigned char *blue_palette;
unsigned char *lightning_palette;
unsigned char EngineSpriteDrawUsingAlpha;
/******************************************************************************/
#ifdef __cplusplus
}
#endif
