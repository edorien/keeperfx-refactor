#include "pre_inc.h"
#include "frontgui_ingame_creature.h"

#include "frontgui_ingame_cells.h"     // fe_hud_cell/fe_hud_bar, grid_pt/grid_sz, COL_*
#include "frontgui_widgets.h"          // imgui.h
#include "frontgui_style.h"            // FeStylePushFont/PopFont, FeFont_*
#include "frontgui_sprite_tex.h"       // FeGuiPanelTexture
#include "frontgui_ingame_relief.h"    // relief::well / accents()
#include "frontgui_ingame_layout.h"    // tcl:: -- named virtual-grid positions

#include "globals.h"
#include "sprites.h"                   // GPS_* sprite id constants
#include "frontmenu_ingame_tabs.h"     // gui_creature_type_highlighted, gui_query_next_creature_of_owner*,
                                        // get_creature_pick_flags, anger_get_creature_highest_anger_type_and_byte_percentage
#include "player_data.h"               // get_my_player, my_player_number, get_player, player_exists/_is_keeper/_allied_with/_is_roaming
#include "packet_data.h"               // set_players_packet_action, PckA_*, PSt_CreatrQuery, CrTend_*
#include "dungeon_data.h"              // get_my_dungeon, dungeon_invalid, get_players_dungeon, player_has_room_of_role
#include "config_terrain.h"            // RoRoF_Prison
#include "config_creature.h"           // get_players_special_digger_model, breed_activities, CREATURE_ANY
#include "creature_graphics.h"         // get_creature_model_graphics, CGI_HandSymbol/CGI_QuerySymbol
#include "creature_states.h"           // state_type_to_gui_state, STATE_TYPES_COUNT
#include "thing_creature.h"            // pick_up_creature_of_model_and_gui_job, go_to_next_creature_of_model_and_gui_job
#include "kfx_frontend_state.h"        // no_of_breeds_owned, top_of_breed_list
#include "creature_states_rsrch.h"     // get_players_current_research_val
#include "room_workshop.h"             // manufacture_points_required
#include "config_spritecolors.h"       // get_player_colored_icon_idx
#include "thing_stats.h"               // creature_statistic_text, CrLStat_*
#include "creature_instances.h"        // creature_instance_get_available_id_for_pos, InstanceInfo
#include "creature_control.h"          // struct CreatureControl
#include "config_strings.h"
#include "kfx_config_state.h"          // conf.crtr_conf.model_count, conf.rules[...].gameplay.pay_day_gap
#include "kfx_sim_state.h"             // creatures_tend_imprison / _flee
#include "frontgui_ingame_icon_overrides.h" // FeIconOverride* -- docs/refactor/ingame-gui/12-png-icon-overrides.md

#include "post_inc.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

// A recessed labelled cell -- the same "pocket" the room/spell cells use.
// `selected` lights a gold rim (for the ABILITIES/STATS toggle). Returns
// 1/2 on L/R click.
int cell_button(const char *sid, const ImVec2 &p0, const ImVec2 &sz,
                const char *text, bool selected)
{
    FeHudCellOpts o;
    o.rounding = 2.0f;
    o.rclick   = true;
    o.selected = selected;
    o.text     = text;
    return fe_hud_cell(sid, p0, sz, o);
}

// A GPS_* panel-sprite cell that toggles. `on` -> pulsing gold border;
// `available == false` -> greyed, non-interactive. Returns true on a
// left click (only when available). Deliberately its own look (pulse
// animation, fill-colour state instead of a well) -- not folded into
// fe_hud_cell (10-maintainability-refactors.md §6).
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
    FeHudBarOpts bo;
    bo.fill          = fill;
    bo.fill_rounding = 1.5f;
    bo.label         = label;
    fe_hud_bar(ImVec2(p0.x + row_h + gap, p0.y + 2.0f), ImVec2(p1.x, p1.y - 2.0f), frac, bo);
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

