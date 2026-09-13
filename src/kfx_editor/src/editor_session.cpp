/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file editor_session.cpp
 *     The in-game level editor's session lifecycle (docs/refactor/editor/
 *     01-entry-and-editor-session.md): editor_open()/editor_close()/
 *     editor_is_active()/editor_frame(), the simulation_suspended
 *     force-and-lock, and the Esc editor menu (Save/Playtest stubbed --
 *     they land with the map serializer, phase 3). The actual tool
 *     palette (phase 2), map serializer (phase 3) and view/light/AP/script
 *     panels (phases 4-5) are not implemented yet -- see 00-overview.md's
 *     phase roadmap.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "kfx_editor.h"
#include "editor_toolbox.h"

#include "kfx_sim_state.h"
#include "player_data.h"
#include "packet_data.h"
#include "camera_data.h" // player->cameras[]/CamIV_*
#include "dungeon_data.h" // get_player_soul_container
#include "thing_data.h" // thing_is_invalid
#include "slab_data.h" // reveal_whole_map
#include "config_crtrmodel.h" // make_all_creatures_free
#include "config_trapdoor.h" // make_available_all_doors/traps
#include "config_terrain.h" // make_all_rooms_free/make_available_all_researchable_rooms
#include "config_magic.h" // make_all_powers_cost_free/make_available_all_researchable_powers
#include "config_keeperfx.h" // Ft_SkipHeartZoom
#include "frontgui_widgets.h"
#include <imgui.h>
#include "post_inc.h"

/******************************************************************************/
namespace {
    bool s_editor_active = false;
    bool s_editor_dirty = false;
    bool s_show_editor_menu = false;
    // Restored in editor_close() -- see editor_open()'s own comment on why
    // Ft_SkipHeartZoom is forced on for the session.
    bool s_prev_skip_heart_zoom = false;

    // §3's planned "Preview motion" affordance -- toggled from the editor
    // menu below. While on, editor_frame() stops re-forcing
    // simulation_suspended, letting a real turn run (creature idles,
    // lava/particle FX, and -- per a live report -- possibly needed for a
    // just-placed creature to render in the 3D view at all: it exists in
    // the sim and shows on the minimap immediately, but stays invisible in
    // the 3D view, matching things elsewhere this session that turned out
    // to need at least one real update() pass after creation/change before
    // they render correctly (the PI_HeartZoom instance was the other
    // example). Not confirmed yet -- this is the way to test it live.
    bool s_preview_motion = false;

    // Internal cleanup, no PckA_QuitToMainMenu -- see its two call sites
    // (editor_close() sends that packet itself; editor_frame()'s safety net
    // below runs after the game session has *already* ended some other way,
    // where sending it again would be meaningless at best).
    void editor_deactivate(void)
    {
        if (!s_editor_active)
            return;
        kfx_sim_state.simulation_suspended = false;
        set_skip_heart_zoom_feature(s_prev_skip_heart_zoom);
        s_editor_active = false;
        s_show_editor_menu = false;
    }

    void editor_menu_frame(void)
    {
        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("##EditorMenu", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);

        FeHeading(s_editor_dirty ? "Editor *" : "Editor");
        FeSeparator();

        const ImVec2 btn_size(220, 0);
        // Save/Save As/Playtest all need editor_save_map() (phase 3, the map
        // serializer) -- there is nothing to write to disk yet, so they're
        // visible but disabled rather than missing entirely.
        ImGui::BeginDisabled(true);
        FeButton("Save", btn_size);
        FeButton("Save As", btn_size);
        FeButton("Playtest", btn_size);
        ImGui::EndDisabled();

        // §3's "Preview motion" -- see s_preview_motion's own comment.
        FeCheckbox("Preview Motion (unpause)", &s_preview_motion);

        if (FeButton("Resume Editing", btn_size))
            s_show_editor_menu = false;
        if (FeButton("Exit to Main Menu", btn_size))
            editor_close();

        ImGui::End();
    }
}

