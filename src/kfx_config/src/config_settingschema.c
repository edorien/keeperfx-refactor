/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file config_settingschema.c
 * @par Purpose:
 *     Declarative schema for keeperfx.cfg-backed settings options -- see
 *     the header for the design. Every option from §6.2's list that fits
 *     SOptT_Bool/Int/Enum is here (35 rows as of Phase G step 12);
 *     SOptT_Float/String/Keybind/Composite are still unimplemented (nothing
 *     currently in the schema needs them -- every option that looked like
 *     it might turned out to fit an existing type once its real
 *     config-parser semantics were traced, see individual rows' own
 *     comments below). Every row needs no new cross-layer plumbing beyond
 *     what Phase G's own steps already added: kfx_config already owns
 *     features_enabled/start_params/keeperfx_ui_config/kfx_config_state
 *     directly, and bflib_mouse.h's lbMouseGrab/bflib_video.h's display_id
 *     and screen-mode registry/bflib_sound.h's atmos_sound_volume/
 *     platform/PlatformManager.h's display-mode enumeration all live in
 *     kfx_platform, below kfx_config in the layering, so they're plain
 *     includes. A few rows (SCREENSHOT, HAND_SIZE, POINTER_SENSITIVITY)
 *     needed a new getter added to ConfigReloadCallbacks (config.h) because
 *     the value was previously only ever written, never read back.
 * @par Comment:
 *     None.
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "config_settingschema.h"

#include "config_keeperfx.h"
#include "config_settings.h" // struct GameSettings settings, save_settings() -- SHADOWS/VIEW_DISTANCE
#include "config_strings.h"
#include "kfx_config_state.h"
#include "bflib_mouse.h"
#include "bflib_video.h"
#include "bflib_sound.h" // atmos_sound_volume
#include "bflib_fmvids.h" // SMK_FullscreenFit/Stretch/Crop -- RESIZE_MOVIES
#include "bflib_basics.h" // ERRORLOG
#include "platform/PlatformManager.h" // INGAME_RES's display-mode enumeration

#include <stdio.h>
#include <string.h>
#include "post_inc.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

static TbBool get_freeze_on_focus_lost(void) { return is_feature_on(Ft_FreezeOnLoseFocus); }
static void set_freeze_on_focus_lost(TbBool val)
{
    if (val) features_enabled |= Ft_FreezeOnLoseFocus;
    else features_enabled &= ~Ft_FreezeOnLoseFocus;
}

static TbBool get_pause_music_on_pause(void) { return is_feature_on(Ft_PauseMusicOnGamePause); }
static void set_pause_music_on_pause(TbBool val)
{
    if (val) features_enabled |= Ft_PauseMusicOnGamePause;
    else features_enabled &= ~Ft_PauseMusicOnGamePause;
}

static TbBool get_mute_audio_on_focus_lost(void) { return is_feature_on(Ft_MuteAudioOnLoseFocus); }
static void set_mute_audio_on_focus_lost(TbBool val)
{
    if (val) features_enabled |= Ft_MuteAudioOnLoseFocus;
    else features_enabled &= ~Ft_MuteAudioOnLoseFocus;
}

// "-altinput"/ALT_INPUT disables the mouse auto-grab/reset behaviour, so
// the option's own on/off sense is the *inverse* of lbMouseGrab -- see
// main.cpp's "altinput" cmdline handler and config_keeperfx.c's ALT_INPUT
// case, both of which set lbMouseGrab = false for "alt input on".
static TbBool get_alt_input(void) { return !lbMouseGrab; }
static void set_alt_input(TbBool val) { lbMouseGrab = !val; }

// §6.2 finding 4: alt input enables the possession-cursor-lock option and
// disables the unlock-when-paused one, and vice versa.
static TbBool get_unlock_cursor_on_pause(void) { return is_feature_on(Ft_UnlockCursorOnPause); }
static void set_unlock_cursor_on_pause(TbBool val)
{
    if (val) features_enabled |= Ft_UnlockCursorOnPause;
    else features_enabled &= ~Ft_UnlockCursorOnPause;
}
static TbBool unlock_cursor_on_pause_enabled(void) { return lbMouseGrab; } // enabled while alt input is off

static TbBool get_lock_cursor_in_possession(void) { return is_feature_on(Ft_LockCursorInPossession); }
static void set_lock_cursor_in_possession(TbBool val)
{
    if (val) features_enabled |= Ft_LockCursorInPossession;
    else features_enabled &= ~Ft_LockCursorInPossession;
}
static TbBool lock_cursor_in_possession_enabled(void) { return !lbMouseGrab; } // enabled while alt input is on

static long get_gui_blink_rate(void) { return keeperfx_ui_config.gui_blink_rate; }
static void set_gui_blink_rate(long val) { keeperfx_ui_config.gui_blink_rate = (int)val; }

static long get_creature_status_size(void) { return keeperfx_ui_config.creature_status_size; }
static void set_creature_status_size(long val) { keeperfx_ui_config.creature_status_size = (int)val; }

// DISPLAY_NUMBER is stored 0-based (display_id) but shown to the user
// 1-based ("1 for the main monitor, 2 for the second monitor, ..." --
// config/keeperfx.cfg's own comment); the schema mirrors that convention
// rather than exposing the raw 0-based storage.
static long get_display_number(void) { return (long)display_id + 1; }
static void set_display_number(long val) { display_id = (unsigned short)(val - 1); }

