#ifndef FRONTGUI_STYLESHEET_TEST_H
#define FRONTGUI_STYLESHEET_TEST_H

// Phase B proving ground (docs/refactor/renderer/04-imgui-gui-foundation.md
// §7 Phase B exit criteria): "a style-sheet test screen exercising every
// wrapper... legible at 640x480 through 4K, with both display faces".
// Registered as the RendererImGuiFrameFn callback (RendererManager.h) from
// main.cpp::setup_game(), gated behind the -imguistyle debug flag -- same
// shape as Phase A's -imguidemo proof, aimed at frontgui_widgets.h instead
// of vanilla ImGui.

void FeStyleSheetSetVisible(bool visible);
void FeStyleSheetFrame(); // the registered callback body

#endif // FRONTGUI_STYLESHEET_TEST_H