void editor_open(LevelNumber lvnum, TbBool is_new)
{
    SYNCDBG(0, "Opening editor session for level %lu (new=%d)", (unsigned long)lvnum, (int)is_new);
    s_editor_active = true;
    // A freshly created blank map has nothing saved yet -- starts dirty.
    s_editor_dirty = is_new != 0;
    s_show_editor_menu = false;
    s_preview_motion = false;

    // §3, revised after live testing: simulation_suspended alone, *not*
    // GOF_Paused, is what freezes the sim now. game_session_loop.cpp's
    // per-turn update() gate checks both flags
    // (!GOF_Paused && !simulation_suspended), so simulation_suspended on
    // its own already fully freezes creature AI/economy/game-turn advance
    // -- but GOF_Paused turned out to do far more than that:
    // get_packet_control_mouse_clicks() (front_input.c) hard-returns
    // without generating *any* PCtr_LBtn*/RBtn* packet control while
    // GOF_Paused is set, which is what actually turns a world click into
    // a PckA_CheatPlaceTerrain (etc.) packet in the first place. Found
    // live: with GOF_Paused forced, every toolbox tool worked (packets
    // like PckA_SetPlyrState/PckA_CheatSwitchTerrain went through fine --
    // process_packets() itself has no pause gate) but clicking in the
    // dungeon view to actually place/build never did anything, because
    // the click was never turned into a packet at all. GOF_Paused is
    // simply the wrong tool here: it means "no player interaction",
    // not just "no simulation ticks", and an editor session needs exactly
    // the opposite combination. Not setting it at all now.
    kfx_sim_state.simulation_suspended = true;

    // Found live: with simulation_suspended now also gating the per-turn
    // update (game_session_loop.cpp), the normal "PI_HeartZoom" intro --
    // set_player_instance(player, PI_HeartZoom, 0) in game_loop(), right
    // after this function returns and wait_at_frontend() hands control
    // back -- can never finish; it needs several real turns of update() to
    // fly the camera to the Dungeon Heart, and none run once suspended.
    // Result: the session never leaves the intro, fully frozen. This
    // sequence is meaningless for an editor session anyway (no player-
    // camera cutscene wanted while editing) -- skip it outright via the
    // same feature flag -skipheartzoom uses, rather than trying to let a
    // few turns through. Restored in editor_close().
    s_prev_skip_heart_zoom = get_skip_heart_zoom_feature();
    set_skip_heart_zoom_feature(true);

    // No "enable cheats" step needed (docs/refactor/editor/
    // 07-investigation-findings.md F10) -- the underlying PckA_Cheat*
    // handlers just work. Called directly rather than via
    // set_players_packet_action() + PckA_Cheat* -- this is one-time editor
    // session setup, not a player action the packet/replay system needs to
    // see (docs/refactor/editor/07-investigation-findings.md D2's "single-
    // player direct-call exception"), and a direct call has no dependency
    // on when process_packets() next runs relative to the first rendered
    // frame. Reveal the whole map and put the editor player in a
    // build-anything state, same effects the cheat menu's "reveal map"/
    // "everything free" buttons already produce.
    struct PlayerInfo *player = get_my_player();
    reveal_whole_map(player);
    make_all_creatures_free();
    make_all_rooms_free();
    make_all_powers_cost_free();
    make_available_all_researchable_rooms(player->id_number);
    make_available_all_researchable_powers(player->id_number);
    make_available_all_doors(player->id_number);
    make_available_all_traps(player->id_number);

    // Found live (screenshot: solid black viewport that reveal-map alone
    // didn't fix): init_player_cameras() (engine_camera.c, called during
    // the normal init_players_local_game() path every session -- editor or
    // not) points the isometric/front-view cameras at
    // get_player_soul_container(player->id_number)'s position -- the
    // Dungeon Heart. On a heart-less New Map that lookup returns the
    // invalid-thing sentinel, so both cameras end up sitting at world
    // (0,0,z) -- off in the map's corner, not over any of the map at all.
    // Every real level has a Heart before this ever runs, so nothing else
    // in the engine has ever needed a fallback here. Only override when
    // there really is no heart -- opening an *existing* map (Open Map) has
    // a real Dungeon Heart and a correctly-centered camera already; forcing
    // map-center here unconditionally would wrongly override that.
    if (thing_is_invalid(get_player_soul_container(player->id_number)))
    {
        MapCoord center_x = subtile_coord_center(kfx_sim_state.map_subtiles_x / 2);
        MapCoord center_y = subtile_coord_center(kfx_sim_state.map_subtiles_y / 2);
        player->cameras[CamIV_Isometric].mappos.x.val = center_x;
        player->cameras[CamIV_Isometric].mappos.y.val = center_y;
        player->cameras[CamIV_FrontView].mappos.x.val = center_x;
        player->cameras[CamIV_FrontView].mappos.y.val = center_y;
    }
}

