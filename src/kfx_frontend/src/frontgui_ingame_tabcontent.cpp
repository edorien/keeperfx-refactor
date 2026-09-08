#include "pre_inc.h"
#include "frontgui_ingame_tabcontent.h"

#include "frontgui_widgets.h"
#include "frontgui_style.h"
#include "frontgui_sprite_tex.h"      // FeGuiPanelTexture
#include "frontgui_ingame_relief.h"   // relief::well / boss / bevel
#include "renderer/RendererManager.h" // RendererImGuiEnabled

#include "globals.h"
#include "sprites.h"                  // GPS_rpanel_frame_portrt_sell
#include "frontend.h"                 // activate_room_build_mode, choose_spell
#include "gui_frontmenu.h"            // menu_is_active
#include "gui_draw.h"                 // gui_room_type_highlighted
#include "frontmenu_ingame_tabs.h"    // go_to_my_next_room_of_type_and_select, find_room_type_capacity_total_percentage
#include "player_data.h"              // get_my_player, my_player_number
#include "packet_data.h"              // set_players_packet_action, PckA_SetPlyrState, PSt_Sell
#include "dungeon_data.h"             // get_my_dungeon, room_buildable / room_list_start / room_resrchable
#include "room_list.h"                // count_player_rooms_of_type
#include "config_terrain.h"           // get_room_kind_stats, RoomConfigStats
#include "config_magic.h"             // get_power_model_stats, is_power_available, PwrK_ARMAGEDDON/HOLDAUDNC
#include "config_trapdoor.h"          // get_manufacture_data, get_trap_model_stats, get_door_model_stats
#include "config_creature.h"          // get_players_special_digger_model, breed_activities, CREATURE_ANY
#include "creature_graphics.h"        // get_creature_model_graphics, CGI_HandSymbol
#include "creature_states.h"          // state_type_to_gui_state, STATE_TYPES_COUNT
#include "thing_creature.h"           // pick_up_creature_of_model_and_gui_job, go_to_next_creature_of_model_and_gui_job
#include "kfx_frontend_state.h"       // no_of_breeds_owned, top_of_breed_list
#include "creature_states_rsrch.h"    // get_players_current_research_val
#include "room_workshop.h"            // manufacture_points_required
#include "config_spritecolors.h"     // get_player_colored_icon_idx
#include "thing_stats.h"             // creature_statistic_text, CrLStat_*
#include "creature_instances.h"      // creature_instance_get_available_id_for_pos, InstanceInfo
#include "creature_control.h"        // struct CreatureControl
#include "power_process.h"            // set_chosen_power, player_uses_power_hold_audience
#include "config_strings.h"
#include "kfx_config_state.h"         // conf.slab_conf.room_types_count
#include "kfx_sim_state.h"            // chosen_room_kind / _spridx / _tooltip

#include "post_inc.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <vector>
#include <imgui.h>

namespace {

// The sidebar's scaled screen rect (== GMnu_MAIN's create_menu rect).
struct { float x, y, w, h; } s_panel = { 0, 0, 0, 0 };

// A point in the 140x400 virtual panel grid -> screen coords.
ImVec2 grid_pt(float vx, float vy)
{
    return ImVec2(s_panel.x + vx * s_panel.w / 140.0f,
                  s_panel.y + vy * s_panel.h / 400.0f);
}
// A size in virtual grid units -> screen pixels.
ImVec2 grid_sz(float vw, float vh)
{
    return ImVec2(vw * s_panel.w / 140.0f, vh * s_panel.h / 400.0f);
}

const ImU32 COL_CELL_DIM = IM_COL32(30, 23, 14, 235);
const ImU32 COL_BORDER   = IM_COL32(92, 70, 42, 255);
const ImU32 COL_SEL      = IM_COL32(255, 219, 102, 255);
const ImU32 COL_HOVER    = IM_COL32(220, 62, 40, 255);
const ImU32 COL_TEXT     = IM_COL32(238, 226, 198, 255);
const ImU32 COL_SUBTEXT  = IM_COL32(210, 190, 140, 255);
const ImU32 COL_HAVE     = IM_COL32(120, 200, 120, 255);

void blit_fit(ImDrawList *dl, short spr, const ImVec2 &p0, const ImVec2 &sz, ImU32 tint)
{
    int w = 0, h = 0;
    void *t = FeGuiPanelTexture(spr, &w, &h);
    if (t == nullptr || w <= 0 || h <= 0)
        return;
    float iw = sz.x, ih = sz.y;
    const float ar = (float)w / (float)h;
    if (iw / ih > ar) iw = ih * ar; else ih = iw / ar;
    const ImVec2 ip0(p0.x + (sz.x - iw) * 0.5f, p0.y + (sz.y - ih) * 0.5f);
    dl->AddImage((ImTextureID)(intptr_t)t, ip0, ImVec2(ip0.x + iw, ip0.y + ih),
                 ImVec2(0, 0), ImVec2(1, 1), tint);
}

// ---- shared build cell (ImGui-drawn -- no legacy frame sprites) ------

// A single glyph filling ~70% of the cell height, centred. Uses the
// heavy UI font (a real vector face -- ImGui 1.92 rasterizes it crisply
// at the requested px; the built-in default font would upscale-blur).
void draw_big_glyph(ImDrawList *dl, const ImVec2 &p0, const ImVec2 &sz, const char *g, ImU32 col)
{
    FeStylePushFont(FeFont_Heading);
    ImFont *font = ImGui::GetFont();
    const float fs = sz.y * 0.70f;
    const ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, g);
    dl->AddText(font, fs, ImVec2(p0.x + (sz.x - ts.x) * 0.5f, p0.y + (sz.y - ts.y) * 0.5f), col, g);
    FeStylePopFont();
}

// portrait-less icon cell: filled bg, the medsym sprite (dimmed when
// unaffordable), a have-one dot, and an ImGui border (gold = selected,
// red = hover, brown = rest). Returns 1/2 on L/R click.
int build_icon(const char *sid, short spr, bool have_one, bool afford,
               bool selected, const ImVec2 &p0, const ImVec2 &sz)
{
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID(sid);
    const bool pressed = ImGui::InvisibleButton("b", sz,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    const bool rclick  = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);

    relief::well(dl, p0, p1, 3.0f);
    blit_fit(dl, spr, ImVec2(p0.x + 3.0f, p0.y + 3.0f), ImVec2(sz.x - 6.0f, sz.y - 6.0f),
             afford ? IM_COL32_WHITE : IM_COL32(255, 255, 255, 110));
    if (have_one)
        dl->AddCircleFilled(ImVec2(p0.x + 4.0f, p0.y + 4.0f), 2.5f, COL_HAVE);

    if (selected)     dl->AddRect(p0, p1, COL_SEL,   3.0f, 0, 2.0f);
    else if (hovered) dl->AddRect(p0, p1, COL_HOVER, 3.0f, 0, 2.0f);
    ImGui::PopID();

    if (rclick)  return 2;
    if (pressed) return 1;
    return 0;
}

// The sell cell -- a big red "$" (no legacy artwork). Returns 1 on click.
int sell_icon(const char *sid, const ImVec2 &p0, const ImVec2 &sz)
{
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID(sid);
    const bool pressed = ImGui::InvisibleButton("s", sz, ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);
    relief::well(dl, p0, p1, 3.0f);
    draw_big_glyph(dl, p0, sz, "$", IM_COL32(220, 60, 50, 255));
    if (hovered)
        dl->AddRect(p0, p1, COL_HOVER, 3.0f, 0, 2.0f);
    ImGui::PopID();
    return pressed ? 1 : 0;
}

