#ifndef FRONTGUI_INGAME_H
#define FRONTGUI_INGAME_H

// docs/refactor/ingame-gui/01-seam-and-toggle.md (Phase 0): the per-GMnu_*
// opt-in seam for migrating the in-game HUD/menus to Dear ImGui, mirroring
// frontgui_screens.cpp's per-FrontendMenuState seam for the frontend.
//
// frontend.cpp's draw dispatch (draw_active_menus_buttons) and
// front_input.c's input dispatch (get_gui_inputs) both consult
// ingame_imgui_menu_active() so the sprite and ImGui paths never both
// drive one menu -- the same "a given menu belongs entirely to one
// system" discipline the frontend migration used.

#include "bflib_basics.h"
#include "globals.h" // MenuID

#ifdef __cplusplus
extern "C" {
#endif

// True when `menu_id` (a GMnu_*) should be drawn/driven by ImGui this
// frame rather than the legacy sprite path: "ImGui enabled (-classicmenu
// off) AND a level is running AND this menu has an ImGui implementation
// registered". Un-migrated menus always return false regardless of the
// toggle; the frontend and in-game arms are mutually exclusive via
// kfx_sim_state.game_kind.
TbBool ingame_imgui_menu_active(MenuID menu_id);

// True when a migrated, turned-on in-game menu is a monopoly (modal) menu.
// While this holds, get_gui_inputs() treats the whole screen as owned by
// the modal (busy_doing_gui = 1, world clicks suppressed) and the cursor
// path already unifies via ImGui's modal-popup WantCaptureMouse.
TbBool ingame_imgui_modal_active(void);

// True when an in-game ImGui window has the mouse this frame (one-frame
// lag via io.WantCaptureMouse). For *non*-monopoly migrated boxes (the
// event box, the cheat boxes) -- get_gui_inputs() sets busy_doing_gui so
// a click over the box doesn't also dig/cast in the world behind it.
TbBool ingame_imgui_wants_mouse(void);

// Submit the active migrated in-game menus' ImGui windows. Called from
// FrontendImGuiFrame() every present; no-op unless a level is running
// with a migrated menu turned on. Also applies this module's own
// deferred actions and refreshes the once-per-game-turn HUD view-model
// cache (§3.1) -- both must run before any ImGui window here is opened.
void ingame_imgui_frame(void);

// The action the migrated quit-confirm modal's "Yes" button performs --
// exposed so src/ftests/ can assert the command-packet contract without
// simulating an ImGui click.
void ingame_quitmenu_confirm(void);

// Collapse the in-game Options settings window back to the 4-button
// launcher. Called from frontgui_options_frame()'s in-game "Back" button.
void ingame_options_back_to_launcher(void);

#ifdef __cplusplus
}
#endif

#endif // FRONTGUI_INGAME_H
