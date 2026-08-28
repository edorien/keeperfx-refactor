/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file light_data.h
 *     Header file for light_data.c.
 * @par Purpose:
 *     light_data functions.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     11 Mar 2010 - 12 May 2010
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef DK_LIGHT_DATA_H
#define DK_LIGHT_DATA_H

#include "globals.h"
#include "bflib_basics.h"
#include "config.h"

#define LIGHT_MAX_RANGE       256 // Large enough to cover the whole map
#define LIGHTS_COUNT         2048

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
#pragma pack(1)

struct StructureList;

enum ShadowCacheFlags {
    ShCF_Allocated = 0x01,
};

enum LightFlags {
    LgtF_Allocated    = 0x01,
    LgtF_CanTurnOff   = 0x02,
    LgtF_Dynamic      = 0x04,
    LgtF_NeedUpdate   = 0x08,
    LgtF_RadiusOscillation = 0x10,
    LgtF_IntensityAnimation = 0x20,
    LgtF_NeverCached  = 0x40,
    LgtF_OutOfDate    = 0x80,
};

enum LightFlags2 {
    LgtF2_InList    = 0x01,
};

struct Light {
  unsigned char flags;
  unsigned char flags2;
  unsigned char intensity;
  unsigned char intensity_toggling_field;//toggles between 1 and 2 when flags has LgtF_IntensityAnimation
  unsigned char intensity_delta;//seems never assigned
  unsigned char range;
  unsigned char radius_oscillation_direction;
  unsigned char max_intensity;//seems never assigned
  unsigned char min_radius;
  unsigned short index;
  unsigned short shadow_index;
  SlabCodedCoords attached_slb;
  unsigned short radius;
  unsigned short force_render_update;
  unsigned short radius_delta;//seems never assigned
  unsigned short max_radius;//seems never assigned
  unsigned short min_radius2;//seems never assigned
  unsigned short min_intensity;
  unsigned short next_in_list;
  struct Coord3d mappos;
  struct Coord3d previous_mappos;
  uint16_t intensity_random;
  uint16_t previous_intensity_random;
  GameTurn last_turn_moved;
  GameTurn last_turn_randomized;
  TbBool reset_interpolation;
};

// struct InitLight moved to kfx_config_state.h (stage 13.3, docs/refactor/
// stage-13-enforce-and-document.md) -- embedded by value in
// EffectConfigStats/ObjectConfigStats (kfx_config's config_effects.h/
// config_objects.h), and read by kfx_sim too; kfx_config is the
// lowest-ranked of its real consumers.

struct LightSystemState {
    int32_t bitmask[32];
    int32_t static_light_needs_updating;
    int32_t total_dynamic_lights;
    int32_t total_stat_lights;
    int32_t rendered_dynamic_lights;
    int32_t rendered_optimised_dynamic_lights;
    int32_t updated_stat_lights;
    int32_t out_of_date_stat_lights;
};

// struct LightsShadows/LightingTable/ShadowCache and the lish global
// moved here from game_legacy.h/game_lghtshdw.h (stage 13.3, docs/
// refactor/stage-13-enforce-and-document.md) -- lish was struct Game's
// last remaining field; engine_render.c/light_data.c are its actual
// functional owners (hundreds of accesses each), and struct LightsShadows
// already embedded struct Light (this file) by value, so kfx_render was
// always its true home rather than kfx_game. kfx_sim's narrow accesses
// (map_blocks.c/map_data.c/thing_list.c) are routed through
// RenderOverlayCallbacks/ConfigReloadCallbacks; kfx_net's raw-blob need
// (net_resync.cpp) is a legal downward reference now.
#define SHADOW_LIMITS_COUNT  2048
#define SHADOW_CACHE_COUNT    512

struct LightingTable { // sizeof = 8
  TbBool is_populated;
  unsigned char distance; // 2 - 15
  char delta_x; // signed
  char delta_y; // signed
  uint32_t diagonal_length;
};

struct ShadowCache { // sizeof = 129
  unsigned char flags;
  unsigned int lighting_bitmask[32];
};

/**
 * Structure which stores data of lights and shadows system.
 */
struct LightsShadows {
    struct LightingTable lighting_tables[1024]; // only the first 700 elements are populated
    unsigned char shadow_limits[SHADOW_LIMITS_COUNT];
    struct Light lights[LIGHTS_COUNT];
    struct ShadowCache shadow_cache[SHADOW_CACHE_COUNT];
    unsigned short stat_light_map[MAX_SUBTILES_X*MAX_SUBTILES_Y];
    int32_t global_ambient_light;
    TbBool light_enabled;
    TbBool light_auto_sync;
    TbBool lighting_tables_initialised;
    int lighting_tables_count; // number of entries in lighting_tables
    unsigned short subtile_lightness[MAX_SUBTILES_X*MAX_SUBTILES_Y];
};

extern struct LightsShadows lish;

/******************************************************************************/

#pragma pack()

typedef struct VALUE VALUE;

/******************************************************************************/
void clear_stat_light_map(void);
void update_light_render_area(void);
void light_delete_light(long idx);
void light_initialise(void);
void light_turn_light_off(long num);
void light_turn_light_on(long num);
unsigned char light_get_light_intensity(long idx);
void light_set_light_intensity(long idx, unsigned char intensity);
unsigned short light_get_light_radius(long idx);
void light_set_light_radius(long idx, unsigned short radius);
long light_create_light(struct InitLight *ilght);
TbBool light_create_light_adv(VALUE *init_data);
void light_set_light_never_cache(long lgt_id);
TbBool light_is_invalid(const struct Light *lgt);
long light_is_light_allocated(long lgt_id);
void light_set_light_position(long lgt_id, struct Coord3d *pos);
void light_reset_interpolation(long lgt_id);
void light_stat_refresh();
void update_global_lighting(void);
void light_set_lights_on(char state);
void light_init_dungeon_heart(long lgt_id, long radius, long intensity);
void light_signal_update_in_area(long sx, long sy, long ex, long ey);
void light_export_system_state(struct LightSystemState *lightst);
void light_import_system_state(const struct LightSystemState *lightst);
TbBool lights_stats_debug_dump(void);
void light_signal_stat_light_update_in_area(long x1, long y1, long x2, long y2);

int light_count_lights();

// Moved from game_lghtshdw.h (stage 13.3) alongside struct LightsShadows.
long get_subtile_lightness(const struct LightsShadows * lish, MapSubtlCoord stl_x, MapSubtlCoord stl_y);
void clear_subtiles_lightness(struct LightsShadows * lish);
void create_shadow_limits(struct LightsShadows * lish, long start, long end);
void clear_shadow_limits(struct LightsShadows * lish);
void clear_light_system(struct LightsShadows * lish);

// Registered on RenderOverlayCallbacks (src/kfx_config/include/
// render_overlay.h) so kfx_sim's map_blocks.c/thing_list.c don't need
// struct Light/struct LightsShadows visible by value to touch a single
// light's attached_slb or the light_enabled flag.
void light_set_attached_slab(long lgt_id, SlabCodedCoords slb_num);
void delete_lights_attached_to_slab_in_area(SlabCodedCoords place_slbnum,
    MapSubtlCoord start_stl_x, MapSubtlCoord start_stl_y,
    MapSubtlCoord end_stl_x, MapSubtlCoord end_stl_y);
TbBool light_get_lights_enabled(void);
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