// One cell standing in for every not-yet-researched item on the panel:
// a big "?" plus the count in the lower-right corner. Unselectable.
void unknown_cell(const char *sid, const ImVec2 &p0, const ImVec2 &sz, int count)
{
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID(sid);
    ImGui::InvisibleButton("u", sz, ImGuiButtonFlags_MouseButtonLeft);   // swallow clicks
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);
    relief::well(dl, p0, p1, 3.0f);
    draw_big_glyph(dl, p0, sz, "?", COL_SUBTEXT);
    if (count > 0)
    {
        char b[8]; std::snprintf(b, sizeof(b), "%d", count);
        FeStylePushFont(FeFont_Body);
        const ImVec2 ts = ImGui::CalcTextSize(b);
        dl->AddText(ImVec2(p1.x - ts.x - 2.0f, p1.y - ts.y - 1.0f), COL_TEXT, b);
        FeStylePopFont();
    }
    ImGui::PopID();
}

// The "big" info strip above the grid -- selected/hovered item's big
// symbol, name, count, gold cost and (rooms only) a capacity bar.
void info_band(short bigsym, const char *name, int count, long cost, float bar_frac)
{
    const ImVec2 b0 = grid_pt(6.0f, 196.0f);
    const ImVec2 b1 = grid_pt(134.0f, 240.0f);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    relief::well(dl, b0, b1, 3.0f);
    const float bh = b1.y - b0.y;

    if (bigsym > 0)
        blit_fit(dl, bigsym, ImVec2(b0.x + 3.0f, b0.y + 3.0f),
                 ImVec2(bh - 6.0f, bh - 6.0f), IM_COL32_WHITE);

    const float tx = b0.x + bh + 2.0f;
    FeStylePushFont(FeFont_Body);
    if (name != nullptr && name[0] != '\0')
        dl->AddText(ImVec2(tx, b0.y + 4.0f), COL_TEXT, name);
    char c[48];
    if (count >= 0 && cost >= 0)
        std::snprintf(c, sizeof(c), "x%d     %ld", count, cost);
    else if (cost >= 0)
        std::snprintf(c, sizeof(c), "%ld", cost);
    else if (count >= 0)
        std::snprintf(c, sizeof(c), "x%d", count);
    else
        c[0] = '\0';
    if (c[0] != '\0')
        dl->AddText(ImVec2(tx, b0.y + 6.0f + ImGui::GetFontSize()), COL_SUBTEXT, c);
    FeStylePopFont();

    if (bar_frac >= 0.0f)
    {
        if (bar_frac > 1.0f) bar_frac = 1.0f;
        const ImVec2 g0(tx, b1.y - 16.0f);
        const ImVec2 g1(b1.x - 4.0f, b1.y - 4.0f);
        relief::well(dl, g0, g1, 2.0f);
        dl->AddRectFilled(ImVec2(g0.x + 1.0f, g0.y + 1.0f),
                          ImVec2(g0.x + (g1.x - g0.x) * bar_frac, g1.y - 1.0f),
                          IM_COL32(96, 194, 235, 255));
    }
}

// 4-column grid geometry, matching the legacy room menu metrics.
// Cell size + pitch for the 4-wide icon grid (scroll-independent).
struct GridGeom { ImVec2 cell; float pitch_x, pitch_y; };
GridGeom grid_geom(void)
{
    GridGeom g;
    g.cell    = grid_sz(30.0f, 34.0f);
    g.pitch_x = 32.0f * s_panel.w / 140.0f;
    g.pitch_y = 38.0f * s_panel.h / 400.0f;
    return g;
}

ImVec2 s_grid_base = { 0, 0 };

// Opens the scrolling icon-grid child (a scrollbar appears once the item
// count overflows the visible region -- replaces the legacy 2nd page).
// Returns the geometry; pair with grid_end(g, slots_used).
GridGeom grid_begin(const char *id)
{
    const ImVec2 r0 = grid_pt(4.0f, 242.0f);
    const ImVec2 r1 = grid_pt(136.0f, 396.0f);
    ImGui::SetCursorScreenPos(r0);
    ImGui::BeginChild(id, ImVec2(r1.x - r0.x, r1.y - r0.y), false, ImGuiWindowFlags_NoBackground);
    s_grid_base = ImGui::GetCursorScreenPos();
    s_grid_base.x += grid_sz(2.0f, 0.0f).x;   // small left inset off the scroll edge
    return grid_geom();
}
ImVec2 grid_cell_pos(const GridGeom &g, int slot)
{
    return ImVec2(s_grid_base.x + (slot % 4) * g.pitch_x,
                  s_grid_base.y + (slot / 4) * g.pitch_y);
}
void grid_end(const GridGeom &g, int slots_used)
{
    ImGui::Dummy(ImVec2(1.0f, ((slots_used + 3) / 4) * g.pitch_y + 4.0f));
    ImGui::EndChild();
}

// ---- room build grid -------------------------------------------------

struct GridItem { int kind; int order; };

void do_sell_rooms(void)
{
    kfx_sim_state.chosen_room_kind = 0;
    kfx_sim_state.chosen_room_spridx = 0;
    kfx_sim_state.chosen_room_tooltip = 0;
    set_players_packet_action(get_my_player(), PckA_SetPlyrState, PSt_Sell, 0, 0, 0);
}

void room_grid(void)
{
    struct Dungeon *dungeon = get_my_dungeon();
    if (dungeon_invalid(dungeon))
        return;
    const int count = kfx_config_state.conf.slab_conf.room_types_count;

    std::vector<GridItem> items;
    for (int k = 1; k < count; k++)
    {
        const struct RoomConfigStats *rs = get_room_kind_stats(k);
        if (rs != nullptr && rs->panel_tab_idx > 0)
            items.push_back({ k, rs->panel_tab_idx });
    }
    std::sort(items.begin(), items.end(),
              [](const GridItem &a, const GridItem &b) { return a.order < b.order; });

    const GridGeom g = grid_begin("##rmGrid");

    int hovered_now = 0;
    int slot = 0;
    int hidden = 0;
    for (const GridItem &it : items)
    {
        const int k = it.kind;
        const struct RoomConfigStats *rs = get_room_kind_stats(k);
        const bool buildable = (dungeon->room_buildable[k] & 1) != 0;
        const bool researchable = dungeon->room_resrchable[k] == 1 || dungeon->room_resrchable[k] == 2
            || (dungeon->room_resrchable[k] == 4 && (dungeon->room_buildable[k] & 2));
        if (!buildable)
        {
            if (researchable) hidden++;
            continue;
        }
        const bool afford = dungeon->total_money_owned >= rs->cost;
        const bool have_one = dungeon->room_list_start[k] > 0;

        char sid[16]; std::snprintf(sid, sizeof(sid), "rm%d", k);
        const int hit = build_icon(sid, (short)rs->medsym_sprite_idx, have_one, afford,
                                   kfx_sim_state.chosen_room_kind == k,
                                   grid_cell_pos(g, slot), g.cell);
        if (ImGui::IsItemHovered())
            hovered_now = (k < 17) ? k : 0;
        if (hit == 1)
            activate_room_build_mode(k, rs->tooltip_stridx);
        else if (hit == 2)
        {
            go_to_my_next_room_of_type_and_select(k);
            kfx_sim_state.chosen_room_kind = k;
            kfx_sim_state.chosen_room_spridx = rs->bigsym_sprite_idx;
            kfx_sim_state.chosen_room_tooltip = rs->tooltip_stridx;
        }
        slot++;
    }
    if (hidden > 0)
        unknown_cell("rmUnk", grid_cell_pos(g, slot++), g.cell, hidden);
    if (sell_icon("rmSell", grid_cell_pos(g, slot++), g.cell) == 1)
        do_sell_rooms();
    grid_end(g, slot);
    // gui_room_type_highlighted feeds roomspace prediction next turn.
    gui_room_type_highlighted = (char)hovered_now;

    // Info strip -- always drawn (the recess stays put); only its contents
    // change with the hovered (else chosen) room.
    int info_kind = hovered_now;
    if (info_kind <= 0) info_kind = kfx_sim_state.chosen_room_kind;
    if (info_kind > 0 && info_kind < count)
    {
        const struct RoomConfigStats *rs = get_room_kind_stats(info_kind);
        const long pct = find_room_type_capacity_total_percentage(my_player_number, info_kind);
        info_band((short)rs->bigsym_sprite_idx, get_string(rs->name_stridx),
                  (int)count_player_rooms_of_type(my_player_number, info_kind),
                  (long)rs->cost, pct >= 0 ? (float)pct / 256.0f : -1.0f);
    }
    else
    {
        info_band(0, nullptr, -1, -1, -1.0f);
    }
}