// one ability cell: hotkey number (top-left), icon, a fat cooldown bar
// at the bottom, active-instance ring.
void instance_cell(struct Thing *thing, struct CreatureControl *cctrl, int pos,
                   const ImVec2 &p0, const ImVec2 &sz)
{
    const CrInstance inst_id = creature_instance_get_available_id_for_pos(thing, pos);
    const struct InstanceInfo *ii = creature_instance_info_get(inst_id);
    const bool active = cctrl->active_instance_id == inst_id;
    const bool on_cd = !creature_instance_has_reset(thing, inst_id);

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

    short spr = (short)ii->symbol_spridx;
    if (on_cd) spr++;   // legacy disabled frame

    // docs/refactor/ingame-gui/12-png-icon-overrides.md: "active" here
    // means ready (not on cooldown) -- the same sense build_icon()'s
    // `afford` uses.
    int ow = 0, oh = 0;
    bool odim = false;
    void *otex = FeIconOverrideActiveInactive("ability", creature_instance_code_name(inst_id),
                                              !on_cd, &ow, &oh, &odim);
    const unsigned int otint = odim ? IM_COL32(255, 255, 255, 120) : IM_COL32_WHITE;

    char kb[4]; std::snprintf(kb, sizeof(kb), "%d", (pos + 1) % 10);

    FeHudCellOpts o;
    o.selected = active;
    o.hotkey   = kb;
    if (ii->tooltip_stridx > 0)
        o.tooltip = get_string(ii->tooltip_stridx);
    o.content = [spr, on_cd, frac, otex, ow, oh, otint](ImDrawList *dl, const ImVec2 &cp0, const ImVec2 &csz) {
        const float bar_h = csz.y * 0.30f;
        if (otex != nullptr)
            blit_fit_tex(dl, otex, ow, oh, ImVec2(cp0.x + 2.0f, cp0.y + 2.0f),
                        ImVec2(csz.x - 4.0f, csz.y - bar_h - 4.0f), otint);
        else
            blit_fit(dl, spr, ImVec2(cp0.x + 2.0f, cp0.y + 2.0f), ImVec2(csz.x - 4.0f, csz.y - bar_h - 4.0f),
                     on_cd ? IM_COL32(255, 255, 255, 120) : IM_COL32_WHITE);
        FeHudBarOpts bo;
        bo.rounding = 1.5f;
        bo.fill     = on_cd ? IM_COL32(200, 120, 60, 255) : relief::accents().bar_good;
        fe_hud_bar(ImVec2(cp0.x + 2.0f, cp0.y + csz.y - bar_h), ImVec2(cp0.x + csz.x - 2.0f, cp0.y + csz.y - 2.0f),
                  frac, bo);
    };

    char sid[12]; std::snprintf(sid, sizeof(sid), "i%d", pos);
    fe_hud_cell(sid, p0, sz, o);
}

// one stat cell: bordered, icon + value; hover shows the stat's tooltip.
void stat_cell(struct Thing *thing, const StatRow &r, const ImVec2 &p0, const ImVec2 &sz)
{
    char sid[16]; std::snprintf(sid, sizeof(sid), "st%d", r.stat);
    FeHudCellOpts o;
    o.sprite = r.spr;
    o.text   = creature_statistic_text(thing, (CreatureLiveStatId)r.stat);
    if (r.tip > 0)
        o.tooltip = get_string(r.tip);
    fe_hud_cell(sid, p0, sz, o);
}