// UI_FONT_SCALE (KeeperFX-only, no equivalent in original DK): a percentage
// multiplier FeStylePushFont (frontgui_style.cpp) applies on top of its own
// resolution-derived base size -- SApply_Live because that function reads
// keeperfx_ui_config.ui_font_scale_pct fresh every call, no restart needed.
//
// A curated set of "standard" sizes (SOptT_Enum) rather than a free-slider
// percentage (found live: a continuous 50-200 range let the player land on
// values that didn't rasterize particularly crisply -- see FeStylePushFont's
// own comment on the pixel-rounding fix that turned out insufficient on its
// own). .name is the literal percentage as a string, not a fancier label
// like "Large": setting_option_apply_enum_index() (config_settingschema.c)
// writes enum_table[index].name straight to keeperfx.cfg, and this key's
// own parser (config_keeperfx.c case 52) reads it back with plain atoi(),
// so the persisted format has to stay a bare number -- unchanged from
// before this row was an enum, and unchanged for any existing keeperfx.cfg
// already carrying a UI_FONT_SCALE line from the old slider.
static const struct NamedCommand ui_font_scale_enum[] = {
    {"50",  50},
    {"75",  75},
    {"100", 100},
    {"125", 125},
    {"150", 150},
    {"200", 200},
    {NULL,  0},
};
static long get_ui_font_scale(void) { return keeperfx_ui_config.ui_font_scale_pct; }
static void set_ui_font_scale(long val) { keeperfx_ui_config.ui_font_scale_pct = (int)val; }

// Reset Progress (Phase E, docs/refactor/gui/05-campaign-progress-and-landview.md
// §3.5): SOptT_Action's only row so far. reset_all_campaign_progress()
// itself is kfx_game-owned (save/progress.cfg); reached the same way
// every other "kfx_config needs something from above" case in this
// schema is, through ConfigReloadCallbacks (config.h) -- not a
// bespoke callback struct just for this one row.
static void reset_campaign_progress_action(void) { config_reload_callbacks->reset_campaign_progress(); }

// SHADOWS/VIEW_DISTANCE: the in-game "Video" options menu's own
// shadows/view_distance toggles (frontmenu_ingame_opts_data.cpp's
// video_menu_buttons -- gui_video_shadows()/gui_video_view_distance_level()
// in frontmenu_options.c) were missing from this route entirely -- both are
// plain rendering preferences with no active-game dependency (unlike that
// same menu's rotate_mode/cluedo_mode/gamma_correction, which are routed
// through the packet/command system and need a live player, so aren't
// modeled here). Both read/write struct GameSettings (config_settings.h)
// directly -- already kfx_config-owned state, same layer as this file --
// and persist via save_settings() to save/settings.toml, not keeperfx.cfg
// (see persist_via_save_settings's own doc comment, config_settingschema.h).
// Range 0-3 for both, matching config_settings.c's own clamp() on load.
static long get_video_shadows(void) { return settings.video_shadows; }
static void set_video_shadows(long val) { settings.video_shadows = (unsigned char)val; }
static long get_view_distance(void) { return settings.view_distance; }
static void set_view_distance(long val) { settings.view_distance = (unsigned char)val; }

static TbBool get_easter_egg(void) { return start_params.easter_egg; }
static void set_easter_egg(TbBool val) { start_params.easter_egg = val; }

static TbBool get_censorship(void) { return is_feature_on(Ft_Censorship); }
static void set_censorship(TbBool val)
{
    if (val) features_enabled |= Ft_Censorship;
    else features_enabled &= ~Ft_Censorship;
}

static TbBool get_flee_button_default(void) { return FLEE_BUTTON_DEFAULT; }
static void set_flee_button_default(TbBool val) { FLEE_BUTTON_DEFAULT = val; }

static TbBool get_imprison_button_default(void) { return IMPRISON_BUTTON_DEFAULT; }
static void set_imprison_button_default(TbBool val) { IMPRISON_BUTTON_DEFAULT = val; }

static long get_line_box_size(void) { return keeperfx_ui_config.line_box_size; }
static void set_line_box_size(long val) { keeperfx_ui_config.line_box_size = (int)val; }

static long get_neutral_flash_rate(void) { return keeperfx_ui_config.neutral_flash_rate; }
static void set_neutral_flash_rate(long val) { keeperfx_ui_config.neutral_flash_rate = (int)val; }

static TbBool get_atmospheric_sounds(void) { return is_feature_on(Ft_Atmossounds); }
static void set_atmospheric_sounds(TbBool val)
{
    if (val) features_enabled |= Ft_Atmossounds;
    else features_enabled &= ~Ft_Atmossounds;
}

// The launcher's "Use CD Music" is the *inverse* of Ft_NoCdMusic
// (config_keeperfx.c's MUSIC_FROM_DISK case, config key 29: MUSIC_FROM_DISK
// = TRUE sets Ft_NoCdMusic, i.e. "don't use CD"). cfg_bool_inverted on this
// row's own SettingOption entry is what makes setting_option_apply_bool()
// write MUSIC_FROM_DISK's opposite polarity when this displayed value is
// toggled -- see the header's own comment.
static TbBool get_use_cd_music(void) { return !is_feature_on(Ft_NoCdMusic); }
static void set_use_cd_music(TbBool val)
{
    if (val) features_enabled &= ~Ft_NoCdMusic;
    else features_enabled |= Ft_NoCdMusic;
}

// CURSOR_EDGE_CAMERA_PANNING's own config key (case 24) already has "true
// enables panning" semantics against the inverted Ft_DisableCursor...
// flag -- no cfg_bool_inverted needed, only the underlying flag name reads
// backwards.
static TbBool get_cursor_edge_camera_panning(void) { return !is_feature_on(Ft_DisableCursorCameraPanning); }
static void set_cursor_edge_camera_panning(TbBool val)
{
    if (val) features_enabled &= ~Ft_DisableCursorCameraPanning;
    else features_enabled |= Ft_DisableCursorCameraPanning;
}

static TbBool get_tag_mode_toggling(void) { return keeperfx_ui_config.right_click_tag_mode_toggle; }
static void set_tag_mode_toggling(TbBool val) { keeperfx_ui_config.right_click_tag_mode_toggle = val; }

static long get_atmos_volume(void) { return atmos_sound_volume; }
static void set_atmos_volume(long val) { atmos_sound_volume = (int)val; }