// ---- power (spell) grid --------------------------------------------

void spell_grid(void)
{
    struct PlayerInfo *player = get_my_player();
    struct Dungeon *dungeon = get_my_dungeon();
    if (dungeon_invalid(dungeon))
        return;
    const int count = kfx_config_state.conf.magic_conf.power_types_count;

    std::vector<GridItem> items;
    for (int k = 1; k < count; k++)
    {
        const struct PowerConfigStats *ps = get_power_model_stats(k);
        if (ps != nullptr && ps->panel_tab_idx > 0)
            items.push_back({ k, (int)ps->panel_tab_idx });
    }
    std::sort(items.begin(), items.end(),
              [](const GridItem &a, const GridItem &b) { return a.order < b.order; });

    const GridGeom g = grid_begin("##pwGrid");

    int hovered_now = 0;
    int slot = 0;
    int hidden = 0;
    for (const GridItem &it : items)
    {
        const int k = it.kind;
        const struct PowerConfigStats *ps = get_power_model_stats(k);
        // Shown once researchable or owned (matches legacy gui_area_spell_button).
        if (!dungeon->magic_resrchable[k] && dungeon->magic_level[k] <= 0)
            continue;
        const bool available = is_power_available(player->id_number, k);
        if (!available)
        {
            hidden++;
            continue;
        }
        bool castable = true;
        if (k == PwrK_ARMAGEDDON && kfx_sim_state.armageddon_cast_turn != 0) castable = false;
        if (k == PwrK_HOLDAUDNC && player_uses_power_hold_audience(my_player_number)) castable = false;

        char sid[16]; std::snprintf(sid, sizeof(sid), "pw%d", k);
        const int hit = build_icon(sid, (short)ps->medsym_sprite_idx, false, castable,
                                   kfx_sim_state.chosen_spell_type == k,
                                   grid_cell_pos(g, slot), g.cell);
        if (ImGui::IsItemHovered())
            hovered_now = k;
        if (castable && hit == 1)
            choose_spell(k, ps->tooltip_stridx);
        else if (castable && hit == 2)
        {
            go_to_next_spell_of_type(k);
            set_chosen_power(k, ps->tooltip_stridx);
        }
        slot++;
    }
    if (hidden > 0)
        unknown_cell("pwUnk", grid_cell_pos(g, slot++), g.cell, hidden);
    grid_end(g, slot);

    int info = hovered_now;
    if (info <= 0) info = kfx_sim_state.chosen_spell_type;
    if (info > 0 && info < count)
    {
        const struct PowerConfigStats *ps = get_power_model_stats(info);
        info_band((short)ps->bigsym_sprite_idx, get_string(ps->name_stridx),
                  -1, (long)ps->cost[0], -1.0f);
    }
    else
    {
        info_band(0, nullptr, -1, -1, -1.0f);
    }
}

// ---- manufacture (trap/door) grid ---------------------------------

void do_sell_traps(void)
{
    kfx_sim_state.manufactr_element = 0;
    kfx_sim_state.manufactr_spridx = 0;
    kfx_sim_state.manufactr_tooltip = 0;
    set_players_packet_action(get_my_player(), PckA_SetPlyrState, PSt_Sell, 0, 0, 0);
}

void trap_grid(void)
{
    struct PlayerInfo *player = get_my_player();
    const int count = kfx_config_state.conf.trapdoor_conf.manufacture_types_count;

    std::vector<GridItem> items;
    for (int m = 1; m < count; m++)
    {
        const struct ManufactureData *md = get_manufacture_data(m);
        if (md != nullptr && md->panel_tab_idx > 0)
            items.push_back({ m, (int)md->panel_tab_idx });
    }
    std::sort(items.begin(), items.end(),
              [](const GridItem &a, const GridItem &b) { return a.order < b.order; });

    const GridGeom g = grid_begin("##mfGrid");

    int hovered_model = 0;
    int hovered_m = 0;
    int slot = 0;
    int hidden = 0;
    for (const GridItem &it : items)
    {
        const int m = it.kind;
        const struct ManufactureData *md = get_manufacture_data(m);
        const bool placeable = is_trap_placeable(player->id_number, md->tngmodel)
                            || is_trap_built(player->id_number, md->tngmodel);
        const bool buildable = is_trap_buildable(player->id_number, md->tngmodel);
        if (!placeable)
        {
            if (buildable) hidden++;
            continue;
        }

        char sid[16]; std::snprintf(sid, sizeof(sid), "mf%d", m);
        const int hit = build_icon(sid, (short)md->medsym_sprite_idx, false, true,
                                   kfx_sim_state.manufactr_element == m,
                                   grid_cell_pos(g, slot), g.cell);
        if (ImGui::IsItemHovered())
        {
            hovered_model = md->tngmodel;
            hovered_m = m;
        }
        if (hit == 1)
            choose_workshop_item(m, md->tooltip_stridx);
        else if (hit == 2)
        {
            go_to_next_trap_of_type(md->tngmodel, player->id_number);
            kfx_sim_state.manufactr_element = m;
            kfx_sim_state.manufactr_spridx = md->bigsym_sprite_idx;
            kfx_sim_state.manufactr_tooltip = md->tooltip_stridx;
        }
        slot++;
    }
    if (hidden > 0)
        unknown_cell("mfUnk", grid_cell_pos(g, slot++), g.cell, hidden);
    if (sell_icon("mfSell", grid_cell_pos(g, slot++), g.cell) == 1)
        do_sell_traps();
    grid_end(g, slot);
    gui_trap_type_highlighted = (char)hovered_model;

    int info_m = hovered_m;
    if (info_m <= 0) info_m = kfx_sim_state.manufactr_element;
    if (info_m > 0 && info_m < count)
    {
        const struct ManufactureData *md = get_manufacture_data(info_m);
        short bigsym = (short)md->bigsym_sprite_idx;
        const char *name = "";
        long req = -1;
        if (md->tngclass == TCls_Trap)
        {
            const struct TrapConfigStats *ts = get_trap_model_stats(md->tngmodel);
            if (ts != nullptr) { name = get_string(ts->name_stridx); req = (long)ts->manufct_required; }
        }
        else
        {
            const struct DoorConfigStats *ds = get_door_model_stats(md->tngmodel);
            if (ds != nullptr) { name = get_string(ds->name_stridx); req = (long)ds->manufct_required; }
        }
        info_band(bigsym, name, -1, req, -1.0f);
    }
    else
    {
        info_band(0, nullptr, -1, -1, -1.0f);
    }
}

// ---- creature activity list --------------------------------------

