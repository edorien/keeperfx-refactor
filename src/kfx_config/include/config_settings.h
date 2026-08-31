/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file config_settings.h
 *     Header file for config_settings.c.
 * @par Purpose:
 *     List of language-specific strings support.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     25 May 2009 - 31 Jul 2009
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef DK_CFGSETTINGS_H
#define DK_CFGSETTINGS_H

#include "globals.h"
#include "bflib_basics.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

struct GameKey {
  unsigned char code;
  unsigned char mods;
  TbControllerButtons controller_buttons;
};

// Moved from kfx_frontend's front_input.h (stage 13.3, docs/refactor/
// stage-13-enforce-and-document.md) -- config_settings.c reads
// game_key_settings[]'s fields directly (default_code/default_mods/
// default_controller_buttons/toml_name) when populating default
// settings; kfx_config is the lowest-ranked real by-value consumer.
// enum BindingMenuVisibility moved down with it (2026-08-29, docs/
// refactor/todo/check-layering-symbol-level-blind-spot.md) -- pure
// name/ID vocabulary, no functional coupling, same shape as
// GameKeys/GAME_KEYS_COUNT's own earlier move to kfx_platform.
enum BindingMenuVisibility {
    BMV_Hidden,
    BMV_KeyMouseOnly,
    BMV_ControllerOnly,
    BMV_Visible,
};

struct GamekeySettings {
    const char* toml_name;
    TextStringId string_id; // For display in the key binding menu
    uint8_t default_code;
    uint8_t default_mods;
    TbControllerButtons default_controller_buttons;
    uint8_t binding_menu_visibility;
};

extern const struct GamekeySettings game_key_settings[GAME_KEYS_COUNT];

struct GameSettings {
    unsigned char video_detail_level;
    unsigned char video_shadows;
    unsigned char view_distance;
    unsigned char video_rotate_mode;
    unsigned char video_textures;
    unsigned char video_cluedo_mode;
    unsigned char sound_volume;
    unsigned char music_volume;
    unsigned char roomflags_on;
    unsigned short gamma_correction;
    int switching_vidmodes_index; /**< The current position in the list of video modes to switch between with Alt+R (-1 means the index is unset). */
    struct GameKey kbkeys[GAME_KEYS_COUNT];
    TbBool tooltips_on;
    unsigned char first_person_move_invert;
    unsigned char first_person_move_sensitivity;
    unsigned int minimap_zoom;
    unsigned long isometric_view_zoom_level;
    unsigned long frontview_zoom_level;
    long mentor_volume;
    int isometric_tilt;
    TbBool highlight_mode;
    };
#pragma pack()
/******************************************************************************/
extern struct GameSettings settings; // KFX settings
/******************************************************************************/
TbBool load_settings(void);
short save_settings(void);
// Had real external linkage but no header declaration at all -- added,
// the usual "add the missing declaration" fix.
void setup_default_settings(void);

int get_max_i_can_see_from_settings(void);
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
