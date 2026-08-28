/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file config.h
 *     Header file for config.c.
 * @par Purpose:
 *     Configuration and campaign files support.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     30 Jan 2009 - 11 Feb 2009
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/

#ifndef DK_CONFIG_KEEPERFX_H
#define DK_CONFIG_KEEPERFX_H

#include "bflib_basics.h"
#include "globals.h"
#include "config.h"

#ifdef FUNCTESTING
#include "ftests/ftest.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
struct CreditsItem;
struct GameCampaign;
/******************************************************************************/

// Max length of the command line
#define CMDLN_MAXLEN 259
#define CMDLINE_OVERRIDES 4

/** Command Line overrides for config settings. Checked after the config file is loaded. */
enum CmdLineOverrides {
    Clo_ConfigFile = 0, /**< Special: handled before the config file is loaded. */
    Clo_CDMusic,
    Clo_GameTurns,
    Clo_FramesPerSecond,
};

// Used by both StartupParameters.mode_flags (below) and struct Game's
// mode_flags field (kfx_game/game_legacy.h).
enum ModeFlags {
    MFlg_IsDemoMode         =  0x01,
    MFlg_EyeLensReady       =  0x02,
    MFlg_Unusedparam04      =  0x04,
    MFlg_DeadBackToPool     =  0x08,
    MFlg_NoCdMusic          =  0x10, // unused
    MFlg_Unusedparam20      =  0x20,
    MFlg_DemoMode           =  0x40,
    MFlg_NoHeroHealthFlower =  0x80,
};

// Used by StartupParameters.debug_flags (below).
enum DebugFlags {
    DFlg_ShotsDamage        =  0x01,
    DFlg_CreatrPaths        =  0x02,
    DFlg_ShowGameTurns      =  0x04,
    DFlg_FrameStep          =  0x08,
    DFlg_PauseAtGameTurn    =  0x10,
};

#ifdef FUNCTESTING
// Used by StartupParameters.functest_flags (below).
enum FunctestFlags {
    FTF_Enabled             = 0x01, // Functional Tests Enabled
    FTF_TestFailed          = 0x02, // Test failed, causes exit code -1 for cmd line automation
    FTF_Abort               = 0x04, // Something went wrong, aborting
    FTF_LevelLoaded         = 0x08, // For tracking if map is ready
    FTF_ExitOnTestFailure   = 0x10, // If users want to exit on any test failure
    FTF_IncludeLongTests    = 0x20, // If users want to run the long running test list
};
#endif

#pragma pack(1)
struct StartupParameters {
    LevelNumber selected_level_number;
    TbBool no_intro;
    TbBool one_player;
    TbBool easter_egg;
    TbBool ignore_mods;
    unsigned char operation_flags;
    unsigned char flags_font;
    unsigned char mode_flags;
    unsigned char debug_flags;
    unsigned short computer_chat_flags;
    long num_fps;
    int32_t num_fps_draw_main; // -1 if auto
    int32_t num_fps_draw_secondary;
    TbBool packet_save_enable;
    TbBool packet_load_enable;
    char packet_fname[150];
    unsigned char packet_checksum_verify;
    int frame_skip;
    char selected_campaign[CMDLN_MAXLEN+1];
    TbBool overrides[CMDLINE_OVERRIDES];
    char config_file[CMDLN_MAXLEN+1];
    GameTurn pause_at_gameturn;
    unsigned char startup_flags;
#ifdef FUNCTESTING
    unsigned char functest_flags;
    char functest_name[FTEST_MAX_NAME_LENGTH];
    unsigned int functest_seed;
#endif
};
#pragma pack()

extern struct StartupParameters start_params;
/******************************************************************************/