// One creature-model cell for the horizontal scroll strip (DK2-style,
// live-tested request): the query/possession portrait (CGI_QuerySymbol,
// not the small CGI_HandSymbol icon) letterboxed (blit_fit -- cropping
// via a cover-scale first pass read as too tight/zoomed once live-tested
// against the fuller cell size the vertical icon column below now
// affords), idle/work/fight counts stacked in a column down the left
// edge (a translucent backing strip keeps them legible against the
// cell's well background), owned count top-right. Read-only counts -- the
// per-job pick that creature_list()'s separate job cells offer isn't
// carried over here; use creature_list_horizontal()'s own vertical
// idle/work/fight icon column for a job-specific pick across every owned
// model, or GUI_POSITION Left/Right for the per-model version. L/R click
// on the cell picks up / zooms to the next of this model in any job, same
// as the portrait alone does in creature_list().
void creature_cell_horizontal(struct Dungeon *dungeon, ThingModel crmodel, int slot,
                              const ImVec2 &p0, const ImVec2 &sz)
{
    unsigned int cnt[3] = { 0, 0, 0 };
    for (int n = 0; n < STATE_TYPES_COUNT; n++)
    {
        const long gsi = state_type_to_gui_state[n];
        if (gsi >= 0 && gsi < 3)
            cnt[gsi] += dungeon->crmodel_state_type_count[crmodel][n];
    }
    const short portrait_spr = get_creature_model_graphics(crmodel, CGI_QuerySymbol);
    const unsigned int owned = dungeon->owned_creatures_of_model[crmodel];
    int ow = 0, oh = 0;
    void *otex = FeIconOverrideSingle("creature_portrait", creature_code_name(crmodel), &ow, &oh);

    FeHudCellOpts o;
    o.rclick  = true;
    o.content = [portrait_spr, cnt, owned, otex, ow, oh](ImDrawList *dl, const ImVec2 &cp0, const ImVec2 &csz) {
        if (otex != nullptr)
            blit_fit_tex(dl, otex, ow, oh, ImVec2(cp0.x + 2.0f, cp0.y + 2.0f),
                        ImVec2(csz.x - 4.0f, csz.y - 4.0f), IM_COL32_WHITE);
        else
            blit_fit(dl, portrait_spr, ImVec2(cp0.x + 2.0f, cp0.y + 2.0f),
                    ImVec2(csz.x - 4.0f, csz.y - 4.0f), IM_COL32_WHITE);

        // idle/work/fight, stacked top-to-bottom in a narrow column down
        // the left edge.
        const float col_w = std::max(csz.x * 0.24f, 16.0f);
        dl->AddRectFilled(cp0, ImVec2(cp0.x + col_w, cp0.y + csz.y), IM_COL32(10, 8, 4, 170));
        FeStylePushFont(FeFont_Caption);
        const float row_h = csz.y / 3.0f;
        static const ImU32 col_colors[3] = {
            IM_COL32(210, 235, 255, 255), IM_COL32(255, 225, 160, 255), IM_COL32(255, 130, 120, 255)
        };
        for (int j = 0; j < 3; j++)
        {
            char b[8]; std::snprintf(b, sizeof(b), "%u", cnt[j]);
            const ImVec2 ts = ImGui::CalcTextSize(b);
            dl->AddText(ImVec2(cp0.x + (col_w - ts.x) * 0.5f, cp0.y + (float)j * row_h + (row_h - ts.y) * 0.5f),
                        col_colors[j], b);
        }
        // owned count -- top-right, over the portrait.
        char ob[8]; std::snprintf(ob, sizeof(ob), "%u", owned);
        const ImVec2 ots = ImGui::CalcTextSize(ob);
        dl->AddText(ImVec2(cp0.x + csz.x - ots.x - 3.0f, cp0.y + 1.0f), relief::accents().hotkey, ob);
        FeStylePopFont();
    };
    char sid[16]; std::snprintf(sid, sizeof(sid), "hcr%d", slot);
    const int hit = fe_hud_cell(sid, p0, sz, o);
    if (hit == 1)      creature_pick(crmodel, CrGUIJob_Any, true);
    else if (hit == 2) creature_pick(crmodel, CrGUIJob_Any, false);
}

} // namespace

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
        void *tex = FeIconOverrideSingle("creature_icon", creature_code_name(crmodel), &sw, &sh);
        if (tex == nullptr)
            tex = FeGuiPanelTexture(get_creature_model_graphics(crmodel, CGI_HandSymbol), &sw, &sh);
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
            dl->AddText(ImVec2(pp0.x + 1.0f, pp1.y - ImGui::GetFontSize()), relief::accents().hotkey, tb);
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

