#ifndef FRONTGUI_INGAME_LAYOUT_H
#define FRONTGUI_INGAME_LAYOUT_H

// Named positions for the sidebar tab-content bodies, in the shared
// 140x400 virtual grid that grid_pt()/grid_sz() (frontgui_ingame_tabcontent.cpp)
// map to screen pixels. Replaces the inline magic y-coords that made every
// "nudge this row" change a scattered hand-edit + collision hunt.
// docs/refactor/ingame-gui/10-maintainability-refactors.md §5.
//
// This is the *within-region* layout. frontgui_hud_layout.h stays the
// *between-region* layout for the horizontal / minimal HUD variants
// (05-sidebar-frame-and-minimap.md §0); the two compose.
//
// C++ only.

#ifdef __cplusplus

namespace tcl {

// The whole tab-content region (below the tab strip, above the panel foot).
constexpr float BODY_Y0 = 190.0f;
constexpr float BODY_Y1 = 398.0f;
constexpr float BODY_X0 = 4.0f;
constexpr float BODY_X1 = 136.0f;

// Room / spell / trap panels: the info strip, then the scrolling icon grid.
constexpr float INFO_Y0 = 196.0f;
constexpr float INFO_Y1 = 240.0f;
constexpr float GRID_Y0 = 242.0f;
constexpr float GRID_Y1 = 396.0f;

// Creature-query / possession panel.
namespace q {
    // Header: portrait + the two vertical anger/xp bars
    // (10:60:10:10:5:10:5 horizontal split -- see 09 retest 5).
    constexpr float HEADER_Y0 = 190.0f;
    constexpr float HEADER_Y1 = 243.0f;
    constexpr float BARS_Y0   = 192.0f;
    constexpr float BARS_Y1   = 241.0f;
    constexpr float LEVEL_Y   = 193.0f;  // level number, over the xp bar top

    // Health bar (name centred, no numeric value).
    constexpr float HEALTH_Y0 = 248.0f;
    constexpr float HEALTH_Y1 = 266.0f;

    // ABILITIES / STATS toggle strip.
    constexpr float DETAIL_TABS_Y = 270.0f;
    constexpr float DETAIL_TABS_H = 16.0f;

    // ABILITIES: the instance grid.
    constexpr float ABIL_ORG_Y  = 291.0f;
    constexpr float ABIL_CELL_W = 43.0f;
    constexpr float ABIL_CELL_H = 37.0f;
    constexpr float ABIL_PITCH  = 40.0f;

    // STATS: the scrolling 2-per-row list.
    constexpr float STATS_Y0 = 289.0f;
    constexpr float STATS_Y1 = 396.0f;
}

// GMnu_SPELL_LOST (top-down lost-keeper state).
namespace lost {
    constexpr float TEXT_Y   = 202.0f;
    constexpr float ICON_Y0  = 250.0f;
    constexpr float ICON_X0  = 50.0f;
    constexpr float ICON_W   = 40.0f;
    constexpr float ICON_H   = 44.0f;
}

} // namespace tcl

#endif // __cplusplus
#endif // FRONTGUI_INGAME_LAYOUT_H
