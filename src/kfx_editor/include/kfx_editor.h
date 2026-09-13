/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file kfx_editor.h
 *     Public interface of the in-game level editor library.
 * @par Purpose:
 *     docs/refactor/editor/00-overview.md / 01-entry-and-editor-session.md:
 *     kfx_editor is the highest-ranked internal library (immediately below
 *     app_entry, above kfx_apploop). It owns the editor session lifecycle,
 *     the in-session ImGui toolbox, the map serializer and the blank-map
 *     builder. main.cpp is the only #include edge into this library --
 *     everything below it reaches the editor only via EditorCallbacks
 *     (src/kfx_config/include/editor_callbacks.h).
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 */
/******************************************************************************/
#ifndef DK_KFX_EDITOR_H
#define DK_KFX_EDITOR_H

#include "bflib_basics.h"
#include "globals.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

// Called once the sim is loaded and running for an editor session
// (kfx_apploop's `case FeSt_START_EDITOR:`, via EditorCallbacks::request_open
// -- kfx_apploop can't call this directly, it's ranked below kfx_editor).
// Marks the session active, reveals the whole map for the editor player and
// puts them in the god/build state (§3 -- no "enable cheats" step needed).
void editor_open(LevelNumber lvnum, TbBool is_new);

// Exit to Main Menu (§6): sends PckA_QuitToMainMenu, clears
// simulation_suspended and marks the session inactive.
void editor_close(void);

TbBool editor_is_active(void);

// ImGui submission + per-frame editor logic -- called every frame from
// main.cpp's ImGui-frame wrapper regardless of what's on screen; no-ops
// unless editor_is_active(). Re-asserts GOF_Paused every frame while
// suspended (§3) and draws the Esc editor menu / toolbox.
void editor_frame(void);

// Playtest (§6) hook: called when a playtest session ends (win/lose/quit),
// so "Return to editor" can reload the pre-playtest scratch-slot state.
void editor_notify_playtest_end(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