// GUI_POSITION Bottom (docs/refactor/ingame-gui/11-horizontal-layout.md):
// one compact cell per owned model (creature_cell_horizontal(), above) in
// a single row that scrolls horizontally -- DK2's own creature panel
// shape (live-tested request), not a wrapping multi-row grid. Every owned
// breed shows, scrolled to rather than paged to -- no manual wheel-paging
// window like creature_list()'s own top_of_breed_list.
void creature_list_horizontal(void)
{
    struct Dungeon *dungeon = get_my_dungeon();
    if (dungeon_invalid(dungeon))
        return;
    const int model_count = kfx_config_state.conf.crtr_conf.model_count;
    const ThingModel digger = get_players_special_digger_model(my_player_number);

    float px, py, pw, ph;
    fe_hud_get_panel_rect(&px, &py, &pw, &ph);

    // Pick-next idle/working/fighting across all owned models -- same
    // capability as creature_list()'s own header row, but as a vertical
    // column of icon cells down the panel's left edge instead of 3 word
    // buttons across a horizontal band (live-tested request: icons over
    // words, vertical over horizontal -- also frees the whole panel
    // height for the scroll strip rather than losing a band off the top
    // to the header). Same legacy sprites/tooltips the vertical layout's
    // own BID_CRTR_NXWNDR/NXWRKR/NXFIGT buttons use.
    static const short job_spr[3] = {
        GPS_rpanel_tab_crtr_wandr_act, GPS_rpanel_tab_crtr_work_act, GPS_rpanel_tab_crtr_fight_act
    };
    static const int job_tip[3] = {
        GUIStr_CreatureIdleDesc, GUIStr_CreatureWorkingDesc, GUIStr_CreatureFightingDesc
    };
    const float col_w    = std::max(ph * 0.12f, 32.0f);
    const float job_cell_h = ph / 3.0f;
    for (int j = 0; j < 3; j++)
    {
        FeHudCellOpts o;
        o.sprite  = job_spr[j];
        o.rclick  = true;
        o.tooltip = get_string(job_tip[j]);
        char sid[8]; std::snprintf(sid, sizeof(sid), "hh%d", j);
        const int hit = fe_hud_cell(sid, ImVec2(px, py + (float)j * job_cell_h),
                                    ImVec2(col_w - 3.0f, job_cell_h - 3.0f), o);
        if (hit != 0)
        {
            const long gj = (j == 0) ? CrGUIJob_Wandering : (j == 1) ? CrGUIJob_Working : CrGUIJob_Fighting;
            creature_pick(CREATURE_ANY, gj, hit == 1);
        }
    }

    // The strip: the special digger first (unconditionally, matching
    // creature_list()'s own row-0 convention), then every owned breed, in
    // one horizontally-scrolling row filling the rest of the width, full
    // panel height (no header band to share it with any more).
    const float strip_x0 = px + col_w;
    const float strip_w  = pw - col_w;
    ImGui::SetCursorScreenPos(ImVec2(strip_x0, py));
    ImGui::BeginChild("##hCrList", ImVec2(strip_w, ph), false,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_HorizontalScrollbar);
    const ImVec2 base = ImGui::GetCursorScreenPos();
    const float cell_h = ph - ImGui::GetStyle().ScrollbarSize;
    const float cell_w = cell_h * 0.8f;
    const float pitch  = cell_w + 6.0f;

    int slot = 0;
    if (digger > 0 && digger < model_count)
    {
        creature_cell_horizontal(dungeon, digger, slot, ImVec2(base.x + (float)slot * pitch, base.y),
                                 ImVec2(cell_w, cell_h));
        slot++;
    }
    const int owned_breeds = kfx_frontend_state.no_of_breeds_owned;
    for (int i = 0; i < owned_breeds; i++)
    {
        const ThingModel crmodel = breed_activities[i];
        if (crmodel <= 0 || crmodel >= model_count || crmodel == digger)
            continue;
        if (dungeon->owned_creatures_of_model[crmodel] <= 0)
            continue;
        creature_cell_horizontal(dungeon, crmodel, slot, ImVec2(base.x + (float)slot * pitch, base.y),
                                 ImVec2(cell_w, cell_h));
        slot++;
    }
    ImGui::Dummy(ImVec2((float)slot * pitch, 1.0f)); // registers the scrollable extent
    if (ImGui::IsWindowHovered())
    {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
            ImGui::SetScrollX(ImGui::GetScrollX() - wheel * pitch);
    }
    ImGui::EndChild();
}

// ---- query / information panel -----------------------------------

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
            relief::accents().bar_good);

    float wrk_f = 0.0f;
    if (dungeon->manufacture_class != TCls_Empty)
    {
        const long req = manufacture_points_required(dungeon->manufacture_class, dungeon->manufacture_kind);
        if (req > 0)
            wrk_f = (float)(dungeon->manufacture_progress >> 8) / (float)req;
    }
    bar_row(GPS_room_workshop_std_s, grid_pt(74.0f, 244.0f), grid_pt(134.0f, 262.0f),
            wrk_f, relief::accents().bar_good);

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

