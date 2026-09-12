#include "pre_inc.h"
#include "frontgui_hud_layout.h"
#include "frontgui_ingame_layout.h" // tcl::BODY_Y0 -- build_minimal()'s pop-up content-fraction sizing
#include "post_inc.h"

#include <algorithm>

namespace {

HudLayout s_current = {};
float s_last_w = -1.0f, s_last_h = -1.0f;
HudBottomWidthMode s_last_b_width_mode = HudBottomWidth_Normal;
HudMinimalCorner s_last_corner = HudMinimalCorner_UpperLeft;

HudRect rect(float x0, float y0, float x1, float y1) { return HudRect{x0, y0, x1, y1}; }

// ~the DK1 140/640 proportion, but tracking display height so the aspect
// of the panel itself stays stable on wide screens. Clamped so it never
// eats the view on tiny windows or looks absurd on huge ones. Shared with
// build_minimal() (docs/refactor/ingame-gui/13-minimal-layout.md: the
// pop-up panel is sized off this same math, not a bespoke size).
float vertical_panel_width(float w, float h)
{
    float panel_w = h * 0.26f;
    panel_w = std::min(std::max(panel_w, 200.0f), 360.0f);
    return std::min(panel_w, w * 0.32f);
}

// Current KeeperFX / DK1: a fixed-width column down the right edge --
// gold at the top, minimap below it, the tab strip, then the tab content
// filling the rest; the event markers run down a thin column just inside
// the left edge of the panel.
void build_vertical_right(HudLayout *o, float w, float h)
{
    o->kind = HudLayout_VerticalRight;

    const float panel_w = vertical_panel_width(w, h);
    const float px0 = w - panel_w;
    const float pad = panel_w * 0.05f;
    const float ev_w = std::max(panel_w * 0.13f, 22.0f);

    const float gold_h = std::max(h * 0.05f, 28.0f);
    float y = pad;
    o->region[HudRegion_Gold] = rect(px0 + ev_w, y, w - pad, y + gold_h);
    y += gold_h + pad;

    // Minimap: a square using the panel's inner width.
    const float mm_w = w - pad - (px0 + ev_w);
    o->region[HudRegion_Minimap] = rect(px0 + ev_w, y, w - pad, y + mm_w);
    y += mm_w + pad;

    const float tabstrip_h = std::max(h * 0.06f, 34.0f);
    o->region[HudRegion_TabStrip] = rect(px0 + ev_w, y, w - pad, y + tabstrip_h);
    y += tabstrip_h + pad * 0.5f;

    o->region[HudRegion_TabContent] = rect(px0 + ev_w, y, w - pad, h - pad);

    o->region[HudRegion_Events] = rect(px0, pad, px0 + ev_w, h - pad);

    o->viewport_inset = panel_w;
}

// docs/refactor/ingame-gui/11-horizontal-layout.md: a strip along the
// bottom edge, three regions left to right -- gold+minimap+nav (a roughly
// square column, reusing frontgui_ingame_panel.cpp's existing head
// drawing verbatim against this column's own rect), tab-header row + a
// grid/panel below it (region B, width set by `b_width_mode` -- see its
// own comment, frontgui_hud_layout.h), then the event-marker row + the
// message queue at a fixed width flush against the right edge.
void build_horizontal_bottom(HudLayout *o, HudBottomWidthMode b_width_mode, float w, float h)
{
    o->kind = HudLayout_HorizontalBottom;

    // Same proportional-clamp shape build_vertical_right() uses for its
    // own axis (there: width off h; here: height off w).
    float strip_h = w * 0.22f;
    strip_h = std::min(std::max(strip_h, 180.0f), 320.0f);
    strip_h = std::min(strip_h, h * 0.35f);

    const float y0 = h - strip_h;
    const float pad = strip_h * 0.05f;

    // Region A: gold (thin strip) above the minimap. render_minimap()/
    // draw_minimap_and_compass() size the minimap's diameter from width
    // alone (mm_upp derives from s_menu_rect.w, not .h -- see
    // draw_panel_horizontal()'s aliasing) -- so the minimap sub-rect MUST
    // be square, its side set by the *height* left over after the gold
    // strip, not by some independently-chosen width. Getting this wrong
    // let the circle (sized to whatever width) overflow past a too-short
    // height, pushing the nav buttons anchored to its bottom edge off the
    // bottom of the screen (live-tested).
    const float gold_h = std::max(strip_h * 0.14f, 20.0f);
    const float mm_side = strip_h - 2.5f * pad - gold_h;
    o->region[HudRegion_Gold] = rect(pad, y0 + pad, pad + mm_side, y0 + pad + gold_h);
    o->region[HudRegion_Minimap] = rect(pad, y0 + pad + gold_h + pad * 0.5f,
                                        pad + mm_side, y0 + strip_h - pad);

    // Region C: event-marker row + the message queue, a FIXED width
    // (enough for the queue's text and a handful of event tokens, not
    // tied to screen width) flush against the right edge.
    const float region_c_w  = std::max(w * 0.22f, 280.0f);
    const float region_c_x0 = w - pad - region_c_w;
    const float ev_h = std::max(strip_h * 0.16f, 28.0f);
    o->region[HudRegion_Events] = rect(region_c_x0, y0 + pad, w - pad, y0 + pad + ev_h);
    o->region[HudRegion_Messages] = rect(region_c_x0, y0 + pad + ev_h + pad * 0.5f,
                                         w - pad, y0 + strip_h - pad);

    // Region B: tab-header row, then the grid/panel. Width depends on
    // b_width_mode: Normal is capped -- big enough for 6 comfortably-
    // spaced Room/Spell/Trap columns, not the screen (an earlier attempt
    // let it fill everything between A and C, which on a wide screen
    // spread the grid's cells apart with wide gaps between them,
    // live-tested: "too wide, icons take up too much space"). Wide fills
    // all the way to region C's edge (the creature strip: "only being
    // able to view 5 creatures isn't ideal"). Narrow is smaller than the
    // default (possession/query: "reduce the whitespace on the right").
    // Any leftover width between B and C in the Normal/Narrow cases is
    // just blank panel face.
    const float region_b_x0 = pad * 2.0f + mm_side;
    const float region_b_w_normal = std::min(std::max(strip_h * 2.2f, 440.0f), w * 0.5f);
    float region_b_w;
    switch (b_width_mode)
    {
        case HudBottomWidth_Wide:   region_b_w = std::max(region_c_x0 - pad - region_b_x0, 260.0f); break;
        case HudBottomWidth_Narrow: region_b_w = std::min(std::max(strip_h * 1.7f, 380.0f), w * 0.4f); break;
        default:                    region_b_w = region_b_w_normal; break;
    }
    const float tabstrip_h  = std::max(strip_h * 0.16f, 28.0f);
    // TabStrip stays pinned to the Normal-mode width regardless of
    // b_width_mode -- it's the tab-switching control itself, and a resize
    // driven by *which tab you just clicked* read as broken (live-tested:
    // "the tabs selection changes width when creature pane selected").
    // Only TabContent (the grid/panel body below it) follows b_width_mode.
    o->region[HudRegion_TabStrip] = rect(region_b_x0, y0 + pad,
                                         region_b_x0 + region_b_w_normal, y0 + pad + tabstrip_h);
    o->region[HudRegion_TabContent] = rect(region_b_x0, y0 + pad + tabstrip_h + pad * 0.5f,
                                           region_b_x0 + region_b_w, y0 + strip_h - pad);

    // Full-screen 3D under the ImGui HUD either way -- see
    // render_overlay_get_status_panel_width()'s own comment (main.cpp).
    o->viewport_inset = 0.0f;
}

// docs/refactor/ingame-gui/13-minimal-layout.md: no persistent panel at
// all -- a corner-pinned minimap+gold+event-marker cluster (upper-left or
// upper-right, `corner`), and a free-floating button cluster + pop-up
// panel in the diagonally opposite (bottom) corner. Both clusters are
// auto-resize ImGui windows in practice (frontgui_ingame_panel.cpp /
// frontgui_ingame_tabcontent.cpp), so most of what this builds are anchor
// points for SetNextWindowPos() rather than boxes those windows are
// clipped into -- HudRegion_TabStrip's rect is nominal (only its corner is
// read); HudRegion_TabContent's *is* a real sized rect, since the pop-up
// isn't docked against anything else that already has one.
void build_minimal(HudLayout *o, HudMinimalCorner corner, float w, float h)
{
    o->kind = HudLayout_Minimal;
    const bool right = (corner == HudMinimalCorner_UpperRight);

    // Minimap cluster: same proportional-clamp shape as the other two
    // layouts' own sizing (there: panel width off h; region-A side off
    // strip_h off w) -- here, a square corner column sized off h.
    float mm_side = h * 0.22f;
    mm_side = std::min(std::max(mm_side, 140.0f), 260.0f);
    mm_side = std::min(mm_side, w * 0.30f);

    const float pad = mm_side * 0.06f;
    const float gold_h = std::max(mm_side * 0.14f, 20.0f);
    const float mm_x0 = right ? (w - pad - mm_side) : pad;
    const float mm_x1 = mm_x0 + mm_side;

    // Minimap first (top), gold directly below it (live-tested refinement:
    // originally gold-above-minimap, matching the vertical layout's own
    // convention, read as backwards once tried against Minimal's own
    // "minimap is the anchor" framing -- moving the minimap up to the very
    // top and the gold strip below it reads better here).
    const float mm_y0 = pad;
    o->region[HudRegion_Minimap] = rect(mm_x0, mm_y0, mm_x1, mm_y0 + mm_side);
    const float gold_y0 = mm_y0 + mm_side + pad * 0.5f;
    o->region[HudRegion_Gold] = rect(mm_x0, gold_y0, mm_x1, gold_y0 + gold_h);

    // Event markers: a vertical stack directly below the gold strip, same
    // edge as the minimap (live-tested decision, doc 13 §1.3 -- "stack
    // alongside the minimap" rather than an independent fixed screen
    // edge). However many tokens fit the remaining height is however many
    // show -- matches the existing layouts' own "no scroll, just crowd"
    // approach to event-marker overflow.
    const float ev_y0 = gold_y0 + gold_h + pad;
    o->region[HudRegion_Events] = rect(mm_x0, ev_y0, mm_x1, h - pad);

    // Button cluster + pop-up: the diagonally opposite (bottom) corner.
    // TabStrip's rect is nominal -- draw_button_cluster_minimal() only
    // reads its (x1,y1) corner (bottom-right if buttons are bottom-right,
    // i.e. minimap upper-left) as the auto-resize window's pivot point.
    // cluster_sz must match draw_button_cluster_minimal()'s own button
    // size exactly (both files comment this) -- the pop-up's own bottom
    // edge is computed from it below, and an earlier flat 44px guess left
    // too little clearance once the cluster window's real height (that
    // function now forces WindowPadding to (0,0) so its true size is
    // exactly cluster_sz) was accounted for: the pop-up (opened after the
    // cluster, so topmost wherever they touch) silently ate clicks meant
    // for the row underneath (live-tested: "not possible to select a
    // different one, once one is open").
    const float cluster_sz = 40.0f;
    const float cluster_x = right ? pad : (w - pad);
    o->region[HudRegion_TabStrip] = rect(cluster_x, h - pad, cluster_x, h - pad);

    // Pop-up panel: sized off the vertical layout's own panel width/height
    // math (decided -- not a bespoke size), its bottom edge a full `pad`
    // clear of the button cluster's own top edge. Height only covers the
    // "content" slice of the vertical layout's 400-unit virtual space
    // (tcl::BODY_Y0..400, frontgui_ingame_layout.h -- every body function
    // this pop-up calls starts drawing around BODY_Y0, the space above
    // that being minimap/gold/tabstrip chrome that doesn't exist here) --
    // sizing to the *full* virtual height left a big blank gap above the
    // actual icons (live-tested: "too much unnecessary whitespace ...
    // between the top of the panel and the icons/selection recess").
    // ingame_tabcontent_draw() (frontgui_ingame_tabcontent.cpp) applies
    // the matching virtual-space remap so content lands flush with the
    // pop-up's own top edge instead of half-way down it.
    const float content_frac = (400.0f - tcl::BODY_Y0) / 400.0f;
    const float popup_w = vertical_panel_width(w, h);
    const float popup_bottom = h - pad - cluster_sz - pad;
    const float popup_h = std::min(std::max(h * 0.62f * content_frac, 220.0f), popup_bottom - pad);
    const float popup_x0 = right ? pad : (w - pad - popup_w);
    o->region[HudRegion_TabContent] = rect(popup_x0, popup_bottom - popup_h, popup_x0 + popup_w, popup_bottom);

    // Full-screen 3D under the ImGui HUD -- no panel silhouette at all.
    o->viewport_inset = 0.0f;
}

// Placeholder shapes for the not-yet-wired layouts, so callers that ask
// for a region never get uninitialised garbage.
void build_stub(HudLayout *o, HudPanelLayout kind, float w, float h)
{
    o->kind = kind;
    for (int i = 0; i < HudRegion_COUNT; i++)
        o->region[i] = rect(0, 0, 0, 0);
    o->viewport_inset = 0.0f;
    (void)w; (void)h;
}

} // namespace

