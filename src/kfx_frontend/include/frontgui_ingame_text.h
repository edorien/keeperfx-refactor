#ifndef FRONTGUI_INGAME_TEXT_H
#define FRONTGUI_INGAME_TEXT_H

// Phase 3 (docs/refactor/ingame-gui/04-messages-tooltips-infobox.md):
// the loose in-engine status text -- the "Paused" caption and the
// top-of-screen warning banner / out-of-sync lines -- on the ImGui font
// engine when RendererImGuiEnabled(). The legacy draws (engine_redraw.c,
// gui_topmsg.c) early-return under ImGui; their state / timer side effects
// still run.

#ifdef __cplusplus
extern "C" {
#endif

void ingame_text_overlays_frame(void);

// The context tooltip (gui_tooltips.c's tool_tip_box). Drawn last, on top
// of the menus, so call it after the menu dispatch.
void ingame_tooltip_frame(void);

#ifdef __cplusplus
}
#endif

#endif // FRONTGUI_INGAME_TEXT_H