static long get_atmos_frequency(void) { return kfx_config_state.atmos_sound_frequency; }
static void set_atmos_frequency(long val) { kfx_config_state.atmos_sound_frequency = (int)val; }

static long get_default_tag_mode(void) { return keeperfx_ui_config.default_tag_mode; }
static void set_default_tag_mode(long val) { keeperfx_ui_config.default_tag_mode = (int)val; }

static long get_language(void) { return install_info.lang_id; }
static void set_language(long val) { install_info.lang_id = (int)val; }

static TbBool get_delta_time(void) { return is_feature_on(Ft_DeltaTime); }
static void set_delta_time(TbBool val)
{
    if (val) features_enabled |= Ft_DeltaTime;
    else features_enabled &= ~Ft_DeltaTime;
}

// ZOOM_TO_MOUSE/ROTATE_AROUND_MOUSE's own config-key parsing (case 42/43,
// config_keeperfx.c) checks logicval_type for ALWAYS/NEVER *before* falling
// back to a dedicated table for the WHEEL/rotation-key tokens -- there's no
// single existing NamedCommand table covering the whole value space the way
// atmos_volume[]/tag_modes[] do for their options. These two purpose-built
// tables exist only for the schema: their names are literal tokens the
// parser's combined ALWAYS/NEVER-then-fallback logic already accepts, and
// their .num values match keeperfx_ui_config.zoom_to_mouse_option/
// rotate_around_mouse_option's own storage (confirmed by tracing the
// parser) -- e.g. writing "ALWAYS" here round-trips to zoom_to_mouse_option
// == 3 (ZoomToMouse_Always) exactly like a hand-written keeperfx.cfg line
// would.
static const struct NamedCommand zoom_to_mouse_enum[] = {
    {"NEVER", 1}, // ZoomToMouse_Never
    {"WHEEL", 2}, // ZoomToMouse_Wheel
    {"ALWAYS", 3}, // ZoomToMouse_Always
    {NULL, 0},
};
static long get_zoom_to_mouse(void) { return keeperfx_ui_config.zoom_to_mouse_option; }
static void set_zoom_to_mouse(long val) { keeperfx_ui_config.zoom_to_mouse_option = (int)val; }

static const struct NamedCommand rotate_around_mouse_enum[] = {
    {"NEVER", 1}, // RotateAroundMouse_Never
    {"ROTATION_KEYS", 2},
    {"MOVEMENT_KEYS", 3},
    {"ALWAYS", 4}, // RotateAroundMouse_Always
    {NULL, 0},
};
static long get_rotate_around_mouse(void) { return keeperfx_ui_config.rotate_around_mouse_option; }
static void set_rotate_around_mouse(long val) { keeperfx_ui_config.rotate_around_mouse_option = (int)val; }

// SCREENSHOT reuses scrshot_type[] (config_keeperfx.c) the same way
// ATMOS_VOLUME reuses atmos_volume[] -- but unlike every other row so far,
// screenshot_format (kfx_render-owned, scrcapt.h) only had a *setter*
// callback (config_keeperfx.c's own SCREENSHOT config-key case has never
// needed to read it back). Added get_screenshot_format to
// ConfigReloadCallbacks (config.h) alongside the existing
// set_screenshot_format for this row to have something to read the
// current value from.
static long get_screenshot_format(void) { return config_reload_callbacks->get_screenshot_format(); }
static void set_screenshot_format_val(long val) { config_reload_callbacks->set_screenshot_format((unsigned char)val); }

// HAND_SIZE's own config-key parsing (case 30, config_keeperfx.c) already
// reads/writes it as an integer *percentage* (atoi(word_buf), then
// set_hand_scale(i/100.0)) even though the live engine value
// (global_hand_scale) is a float scale factor -- so this is a plain
// SOptT_Int row keyed on the same percentage the file already uses, not a
// new float schema type. Needed a new get_hand_scale callback the same way
// SCREENSHOT needed get_screenshot_format -- config_keeperfx.c's own
// parser only ever wrote this value, never read it back.
static long get_hand_size_pct(void) { return (long)(config_reload_callbacks->get_hand_scale() * 100.0f + 0.5f); }
static void set_hand_size_pct(long val) { config_reload_callbacks->set_hand_scale((float)val / 100.0f); }

// RESIZE_MOVIES's own config-key parsing (case 14, config_keeperfx.c) is
// two storage locations combined into one option: Ft_Resizemovies (on/off)
// plus vid_scale_flags (which scaling mode, only meaningful while the
// feature is on). vidscale_type[] (config_keeperfx.c) itself isn't a clean
// fit for a combo box -- it's an *alias* table for the parser ("ON"/
// "ENABLED"/"TRUE"/"YES"/"1" all mean the same thing as "FIT"), so reusing
// it verbatim would show duplicate-valued entries. This purpose-built local
// table keeps just the canonical one name per distinct value, the same
// "not every table is fit to reuse" reasoning ZOOM_TO_MOUSE/
// ROTATE_AROUND_MOUSE's own tables were built for.
static const struct NamedCommand resize_movies_enum[] = {
    {"OFF", 0},
    {"FIT", SMK_FullscreenFit},
    {"STRETCH", SMK_FullscreenStretch},
    {"CROP", SMK_FullscreenCrop},
    {"4BY3", SMK_FullscreenFit | SMK_FullscreenStretch},
    {"PIXELPERFECT", SMK_FullscreenFit | SMK_FullscreenCrop},
    {"4BY3PP", SMK_FullscreenFit | SMK_FullscreenStretch | SMK_FullscreenCrop},
    {NULL, 0},
};
static long get_resize_movies(void) { return is_feature_on(Ft_Resizemovies) ? (long)vid_scale_flags : 0; }
static void set_resize_movies(long val)
{
    if (val == 0)
    {
        features_enabled &= ~Ft_Resizemovies;
    }
    else
    {
        features_enabled |= Ft_Resizemovies;
        vid_scale_flags = (unsigned int)val;
    }
}