void hud_layout_build(HudLayout *out, HudPanelLayout kind, HudBottomWidthMode b_width_mode,
                      HudMinimalCorner corner, float display_w, float display_h)
{
    if (out == nullptr)
        return;
    switch (kind)
    {
        case HudLayout_VerticalRight:     build_vertical_right(out, display_w, display_h);   break;
        case HudLayout_HorizontalBottom:  build_horizontal_bottom(out, b_width_mode, display_w, display_h); break;
        case HudLayout_Minimal:           build_minimal(out, corner, display_w, display_h);   break;
        default:                          build_stub(out, kind, display_w, display_h);        break;
    }
}

const HudLayout &hud_layout_current(void) { return s_current; }

void hud_layout_frame(HudPanelLayout kind, HudBottomWidthMode b_width_mode,
                      HudMinimalCorner corner, float display_w, float display_h)
{
    if (kind == s_current.kind && b_width_mode == s_last_b_width_mode && corner == s_last_corner
     && display_w == s_last_w && display_h == s_last_h)
        return;
    s_last_w = display_w;
    s_last_h = display_h;
    s_last_b_width_mode = b_width_mode;
    s_last_corner = corner;
    hud_layout_build(&s_current, kind, b_width_mode, corner, display_w, display_h);
}

extern "C" long hud_layout_viewport_inset(void)
{
    return (long)(s_current.viewport_inset + 0.5f);
}
