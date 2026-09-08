#ifndef FRONTGUI_INGAME_PANEL_H
#define FRONTGUI_INGAME_PANEL_H

// Phase 4 (docs/refactor/ingame-gui/05-sidebar-frame-and-minimap.md): the
// always-on sidebar frame -- procedural background, gold counter, the 5
// tab headers, the zoom / map / autopilot buttons and the 13 event
// markers -- as ImGui. First pass keeps the current fixed-width column and
// leaves the minimap + tab content legacy-drawn (they show through a
// transparent hole; their geometry and this panel's both derive from
// status_panel_width / the GMnu_MAIN menu rect).

#ifdef __cplusplus
// C++ only: called from ingame_imgui_frame().
void ingame_panel_frame(void);
#endif

#endif // FRONTGUI_INGAME_PANEL_H