// A recessed labelled cell -- the same "pocket" the room / spell cells
// use. `selected` lights a gold rim (for the ABILITIES/STATS toggle).
// Returns 1/2 on L/R click.
int cell_button(const char *sid, const ImVec2 &p0, const ImVec2 &sz,
                const char *text, bool selected)
{
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID(sid);
    const bool pressed = ImGui::InvisibleButton("c", sz,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    const bool rclick  = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);
    relief::well(dl, p0, p1, 2.0f);
    if (text != nullptr && text[0] != '\0')
    {
        const ImVec2 ts = ImGui::CalcTextSize(text);
        dl->AddText(ImVec2(p0.x + (sz.x - ts.x) * 0.5f, p0.y + (sz.y - ts.y) * 0.5f), COL_TEXT, text);
    }
    if (selected)     dl->AddRect(p0, p1, COL_SEL,   2.0f, 0, 2.0f);
    else if (hovered) dl->AddRect(p0, p1, COL_HOVER, 2.0f, 0, 2.0f);
    ImGui::PopID();
    if (rclick)  return 2;
    if (pressed) return 1;
    return 0;
}

void creature_pick(ThingModel crmodel, long gj, bool pick_up)
{
    if (pick_up)
        pick_up_creature_of_model_and_gui_job(crmodel, gj, my_player_number, get_creature_pick_flags(1));
    else
        go_to_next_creature_of_model_and_gui_job(crmodel, gj, get_creature_pick_flags(0));
}

// Up to 6 rows from breed_activities[] / no_of_breeds_owned (kept current
// every frame by update_creatr_model_activities_list() in get_gui_inputs(),
// independent of this menu's draw). Row 0 is always the special digger.
// Rows are only marginally taller than the portrait icon.
void creature_list(void)
{
    struct Dungeon *dungeon = get_my_dungeon();
    if (dungeon_invalid(dungeon))
        return;
    const int model_count = kfx_config_state.conf.crtr_conf.model_count;
    const ThingModel digger = get_players_special_digger_model(my_player_number);

    const int owned = kfx_frontend_state.no_of_breeds_owned;
    int top = kfx_frontend_state.top_of_breed_list;
    const int visible = owned < 1 ? 1 : (owned > 6 ? 6 : owned);

    if (owned > 6)
    {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f && ImGui::IsWindowHovered())
        {
            top -= (int)wheel;
            if (top < 0) top = 0;
            if (top > owned - 6) top = owned - 6;
            kfx_frontend_state.top_of_breed_list = (char)top;
        }
    }

    const ImVec2 job_sz  = grid_sz(34.0f, 20.0f);
    const ImVec2 port_sz = grid_sz(22.0f, 22.0f);
    const float  row_v   = 24.0f;   // virtual pitch -- icon (22) + 2

    // Header: pick-next idle / working / fighting across all owned models.
    static const char *const head_txt[3] = { "IDLE", "WORK", "FIGHT" };
    for (int j = 0; j < 3; j++)
    {
        char sid[8]; std::snprintf(sid, sizeof(sid), "h%d", j);
        const int hit = cell_button(sid, grid_pt(26.0f + j * 36.0f, 204.0f),
                                    grid_sz(34.0f, 12.0f), head_txt[j], false);
        if (hit != 0)
        {
            const long gj = (j == 0) ? CrGUIJob_Wandering : (j == 1) ? CrGUIJob_Working : CrGUIJob_Fighting;
            creature_pick(CREATURE_ANY, gj, hit == 1);
        }
    }

    int hovered_model = 0;
    for (int i = 0; i < visible; i++)
    {
        ThingModel crmodel;
        if (i > 0 && top + i < model_count)
            crmodel = breed_activities[top + i];
        else
            crmodel = digger;
        if (crmodel <= 0 || crmodel >= model_count)
            continue;
        if (dungeon->owned_creatures_of_model[crmodel] <= 0 && crmodel != digger)
            continue;

        const float vy = 218.0f + i * row_v;

        unsigned int cnt[3] = { 0, 0, 0 };
        for (int n = 0; n < STATE_TYPES_COUNT; n++)
        {
            const long gsi = state_type_to_gui_state[n];
            if (gsi >= 0 && gsi < 3)
                cnt[gsi] += dungeon->crmodel_state_type_count[crmodel][n];
        }

        // Portrait -- L: pick up next of model, R: zoom to next of model.
        const ImVec2 pp0 = grid_pt(3.0f, vy);
        const ImVec2 pp1(pp0.x + port_sz.x, pp0.y + port_sz.y);
        ImGui::SetCursorScreenPos(pp0);
        char psid[12]; std::snprintf(psid, sizeof(psid), "cp%d", i);
        ImGui::PushID(psid);
        const bool ppressed = ImGui::InvisibleButton("p", port_sz,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool phover = ImGui::IsItemHovered();
        const bool prclick = phover && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        ImDrawList *dl = ImGui::GetWindowDrawList();
        relief::well(dl, pp0, pp1, 2.0f);
        int sw = 0, sh = 0;
        void *tex = FeGuiPanelTexture(get_creature_model_graphics(crmodel, CGI_HandSymbol), &sw, &sh);
        if (tex != nullptr && sw > 0 && sh > 0)
            dl->AddImage((ImTextureID)(intptr_t)tex, ImVec2(pp0.x + 1.0f, pp0.y + 1.0f),
                         ImVec2(pp1.x - 1.0f, pp1.y - 1.0f));
        if (phover)
        {
            dl->AddRect(pp0, pp1, COL_HOVER, 2.0f, 0, 2.0f);
            hovered_model = crmodel;
        }
        {
            char tb[8]; std::snprintf(tb, sizeof(tb), "%d", dungeon->owned_creatures_of_model[crmodel]);
            dl->AddText(ImVec2(pp0.x + 1.0f, pp1.y - ImGui::GetFontSize()), IM_COL32(255, 235, 120, 255), tb);
        }
        ImGui::PopID();
        if (ppressed)      creature_pick(crmodel, CrGUIJob_Any, true);
        else if (prclick)  creature_pick(crmodel, CrGUIJob_Any, false);

        for (int j = 0; j < 3; j++)
        {
            char jb[8]; std::snprintf(jb, sizeof(jb), "%u", cnt[j]);
            char jsid[16]; std::snprintf(jsid, sizeof(jsid), "cj%d_%d", i, j);
            const int hit = cell_button(jsid, grid_pt(26.0f + j * 36.0f, vy), job_sz,
                                        cnt[j] > 0 ? jb : "-", false);
            if (hit == 0)
                continue;
            const long gj = (j == 0) ? CrGUIJob_Wandering : (j == 1) ? CrGUIJob_Working : CrGUIJob_Fighting;
            creature_pick(crmodel, gj, hit == 1);
        }
    }

    if (hovered_model > 0)
        gui_creature_type_highlighted = (char)hovered_model;
}

// ---- query / information panel -----------------------------------

void prog_bar(const ImVec2 &p0, const ImVec2 &p1, float frac, ImU32 fill, const char *label)
{
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    relief::well(dl, p0, p1, 2.0f);
    if (frac > 0.0f)
        dl->AddRectFilled(ImVec2(p0.x + 1.0f, p0.y + 1.0f),
                          ImVec2(p0.x + (p1.x - p0.x) * frac, p1.y - 1.0f), fill, 1.5f);
    if (label != nullptr && label[0] != '\0')
    {
        FeStylePushFont(FeFont_Caption);
        const ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(p0.x + ((p1.x - p0.x) - ts.x) * 0.5f, p0.y + ((p1.y - p0.y) - ts.y) * 0.5f),
                    COL_TEXT, label);
        FeStylePopFont();
    }
}

