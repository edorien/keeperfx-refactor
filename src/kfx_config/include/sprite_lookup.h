/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file sprite_lookup.h
 *     Header file for sprite_lookup.c.
 * @par Purpose:
 *     Callback-registration interface letting kfx_config resolve
 *     sprite/icon/animation name strings (parsed from TOML config
 *     files) to numeric IDs, and look up button/panel sprites by
 *     index, without depending on custom_sprites.h directly (that's
 *     kfx_render layer, above kfx_config). See
 *     docs/refactor/stage-13-enforce-and-document.md.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 */
/******************************************************************************/

#ifndef DK_SPRITE_LOOKUP_H
#define DK_SPRITE_LOOKUP_H

#include "bflib_basics.h"
#include "globals.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
struct ObjectConfigStats;
struct TbSprite;

struct SpriteLookupCallbacks {
    short (*get_icon_id)(const char *name);
    short (*get_anim_id)(const char *name, struct ObjectConfigStats *objst);
    short (*get_anim_id_)(const char *name);
    const struct TbSprite *(*get_button_sprite)(short sprite_idx);
    const struct TbSprite *(*get_panel_sprite)(short sprite_idx);
    // custom_sprites.h -- config_campaigns.c resolves a campaign's
    // SET_LEVEL_ENSIGN-style config text to a custom-ensign slot, and
    // triggers loading that campaign's custom ensign sprite sheet.
    short (*get_ensign_id)(const char *name);
    void (*init_custom_campaign_sprites)(const char *dir_path, const char *dir_desc);
};
void set_sprite_lookup_callbacks(const struct SpriteLookupCallbacks *callbacks);
extern const struct SpriteLookupCallbacks *sprite_lookup;
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