enum TbFeature {
    Ft_EyeLens      =  0x0001,
    Ft_HiResVideo   =  0x0002,
    Ft_BigPointer   =  0x0004,
    Ft_HiResCreatr  =  0x0008,
    Ft_AdvAmbSound  =  0x0010,
    Ft_Censorship   =  0x0020,
    Ft_Atmossounds  =  0x0040,
    Ft_Resizemovies =  0x0080,
    Ft_FreezeOnLoseFocus            = 0x0400,
    Ft_UnlockCursorOnPause          = 0x0800,
    Ft_LockCursorInPossession       = 0x1000,
    Ft_PauseMusicOnGamePause        = 0x2000,
    Ft_MuteAudioOnLoseFocus         = 0x4000,
    Ft_SkipHeartZoom                = 0x8000,
    Ft_SkipSplashScreens            = 0x10000, // no longer used
    Ft_DisableCursorCameraPanning   = 0x20000,
    Ft_DeltaTime                    = 0x40000,
    Ft_NoCdMusic                    = 0x80000,
    Ft_RelativeMouseMode            = 0x100000,
};

// enum TbLanguage moved to globals.h (stage 13.3) -- see there.

enum StartupFlags {
    SFlg_Legal        =  0x01,
    SFlg_FX           =  0x02,
    SFlg_Bullfrog     =  0x04,
    SFlg_EA           =  0x08,
    SFlg_Intro        =  0x10,
};


#pragma pack(1)


/******************************************************************************/

struct InstallInfo {
  char inst_path[150];
  int lang_id;
};

extern unsigned short AtmosRepeat;
extern unsigned short AtmosStart;
extern unsigned short AtmosEnd;
extern TbBool AssignCpuKeepers;

extern unsigned int vid_scale_flags;

// UI/render tuning values resolved from config file commands, applied by
// their owning modules (engine_render.c/frontend.c/front_input.c/
// gui_draw.c) at startup instead of being written here directly, so this
// file doesn't need their headers. Defaults mirror each target global's
// own default. zoom_to_mouse_option/rotate_around_mouse_option are plain
// int (not front_input.h's enum types) for the same reason. See
// docs/refactor/stage-04-kfx-config.md issue B.
struct KeeperFxUiConfig {
    int gui_blink_rate;
    int neutral_flash_rate;
    int creature_status_size;
    int line_box_size;
    TbBool right_click_tag_mode_toggle;
    unsigned char default_tag_mode;
    int zoom_to_mouse_option;
    int rotate_around_mouse_option;
};
extern struct KeeperFxUiConfig keeperfx_ui_config;

// Registered once at startup so freeze_game_on_focus_lost() doesn't need
// game_legacy.h's network_is_active() directly. Queried live since
// network state changes during a session.
typedef TbBool (*NetworkIsActiveFn)(void);
void set_config_network_is_active_check(NetworkIsActiveFn fn);
TbBool config_network_is_active(void);
/******************************************************************************/
extern struct InstallInfo install_info;
extern char keeper_runtime_directory[152];

#pragma pack()
/******************************************************************************/
extern unsigned long features_enabled;
extern const struct NamedCommand lang_type[];
extern const struct NamedCommand scrshot_type[];
extern char cmd_char;
extern short api_enabled;
extern uint16_t api_port;
extern TbBool exit_on_lua_error;
extern TbBool FLEE_BUTTON_DEFAULT;
extern TbBool IMPRISON_BUTTON_DEFAULT;
/******************************************************************************/
void load_configuration_for_mod_all(void);
short load_configuration(void);
void process_cmdline_overrides(void);
int parse_draw_fps_config_val(const char *arg, int32_t *fps_draw_main, int32_t *fps_draw_secondary);
/******************************************************************************/
TbBool is_feature_on(unsigned long feature);
void set_skip_heart_zoom_feature(TbBool enable);
TbBool get_skip_heart_zoom_feature(void);
TbBool censorship_enabled(void);
TbBool atmos_sounds_enabled(void);
TbBool resize_movies_enabled(void);
TbBool freeze_game_on_focus_lost(void);
TbBool unlock_cursor_when_game_paused(void);
TbBool lock_cursor_in_possession(void);
TbBool use_relative_mouse_mode(void);
TbBool pause_music_when_game_paused(void);
TbBool mute_audio_on_focus_lost(void);
/******************************************************************************/
const char *get_language_lwrstr(int lang_id);
TbBool is_dbc_language(short language);
/******************************************************************************/

#ifdef __cplusplus
}
#endif
#endif
