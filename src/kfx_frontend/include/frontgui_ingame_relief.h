#ifndef FRONTGUI_INGAME_RELIEF_H
#define FRONTGUI_INGAME_RELIEF_H

// Procedural relief / emboss primitives for the ImGui in-game sidebar
// (docs/refactor/ingame-gui/09-relief-and-emboss-pass.md). ImDrawList only,
// no textures -- turns the flat HUD chrome into raised plateaus, recessed
// wells, bevelled bosses and engraved grooves.
//
// This is the VerticalRight layout's skin. The primitives are geometry-
// driven (take a rect / centre), so a sibling layout can reuse them, but a
// different composition module may call them differently or bring its own.
//
// C++ only (ImGui types). Callers: frontgui_ingame_panel.cpp,
// frontgui_ingame_tabcontent.cpp. Every bevel width is an absolute pixel
// count -- deliberately not resolution-scaled (09 §3).

#ifdef __cplusplus

struct ImDrawList;
struct ImVec2;

namespace relief {

// One shared tone ramp -- the single place to retune, or to later sample
// from front.pal instead of these placeholders (09 §5 "palette drift").
struct Tones {
    unsigned int base;         // flat mid surface
    unsigned int plateau_top;  // raised-plateau gradient, lit top
    unsigned int plateau_bot;  // raised-plateau gradient, bottom
    unsigned int well_top;     // recessed-well gradient, dark top
    unsigned int well_bot;     // recessed-well gradient, bottom
    unsigned int hi;           // bevel highlight (top/left of a raised edge)
    unsigned int lo;           // bevel shadow    (bottom/right of a raised edge)
    unsigned int crown_lit;    // boss crown when lit (selected / hover / on)
    unsigned int groove_dk;    // engraved groove, dark line
    unsigned int groove_lt;    // engraved groove, light line
    unsigned int mottle_lt;    // mottle fleck, light
    unsigned int mottle_dk;    // mottle fleck, dark
};
const Tones &tones();

// State-signal + bar-fill colours -- the accents layered on top of the
// relief material by the HUD cell / bar code (frontgui_ingame_tabcontent.cpp,
// frontgui_ingame_panel.cpp). One place to retune, alongside `Tones`.
// docs/refactor/ingame-gui/10-maintainability-refactors.md §4.
struct Accents {
    unsigned int text;      // primary label
    unsigned int subtext;   // secondary / dim label
    unsigned int border;    // resting cell outline
    unsigned int disabled;  // greyed content
    unsigned int sel;       // selected (gold rim)
    unsigned int hover;     // hovered (red rim)
    unsigned int have;      // "already own one" dot (green)
    unsigned int hotkey;    // ability hotkey number (yellow)
    unsigned int bar_good;  // capacity / xp / cooldown-ready fill
    unsigned int bar_warn;  // anger / mid health
    unsigned int bar_bad;   // low health / sell "$"
};
const Accents &accents();

// Linear blend of two packed colours (through float space).
unsigned int mix(unsigned int a, unsigned int b, float t);

// Tab fills: `active` matches the panel face (so the active tab blends into
// the content); inactive sits halfway between the face and the recess.
unsigned int tab_fill(bool active);

// Generalised two-tone bevel on a rect: `raised` => hi on top+left, lo on
// bottom+right; !raised => sunken (swapped). `width` px, clamped so it
// never exceeds half the smaller side. `omit_bottom` skips the bottom edge
// (an element that merges downward into whatever is below it -- e.g. the
// active tab into the content area).
void bevel(ImDrawList *dl, const ImVec2 &p_min, const ImVec2 &p_max,
           float width, bool raised, float rounding = 0.0f, bool omit_bottom = false);

// The panel's raised stone face: vertical gradient (lit top) + one mottle
// pass, no bevel. For large surfaces that are framed by something else
// (the outer edge_frame, an adjacent well) rather than standing alone.
void face(ImDrawList *dl, const ImVec2 &p_min, const ImVec2 &p_max);

// Raised flat plateau: `face` + a 2px raised bevel -- a standalone raised
// element.
void plateau(ImDrawList *dl, const ImVec2 &p_min, const ImVec2 &p_max, float rounding = 0.0f);

// Recessed well: inverted gradient (dark top) + 2px sunken bevel + a hard
// inner shadow line along the top lip.
void well(ImDrawList *dl, const ImVec2 &p_min, const ImVec2 &p_max, float rounding = 3.0f);

// Circular recessed well floor (the minimap sits in one).
void well_circle(ImDrawList *dl, const ImVec2 &c, float radius);

// Recessed triangular pocket (the corner nav-button wells around the
// minimap): filled with the well tone + a sunken bevel on the 3 edges.
void well_tri(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImVec2 &c);

// Raised boss (button / plinth): drop shadow + crown gradient + raised
// bevel. `lit` 0..1 lerps the crown toward `crown_lit`. `omit_bottom`
// drops the bottom bevel + drop shadow so the element blends into whatever
// sits below it (the active tab -> the content area).
void boss(ImDrawList *dl, const ImVec2 &p_min, const ImVec2 &p_max,
          float lit = 0.0f, float rounding = 3.0f, bool omit_bottom = false);

// Engraved horizontal groove between plate sections: dark line, light line
// one px below.
void groove_h(ImDrawList *dl, float x0, float x1, float y);

// Engraved groove along an arbitrary line: a dark line with a light line
// offset one px to its lower-right side (for the diagonal facets around
// the minimap / the chamfered panel corners).
void groove(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b);

// Raised bezel ring around a circular recess. `lit_dir` = screen-space
// angle (radians) the light comes from; default up-left.
void ring(ImDrawList *dl, const ImVec2 &c, float r_outer, float r_inner, float lit_dir = -2.356f);

// The whole-panel raised outer rim (bevel only for now -- chamfered top
// corners are 09 §7's open question).
void edge_frame(ImDrawList *dl, const ImVec2 &p_min, const ImVec2 &p_max);

// One faint static mottle pass over a rect. Fixed seed -> identical
// between runs; the fleck grid walks the rect, so it re-lays on a
// resolution / layout change (accepted, 09 §5).
void mottle(ImDrawList *dl, const ImVec2 &p_min, const ImVec2 &p_max);

} // namespace relief

#endif // __cplusplus
#endif // FRONTGUI_INGAME_RELIEF_H