// INGAME_RES (case 7, config_keeperfx.c) is a fullscreen "WxHxBPP" string
// (e.g. "1920x1080x32") handed to LbRegisterVideoModeString(), which
// registers a *new* mode on the fly if the exact string hasn't been seen
// before (bflib_video.c) -- it's not limited to a fixed, pre-registered
// list. That, plus the fact the real list of resolutions a monitor
// supports isn't known until the platform layer is asked, makes this the
// one enum row whose table can't be a compile-time constant like every
// other SOptT_Enum row -- see ensure_enum_table's own doc comment
// (config_settingschema.h). Only fullscreen resolutions are offered here
// (PlatformManager_GetFullscreenDisplayModeAt); DESKTOP/DESKTOP_FULL/
// windowed "WxHwBPP"/ALL are all still valid INGAME_RES values but aren't
// surfaced by this picker.
//
// .num encodes width/height as (width << 16) | height rather than reusing
// LbRegisterVideoModeString()'s own TbScreenMode return value, because
// that's a registry *index* assigned in registration order -- unstable
// across runs/platforms and meaningless to compare against "what
// resolution is active now" without first re-deriving width/height from
// it anyway. Both values comfortably fit 16 bits (SDL reports pixel
// dimensions, never anywhere near 65536).
#define INGAME_RES_ENCODE(w, h) (((long)(w) << 16) | (long)((h) & 0xFFFF))
#define INGAME_RES_WIDTH(val) (int)((val) >> 16)
#define INGAME_RES_HEIGHT(val) (int)((val) & 0xFFFF)

#define INGAME_RES_MAX_ENTRIES 32
static struct NamedCommand ingame_res_enum[INGAME_RES_MAX_ENTRIES + 1];
static char ingame_res_names[INGAME_RES_MAX_ENTRIES][16];
static TbBool ingame_res_enum_ready = false;

static void ensure_ingame_res_enum(void)
{
    // Detected resolutions don't change while the game is running (barring
    // a monitor hotplug, an edge case not worth re-querying every frame
    // for) -- populate once, lazily, on first actual use.
    if (ingame_res_enum_ready)
        return;
    ingame_res_enum_ready = true;
    int count = PlatformManager_GetFullscreenDisplayModeCount(0);
    if (count > INGAME_RES_MAX_ENTRIES) count = INGAME_RES_MAX_ENTRIES;
    int n = 0;
    for (int i = 0; i < count; i++)
    {
        int w = 0, h = 0;
        if (!PlatformManager_GetFullscreenDisplayModeAt(0, i, &w, &h) || (w <= 0) || (h <= 0))
            continue;
        snprintf(ingame_res_names[n], sizeof(ingame_res_names[n]), "%dx%dx32", w, h);
        ingame_res_enum[n].name = ingame_res_names[n];
        ingame_res_enum[n].num = INGAME_RES_ENCODE(w, h);
        n++;
    }
    ingame_res_enum[n].name = NULL;
    ingame_res_enum[n].num = 0;
}

static long get_ingame_res(void)
{
    // INGAME_RES is SApply_NeedsRestart: reading LbScreenActiveMode() (the
    // currently-applied mode) would make the combo immediately revert to
    // the old resolution the instant a new one is picked, since the active
    // mode doesn't actually change until next launch. get_screen_vidmode()
    // reads the *pending* mode (screen_vidmode itself, kfx_render/vidmode.c)
    // instead, which set_ingame_res() below updates immediately.
    TbScreenMode mode = config_reload_callbacks->get_screen_vidmode();
    TbScreenModeInfo *info = LbScreenGetModeInfo(mode);
    if (info == NULL)
        return 0;
    return INGAME_RES_ENCODE(info->Width, info->Height);
}

static void set_ingame_res(long val)
{
    char word_buf[16];
    snprintf(word_buf, sizeof(word_buf), "%dx%dx32", INGAME_RES_WIDTH(val), INGAME_RES_HEIGHT(val));
    TbScreenMode mode = LbRegisterVideoModeString(word_buf);
    if (mode != Lb_SCREEN_MODE_INVALID)
        config_reload_callbacks->set_screen_vidmode(mode);
    else
        ERRORLOG("Couldn't register video mode \"%s\" chosen from the settings screen.", word_buf);
}

// POINTER_SENSITIVITY's own config-key parsing (case 9, config_keeperfx.c)
// stores an integer percentage in the file (0-10000) but scales it by
// 256/100 before handing it to set_base_mouse_sensitivity -- same
// percentage-in-file-vs-scaled-internal-value shape as HAND_SIZE, so this
// is a plain SOptT_Int row too. (§6.2 finding 3 describes the launcher's
// own UI as a checkbox-gated slider where 0 has a special "raw input"
// meaning; not modeled here -- what "0" actually does at the engine level
// beyond zeroing LbMouseChangeMoveRatio() isn't something this pass could
// confirm, and a checkbox asserting an unverified behaviour is worse than
// no checkbox. The plain slider still reaches 0 by dragging it down.)
static long get_pointer_sensitivity_pct(void) { return config_reload_callbacks->get_base_mouse_sensitivity() * 100 / 256; }
static void set_pointer_sensitivity_pct(long val) { config_reload_callbacks->set_base_mouse_sensitivity(val * 256 / 100); }