// A GPS_* panel-sprite cell that toggles. `on` -> pulsing gold border;
// `available == false` -> greyed, non-interactive. Returns true on a
// left click (only when available).
bool sprite_toggle(const char *sid, const ImVec2 &p0, const ImVec2 &sz,
                   short spr, bool on, bool available)
{
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID(sid);
    const bool pressed = ImGui::InvisibleButton("t", sz, ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered = available && ImGui::IsItemHovered();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);

    dl->AddRectFilled(p0, p1, on ? IM_COL32(60, 44, 26, 235) : COL_CELL_DIM, 3.0f);
    blit_fit(dl, spr, ImVec2(p0.x + 2.0f, p0.y + 2.0f), ImVec2(sz.x - 4.0f, sz.y - 4.0f),
             !available ? IM_COL32(255, 255, 255, 70)
                        : (on ? IM_COL32_WHITE : IM_COL32(235, 235, 235, 205)));

    ImU32 bcol = COL_BORDER;
    float bth = 1.0f;
    if (!available)
        bcol = IM_COL32(70, 58, 40, 200);
    else if (on)
    {
        const float pulse = 0.45f + 0.40f * (float)std::sin(ImGui::GetTime() * 6.0);
        bcol = ImGui::GetColorU32(ImVec4(1.0f, 0.86f, 0.35f, pulse));
        bth = 2.0f;
    }
    if (hovered) { bcol = COL_HOVER; bth = 2.0f; }
    dl->AddRect(p0, p1, bcol, 3.0f, 0, bth);
    ImGui::PopID();
    return pressed && available;
}

// Same as sprite_toggle but draws a crisp vector glyph (e.g. a red "?")
// instead of a panel sprite. Returns true on left click.
bool glyph_toggle(const char *sid, const ImVec2 &p0, const ImVec2 &sz,
                  const char *glyph, ImU32 glyph_col, bool on)
{
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID(sid);
    const bool pressed = ImGui::InvisibleButton("g", sz, ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);

    dl->AddRectFilled(p0, p1, on ? IM_COL32(60, 44, 26, 235) : COL_CELL_DIM, 3.0f);
    draw_big_glyph(dl, p0, sz, glyph, glyph_col);

    ImU32 bcol = COL_BORDER;
    float bth = 1.0f;
    if (on)
    {
        const float pulse = 0.45f + 0.40f * (float)std::sin(ImGui::GetTime() * 6.0);
        bcol = ImGui::GetColorU32(ImVec4(1.0f, 0.86f, 0.35f, pulse));
        bth = 2.0f;
    }
    if (hovered) { bcol = COL_HOVER; bth = 2.0f; }
    dl->AddRect(p0, p1, bcol, 3.0f, 0, bth);
    ImGui::PopID();
    return pressed;
}

// [sprite icon]   [==== progress bar ====] row. The icon sits fully in
// its own square at the left with a clear gap -- no overlap with the track.
void bar_row(short icon_spr, const ImVec2 &p0, const ImVec2 &p1, float frac, ImU32 fill,
             const char *label = nullptr)
{
    const float row_h = p1.y - p0.y;
    const float gap   = row_h * 0.4f;
    blit_fit(ImGui::GetWindowDrawList(), icon_spr, p0, ImVec2(row_h, row_h), IM_COL32_WHITE);
    prog_bar(ImVec2(p0.x + row_h + gap, p0.y + 2.0f), ImVec2(p1.x, p1.y - 2.0f), frac, fill, label);
}

int query_keeper_count(void)
{
    int n = 0;
    for (int i = 0; i < PLAYERS_COUNT; i++)
        if (player_exists(get_player(i)) && player_is_keeper(i))
            n++;
    return n;
}

// pos 0 == me; pos 1.. walks existing non-roaming players, offset by page*3
// (mirrors the legacy info_panel_pos_to_player_number).
PlayerNumber query_pos_to_player(int pos, int page)
{
    if (pos == 0)
        return my_player_number;
    int seen = 0;
    const int want = pos + page * 3;
    for (int i = 0; i < PLAYERS_COUNT; i++)
    {
        if (i == my_player_number || player_is_roaming(i))
            continue;
        if (player_exists(get_player(i)))
            seen++;
        if (seen == want)
            return (PlayerNumber)i;
    }
    return -1;
}

int s_query_page = 0;

void player_count_readout(short icon_spr, const ImVec2 &p0, float ih, unsigned value)
{
    ImDrawList *dl = ImGui::GetWindowDrawList();
    blit_fit(dl, icon_spr, p0, ImVec2(ih, ih), IM_COL32_WHITE);
    char t[16]; std::snprintf(t, sizeof(t), "%u", value);
    FeStylePushFont(FeFont_Body);
    dl->AddText(ImVec2(p0.x + ih + 3.0f, p0.y + (ih - ImGui::GetFontSize()) * 0.5f), COL_TEXT, t);
    FeStylePopFont();
}

void query_panel(void)
{
    struct Dungeon *dungeon = get_my_dungeon();
    if (dungeon_invalid(dungeon))
        return;
    struct PlayerInfo *me = get_my_player();

    // Tendency toggles -- legacy sprites, flashing when on, greyed when
    // unavailable (imprison needs a prison room). Packet-routed.
    const bool imp_avail = player_has_room_of_role(my_player_number, RoRoF_Prison);
    if (sprite_toggle("tImp", grid_pt(30.0f, 192.0f), grid_sz(34.0f, 26.0f),
                      imp_avail ? (short)GPS_rpanel_tendency_prisnd_act : (short)GPS_rpanel_tendency_prisnu_dis,
                      imp_avail && kfx_sim_state.creatures_tend_imprison != 0, imp_avail))
        set_players_packet_action(me, PckA_ToggleTendency, CrTend_Imprison, 0, 0, 0);
    if (sprite_toggle("tFlee", grid_pt(72.0f, 192.0f), grid_sz(34.0f, 26.0f),
                      GPS_rpanel_tendency_fleed_act, kfx_sim_state.creatures_tend_flee != 0, true))
        set_players_packet_action(me, PckA_ToggleTendency, CrTend_Flee, 0, 0, 0);

    // Payday / research / workshop -- legacy icon + ImGui progress bar.
    // Payday bar carries the total wage due, centred in the track.
    const long payday_gap = kfx_config_state.conf.rules[my_player_number].gameplay.pay_day_gap;
    char payday_lbl[24];
    std::snprintf(payday_lbl, sizeof(payday_lbl), "%ld", (long)dungeon->creatures_total_pay);
    bar_row(GPS_room_treasury_std_s, grid_pt(6.0f, 222.0f), grid_pt(134.0f, 240.0f),
            payday_gap > 0 ? (float)kfx_config_state.pay_day_progress[my_player_number] / (float)payday_gap : 0.0f,
            IM_COL32(220, 190, 70, 255), payday_lbl);

    struct ResearchVal *rv = get_players_current_research_val(my_player_number);
    bar_row(GPS_room_research_std_s, grid_pt(6.0f, 244.0f), grid_pt(70.0f, 262.0f),
            (rv != nullptr && rv->req_amount > 0)
                ? (float)(dungeon->research_progress >> 8) / (float)rv->req_amount : 0.0f,
            IM_COL32(96, 194, 235, 255));

    float wrk_f = 0.0f;
    if (dungeon->manufacture_class != TCls_Empty)
    {
        const long req = manufacture_points_required(dungeon->manufacture_class, dungeon->manufacture_kind);
        if (req > 0)
            wrk_f = (float)(dungeon->manufacture_progress >> 8) / (float)req;
    }
    bar_row(GPS_room_workshop_std_s, grid_pt(74.0f, 244.0f), grid_pt(134.0f, 262.0f),
            wrk_f, IM_COL32(96, 194, 235, 255));

    // Per-player rows: room symbol + count | ally toggle | player symbol + count.
    const int keepers = query_keeper_count();
    const int pages = keepers > 7 ? 3 : (keepers > 4 ? 2 : 1);
    if (s_query_page >= pages)
        s_query_page = 0;

    const float ih = grid_sz(0.0f, 20.0f).y;
    for (int row = 0; row < 4; row++)
    {
        const PlayerNumber p = query_pos_to_player(row, s_query_page);
        if (p < 0)
            continue;
        struct PlayerInfo *pl = get_player(p);
        if (!player_exists(pl))
            continue;
        struct Dungeon *pd = get_players_dungeon(pl);
        if (dungeon_invalid(pd))
            continue;
        const float vy = 270.0f + row * 26.0f;

        player_count_readout(get_player_colored_icon_idx(GPS_plyrsym_symbol_room_red_std_a, p),
                             grid_pt(4.0f, vy), ih, pd->total_rooms);
        player_count_readout(get_player_colored_icon_idx(GPS_plyrsym_symbol_player_red_std_a, p),
                             grid_pt(78.0f, vy), ih, pd->num_active_creatrs);

        if (p != my_player_number && (pl->allocflags & PlaF_Allocated))
        {
            const bool allied = player_allied_with(me, p);
            const short as = allied
                ? get_player_colored_icon_idx(GPS_plyrsym_symbol_player_red_std_b, p)
                : (short)GPS_plyrsym_symbol_player_any_dis;
            char asid[12]; std::snprintf(asid, sizeof(asid), "ally%d", row);
            if (sprite_toggle(asid, grid_pt(58.0f, vy), grid_sz(16.0f, 20.0f), as, allied, true))
                set_players_packet_action(me, PckA_PlyrToggleAlly, p, 0, 0, 0);
        }
    }

    // Query-mode button + MP player-page cycle.
    if (glyph_toggle("qMode", grid_pt(44.0f, 372.0f), grid_sz(50.0f, 20.0f),
                     "?", IM_COL32(220, 70, 60, 255), me->work_state == PSt_CreatrQuery))
        set_players_packet_action(me, PckA_SetPlyrState, PSt_CreatrQuery, 0, 0, 0);
    if (pages > 1)
    {
        ImGui::SetCursorScreenPos(grid_pt(6.0f, 372.0f));
        ImGui::PushID("qPage");
        if (ImGui::InvisibleButton("p", grid_sz(34.0f, 20.0f), ImGuiButtonFlags_MouseButtonLeft))
            s_query_page = (s_query_page + 1) % pages;
        const bool hv = ImGui::IsItemHovered();
        ImDrawList *dl = ImGui::GetWindowDrawList();
        const ImVec2 q0 = grid_pt(6.0f, 372.0f), q1 = grid_pt(40.0f, 392.0f);
        dl->AddRectFilled(q0, q1, COL_CELL_DIM, 3.0f);
        draw_big_glyph(dl, q0, ImVec2(q1.x - q0.x, q1.y - q0.y), ">", COL_SUBTEXT);
        dl->AddRect(q0, q1, hv ? COL_HOVER : COL_BORDER, 3.0f, 0, hv ? 2.0f : 1.0f);
        ImGui::PopID();
    }
}

