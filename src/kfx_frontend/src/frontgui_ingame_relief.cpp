#include "pre_inc.h"
#include "frontgui_ingame_relief.h"
#include <imgui.h>
#include <imgui_internal.h> // ImDrawListSharedData::TexUvWhitePixel -- per-vertex-colour annulus
#include <cmath>
#include "post_inc.h"

// docs/refactor/ingame-gui/09-relief-and-emboss-pass.md -- procedural
// relief for the ImGui sidebar. ImDrawList primitives only.

namespace relief {

namespace {

// Lerp two packed colours through linear float space.
ImU32 col_lerp(ImU32 a, ImU32 b, float t)
{
    const ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a);
    const ImVec4 cb = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(
        ca.x + (cb.x - ca.x) * t,
        ca.y + (cb.y - ca.y) * t,
        ca.z + (cb.z - ca.z) * t,
        ca.w + (cb.w - ca.w) * t));
}

// Vertical gradient fill. AddRectFilledMultiColor can't round, so a
// rounded rect gets a flat mid fill instead (still reads recessed/raised
// once the bevel is on top).
void fill_grad(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, ImU32 top, ImU32 bot, float rounding)
{
    if (rounding <= 0.0f)
        dl->AddRectFilledMultiColor(a, b, top, top, bot, bot);
    else
        dl->AddRectFilled(a, b, col_lerp(top, bot, 0.5f), rounding);
}

// Bezel shade at one point on the ring: k = cos(angle - lit_dir), so +1 at
// the lit direction, -1 opposite. Smooth (no hard arc-to-base seam).
ImU32 ring_shade(unsigned int base, unsigned int hi, unsigned int lo, float k)
{
    if (k >= 0.0f)
        return col_lerp(base, hi, std::pow(k, 0.85f) * 0.92f);
    return col_lerp(base, lo, std::pow(-k, 0.90f) * 0.88f);
}

// --- cheap deterministic value noise, for the marble surface pass -------
float m_hash(int x, int y)
{
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFFu) * (1.0f / 65535.0f);
}

float m_vnoise(float x, float y)
{
    const float fx = std::floor(x), fy = std::floor(y);
    const int xi = (int)fx, yi = (int)fy;
    const float xf = x - fx, yf = y - fy;
    const float u = xf * xf * (3.0f - 2.0f * xf);
    const float v = yf * yf * (3.0f - 2.0f * yf);
    const float a = m_hash(xi, yi),     b = m_hash(xi + 1, yi);
    const float c = m_hash(xi, yi + 1), d = m_hash(xi + 1, yi + 1);
    return a + (b - a) * u + (c - a) * v + (a - b + d - c) * u * v;
}

float m_fbm(float x, float y)
{
    return 0.6f  * m_vnoise(x, y)
         + 0.3f  * m_vnoise(x * 2.1f + 5.2f, y * 2.1f + 1.3f)
         + 0.15f * m_vnoise(x * 4.3f + 9.1f, y * 4.3f + 7.7f);
}

} // namespace

const Tones &tones()
{
    // Ramp built around the panel base colour rgb(60,44,12) (sampled from
    // the reference art -- a warm brown, almost no blue).
    static const Tones T = {
        /* base        */ IM_COL32( 60,  44,  12, 236),
        /* plateau_top */ IM_COL32( 86,  64,  20, 242), // clearly lit -- a visible top-down gradient
        /* plateau_bot */ IM_COL32( 38,  28,   8, 242),
        /* well_top    */ IM_COL32( 20,  14,   4, 244), // a dark brown pocket -- not near-black
        /* well_bot    */ IM_COL32( 37,  27,   8, 242),
        /* hi          */ IM_COL32(204, 168, 104, 235), // bright bronze -- the raised-edge catch-light
        /* lo          */ IM_COL32(  0,   0,   0, 205),
        /* crown_lit   */ IM_COL32(176, 110,  34, 246),
        /* groove_dk   */ IM_COL32(  0,   0,   0, 225),
        /* groove_lt   */ IM_COL32(172, 136,  78, 150),
        /* mottle_lt   */ IM_COL32(184, 146,  86, 255),
        /* mottle_dk   */ IM_COL32(  6,   3,   0, 255),
    };
    return T;
}

unsigned int mix(unsigned int a, unsigned int b, float t)
{
    return col_lerp(a, b, t);
}

unsigned int tab_fill(bool active)
{
    const Tones &T = tones();
    const unsigned int face_mid = col_lerp(T.plateau_top, T.plateau_bot, 0.55f);
    if (active)
        return face_mid;
    return col_lerp(face_mid, col_lerp(T.well_top, T.well_bot, 0.5f), 0.5f);
}