// STARTUP's own config-key parsing (case 22, config_keeperfx.c) accepts a
// space-separated token list -- LEGAL/FX/BULLFROG(hidden)/EA(hidden)/INTRO,
// each just a bit in start_params.startup_flags -- and §6.2's own Game-tab
// description names it "the splash-screens + intro list" (the legacy
// DISABLE_SPLASH_SCREENS + "-nointro" pair). Two rows expose it, matching
// finding 3's "two checkboxes": "splash screens" toggles LEGAL and FX
// together (set_default_startup_parameters()'s own default is
// SFlg_Legal|SFlg_FX|SFlg_Intro -- they've always moved as a pair), "intro"
// toggles INTRO alone. BULLFROG/EA are legacy/hidden -- never toggled by
// either checkbox, but format_startup_cfg_value() reconstructs the *whole*
// list from the live bits on every apply, so a config file that already had
// them keeps them rather than losing them the moment either checkbox is
// touched.
static TbBool get_startup_splash(void) { return (start_params.startup_flags & (SFlg_Legal | SFlg_FX)) == (SFlg_Legal | SFlg_FX); }
static void set_startup_splash(TbBool val)
{
    if (val) start_params.startup_flags |= (SFlg_Legal | SFlg_FX);
    else start_params.startup_flags &= ~(SFlg_Legal | SFlg_FX);
}

static TbBool get_startup_intro(void) { return (start_params.startup_flags & SFlg_Intro) != 0; }
static void set_startup_intro(TbBool val)
{
    if (val) start_params.startup_flags |= SFlg_Intro;
    else start_params.startup_flags &= ~SFlg_Intro;
}

static const char *format_startup_cfg_value(void)
{
    static char buf[48];
    buf[0] = '\0';
    if (start_params.startup_flags & SFlg_Legal) strcat(buf, "LEGAL ");
    if (start_params.startup_flags & SFlg_FX) strcat(buf, "FX ");
    if (start_params.startup_flags & SFlg_Bullfrog) strcat(buf, "BULLFROG ");
    if (start_params.startup_flags & SFlg_EA) strcat(buf, "EA ");
    if (start_params.startup_flags & SFlg_Intro) strcat(buf, "INTRO ");
    size_t len = strlen(buf);
    if (len > 0) buf[len - 1] = '\0'; // trim the trailing space
    return buf;
}

// FRAMES_PER_SECOND's own config-key parsing (case 39, config_keeperfx.c,
// via parse_draw_fps_config_val()) stores a mode plus a number: "AUTO" (or
// "AUTO <secondary>") sets num_fps_draw_main to -1 (bflib_video.h's own
// comment: "-1 if auto" -- confirmed against redetect_screen_refresh_rate_
// for_draw()'s actual use of it: -1 means "use the display's own refresh
// rate, falling back to num_fps_draw_secondary if that can't be
// detected"), while a plain number sets num_fps_draw_main directly (0,
// its real default, means uncapped; positive means a fixed FPS cap).
// Two SOptT_Bool/Int rows share the cfg_key, same shared-key +
// format_cfg_value shape STARTUP already established: "Auto Frame Rate"
// toggles the -1 sentinel, "Frame Rate Limit" edits the fixed-cap number
// (greyed out while Auto is on -- same is_enabled gating ALT_INPUT's own
// pair of rows uses). num_fps_draw_secondary isn't exposed as a control
// of its own (a rarely-needed fallback-only-under-Auto value) but
// format_fps_cfg_value() preserves it verbatim if a config file already
// set it, the same "don't drop what wasn't touched" reasoning STARTUP's
// hidden BULLFROG/EA bits needed.
static TbBool get_fps_auto(void) { return start_params.num_fps_draw_main == -1; }
static void set_fps_auto(TbBool val)
{
    start_params.num_fps_draw_main = val ? -1 : 0; // 0 is num_fps_draw_main's own real default (uncapped)
}

static long get_fps_limit(void) { return (start_params.num_fps_draw_main > 0) ? start_params.num_fps_draw_main : 0; }
static void set_fps_limit(long val) { start_params.num_fps_draw_main = val; }
static TbBool fps_limit_enabled(void) { return start_params.num_fps_draw_main != -1; }

static const char *format_fps_cfg_value(void)
{
    static char buf[32];
    if (start_params.num_fps_draw_main == -1)
    {
        if (start_params.num_fps_draw_secondary > 0)
            snprintf(buf, sizeof(buf), "AUTO %ld", (long)start_params.num_fps_draw_secondary);
        else
            snprintf(buf, sizeof(buf), "AUTO");
    }
    else
    {
        snprintf(buf, sizeof(buf), "%ld", (long)start_params.num_fps_draw_main);
    }
    return buf;
}