// ---- creature query / possession detail panel -------------------
// Shared by top-down query mode (PSt_CreatrQuery -> click a creature ->
// GMnu_CREATURE_QUERY1) and possession (toggle_first_person_menu). Both
// point player->controlled_thing_idx at the subject creature (query mode
// via set_selected_creature in pinstfs_query_creature). The legacy 4
// pages collapse to two tabs here; the legacy next-page menu chain still
// runs but every GMnu_CREATURE_QUERY* maps to this one panel so it is a
// visual no-op.

int s_query_detail_tab = 0;   // 0 = abilities, 1 = stats

struct StatRow { int stat; short spr; int tip; };
const StatRow k_stat_rows[] = {
    { CrLStat_Health,          GPS_crspell_heal_std_s,                 GUIStr_CreatureHealthDesc },
    { CrLStat_Strength,        GPS_symbols_creatr_stat_strength_std,   GUIStr_CreatureStrengthDesc },
    { CrLStat_Armour,          GPS_symbols_creatr_stat_armor_std,      GUIStr_CreatureArmourDesc },
    { CrLStat_Defence,         GPS_symbols_creatr_stat_defense_std,    GUIStr_CreatureDefenceDesc },
    { CrLStat_Luck,            GPS_symbols_creatr_stat_luck_std,       GUIStr_CreatureLuckDesc },
    { CrLStat_Dexterity,       GPS_symbols_creatr_stat_dexterity_std,  GUIStr_CreatureDexterityDesc },
    { CrLStat_Speed,           GPS_crspell_speedup_dis_s,              GUIStr_CreatureSpeedDesc },
    { CrLStat_Loyalty,         GPS_crspell_whip_std_s,                 GUIStr_CreatureLoyaltyDesc },
    { CrLStat_GoldWage,        GPS_symbols_creatr_stat_wage_std,       GUIStr_CreatureWageDesc },
    { CrLStat_GoldHeld,        GPS_symbols_creatr_stat_gold_std,       GUIStr_CreatureGoldHeldDesc },
    { CrLStat_AgeTime,         GPS_symbols_creatr_stat_age_std,        GUIStr_CreatureTimeInDungeonDesc },
    { CrLStat_Kills,           GPS_symbols_creatr_stat_kills_std,      GUIStr_CreatureKillsDesc },
    { CrLStat_ResearchSkill,   GPS_room_research_std_s,                GUIStr_CreatureResrchSkillDesc },
    { CrLStat_ManufactureSkill,GPS_room_workshop_std_s,                GUIStr_CreatureManfctrSkillDesc },
    { CrLStat_TrainingSkill,   GPS_room_training_std_s,                GUIStr_CreatureTraingSkillDesc },
    { CrLStat_ScavengeSkill,   GPS_room_scavenge_std_s,                GUIStr_CreatureScavngSkillDesc },
    { CrLStat_TrainingCost,    GPS_symbols_creatr_stat_traingcst_std,  GUIStr_CreatureTraingCostDesc },
    { CrLStat_ScavengeCost,    GPS_symbols_creatr_stat_scavngcst_std,  GUIStr_CreatureScavngCostDesc },
    { CrLStat_Weight,          GPS_trapdoor_trap_boulder_std_s,        GUIStr_CreatureWeightDesc },
    { CrLStat_BloodType,       GPS_symbols_creatr_stat_blood_std,      GUIStr_CreatureBloodTypeDesc },
};

// A word-wrapped tooltip (the creature stat / instance descriptions are
// long sentences that otherwise run off the screen edge).
void wrapped_tooltip(const char *s)
{
    if (s == nullptr || s[0] == '\0')
        return;
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 16.0f);
    ImGui::TextUnformatted(s);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