// GUI_POSITION Bottom (docs/refactor/ingame-gui/11-horizontal-layout.md):
// region B only, HudBottomWidth_Normal (determine_bottom_width_mode()
// doesn't special-case GMnu_QUERY -- this panel's several independent
// widgets need more width than Narrow gives, and nothing here benefits
// from Wide's extra space the way the creature strip's scroll does).
// query_panel()'s stacked-rows-down-a-140x400-virtual-space shape doesn't
// fit a wide-short rect, so it reflows into columns left to right instead:
// tendency toggles | an info column (payday full-width and thin, research +
// workshop side by side below it, then up to 4 per-player room/creature
// rows filling what's left) | query-mode + MP page-cycle. Same widgets/
// packets as query_panel(), just laid out against the panel rect directly
// rather than tcl:: grid constants.
void query_panel_horizontal(void)
{
    struct Dungeon *dungeon = get_my_dungeon();
    if (dungeon_invalid(dungeon))
        return;
    struct PlayerInfo *me = get_my_player();

    float px, py, pw, ph;
    fe_hud_get_panel_rect(&px, &py, &pw, &ph);
    float x = px;

    // Column: tendency toggles, stacked.
    const float tend_w = std::max(pw * 0.08f, 32.0f);
    const float tend_h = ph * 0.42f;
    const bool imp_avail = player_has_room_of_role(my_player_number, RoRoF_Prison);
    if (sprite_toggle("htImp", ImVec2(x, py), ImVec2(tend_w - 4.0f, tend_h),
                      imp_avail ? (short)GPS_rpanel_tendency_prisnd_act : (short)GPS_rpanel_tendency_prisnu_dis,
                      imp_avail && kfx_sim_state.creatures_tend_imprison != 0, imp_avail))
        set_players_packet_action(me, PckA_ToggleTendency, CrTend_Imprison, 0, 0, 0);
    if (sprite_toggle("htFlee", ImVec2(x, py + tend_h + 4.0f), ImVec2(tend_w - 4.0f, tend_h),
                      GPS_rpanel_tendency_fleed_act, kfx_sim_state.creatures_tend_flee != 0, true))
        set_players_packet_action(me, PckA_ToggleTendency, CrTend_Flee, 0, 0, 0);
    x += tend_w + 8.0f;

    // Query-mode + MP page-cycle column, at the right edge -- worked out
    // first since the info column (below) fills whatever's left before it.
    const float ctrl_w = std::max(pw * 0.09f, 38.0f);
    const float ctrl_x = px + pw - ctrl_w;
    const float info_w = ctrl_x - 6.0f - x;

    // Info column: payday full-width and thin (live-tested: the previous
    // one-third-height bar read as too short/stubby for how much width was
    // going spare), then research | workshop side by side on their own row
    // below it, then up to 4 per-player room/creature-count rows stacked
    // in whatever height is left -- top to bottom, most-frequently-glanced
    // (payday) first, same bar_row()/player_count_readout() vocabulary
    // query_panel() itself uses throughout.
    const float payday_h = std::max(ph * 0.15f, 16.0f);
    const long payday_gap = kfx_config_state.conf.rules[my_player_number].gameplay.pay_day_gap;
    char payday_lbl[24];
    std::snprintf(payday_lbl, sizeof(payday_lbl), "%ld", (long)dungeon->creatures_total_pay);
    bar_row(GPS_room_treasury_std_s, ImVec2(x, py), ImVec2(x + info_w, py + payday_h),
            payday_gap > 0 ? (float)kfx_config_state.pay_day_progress[my_player_number] / (float)payday_gap : 0.0f,
            IM_COL32(220, 190, 70, 255), payday_lbl);

    const float row2_y0 = py + payday_h + 4.0f;
    const float row2_h  = std::max(ph * 0.17f, 18.0f);
    const float half_w  = (info_w - 4.0f) * 0.5f;
    struct ResearchVal *rv = get_players_current_research_val(my_player_number);
    bar_row(GPS_room_research_std_s, ImVec2(x, row2_y0), ImVec2(x + half_w, row2_y0 + row2_h),
            (rv != nullptr && rv->req_amount > 0)
                ? (float)(dungeon->research_progress >> 8) / (float)rv->req_amount : 0.0f,
            relief::accents().bar_good);

    float wrk_f = 0.0f;
    if (dungeon->manufacture_class != TCls_Empty)
    {
        const long req = manufacture_points_required(dungeon->manufacture_class, dungeon->manufacture_kind);
        if (req > 0)
            wrk_f = (float)(dungeon->manufacture_progress >> 8) / (float)req;
    }
    bar_row(GPS_room_workshop_std_s, ImVec2(x + half_w + 4.0f, row2_y0), ImVec2(x + info_w, row2_y0 + row2_h),
            wrk_f, relief::accents().bar_good);

    // Player rows: whatever height is left below the two bar rows.
    const float players_y0 = row2_y0 + row2_h + 4.0f;
    const float players_h  = py + ph - players_y0;
    const int keepers = query_keeper_count();
    const int pages = keepers > 7 ? 3 : (keepers > 4 ? 2 : 1);
    if (s_query_page >= pages)
        s_query_page = 0;
    const float row_h = players_h / 4.0f;
    const float ih = std::min(row_h * 0.6f, 20.0f);
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
        const float ry = players_y0 + (float)row * row_h;

        player_count_readout(get_player_colored_icon_idx(GPS_plyrsym_symbol_room_red_std_a, p),
                             ImVec2(x, ry), ih, pd->total_rooms);
        player_count_readout(get_player_colored_icon_idx(GPS_plyrsym_symbol_player_red_std_a, p),
                             ImVec2(x + info_w * 0.5f, ry), ih, pd->num_active_creatrs);

        if (p != my_player_number && (pl->allocflags & PlaF_Allocated))
        {
            const bool allied = player_allied_with(me, p);
            const short as = allied
                ? get_player_colored_icon_idx(GPS_plyrsym_symbol_player_red_std_b, p)
                : (short)GPS_plyrsym_symbol_player_any_dis;
            char asid[16]; std::snprintf(asid, sizeof(asid), "hally%d", row);
            if (sprite_toggle(asid, ImVec2(x + info_w - ih - 2.0f, ry), ImVec2(ih, ih), as, allied, true))
                set_players_packet_action(me, PckA_PlyrToggleAlly, p, 0, 0, 0);
        }
    }

    if (glyph_toggle("hqMode", ImVec2(ctrl_x, py), ImVec2(ctrl_w, ph * 0.46f),
                     "?", IM_COL32(220, 70, 60, 255), me->work_state == PSt_CreatrQuery))
        set_players_packet_action(me, PckA_SetPlyrState, PSt_CreatrQuery, 0, 0, 0);
    if (pages > 1)
    {
        const ImVec2 q0(ctrl_x, py + ph * 0.46f + 4.0f), q1(ctrl_x + ctrl_w, py + ph);
        ImGui::SetCursorScreenPos(q0);
        ImGui::PushID("hqPage");
        if (ImGui::InvisibleButton("p", ImVec2(q1.x - q0.x, q1.y - q0.y), ImGuiButtonFlags_MouseButtonLeft))
            s_query_page = (s_query_page + 1) % pages;
        const bool hv = ImGui::IsItemHovered();
        ImDrawList *dl = ImGui::GetWindowDrawList();
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

// GMnu_SPELL_LOST -- shown top-down after you lose your dungeon heart.
// The legacy menu is one usable button (the possess icon -> go spectator)
// plus 15 empty frames.
void spell_lost_panel(void)
{
    ImDrawList *dl = ImGui::GetWindowDrawList();
    FeStylePushFont(FeFont_Body);
    dl->AddText(grid_pt(8.0f, tcl::lost::TEXT_Y), COL_SUBTEXT, get_string(CpgStr_LevelLost));
    FeStylePopFont();

    // 2px icon inset here (not the usual 3px) -- a deliberate difference
    // from build_icon's content, so it's kept as a custom `content`
    // callback rather than the plain `sprite` field.
    FeHudCellOpts o;
    o.tooltip = get_string(CpgStr_PowerDesc1 + 0);
    o.content = [](ImDrawList *cdl, const ImVec2 &cp0, const ImVec2 &csz) {
        blit_fit(cdl, GPS_keepower_possess_std_s, ImVec2(cp0.x + 2.0f, cp0.y + 2.0f),
                 ImVec2(csz.x - 4.0f, csz.y - 4.0f), IM_COL32_WHITE);
    };
    const int hit = fe_hud_cell("slPossess", grid_pt(tcl::lost::ICON_X0, tcl::lost::ICON_Y0),
                                grid_sz(tcl::lost::ICON_W, tcl::lost::ICON_H), o);
    if (hit == 1)
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
        void *qtex = FeIconOverrideSingle("creature_portrait", creature_code_name(thing->model), &qw, &qh);
        if (qtex == nullptr)
            FeGuiPanelTexture(qspr, &qw, &qh);
        const float img_h = grid_pt(0.0f, tcl::q::HEADER_Y1).y - grid_pt(0.0f, tcl::q::HEADER_Y0).y - 6.0f;
        const float qar   = (qw > 0 && qh > 0) ? (float)qw / (float)qh : 1.0f;
        const float img_w = img_h * qar;
        const ImVec2 qw0 = grid_pt(port_x, tcl::q::HEADER_Y0);
        const ImVec2 qw1(qw0.x + img_w + 6.0f, qw0.y + img_h + 6.0f);
        relief::well(dl, qw0, qw1, 3.0f);
        if (qtex != nullptr)
            blit_fit_tex(dl, qtex, qw, qh, ImVec2(qw0.x + 3.0f, qw0.y + 3.0f), ImVec2(img_w, img_h), IM_COL32_WHITE);
        else
            blit_fit(dl, qspr, ImVec2(qw0.x + 3.0f, qw0.y + 3.0f), ImVec2(img_w, img_h), IM_COL32_WHITE);

        int32_t atyp = 0, apct = 0;
        anger_get_creature_highest_anger_type_and_byte_percentage(thing, &atyp, &apct);
        const long xp_need = (long)(crconf->to_level[cctrl->exp_level]) << 8;

        auto vbar = [&](float x0, float x1, float frac, ImU32 fill) {
            const ImVec2 b0(x0, grid_pt(0.0f, tcl::q::BARS_Y0).y), b1(x1, grid_pt(0.0f, tcl::q::BARS_Y1).y);
            FeHudBarOpts bo;
            bo.fill          = fill;
            bo.fill_rounding = 1.5f;
            bo.vertical      = true;
            fe_hud_bar(b0, b1, frac, bo);
        };
        vbar(ang_x0, ang_x1, (float)apct / 256.0f, relief::accents().bar_warn);
        vbar(xp_x0, xp_x1, xp_need > 0 ? (float)cctrl->exp_points / (float)xp_need : 0.0f,
             relief::accents().bar_good);

        // creature level, centred over the top of the xp bar.
        char xl[8]; std::snprintf(xl, sizeof(xl), "%d", (int)(cctrl->exp_level + 1));
        FeStylePushFont(FeFont_Caption);
        const ImVec2 ts = ImGui::CalcTextSize(xl);
        dl->AddText(ImVec2((xp_x0 + xp_x1) * 0.5f - ts.x * 0.5f, grid_pt(0.0f, tcl::q::LEVEL_Y).y),
                    IM_COL32(255, 235, 160, 255), xl);
        FeStylePopFont();
    }

    // Health bar -- name centred inside it, no numeric value. Click cycles
    // to the next queried creature.
    {
        ImGui::SetCursorScreenPos(grid_pt(4.0f, tcl::q::HEALTH_Y0));
        ImGui::PushID("cqName");
        const bool pressed = ImGui::InvisibleButton("n", grid_sz(130.0f, tcl::q::HEALTH_Y1 - tcl::q::HEALTH_Y0),
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool rclick = ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        long maxh = (cctrl->max_health > 0) ? cctrl->max_health : 1;
        long hp = thing->health < 0 ? 0 : thing->health;
        FeHudBarOpts bo;
        bo.fill          = relief::accents().bar_bad;
        bo.fill_rounding = 1.5f;
        bo.label         = creature_statistic_text(thing, CrLStat_FirstName);
        fe_hud_bar(grid_pt(4.0f, tcl::q::HEALTH_Y0), grid_pt(134.0f, tcl::q::HEALTH_Y1),
                  (float)hp / (float)maxh, bo);
        ImGui::PopID();
        if (pressed)      gui_query_next_creature_of_owner(nullptr);
        else if (rclick)  gui_query_next_creature_of_owner_and_model(nullptr);
    }

    // 2-tab strip (replaces the legacy next-page button).
    static const char *const tabs[2] = { "ABILITIES", "STATS" };
    for (int t = 0; t < 2; t++)
    {
        char sid[8]; std::snprintf(sid, sizeof(sid), "cqt%d", t);
        if (cell_button(sid, grid_pt(4.0f + t * 67.0f, tcl::q::DETAIL_TABS_Y),
                        grid_sz(64.0f, tcl::q::DETAIL_TABS_H),
                        tabs[t], s_query_detail_tab == t))
            s_query_detail_tab = t;
    }

    if (s_query_detail_tab == 0)
    {
        // Ability grid -- 3 wide, tight, fat cooldown bars, hotkey numbers.
        const ImVec2 cell = grid_sz(tcl::q::ABIL_CELL_W, tcl::q::ABIL_CELL_H);
        const float pitch_x = (tcl::q::ABIL_CELL_W + 1.0f) * grid_sz(1.0f, 0.0f).x;
        const float pitch_y = tcl::q::ABIL_PITCH * grid_sz(0.0f, 1.0f).y;
        const ImVec2 org = grid_pt(3.0f, tcl::q::ABIL_ORG_Y);
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
        ImGui::SetCursorScreenPos(grid_pt(2.0f, tcl::q::STATS_Y0));
        ImGui::BeginChild("##cqStats",
                          ImVec2(grid_sz(136.0f, 0.0f).x,
                                 grid_pt(0.0f, tcl::q::STATS_Y1).y - grid_pt(0.0f, tcl::q::STATS_Y0).y),
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

// GUI_POSITION Bottom (docs/refactor/ingame-gui/11-horizontal-layout.md):
// region B only -- the minimap in region A is untouched, same as every
// other tab; the 5 tab-header icons above this still show too (drawn
// unconditionally by draw_tabs_horizontal(), frontgui_ingame_panel.cpp).
// Region B itself is narrower here than its Room/Spell/Trap default
// (HudBottomWidth_Narrow, frontgui_ingame_panel.cpp's
// determine_bottom_width_mode()) -- there's nothing here that benefits
// from extra width the way the creature strip's scroll does.
// Live-tested layout, left to right: a large portrait (70% of the panel's
// height) with the anger/xp bars horizontal underneath it, a vertical
// health bar, then the ability grid filling the rest. No Abilities/Stats
// toggle -- stats aren't shown here at all for now (dropped rather than
// fixed-and-kept: a second, narrower detail view competing for the same
// space this panel is already tight on wasn't worth it; Left/Right still
// have the full Stats page). An ability instance has a hard cap of 10
// (creature_instances.h), so the grid is a fixed 5 columns x 2 rows --
// wide-and-short cells (the original 2x5) rendered the icons tiny for
// how little vertical room region B has; 5x2 gives each cell far more
// height to work with. Every ability visible at once, never scrolled.
void creature_query_panel_horizontal(void)
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

    float px, py, pw, ph;
    fe_hud_get_panel_rect(&px, &py, &pw, &ph);

    // Portrait -- square, 70% of the panel's height, top-left.
    const float port_side = ph * 0.70f;
    const ImVec2 qw0(px + 2.0f, py + 2.0f);
    const ImVec2 qw1(qw0.x + port_side - 4.0f, qw0.y + port_side - 4.0f);
    const short qspr = get_creature_model_graphics(thing->model, CGI_QuerySymbol);
    relief::well(dl, qw0, qw1, 3.0f);
    {
        int ow = 0, oh = 0;
        void *otex = FeIconOverrideSingle("creature_portrait", creature_code_name(thing->model), &ow, &oh);
        const ImVec2 ip0(qw0.x + 3.0f, qw0.y + 3.0f), isz(qw1.x - qw0.x - 6.0f, qw1.y - qw0.y - 6.0f);
        if (otex != nullptr)
            blit_fit_tex(dl, otex, ow, oh, ip0, isz, IM_COL32_WHITE);
        else
            blit_fit(dl, qspr, ip0, isz, IM_COL32_WHITE);
    }

    // Anger + xp, horizontal bars stacked in the height left below the
    // portrait, same width as it. Level number overlaid on the xp bar.
    int32_t atyp = 0, apct = 0;
    anger_get_creature_highest_anger_type_and_byte_percentage(thing, &atyp, &apct);
    const long xp_need = (long)(crconf->to_level[cctrl->exp_level]) << 8;

    const float bars_y0  = qw0.y + port_side;
    const float bar_each = (py + ph - bars_y0 - 2.0f) * 0.5f - 1.0f;
    FeHudBarOpts bo_ang;
    bo_ang.fill          = relief::accents().bar_warn;
    bo_ang.fill_rounding = 1.5f;
    fe_hud_bar(ImVec2(qw0.x, bars_y0), ImVec2(qw1.x, bars_y0 + bar_each),
              (float)apct / 256.0f, bo_ang);

    char xl[8]; std::snprintf(xl, sizeof(xl), "Lv %d", (int)(cctrl->exp_level + 1));
    FeHudBarOpts bo_xp;
    bo_xp.fill          = relief::accents().bar_good;
    bo_xp.fill_rounding = 1.5f;
    bo_xp.label         = xl;
    fe_hud_bar(ImVec2(qw0.x, bars_y0 + bar_each + 2.0f), ImVec2(qw1.x, bars_y0 + 2.0f * bar_each + 2.0f),
              xp_need > 0 ? (float)cctrl->exp_points / (float)xp_need : 0.0f, bo_xp);

    // Vertical health bar -- full panel height, name overlay, right of
    // the portrait block. Click cycles creatures.
    const float hb_x0 = qw1.x + 8.0f;
    const float hb_x1 = hb_x0 + 24.0f;
    ImGui::SetCursorScreenPos(ImVec2(hb_x0, py));
    ImGui::PushID("hcqName");
    const bool pressed = ImGui::InvisibleButton("n", ImVec2(hb_x1 - hb_x0, ph),
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool rclick = ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    ImGui::PopID();
    long maxh = (cctrl->max_health > 0) ? cctrl->max_health : 1;
    long hp = thing->health < 0 ? 0 : thing->health;
    FeHudBarOpts bo_hp;
    bo_hp.fill          = relief::accents().bar_bad;
    bo_hp.fill_rounding = 1.5f;
    bo_hp.vertical      = true;
    bo_hp.label         = creature_statistic_text(thing, CrLStat_FirstName);
    fe_hud_bar(ImVec2(hb_x0, py), ImVec2(hb_x1, py + ph), (float)hp / (float)maxh, bo_hp);
    if (pressed)      gui_query_next_creature_of_owner(nullptr);
    else if (rclick)  gui_query_next_creature_of_owner_and_model(nullptr);

    // Ability grid -- fixed 5 columns x 2 rows, filling the rest of the
    // panel's width and height.
    const float body_x0 = hb_x1 + 8.0f;
    const float body_w  = px + pw - body_x0;
    const int cols = 5, rows = 2;
    const float pitch_x = body_w / (float)cols;
    const float pitch_y = ph / (float)rows;
    const float cell_w  = pitch_x * 0.92f;
    const float cell_h  = pitch_y * 0.92f;
    for (int pos = 0; pos < 10; pos++)
    {
        if (creature_instance_get_available_id_for_pos(thing, pos) <= 0)
            break;
        const ImVec2 p0(body_x0 + (pos % cols) * pitch_x, py + (pos / cols) * pitch_y);
        instance_cell(thing, cctrl, pos, p0, ImVec2(cell_w, cell_h));
    }
}