void bevel(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, float width, bool raised, float rounding,
           bool omit_bottom)
{
    (void)rounding; // lines track the rect edges; corner rounding is cosmetic-minor here
    const Tones &T = tones();
    const ImU32 e_hi = raised ? T.hi : T.lo;
    const ImU32 e_lo = raised ? T.lo : T.hi;

    const float shorter = (b.x - a.x < b.y - a.y) ? (b.x - a.x) : (b.y - a.y);
    int w = (int)(width + 0.5f);
    if (w < 1) w = 1;
    int wmax = (int)(shorter * 0.5f);
    if (wmax < 1) wmax = 1;
    if (w > wmax) w = wmax;

    for (int i = 0; i < w; i++)
    {
        // Fade outer->inner but keep the innermost line at ~0.45, not 0.
        const float f = (w > 1) ? 1.0f - 0.55f * (float)i / (float)(w - 1) : 1.0f;
        const ImU32 h = ImGui::GetColorU32(e_hi, f);
        const ImU32 l = ImGui::GetColorU32(e_lo, f);
        const float x0 = a.x + (float)i + 0.5f, y0 = a.y + (float)i + 0.5f;
        const float x1 = b.x - (float)i - 0.5f, y1 = b.y - (float)i - 0.5f;
        dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y0), h); // top
        dl->AddLine(ImVec2(x0, y0), ImVec2(x0, y1), h); // left
        if (!omit_bottom)
            dl->AddLine(ImVec2(x0, y1), ImVec2(x1, y1), l); // bottom
        dl->AddLine(ImVec2(x1, y0), ImVec2(x1, y1), l); // right
    }
}

void face(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b)
{
    const Tones &T = tones();
    fill_grad(dl, a, b, T.plateau_top, T.plateau_bot, 0.0f);
    mottle(dl, a, b);
}

void plateau(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, float rounding)
{
    const Tones &T = tones();
    fill_grad(dl, a, b, T.plateau_top, T.plateau_bot, rounding);
    bevel(dl, a, b, 2.0f, true, rounding);
}

void well(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, float rounding)
{
    const Tones &T = tones();
    fill_grad(dl, a, b, T.well_top, T.well_bot, rounding);
    bevel(dl, a, b, 2.0f, false, rounding);
    dl->AddLine(ImVec2(a.x + 1.5f, a.y + 1.5f), ImVec2(b.x - 1.5f, a.y + 1.5f),
                ImGui::GetColorU32(T.lo, 0.55f));
}

void well_circle(ImDrawList *dl, const ImVec2 &c, float radius)
{
    if (radius < 1.0f) return;
    dl->AddCircleFilled(c, radius, tones().well_bot, 72);
}

void well_tri(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImVec2 &c)
{
    const Tones &T = tones();
    dl->AddTriangleFilled(a, b, c, col_lerp(T.well_top, T.well_bot, 0.5f));
    const float cx = (a.x + b.x + c.x) / 3.0f;
    const float cy = (a.y + b.y + c.y) / 3.0f;
    auto edge = [&](const ImVec2 &p, const ImVec2 &q) {
        const float mx = (p.x + q.x) * 0.5f, my = (p.y + q.y) * 0.5f;
        const bool up_left = ((mx - cx) + (my - cy)) < 0.0f; // edge faces the light
        dl->AddLine(p, q, up_left ? ImGui::GetColorU32(T.lo, 0.85f)
                                  : ImGui::GetColorU32(T.hi, 0.85f), 1.5f);
    };
    edge(a, b); edge(b, c); edge(c, a);
}

void boss(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, float lit, float rounding, bool omit_bottom)
{
    const Tones &T = tones();
    if (!omit_bottom)
        dl->AddRectFilled(ImVec2(a.x + 1.5f, a.y + 2.5f), ImVec2(b.x + 1.5f, b.y + 2.5f),
                          IM_COL32(0, 0, 0, 70), rounding);
    const float k = (lit < 0.0f) ? 0.0f : (lit > 1.0f ? 1.0f : lit);
    const ImU32 crown = (k > 0.0f) ? col_lerp(T.plateau_top, T.crown_lit, k) : T.plateau_top;
    fill_grad(dl, a, b, crown, T.base, rounding);
    bevel(dl, a, b, 2.0f, true, rounding, omit_bottom);
}

void groove_h(ImDrawList *dl, float x0, float x1, float y)
{
    const Tones &T = tones();
    dl->AddLine(ImVec2(x0, y + 0.5f), ImVec2(x1, y + 0.5f), T.groove_dk, 2.0f);
    dl->AddLine(ImVec2(x0, y + 2.5f), ImVec2(x1, y + 2.5f), T.groove_lt, 1.0f);
}

