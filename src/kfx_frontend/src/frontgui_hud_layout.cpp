#include "pre_inc.h"
#include "frontgui_hud_layout.h"
#include "post_inc.h"

#include <algorithm>

namespace {

HudLayout s_current = {};
float s_last_w = -1.0f, s_last_h = -1.0f;

HudRect rect(float x0, float y0, float x1, float y1) { return HudRect{x0, y0, x1, y1}; }

// Current KeeperFX / DK1: a fixed-width column down the right edge --
// gold at the top, minimap below it, the tab strip, then the tab content
// filling the rest; the event markers run down a thin column just inside
// the left edge of the panel.
void build_vertical_right(HudLayout *o, float w, float h)
{
    o->kind = HudLayout_VerticalRight;

    // ~the DK1 140/640 proportion, but tracking display height so the
    // aspect of the panel itself stays stable on wide screens. Clamped so
    // it never eats the view on tiny windows or looks absurd on huge ones.
    float panel_w = h * 0.26f;
    panel_w = std::min(std::max(panel_w, 200.0f), 360.0f);
    panel_w = std::min(panel_w, w * 0.32f);

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

void hud_layout_build(HudLayout *out, HudPanelLayout kind, float display_w, float display_h)
{
    if (out == nullptr)
        return;
    switch (kind)
    {
        case HudLayout_VerticalRight: build_vertical_right(out, display_w, display_h); break;
        default:                      build_stub(out, kind, display_w, display_h);    break;
    }
}

const HudLayout &hud_layout_current(void) { return s_current; }

void hud_layout_frame(float display_w, float display_h)
{
    if (display_w == s_last_w && display_h == s_last_h)
        return;
    s_last_w = display_w;
    s_last_h = display_h;
    hud_layout_build(&s_current, s_current.kind, display_w, display_h);
}

extern "C" long hud_layout_viewport_inset(void)
{
    return (long)(s_current.viewport_inset + 0.5f);
}
