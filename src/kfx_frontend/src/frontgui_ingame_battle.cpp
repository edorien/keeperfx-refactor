#include "pre_inc.h"
#include "frontgui_ingame_battle.h"

#include "frontgui_widgets.h"
#include "frontgui_style.h"
#include "frontgui_sprite_tex.h"     // FeGuiPanelTexture, FeGuiPanelIconButton

#include "globals.h"
#include "sprites.h"                  // GPS_message_*, GBS_guisymbols_sym_fight
#include "frontmenu_ingame_evnt.h"    // gui_next/previous_battle, gui_get_creature_in_battle, gui_go_to_person_in_battle, battle_creature_over
#include "frontend.h"                 // gui_close_objective
#include "player_data.h"              // my_player_number
#include "dungeon_data.h"             // get_players_num_dungeon, visible_battles
#include "creature_battle.h"          // friendly_battler_list, enemy_battler_list, creature_battle_get, MESSAGE_BATTLERS_COUNT
#include "creature_control.h"         // creature_control_get_from_thing, max_health
#include "creature_graphics.h"        // get_creature_model_graphics, CGI_HandSymbol
#include "thing_data.h"               // thing_get, thing_is_creature
#include "config_strings.h"           // GUIStr_*
#include "kfx_sim_state.h"

#include "post_inc.h"

#include <imgui.h>

namespace {

// One battler cell: the creature's hand-symbol icon (a square, so it
// matches the "vs" symbol and its neighbours) with a slim health bar
// under it. Hovering it sets battle_creature_over (what gui_setup_*_over
// did from the legacy button's mouse-x); click / right-click then run the
// same actions.
void battler_cell(unsigned short thing_idx, float x, float cell, float icon_h)
{
    struct Thing *thing = thing_get(thing_idx);
    if (!thing_is_creature(thing))
        return;

    const short spr_idx = get_creature_model_graphics(thing->model, CGI_HandSymbol);
    int sw = 0, sh = 0;
    void *tex = FeGuiPanelTexture(spr_idx, &sw, &sh);

    ImGui::SameLine(x + (cell - icon_h) * 0.5f);
    ImGui::PushID((int)thing_idx);
    ImGui::BeginGroup();

    if (tex != nullptr)
        ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(icon_h, icon_h));
    else
        ImGui::Dummy(ImVec2(icon_h, icon_h));

    const struct CreatureControl *cctrl = creature_control_get_from_thing(thing);
    long maxh = (cctrl != nullptr && cctrl->max_health > 0) ? cctrl->max_health : 1;
    long hp = thing->health;
    if (hp < 0) hp = 0;
    const float frac = (float)hp / (float)maxh;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float bar_h = 4.0f;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + icon_h, p.y + bar_h), IM_COL32(20, 12, 8, 220));
    dl->AddRectFilled(p, ImVec2(p.x + icon_h * frac, p.y + bar_h),
                      frac > 0.5f ? IM_COL32(90, 200, 90, 255)
                    : frac > 0.25f ? IM_COL32(220, 200, 60, 255)
                                   : IM_COL32(220, 70, 50, 255));
    ImGui::Dummy(ImVec2(icon_h, bar_h));

    ImGui::EndGroup();

    if (ImGui::IsItemHovered())
    {
        battle_creature_over = thing->index;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            gui_get_creature_in_battle(nullptr);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            gui_go_to_person_in_battle(nullptr);
    }
    ImGui::PopID();
}

int battler_count(const unsigned short *list, int visbtl_id)
{
    int n = 0;
    for (int b = 0; b < MESSAGE_BATTLERS_COUNT - 1; b++)
        if (list[MESSAGE_BATTLERS_COUNT * visbtl_id + b] != 0)
            n++;
    return n;
}