// one ability cell: hotkey number (top-left), icon, a fat cooldown bar
// at the bottom, active-instance ring.
void instance_cell(struct Thing *thing, struct CreatureControl *cctrl, int pos,
                   const ImVec2 &p0, const ImVec2 &sz)
{
    const CrInstance inst_id = creature_instance_get_available_id_for_pos(thing, pos);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);
    const bool active = cctrl->active_instance_id == inst_id;

    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID(pos);
    ImGui::InvisibleButton("i", sz, ImGuiButtonFlags_MouseButtonLeft);
    const bool hov = ImGui::IsItemHovered();
    const struct InstanceInfo *ii = creature_instance_info_get(inst_id);
    if (hov && ii->tooltip_stridx > 0)
        wrapped_tooltip(get_string(ii->tooltip_stridx));
    ImGui::PopID();

    relief::well(dl, p0, p1, 3.0f); // recessed pocket, matching the room/spell cells

    const bool on_cd = !creature_instance_has_reset(thing, inst_id);
    short spr = (short)ii->symbol_spridx;
    if (on_cd) spr++;   // legacy disabled frame
    const float bar_h = sz.y * 0.30f;
    blit_fit(dl, spr, ImVec2(p0.x + 2.0f, p0.y + 2.0f), ImVec2(sz.x - 4.0f, sz.y - bar_h - 4.0f),
             on_cd ? IM_COL32(255, 255, 255, 120) : IM_COL32_WHITE);

    // fat cooldown bar
    float frac = 1.0f;
    if (cctrl->instance_id == inst_id)
        frac = 0.0f;
    else if (on_cd)
    {
        const long req = (thing->alloc_flags & TAlF_IsControlled) ? ii->fp_reset_time : ii->reset_time;
        const long prog = (long)get_gameturn() - (long)cctrl->instance_use_turn[inst_id]
                        + cctrl->inst_action_turns - cctrl->inst_total_turns;
        frac = req > 0 ? (float)prog / (float)req : 1.0f;
    }
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    const ImVec2 c0(p0.x + 2.0f, p1.y - bar_h), c1(p1.x - 2.0f, p1.y - 2.0f);
    relief::well(dl, c0, c1, 1.5f);
    if (frac > 0.0f)
        dl->AddRectFilled(ImVec2(c0.x + 1.0f, c0.y + 1.0f),
                          ImVec2(c0.x + (c1.x - c0.x) * frac, c1.y - 1.0f),
                          on_cd ? IM_COL32(200, 120, 60, 255) : IM_COL32(96, 194, 235, 255));

    // hotkey number (1..9, 0)
    char kb[4]; std::snprintf(kb, sizeof(kb), "%d", (pos + 1) % 10);
    dl->AddText(ImVec2(p0.x + 3.0f, p0.y + 1.0f), IM_COL32(255, 235, 120, 255), kb);

    if (active)    dl->AddRect(p0, p1, COL_SEL,   3.0f, 0, 2.0f);
    else if (hov)  dl->AddRect(p0, p1, COL_HOVER, 3.0f, 0, 2.0f);
}

// one stat cell: bordered, icon + value; hover shows the stat's tooltip.
void stat_cell(struct Thing *thing, const StatRow &r, const ImVec2 &p0, const ImVec2 &sz)
{
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID(r.stat);
    ImGui::InvisibleButton("s", sz, ImGuiButtonFlags_MouseButtonLeft);
    const bool hov = ImGui::IsItemHovered();
    if (hov && r.tip > 0)
        wrapped_tooltip(get_string(r.tip));
    ImGui::PopID();

    relief::well(dl, p0, p1, 3.0f);
    blit_fit(dl, r.spr, ImVec2(p0.x + 2.0f, p0.y + 2.0f), ImVec2(sz.y - 4.0f, sz.y - 4.0f), IM_COL32_WHITE);
    FeStylePushFont(FeFont_Body);
    dl->AddText(ImVec2(p0.x + sz.y + 3.0f, p0.y + (sz.y - ImGui::GetFontSize()) * 0.5f),
                COL_TEXT, creature_statistic_text(thing, (CreatureLiveStatId)r.stat));
    FeStylePopFont();
    if (hov)
        dl->AddRect(p0, p1, COL_HOVER, 3.0f, 0, 2.0f);
}

// GMnu_SPELL_LOST -- shown top-down after you lose your dungeon heart.
// The legacy menu is one usable button (the possess icon -> go spectator)
// plus 15 empty frames.
void spell_lost_panel(void)
{
    ImDrawList *dl = ImGui::GetWindowDrawList();
    FeStylePushFont(FeFont_Body);
    dl->AddText(grid_pt(8.0f, 202.0f), COL_SUBTEXT, get_string(CpgStr_LevelLost));
    FeStylePopFont();

    const ImVec2 p0 = grid_pt(50.0f, 250.0f), sz = grid_sz(40.0f, 44.0f);
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID("slPossess");
    const bool pressed = ImGui::InvisibleButton("p", sz, ImGuiButtonFlags_MouseButtonLeft);
    const bool hov = ImGui::IsItemHovered();
    const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);
    relief::well(dl, p0, p1, 3.0f);
    blit_fit(dl, GPS_keepower_possess_std_s, ImVec2(p0.x + 2.0f, p0.y + 2.0f),
             ImVec2(sz.x - 4.0f, sz.y - 4.0f), IM_COL32_WHITE);
    if (hov)
        dl->AddRect(p0, p1, COL_HOVER, 3.0f, 0, 2.0f);
    if (hov)
        wrapped_tooltip(get_string(CpgStr_PowerDesc1 + 0));
    ImGui::PopID();
    if (pressed)
        set_players_packet_action(get_my_player(), PckA_GoSpectator, 0, 0, 0, 0);
}

