/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file matchmaking_config.h
 *     Header file for matchmaking_config.c.
 * @par Purpose:
 *     Callback-registration interface letting config_keeperfx.c apply the
 *     MATCHMAKING_SERVER config var without depending on
 *     net_matchmaking.h directly (that's kfx_net layer, above kfx_config).
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 */
/******************************************************************************/

#ifndef DK_MATCHMAKING_CONFIG_H
#define DK_MATCHMAKING_CONFIG_H

#include "bflib_basics.h"
#include "globals.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
struct MatchmakingConfigCallbacks {
    /* net_matchmaking.h */
    void (*set_enabled)(TbBool enabled);
    void (*set_server)(const char *host);
    const char *(*get_ws_url)(void);
};
void set_matchmaking_config_callbacks(const struct MatchmakingConfigCallbacks *callbacks);
extern const struct MatchmakingConfigCallbacks *matchmaking_config;
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