void editor_close(void)
{
    if (!s_editor_active)
        return;
    struct PlayerInfo *player = get_my_player();
    set_players_packet_action(player, PckA_QuitToMainMenu, 0, 0, 0, 0);
    editor_deactivate();
}

TbBool editor_is_active(void)
{
    return s_editor_active;
}

void editor_frame(void)
{
    if (!s_editor_active)
        return;

    // Safety net: found live -- quitting via the *normal* in-game pause
    // menu (not our own "Exit to Main Menu") left the toolbox rendering
    // over the main menu forever, because nothing but that one button ever
    // called editor_close(). keeper_gameplay_loop() (game_session_loop.cpp)
    // unconditionally resets game_kind to GKind_Unset once its loop ends,
    // by every exit path (quit, level lost/won, ...) -- treat that as "the
    // session is over" regardless of how, and clean up here instead of
    // depending on catching every possible exit route individually.
    if (kfx_sim_state.game_kind != GKind_LocalGame)
    {
        editor_deactivate();
        return;
    }

    // §3, revised: re-assert simulation_suspended every frame (not
    // GOF_Paused -- see editor_open()'s comment on why) so nothing
    // (a stray PckA_TogglePause, e.g.) can lift the freeze while active --
    // unless Preview Motion is on, in which case leave it lifted so a real
    // turn can run.
    kfx_sim_state.simulation_suspended = !s_preview_motion;

    // F10, not Escape: found live -- front_input.c's get_options_menu_inputs()
    // (the normal in-game pause menu) reads raw lbKeyOn[]/is_key_pressed(),
    // completely independent of ImGui's own key-event tracking here, so
    // both this menu and the normal GMnu_OPTIONS pause launcher opened at
    // once on the same Escape press. kfx_frontend can't be made to skip its
    // own handler while the editor is active without a new callback (it's
    // ranked below kfx_editor); picking a key the base game has no default
    // binding for sidesteps the conflict entirely. Temporary -- D6
    // (docs/refactor/editor/02-editing-toolbox.md §3) makes every editor
    // shortcut a definable Gkey_Editor* key; this is a placeholder default.
    if (ImGui::IsKeyPressed(ImGuiKey_F10, false))
        s_show_editor_menu = !s_show_editor_menu;

    // docs/refactor/editor/02-editing-toolbox.md -- shown whenever the
    // session is active, same as the original editor's always-visible
    // toolbox; the Esc menu (below) is a separate, independently toggled
    // window layered on top of it.
    editor_toolbox_frame();

    if (s_show_editor_menu)
        editor_menu_frame();
}

void editor_notify_playtest_end(void)
{
    // Playtest itself needs editor_save_map() to a scratch slot (§6, phase
    // 3) -- nothing to reload from yet, so this is a no-op until then.
}
/******************************************************************************/
