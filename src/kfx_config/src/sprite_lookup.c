/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file sprite_lookup.c
 *     Callback-registration implementation. See sprite_lookup.h.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "sprite_lookup.h"
#include "post_inc.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
static short noop_get_icon_id(const char *name) { return 0; }
static short noop_get_anim_id(const char *name, struct ObjectConfigStats *objst) { return 0; }
static short noop_get_anim_id_(const char *name) { return 0; }
static const struct TbSprite *noop_get_button_sprite(short sprite_idx) { return NULL; }
static const struct TbSprite *noop_get_panel_sprite(short sprite_idx) { return NULL; }

static const struct SpriteLookupCallbacks default_sprite_lookup_callbacks = {
    &noop_get_icon_id,
    &noop_get_anim_id,
    &noop_get_anim_id_,
    &noop_get_button_sprite,
    &noop_get_panel_sprite,
};

const struct SpriteLookupCallbacks *sprite_lookup = &default_sprite_lookup_callbacks;

void set_sprite_lookup_callbacks(const struct SpriteLookupCallbacks *callbacks)
{
    sprite_lookup = callbacks ? callbacks : &default_sprite_lookup_callbacks;
}
/******************************************************************************/
#ifdef __cplusplus
}
#endif
