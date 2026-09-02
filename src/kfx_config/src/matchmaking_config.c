/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file matchmaking_config.c
 *     Callback-registration implementation. See matchmaking_config.h.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "matchmaking_config.h"
#include "post_inc.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
static void noop_set_enabled(TbBool enabled) {}
static void noop_set_server(const char *host) {}
static const char *noop_get_ws_url(void) { return ""; }

static const struct MatchmakingConfigCallbacks default_matchmaking_config_callbacks = {
    &noop_set_enabled,
    &noop_set_server,
    &noop_get_ws_url,
};

const struct MatchmakingConfigCallbacks *matchmaking_config = &default_matchmaking_config_callbacks;

void set_matchmaking_config_callbacks(const struct MatchmakingConfigCallbacks *callbacks)
{
    matchmaking_config = callbacks ? callbacks : &default_matchmaking_config_callbacks;
}
/******************************************************************************/
#ifdef __cplusplus
}
#endif