void groove(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b)
{
    const Tones &T = tones();
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f) return;
    const float nx = -dy / len, ny = dx / len; // one perpendicular
    // Put the light line on whichever side faces down-right.
    const float s = (nx + ny >= 0.0f) ? 1.6f : -1.6f;
    dl->AddLine(a, b, T.groove_dk, 2.0f);
    dl->AddLine(ImVec2(a.x + nx * s, a.y + ny * s), ImVec2(b.x + nx * s, b.y + ny * s),
                T.groove_lt, 1.0f);
}

void ring(ImDrawList *dl, const ImVec2 &c, float r_outer, float r_inner, float lit_dir)
{
    const Tones &T = tones();
    if (r_outer < r_inner + 2.0f) r_outer = r_inner + 2.0f;

    // Filled annulus with a smooth angular gradient -- brightest toward
    // lit_dir, darkest opposite -- via per-vertex colour. No hard seam
    // where a lit arc would butt into the base tone.
    const int N = 72;
    const ImVec2 uv = dl->_Data->TexUvWhitePixel;
    dl->PrimReserve(N * 6, N * 4);
    for (int i = 0; i < N; i++)
    {
        const float a0 = (float)i       * (6.2831853f / (float)N);
        const float a1 = (float)(i + 1) * (6.2831853f / (float)N);
        const ImU32 s0 = ring_shade(T.base, T.hi, T.lo, std::cos(a0 - lit_dir));
        const ImU32 s1 = ring_shade(T.base, T.hi, T.lo, std::cos(a1 - lit_dir));
        const float ca0 = std::cos(a0), sa0 = std::sin(a0);
        const float ca1 = std::cos(a1), sa1 = std::sin(a1);
        const unsigned int bi = dl->_VtxCurrentIdx;
        dl->PrimWriteIdx((ImDrawIdx)(bi + 0)); dl->PrimWriteIdx((ImDrawIdx)(bi + 1)); dl->PrimWriteIdx((ImDrawIdx)(bi + 2));
        dl->PrimWriteIdx((ImDrawIdx)(bi + 0)); dl->PrimWriteIdx((ImDrawIdx)(bi + 2)); dl->PrimWriteIdx((ImDrawIdx)(bi + 3));
        dl->PrimWriteVtx(ImVec2(c.x + ca0 * r_outer, c.y + sa0 * r_outer), uv, s0);
        dl->PrimWriteVtx(ImVec2(c.x + ca0 * r_inner, c.y + sa0 * r_inner), uv, s0);
        dl->PrimWriteVtx(ImVec2(c.x + ca1 * r_inner, c.y + sa1 * r_inner), uv, s1);
        dl->PrimWriteVtx(ImVec2(c.x + ca1 * r_outer, c.y + sa1 * r_outer), uv, s1);
    }
    // Soft lip lines -- gentle, not the earlier hard black rings.
    dl->AddCircle(c, r_inner, ImGui::GetColorU32(T.lo, 0.5f), 96, 1.5f);
    dl->AddCircle(c, r_outer, ImGui::GetColorU32(T.groove_dk, 0.6f), 96, 1.0f);
}

void edge_frame(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b)
{
    bevel(dl, a, b, 3.0f, true, 0.0f);
}

void mottle(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b)
{
    const Tones &T = tones();
    const float x0 = a.x + 2.0f, y0 = a.y + 2.0f;
    const float x1 = b.x - 2.0f, y1 = b.y - 2.0f;
    if (x1 <= x0 || y1 <= y0) return;

    // Procedural marble: veins where sin(freq*(p + amp*fbm(p))) peaks. A
    // domain-warped sine gives graduated wavy veins rather than random
    // flecks. Sampled in rect-relative coords -> deterministic, stable
    // between runs, just re-lays when the rect changes.
    const float cell = 6.0f;
    const float ns   = 0.012f;  // noise sampling frequency
    const float amp  = 44.0f;   // domain warp strength
    const float freq = 0.030f;  // vein frequency
    for (float y = y0; y < y1; y += cell)
        for (float x = x0; x < x1; x += cell)
        {
            const float px = x - a.x, py = y - a.y;
            const float warp = m_fbm(px * ns, py * ns) - 0.4f;
            const float m = std::sin(freq * (px + py * 0.4f + amp * warp)); // -1..1
            const float t = 0.5f + 0.5f * m;
            const float lightv = t * t * t;                     // veins
            const float darkv  = (1.0f - t) * (1.0f - t) * (1.0f - t);
            if (lightv < 0.09f && darkv < 0.09f)
                continue;
            const ImU32 col = (lightv >= darkv)
                ? ImGui::GetColorU32(T.mottle_lt, lightv * 0.18f)
                : ImGui::GetColorU32(T.mottle_dk, darkv  * 0.22f);
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cell + 0.5f, y + cell + 0.5f), col);
        }
}

} // namespace relief
