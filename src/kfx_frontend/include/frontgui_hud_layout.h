#ifndef FRONTGUI_HUD_LAYOUT_H
#define FRONTGUI_HUD_LAYOUT_H

// Phase 4 (docs/refactor/ingame-gui/05-sidebar-frame-and-minimap.md §0):
// the in-game HUD is composed of named regions, and a layout descriptor
// assigns each one a screen rect. Region code lays out *within* its rect --
// never against a 140-virtual-px constant or a hard screen edge -- so the
// current vertical-right sidebar and a future horizontal / minimal variant
// share one set of region code.
//
// This phase wires only HudLayout_VerticalRight. The other two exist in
// the enum so callers are written against the abstraction from the start.

#ifdef __cplusplus

struct HudRect { float x0, y0, x1, y1;
    float w() const { return x1 - x0; }
    float h() const { return y1 - y0; } };

enum HudPanelLayout {
    HudLayout_VerticalRight = 0, // default -- current KeeperFX / DK1
    HudLayout_HorizontalBottom,  // DK2-style bottom strip (not wired yet)
    HudLayout_Minimal,           // icons only, no tab content (not wired yet)
};

enum HudRegion {
    HudRegion_Minimap = 0, // minimap texture + compass
    HudRegion_TabStrip,    // the 5 tab headers + zoom / map / autopilot buttons
    HudRegion_TabContent,  // the active tab's body (Phase 5)
    HudRegion_Gold,        // the gold counter
    HudRegion_Events,      // the event-notification markers
    HudRegion_COUNT,
};

struct HudLayout {
    HudPanelLayout kind;
    HudRect region[HudRegion_COUNT];
    // How far the 3D viewport must inset, on this layout's axis:
    // a left-edge width for VerticalRight, a bottom-edge height for
    // HorizontalBottom. Consumed via render_overlay->get_status_panel_width().
    float viewport_inset;
};

// Recompute every region rect for `kind` at the given display size.
// Cheap -- call it on resize / layout switch, not per frame.
void hud_layout_build(HudLayout *out, HudPanelLayout kind, float display_w, float display_h);

// The process-wide current layout, rebuilt by hud_layout_frame() when the
// display size changes. hud_layout_frame() must run once per ImGui frame
// before any region is laid out.
const HudLayout &hud_layout_current(void);
void hud_layout_frame(float display_w, float display_h);
inline const HudRect &hud_region_rect(HudRegion r) { return hud_layout_current().region[r]; }

#endif // __cplusplus

// C-callable: the current viewport inset (panel width for VerticalRight),
// for render_overlay->get_status_panel_width(). 0 before the first frame.
#ifdef __cplusplus
extern "C" {
#endif
long hud_layout_viewport_inset(void);
#ifdef __cplusplus
}
#endif

#endif // FRONTGUI_HUD_LAYOUT_H