// Designated initializers throughout -- every field not named defaults to
// 0/NULL/false, and (unlike positional init) doesn't trip
// -Wmissing-field-initializers/-Werror when a row leaves most of them out,
// which almost every row here does (only one accessor pair, rarely
// is_enabled, almost never cfg_bool_inverted).
const struct SettingOption setting_options[] = {
    {
        .cfg_key = "FREEZE_GAME_ON_FOCUS_LOST", .type = SOptT_Bool, .category = SCat_Game, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetFreezeOnFocusLost,
        .help_stridx = GUIStr_HelpFreezeOnFocusLost,
        .get_bool = &get_freeze_on_focus_lost, .set_bool = &set_freeze_on_focus_lost,
    },
    {
        .cfg_key = "STARTUP", .type = SOptT_Bool, .category = SCat_Game, .apply_class = SApply_NeedsRestart,
        .label_stridx = GUIStr_SetStartupSplash,
        .help_stridx = GUIStr_HelpStartupSplash,
        .get_bool = &get_startup_splash, .set_bool = &set_startup_splash,
        .format_cfg_value = &format_startup_cfg_value,
    },
    {
        .cfg_key = "STARTUP", .type = SOptT_Bool, .category = SCat_Game, .apply_class = SApply_NeedsRestart,
        .label_stridx = GUIStr_SetStartupIntro,
        .help_stridx = GUIStr_HelpStartupIntro,
        .get_bool = &get_startup_intro, .set_bool = &set_startup_intro,
        .format_cfg_value = &format_startup_cfg_value,
    },
    {
        .cfg_key = "EASTER_EGG", .type = SOptT_Bool, .category = SCat_Game, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetEasterEgg,
        .help_stridx = GUIStr_HelpEasterEgg,
        .get_bool = &get_easter_egg, .set_bool = &set_easter_egg,
    },
    {
        .cfg_key = "CENSORSHIP", .type = SOptT_Bool, .category = SCat_Game, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetCensorship,
        .help_stridx = GUIStr_HelpCensorship,
        .get_bool = &get_censorship, .set_bool = &set_censorship,
    },
    {
        .cfg_key = "FLEE_BUTTON_DEFAULT", .type = SOptT_Bool, .category = SCat_Game, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetFleeButtonDefault,
        .help_stridx = GUIStr_HelpFleeButtonDefault,
        .get_bool = &get_flee_button_default, .set_bool = &set_flee_button_default,
    },
    {
        .cfg_key = "IMPRISON_BUTTON_DEFAULT", .type = SOptT_Bool, .category = SCat_Game, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetImprisonButtonDefault,
        .help_stridx = GUIStr_HelpImprisonButtonDefault,
        .get_bool = &get_imprison_button_default, .set_bool = &set_imprison_button_default,
    },
    {
        .cfg_key = "LANGUAGE", .type = SOptT_Enum, .category = SCat_Game, .apply_class = SApply_NeedsRestart,
        .label_stridx = GUIStr_SetLanguage,
        .help_stridx = GUIStr_HelpLanguage,
        .enum_table = lang_type, .get_enum = &get_language, .set_enum = &set_language,
    },
    {
        .cfg_key = "DELTA_TIME", .type = SOptT_Bool, .category = SCat_Game, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetDeltaTime,
        .help_stridx = GUIStr_HelpDeltaTime,
        .get_bool = &get_delta_time, .set_bool = &set_delta_time,
    },
    {
        .cfg_key = "DISPLAY_NUMBER", .type = SOptT_Int, .category = SCat_Graphics, .apply_class = SApply_NeedsRestart,
        .label_stridx = GUIStr_SetDisplayNumber,
        .help_stridx = GUIStr_HelpDisplayNumber,
        .get_int = &get_display_number, .set_int = &set_display_number, .int_min = 1, .int_max = 8,
    },
    {
        .cfg_key = "FRAMES_PER_SECOND", .type = SOptT_Bool, .category = SCat_Graphics, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetFpsAuto,
        .help_stridx = GUIStr_HelpFpsAuto,
        .get_bool = &get_fps_auto, .set_bool = &set_fps_auto,
        .format_cfg_value = &format_fps_cfg_value,
    },
    {
        .cfg_key = "FRAMES_PER_SECOND", .type = SOptT_Int, .category = SCat_Graphics, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetFpsLimit,
        .help_stridx = GUIStr_HelpFpsLimit,
        .get_int = &get_fps_limit, .set_int = &set_fps_limit, .int_min = 0, .int_max = 500,
        .is_enabled = &fps_limit_enabled,
        .format_cfg_value = &format_fps_cfg_value,
    },
    {
        .cfg_key = "GUI_BLINK_RATE", .type = SOptT_Int, .category = SCat_Graphics, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetGuiBlinkRate,
        .help_stridx = GUIStr_HelpGuiBlinkRate,
        .get_int = &get_gui_blink_rate, .set_int = &set_gui_blink_rate, .int_min = 1, .int_max = 160,
    },
    {
        .cfg_key = "CREATURE_STATUS_SIZE", .type = SOptT_Int, .category = SCat_Graphics, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetCreatureStatusSize,
        .help_stridx = GUIStr_HelpCreatureStatusSize,
        .get_int = &get_creature_status_size, .set_int = &set_creature_status_size, .int_min = 8, .int_max = 64,
    },
    {
        .cfg_key = "LINE_BOX_SIZE", .type = SOptT_Int, .category = SCat_Graphics, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetLineBoxSize,
        .help_stridx = GUIStr_HelpLineBoxSize,
        .get_int = &get_line_box_size, .set_int = &set_line_box_size, .int_min = 0, .int_max = 500,
    },
    {
        .cfg_key = "NEUTRAL_FLASH_RATE", .type = SOptT_Int, .category = SCat_Graphics, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetNeutralFlashRate,
        .help_stridx = GUIStr_HelpNeutralFlashRate,
        .get_int = &get_neutral_flash_rate, .set_int = &set_neutral_flash_rate, .int_min = 1, .int_max = 160,
    },
    {
        .cfg_key = "PAUSE_MUSIC_WHEN_GAME_PAUSED", .type = SOptT_Bool, .category = SCat_Sound, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetPauseMusicOnPause,
        .help_stridx = GUIStr_HelpPauseMusicOnPause,
        .get_bool = &get_pause_music_on_pause, .set_bool = &set_pause_music_on_pause,
    },
    {
        .cfg_key = "MUTE_AUDIO_ON_FOCUS_LOST", .type = SOptT_Bool, .category = SCat_Sound, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetMuteAudioOnFocusLost,
        .help_stridx = GUIStr_HelpMuteAudioOnFocusLost,
        .get_bool = &get_mute_audio_on_focus_lost, .set_bool = &set_mute_audio_on_focus_lost,
    },
    {
        .cfg_key = "ATMOSPHERIC_SOUNDS", .type = SOptT_Bool, .category = SCat_Sound, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetAtmosphericSounds,
        .help_stridx = GUIStr_HelpAtmosphericSounds,
        .get_bool = &get_atmospheric_sounds, .set_bool = &set_atmospheric_sounds,
    },
    {
        // See get_use_cd_music()'s own comment -- this row's displayed
        // sense is the inverse of the MUSIC_FROM_DISK key it writes.
        .cfg_key = "MUSIC_FROM_DISK", .type = SOptT_Bool, .category = SCat_Sound, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetUseCdMusic,
        .help_stridx = GUIStr_HelpUseCdMusic,
        .get_bool = &get_use_cd_music, .set_bool = &set_use_cd_music,
        .cfg_bool_inverted = true,
    },
    {
        .cfg_key = "ATMOS_VOLUME", .type = SOptT_Enum, .category = SCat_Sound, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetAtmosVolume,
        .help_stridx = GUIStr_HelpAtmosVolume,
        .enum_table = atmos_volume, .get_enum = &get_atmos_volume, .set_enum = &set_atmos_volume,
    },
    {
        .cfg_key = "ATMOS_FREQUENCY", .type = SOptT_Enum, .category = SCat_Sound, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetAtmosFrequency,
        .help_stridx = GUIStr_HelpAtmosFrequency,
        .enum_table = atmos_freq, .get_enum = &get_atmos_frequency, .set_enum = &set_atmos_frequency,
    },
    {
        .cfg_key = "POINTER_SENSITIVITY", .type = SOptT_Int, .category = SCat_Input, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetPointerSensitivity,
        .help_stridx = GUIStr_HelpPointerSensitivity,
        .get_int = &get_pointer_sensitivity_pct, .set_int = &set_pointer_sensitivity_pct, .int_min = 0, .int_max = 500,
    },
    {
        .cfg_key = "ALT_INPUT", .type = SOptT_Bool, .category = SCat_Input, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetAltInput,
        .help_stridx = GUIStr_HelpAltInput,
        .get_bool = &get_alt_input, .set_bool = &set_alt_input,
    },
    {
        .cfg_key = "UNLOCK_CURSOR_WHEN_GAME_PAUSED", .type = SOptT_Bool, .category = SCat_Input, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetUnlockCursorOnPause,
        .help_stridx = GUIStr_HelpUnlockCursorOnPause,
        .get_bool = &get_unlock_cursor_on_pause, .set_bool = &set_unlock_cursor_on_pause,
        .is_enabled = &unlock_cursor_on_pause_enabled,
    },
    {
        .cfg_key = "LOCK_CURSOR_IN_POSSESSION", .type = SOptT_Bool, .category = SCat_Input, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetLockCursorInPossession,
        .help_stridx = GUIStr_HelpLockCursorInPossession,
        .get_bool = &get_lock_cursor_in_possession, .set_bool = &set_lock_cursor_in_possession,
        .is_enabled = &lock_cursor_in_possession_enabled,
    },
    {
        .cfg_key = "CURSOR_EDGE_CAMERA_PANNING", .type = SOptT_Bool, .category = SCat_Input, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetCursorEdgeCameraPanning,
        .help_stridx = GUIStr_HelpCursorEdgeCameraPanning,
        .get_bool = &get_cursor_edge_camera_panning, .set_bool = &set_cursor_edge_camera_panning,
    },
    {
        .cfg_key = "TAG_MODE_TOGGLING", .type = SOptT_Bool, .category = SCat_Input, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetTagModeToggling,
        .help_stridx = GUIStr_HelpTagModeToggling,
        .get_bool = &get_tag_mode_toggling, .set_bool = &set_tag_mode_toggling,
    },
    {
        .cfg_key = "DEFAULT_TAG_MODE", .type = SOptT_Enum, .category = SCat_Input, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetDefaultTagMode,
        .help_stridx = GUIStr_HelpDefaultTagMode,
        .enum_table = tag_modes, .get_enum = &get_default_tag_mode, .set_enum = &set_default_tag_mode,
    },
    {
        .cfg_key = "ZOOM_TO_MOUSE", .type = SOptT_Enum, .category = SCat_Input, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetZoomToMouse,
        .help_stridx = GUIStr_HelpZoomToMouse,
        .enum_table = zoom_to_mouse_enum, .get_enum = &get_zoom_to_mouse, .set_enum = &set_zoom_to_mouse,
    },
    {
        .cfg_key = "ROTATE_AROUND_MOUSE", .type = SOptT_Enum, .category = SCat_Input, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetRotateAroundMouse,
        .help_stridx = GUIStr_HelpRotateAroundMouse,
        .enum_table = rotate_around_mouse_enum, .get_enum = &get_rotate_around_mouse, .set_enum = &set_rotate_around_mouse,
    },
    {
        .cfg_key = "SCREENSHOT", .type = SOptT_Enum, .category = SCat_Graphics, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetScreenshotFormat,
        .help_stridx = GUIStr_HelpScreenshotFormat,
        .enum_table = scrshot_type, .get_enum = &get_screenshot_format, .set_enum = &set_screenshot_format_val,
    },
    {
        .cfg_key = "HAND_SIZE", .type = SOptT_Int, .category = SCat_Graphics, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetHandSize,
        .help_stridx = GUIStr_HelpHandSize,
        .get_int = &get_hand_size_pct, .set_int = &set_hand_size_pct, .int_min = 10, .int_max = 500,
    },
    {
        .cfg_key = "RESIZE_MOVIES", .type = SOptT_Enum, .category = SCat_Graphics, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetResizeMovies,
        .help_stridx = GUIStr_HelpResizeMovies,
        .enum_table = resize_movies_enum, .get_enum = &get_resize_movies, .set_enum = &set_resize_movies,
    },
    {
        // set_screen_vidmode() (kfx_render/vidmode.c) just stores the mode
        // in screen_vidmode for setup_screen_mode() to pick up -- same
        // "takes effect next launch" shape as DISPLAY_NUMBER above, hence
        // SApply_NeedsRestart.
        .cfg_key = "INGAME_RES", .type = SOptT_Enum, .category = SCat_Graphics, .apply_class = SApply_NeedsRestart,
        .label_stridx = GUIStr_SetIngameRes,
        .help_stridx = GUIStr_HelpIngameRes,
        .enum_table = ingame_res_enum, .get_enum = &get_ingame_res, .set_enum = &set_ingame_res,
        .ensure_enum_table = &ensure_ingame_res_enum,
    },
    {
        .cfg_key = "UI_FONT_SCALE", .type = SOptT_Enum, .category = SCat_Graphics, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetUiFontScale,
        .help_stridx = GUIStr_HelpUiFontScale,
        .enum_table = ui_font_scale_enum, .get_enum = &get_ui_font_scale, .set_enum = &set_ui_font_scale,
    },
    {
        // Ported from the in-game Video options menu -- see
        // get_video_shadows/get_view_distance's own doc comment above for
        // why this isn't a keeperfx.cfg-backed row like everything else in
        // this table (persist_via_save_settings).
        .type = SOptT_Int, .category = SCat_Graphics, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetVideoShadows,
        .help_stridx = GUIStr_HelpVideoShadows,
        .get_int = &get_video_shadows, .set_int = &set_video_shadows, .int_min = 0, .int_max = 3,
        .persist_via_save_settings = true,
    },
    {
        .type = SOptT_Int, .category = SCat_Graphics, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetViewDistance,
        .help_stridx = GUIStr_HelpViewDistance,
        .get_int = &get_view_distance, .set_int = &set_view_distance, .int_min = 0, .int_max = 3,
        .persist_via_save_settings = true,
    },
    {
        // No cfg_key -- see enum SettingOptionType's own SOptT_Action
        // comment (config_settingschema.h). Not required for -classicmenu
        // (docs/refactor/gui/05-campaign-progress-and-landview.md §3.4) --
        // the legacy Options screen doesn't read this schema at all, so no
        // extra gating is needed here for that.
        .type = SOptT_Action, .category = SCat_Game, .apply_class = SApply_Live,
        .label_stridx = GUIStr_SetResetCampaignProgress,
        .help_stridx = GUIStr_HelpResetCampaignProgress,
        .on_action = &reset_campaign_progress_action,
    },
};

