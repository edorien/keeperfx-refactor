/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file kfx_frontend_state.c
 *     Global instance for kfx_frontend_state.h.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "kfx_frontend_state.h"
#include "bflib_fileio.h"
#include <string.h>
#include "post_inc.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
struct KfxFrontendState kfx_frontend_state;

// Registered on GameCallbacks (src/kfx_config/include/game_callbacks.h)
// so kfx_game's game_saves.c/main_game.c don't need to reach up into
// kfx_frontend_state.h directly to save/load/reset this struct as a
// raw blob.
TbBool save_frontend_state(TbFileHandle fhandle)
{
    return LbFileWrite(fhandle, &kfx_frontend_state, sizeof(struct KfxFrontendState)) == sizeof(struct KfxFrontendState);
}

TbBool load_frontend_state(TbFileHandle fhandle)
{
    return LbFileRead(fhandle, &kfx_frontend_state, sizeof(struct KfxFrontendState)) == sizeof(struct KfxFrontendState);
}

void reset_frontend_state(void)
{
    memset(&kfx_frontend_state, 0, sizeof(struct KfxFrontendState));
}

size_t get_frontend_state_size(void)
{
    return sizeof(struct KfxFrontendState);
}
/******************************************************************************/
#ifdef __cplusplus
}
#endif