// A row per visible battle: friendly cells | crossed-swords "vs" | enemy
// cells, with the "vs" symbol centred on the row.
void battle_row(int visbtl_id, float icon_h)
{
    const struct Dungeon *dungeon = get_players_num_dungeon(my_player_number);
    const BattleIndex battle_id = dungeon->visible_battles[visbtl_id];
    const struct CreatureBattle *battle = creature_battle_get(battle_id);
    if (creature_battle_invalid(battle) || battle->fighters_num == 0)
        return;

    const float avail = ImGui::GetContentRegionAvail().x;
    const float cell = icon_h * 1.10f;
    const float gap  = icon_h * 0.35f;
    const float half_vs = icon_h * 0.5f;
    const float cx = avail * 0.5f;

    const int nf = battler_count(friendly_battler_list, visbtl_id);

    ImGui::PushID(visbtl_id);

    float fx = cx - half_vs - gap - nf * cell;
    if (fx < 0.0f) fx = 0.0f;
    for (int b = 0, drawn = 0; b < MESSAGE_BATTLERS_COUNT - 1; b++)
    {
        const unsigned short idx = friendly_battler_list[MESSAGE_BATTLERS_COUNT * visbtl_id + b];
        if (idx == 0) continue;
        battler_cell(idx, fx + drawn * cell, cell, icon_h);
        drawn++;
    }

    ImGui::SameLine(cx - half_vs);
    int fw = 0, fh = 0;
    void *fight = FeSpriteTexture(GBS_guisymbols_sym_fight, &fw, &fh);
    if (fight != nullptr && fh > 0)
        ImGui::Image((ImTextureID)(intptr_t)fight, ImVec2(icon_h, icon_h));
    else
        ImGui::TextUnformatted("vs");

    const float ex = cx + half_vs + gap;
    for (int b = 0, drawn = 0; b < MESSAGE_BATTLERS_COUNT - 1; b++)
    {
        const unsigned short idx = enemy_battler_list[MESSAGE_BATTLERS_COUNT * visbtl_id + b];
        if (idx == 0) continue;
        battler_cell(idx, ex + drawn * cell, cell, icon_h);
        drawn++;
    }
    ImGui::NewLine();
    ImGui::Dummy(ImVec2(0.0f, icon_h * 0.35f)); // row gap
    ImGui::PopID();
}

void do_close_battle(void) { gui_close_objective(nullptr); }
void do_prev_battle(void)  { gui_previous_battle(nullptr); }
void do_next_battle(void)  { gui_next_battle(nullptr); }

void (*s_pending_battle)(void) = nullptr;

} // namespace

void battlemenu_frame(void)
{
    ImGuiIO &io = ImGui::GetIO();

    if (s_pending_battle != nullptr)
    {
        void (*fn)(void) = s_pending_battle;
        s_pending_battle = nullptr;
        fn();
    }

    battle_creature_over = 0; // recomputed each frame from hover

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 10.0f),
                            ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.48f, io.DisplaySize.y * 0.24f), ImGuiCond_Always);
    ImGui::Begin("##IngameBattleBox", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    const float icon_h = io.DisplaySize.y * 0.058f;

    // Left column: just the close button now -- prev / next battle is the
    // scroll wheel over the list (user's call), no arrow buttons.
    ImGui::BeginGroup();
    if (FeGuiPanelIconButton("##btl_close", GPS_message_message_btn_accept_std,
                             get_string(GUIStr_CloseWindow), icon_h))
        s_pending_battle = do_close_battle;
    ImGui::EndGroup();
    ImGui::SameLine();

    // The battle rows in a scroll area -- a wheel past the top / bottom
    // pages through battles via the same gui_previous_battle /
    // gui_next_battle the legacy arrows used.
    const bool open = FeBeginScrollArea("##battle_rows", ImVec2(0, 0));
    if (open)
    {
        const float wheel = io.MouseWheel;
        if (wheel != 0.0f && ImGui::IsWindowHovered())
            s_pending_battle = (wheel < 0.0f) ? &do_next_battle : &do_prev_battle;

        for (int i = 0; i < 3; i++)
            battle_row(i, icon_h);
    }
    FeEndScrollArea();

    ImGui::End();
}