const int setting_options_count = sizeof(setting_options) / sizeof(setting_options[0]);

void setting_option_apply_bool(const struct SettingOption *opt, TbBool val)
{
    if ((opt == NULL) || (opt->type != SOptT_Bool) || (opt->set_bool == NULL))
    {
        ERRORLOG("setting_option_apply_bool called on a non-bool or incomplete option.");
        return;
    }
    opt->set_bool(val);
    if (opt->persist_via_save_settings)
    {
        save_settings();
        return;
    }
    const char *write_val;
    if (opt->format_cfg_value != NULL)
    {
        write_val = opt->format_cfg_value();
    }
    else
    {
        TbBool inverted_val = opt->cfg_bool_inverted ? !val : val;
        write_val = inverted_val ? "TRUE" : "FALSE";
    }
    struct KeeperfxCfgEdit edit = { opt->cfg_key, write_val };
    keeperfx_cfg_write_values(&edit, 1);
}

void setting_option_apply_int(const struct SettingOption *opt, long val)
{
    if ((opt == NULL) || (opt->type != SOptT_Int) || (opt->set_int == NULL))
    {
        ERRORLOG("setting_option_apply_int called on a non-int or incomplete option.");
        return;
    }
    if (val < opt->int_min) val = opt->int_min;
    if (val > opt->int_max) val = opt->int_max;
    opt->set_int(val);
    if (opt->persist_via_save_settings)
    {
        save_settings();
        return;
    }
    char valbuf[16];
    const char *write_val;
    if (opt->format_cfg_value != NULL)
    {
        write_val = opt->format_cfg_value();
    }
    else
    {
        snprintf(valbuf, sizeof(valbuf), "%ld", val);
        write_val = valbuf;
    }
    struct KeeperfxCfgEdit edit = { opt->cfg_key, write_val };
    keeperfx_cfg_write_values(&edit, 1);
}