void creature_query_panel(void)
{
    struct PlayerInfo *me = get_my_player();
    struct Thing *thing = thing_get(me->controlled_thing_idx);
    if (!thing_is_creature(thing))
        return;
    struct CreatureControl *cctrl = creature_control_get_from_thing(thing);
    struct CreatureModelConfig *crconf = creature_stats_get_from_thing(thing);
    if (cctrl == nullptr || crconf == nullptr)
        return;
    ImDrawList *dl = ImGui::GetWindowDrawList();

    // Header laid out 10:60:10:10:5:10:5
    // (spacer : portrait : spacer : anger : spacer : xp : spacer) across
    // vx 3..137. The portrait well hugs the fitted image, left-aligned in
    // its slot; the two vertical bars take the fixed columns to its right.
    {
        auto hx = [](float units) { return grid_pt(3.0f + units * 134.0f / 110.0f, 0.0f).x; };
        const float port_x = 3.0f + 10.0f * 134.0f / 110.0f;
        const float ang_x0 = hx(80.0f), ang_x1 = hx(90.0f);
        const float xp_x0  = hx(95.0f), xp_x1  = hx(105.0f);

        const short qspr = get_creature_model_graphics(thing->model, CGI_QuerySymbol);
        int qw = 0, qh = 0;
        FeGuiPanelTexture(qspr, &qw, &qh);
        const float img_h = grid_pt(0.0f, 243.0f).y - grid_pt(0.0f, 190.0f).y - 6.0f;
        const float qar   = (qw > 0 && qh > 0) ? (float)qw / (float)qh : 1.0f;
        const float img_w = img_h * qar;
        const ImVec2 qw0 = grid_pt(port_x, 190.0f);
        const ImVec2 qw1(qw0.x + img_w + 6.0f, qw0.y + img_h + 6.0f);
        relief::well(dl, qw0, qw1, 3.0f);
        blit_fit(dl, qspr, ImVec2(qw0.x + 3.0f, qw0.y + 3.0f), ImVec2(img_w, img_h), IM_COL32_WHITE);

        int32_t atyp = 0, apct = 0;
        anger_get_creature_highest_anger_type_and_byte_percentage(thing, &atyp, &apct);
        const long xp_need = (long)(crconf->to_level[cctrl->exp_level]) << 8;

        auto vbar = [&](float x0, float x1, float frac, ImU32 fill) {
            if (frac < 0.0f) frac = 0.0f; else if (frac > 1.0f) frac = 1.0f;
            const ImVec2 b0(x0, grid_pt(0.0f, 192.0f).y), b1(x1, grid_pt(0.0f, 241.0f).y);
            relief::well(dl, b0, b1, 2.0f);
            dl->AddRectFilled(ImVec2(b0.x + 1.0f, b1.y - (b1.y - b0.y - 2.0f) * frac),
                              ImVec2(b1.x - 1.0f, b1.y - 1.0f), fill, 1.5f);
        };
        vbar(ang_x0, ang_x1, (float)apct / 256.0f, IM_COL32(210, 90, 60, 255));
        vbar(xp_x0, xp_x1, xp_need > 0 ? (float)cctrl->exp_points / (float)xp_need : 0.0f,
             IM_COL32(120, 200, 235, 255));

        // creature level, centred over the top of the xp bar.
        char xl[8]; std::snprintf(xl, sizeof(xl), "%d", (int)(cctrl->exp_level + 1));
        FeStylePushFont(FeFont_Caption);
        const ImVec2 ts = ImGui::CalcTextSize(xl);
        dl->AddText(ImVec2((xp_x0 + xp_x1) * 0.5f - ts.x * 0.5f, grid_pt(0.0f, 193.0f).y),
                    IM_COL32(255, 235, 160, 255), xl);
        FeStylePopFont();
    }

    // Health bar -- name centred inside it, no numeric value. Click cycles
    // to the next queried creature.
    {
        ImGui::SetCursorScreenPos(grid_pt(4.0f, 248.0f));
        ImGui::PushID("cqName");
        const bool pressed = ImGui::InvisibleButton("n", grid_sz(130.0f, 18.0f),
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool rclick = ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        long maxh = (cctrl->max_health > 0) ? cctrl->max_health : 1;
        long hp = thing->health < 0 ? 0 : thing->health;
        prog_bar(grid_pt(4.0f, 248.0f), grid_pt(134.0f, 266.0f),
                 (float)hp / (float)maxh, IM_COL32(200, 60, 50, 255),
                 creature_statistic_text(thing, CrLStat_FirstName));
        ImGui::PopID();
        if (pressed)      gui_query_next_creature_of_owner(nullptr);
        else if (rclick)  gui_query_next_creature_of_owner_and_model(nullptr);
    }

    // 2-tab strip (replaces the legacy next-page button).
    static const char *const tabs[2] = { "ABILITIES", "STATS" };
    for (int t = 0; t < 2; t++)
    {
        char sid[8]; std::snprintf(sid, sizeof(sid), "cqt%d", t);
        if (cell_button(sid, grid_pt(4.0f + t * 67.0f, 270.0f), grid_sz(64.0f, 16.0f),
                        tabs[t], s_query_detail_tab == t))
            s_query_detail_tab = t;
    }

    if (s_query_detail_tab == 0)
    {
        // Ability grid -- 3 wide, tight, fat cooldown bars, hotkey numbers.
        const ImVec2 cell = grid_sz(43.0f, 37.0f);
        const float pitch_x = 44.0f * s_panel.w / 140.0f;
        const float pitch_y = 40.0f * s_panel.h / 400.0f;
        const ImVec2 org = grid_pt(3.0f, 291.0f);
        for (int pos = 0; pos < 10; pos++)
        {
            if (creature_instance_get_available_id_for_pos(thing, pos) <= 0)
                break;
            const ImVec2 p0(org.x + (pos % 3) * pitch_x, org.y + (pos / 3) * pitch_y);
            instance_cell(thing, cctrl, pos, p0, cell);
        }
    }
    else
    {
        // Stats -- 2 per row, bordered cells, scrollable, tooltips.
        ImGui::SetCursorScreenPos(grid_pt(2.0f, 289.0f));
        ImGui::BeginChild("##cqStats",
                          ImVec2(grid_sz(136.0f, 0.0f).x, grid_pt(0.0f, 396.0f).y - grid_pt(0.0f, 289.0f).y),
                          false, ImGuiWindowFlags_NoBackground);
        const ImVec2 base = ImGui::GetCursorScreenPos();
        const int nrows = (int)(sizeof(k_stat_rows) / sizeof(k_stat_rows[0]));
        const float cw = grid_sz(67.0f, 0.0f).x;
        const float chh = grid_sz(0.0f, 12.0f).y;
        const float rp = chh + 1.0f;
        int i = 0;
        for (const StatRow &r : k_stat_rows)
        {
            const ImVec2 p0(base.x + (i % 2) * (cw + 1.0f), base.y + (i / 2) * rp);
            stat_cell(thing, r, p0, ImVec2(cw, chh));
            i++;
        }
        ImGui::Dummy(ImVec2(1.0f, ((nrows + 1) / 2) * rp));
        ImGui::EndChild();
    }
}

} // namespace

void ingame_tabcontent_draw(float px, float py, float pw, float ph)
{
    if (!RendererImGuiEnabled())
        return;
    if (pw <= 0.0f || ph <= 0.0f)
        return;
    s_panel = { px, py, pw, ph };

    // The query / lost-keeper panels take priority: possession leaves the
    // pre-possession tab body (e.g. GMnu_CREATURE) still turned on
    // (pinstfs_passenger_control_creature only calls turn_off_all_window_menus,
    // not turn_off_all_panel_menus), then turns GMnu_CREATURE_QUERY1 on over it.
    // In possession the query menu can also lag a frame behind the view
    // switch, so key off the view type directly as well.
    const struct PlayerInfo *me = get_my_player();
    const bool in_possession = me != nullptr
        && (me->view_type == PVT_CreatureContrl || me->view_type == PVT_CreaturePasngr);

    void (*body)(void) = nullptr;
    if (in_possession
     || menu_is_active(GMnu_CREATURE_QUERY1) || menu_is_active(GMnu_CREATURE_QUERY2)
     || menu_is_active(GMnu_CREATURE_QUERY3) || menu_is_active(GMnu_CREATURE_QUERY4))
        body = &creature_query_panel;
    else if (menu_is_active(GMnu_SPELL_LOST)) body = &spell_lost_panel;
    else if (menu_is_active(GMnu_ROOM))       body = &room_grid;
    else if (menu_is_active(GMnu_SPELL))      body = &spell_grid;
    else if (menu_is_active(GMnu_TRAP))       body = &trap_grid;
    else if (menu_is_active(GMnu_CREATURE))   body = &creature_list;
    else if (menu_is_active(GMnu_QUERY))      body = &query_panel;
    if (body == nullptr)
        return;

    // Opaque bg over the tab-content region (the frame left a transparent
    // hole there so 3D doesn't bleed through). A raised, mottled stone face
    // -- same material as the panel head -- so the recessed cells the body
    // draws read as pockets in it. Full inner width (x+2 .. x+w-2), matching
    // the tab-strip channel above. Runs inside the sidebar window's Begin/End.
    ImDrawList *bg_dl = ImGui::GetWindowDrawList();
    const ImVec2 c0(s_panel.x + 2.0f, grid_pt(0.0f, 190.0f).y);
    const ImVec2 c1(s_panel.x + s_panel.w - 2.0f, grid_pt(0.0f, 398.0f).y);
    relief::face(bg_dl, c0, c1);
    body();
}

#ifdef FUNCTESTING
extern "C" void ingame_tabcontent_test_fire(int action, long arg)
{
    struct PlayerInfo *me = get_my_player();
    switch ((enum IngameTabTestAction)action)
    {
    case ITTA_RoomBuild:
        // room_grid() left-click
        activate_room_build_mode((RoomKind)arg, GUIStr_Empty);
        break;
    case ITTA_RoomSell:
        do_sell_rooms();
        break;
    case ITTA_SpellChoose:
        // spell_grid() left-click
        choose_spell((PowerKind)arg, GUIStr_Empty);
        break;
    case ITTA_TrapChoose:
        // trap_grid() left-click
        choose_workshop_item((int)arg, GUIStr_Empty);
        break;
    case ITTA_TrapSell:
        do_sell_traps();
        break;
    case ITTA_CreaturePickAny:
        // creature_list() portrait left-click
        creature_pick((ThingModel)arg, CrGUIJob_Any, true);
        break;
    case ITTA_QueryMode:
        // query_panel() QUERY button
        set_players_packet_action(me, PckA_SetPlyrState, PSt_CreatrQuery, 0, 0, 0);
        break;
    case ITTA_AllyToggle:
        // query_panel() ally toggle
        set_players_packet_action(me, PckA_PlyrToggleAlly, (PlayerNumber)arg, 0, 0, 0);
        break;
    case ITTA_TendImprison:
        set_players_packet_action(me, PckA_ToggleTendency, CrTend_Imprison, 0, 0, 0);
        break;
    case ITTA_TendFlee:
        set_players_packet_action(me, PckA_ToggleTendency, CrTend_Flee, 0, 0, 0);
        break;
    }
}
#endif