int setting_option_enum_count(const struct SettingOption *opt)
{
    if ((opt == NULL) || (opt->type != SOptT_Enum) || (opt->enum_table == NULL))
        return 0;
    if (opt->ensure_enum_table != NULL) opt->ensure_enum_table();
    int n = 0;
    while (opt->enum_table[n].name != NULL) n++;
    return n;
}

int setting_option_enum_current_index(const struct SettingOption *opt)
{
    if ((opt == NULL) || (opt->type != SOptT_Enum) || (opt->enum_table == NULL) || (opt->get_enum == NULL))
        return 0;
    if (opt->ensure_enum_table != NULL) opt->ensure_enum_table();
    long val = opt->get_enum();
    for (int i = 0; opt->enum_table[i].name != NULL; i++)
    {
        if (opt->enum_table[i].num == val)
            return i;
    }
    return 0;
}

const char *setting_option_enum_item_name(const struct SettingOption *opt, int index)
{
    if ((opt == NULL) || (opt->type != SOptT_Enum) || (opt->enum_table == NULL))
        return "";
    // setting_option_enum_count() below already calls ensure_enum_table().
    if ((index < 0) || (index >= setting_option_enum_count(opt)))
        return "";
    return opt->enum_table[index].name;
}

void setting_option_apply_enum_index(const struct SettingOption *opt, int index)
{
    if ((opt == NULL) || (opt->type != SOptT_Enum) || (opt->enum_table == NULL) || (opt->set_enum == NULL))
    {
        ERRORLOG("setting_option_apply_enum_index called on a non-enum or incomplete option.");
        return;
    }
    // setting_option_enum_count() below already calls ensure_enum_table().
    if ((index < 0) || (index >= setting_option_enum_count(opt)))
    {
        ERRORLOG("setting_option_apply_enum_index called with an out-of-range index.");
        return;
    }
    opt->set_enum(opt->enum_table[index].num);
    struct KeeperfxCfgEdit edit = { opt->cfg_key, opt->enum_table[index].name };
    keeperfx_cfg_write_values(&edit, 1);
}

/******************************************************************************/
#ifdef __cplusplus
}
#endif
