#include "pre_inc.h"
#include "frontgui_ingame_debug.h"

#include "frontgui_widgets.h"
#include "frontgui_style.h"
#include "config_keeperfx.h" // ingame_gui_use_classic_hud

#include "globals.h"
#include "frontmenu_ingame_evnt.h" // *_enabled() predicates, TimerTurns, debug_display_network_stats
#include "front_input.h"           // update_time
#include "player_data.h"           // get_my_player, my_player_number
#include "dungeon_data.h"          // get_dungeon, turn_timers
#include "lvl_script_conditions.h" // get_condition_value
#include "game_merge.h"            // GGUI_ScriptTimer, game_flags2
#include "kfx_game_state.h"        // kfx_game_state.script_*/timer_real/bonus_time/flags_gui
#include "kfx_sim_state.h"         // kfx_sim_state.Timer / TimerGame / turns_per_second / armageddon_cast_turn
#include "kfx_net_state.h"         // kfx_net_state.input_lag_turns
#include "bflib_datetm.h"          // frametime_measurements, debug_display_frametime, stutter_detection_*
#include "bflib_basics.h"          // consoleLogArray / consoleLogArraySize / debug_display_consolelog
#include "bflib_enet.h"            // GetPing / GetPacketLoss / rate getters
#include "net_input_lag.h"         // input_lag_get_stats, INPUT_LAG_INCREASE_SAMPLE_MS
#include "net_exchange_gameplay.h" // multiplayer_speed_adjustment_ns
#include "vidmode.h"               // MyScreenWidth / MyScreenHeight

#include "post_inc.h"

#include <cinttypes> // PRId64
#include <cstdio>
#include <imgui.h>

namespace {

constexpr ImGuiWindowFlags kOverlayFlags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav
    | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
    | ImGuiWindowFlags_AlwaysAutoResize;

const float kPad = 12.0f;

// A borderless overlay panel pinned to a screen corner. pivot picks the
// anchored corner: (0,0) top-left ... (1,1) bottom-right.
bool begin_corner_overlay(const char *id, ImVec2 pivot)
{
    const ImGuiIO &io = ImGui::GetIO();
    const ImVec2 pos(kPad + pivot.x * (io.DisplaySize.x - 2.0f * kPad),
                     kPad + pivot.y * (io.DisplaySize.y - 2.0f * kPad));
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);
    ImGui::SetNextWindowBgAlpha(0.62f); // legible over the live 3D view
    return ImGui::Begin(id, nullptr, kOverlayFlags);
}

// --- script / player-facing readouts (top-right stack) -----------------

void real_time_clock(char *buf, size_t n, int nturns)
{
    if (nturns < 0) { std::snprintf(buf, n, "00:00:00"); return; }
    unsigned long total_seconds = ((unsigned long)nturns / kfx_sim_state.turns_per_second) + 1;
    unsigned long total_minutes = total_seconds / 60;
    std::snprintf(buf, n, "%02lu:%02lu:%02lu",
                  total_minutes / 60, total_minutes % 60, total_seconds % 60);
}

bool bonus_timer_line(char *buf, size_t n)
{
    if (!bonus_timer_enabled())
        return false;
    int nturns = kfx_game_state.bonus_time - (int)get_gameturn();
    if (kfx_game_state.timer_real)
        real_time_clock(buf, n, nturns);
    else
    {
        if (nturns < 0) nturns = 0;
        else if (nturns > 99999) nturns = 99999;
        std::snprintf(buf, n, "%05d", nturns / 2);
    }
    return true;
}

bool script_timer_line(char *buf, size_t n)
{
    if (!script_timer_enabled())
        return false;
    const struct Dungeon *dungeon = get_dungeon(kfx_game_state.script_timer_player);
    const unsigned long limit = kfx_game_state.script_timer_limit;
    const long base = (long)get_gameturn() - (long)dungeon->turn_timers[kfx_game_state.script_timer_id].count;
    const long nturns = (limit > 0) ? (long)limit - base : base;
    if (nturns < 0)
    {
        // Same self-hide the legacy draw_script_timer() does when a
        // SET_TIMER runs out.
        kfx_game_state.flags_gui &= ~GGUI_ScriptTimer;
        return false;
    }
    if (kfx_game_state.timer_real)
        real_time_clock(buf, n, (int)nturns);
    else
        std::snprintf(buf, n, "%08ld", nturns);
    return true;
}

bool script_variable_line(char *buf, size_t n)
{
    if (!display_variable_enabled())
        return false;
    if (kfx_game_state.active_script_var_count == 0)
        return false;
    const struct ScriptVariable *scvar = &kfx_game_state.script_variables[0];
    long value = get_condition_value(scvar->variable_player,
                                     scvar->value_type,
                                     scvar->value_id);
    const long target = scvar->variable_target;
    const unsigned char tt = scvar->variable_target_type;
    if (target != 0)
    {
        if (tt == 0 || tt == 2) value = target - value;
        else if (tt == 1)       value = ((~target) + 1) + value;
    }
    if (tt != 2 && value < 0)
        value = 0;
    std::snprintf(buf, n, "%ld", value);
    return true;
}

bool game_timer_line(char *buf, size_t n)
{
    if (!timer_enabled())
        return false;
    if (kfx_sim_state.TimerGame)
    {
        if (get_my_player()->victory_state != VicS_WonLevel)
            TimerTurns = get_gameturn();
        std::snprintf(buf, n, "%08lu", TimerTurns);
    }
    else
    {
        if (!kfx_sim_state.TimerFreeze)
            update_time();
        std::snprintf(buf, n, "%02d:%02d:%02d",
                      kfx_sim_state.Timer.Hours, kfx_sim_state.Timer.Minutes, kfx_sim_state.Timer.Seconds);
    }
    return true;
}

void draw_script_readouts(void)
{
    char bonus[32], var[32], timer[32];
    const bool has_bonus = bonus_timer_line(bonus, sizeof(bonus)) || script_timer_line(bonus, sizeof(bonus));
    const bool has_var   = script_variable_line(var, sizeof(var));
    const bool has_timer = game_timer_line(timer, sizeof(timer));
    if (!has_bonus && !has_var && !has_timer)
        return;

    if (begin_corner_overlay("##ingame_script_readouts", ImVec2(1.0f, 0.0f)))
    {
        FeStylePushFont(FeFont_Heading);
        if (has_bonus) ImGui::TextUnformatted(bonus);
        if (has_var)   ImGui::TextUnformatted(var);
        if (has_timer) ImGui::TextUnformatted(timer);
        FeStylePopFont();
    }
    ImGui::End();
}

// --- dev-only overlays -------------------------------------------------

void draw_gameturn_overlay(void)
{
    if (!gameturn_timer_enabled())
        return;
    if (begin_corner_overlay("##ingame_gameturn", ImVec2(1.0f, 1.0f)))
        ImGui::Text("GameTurn %lu", (unsigned long)get_gameturn());
    ImGui::End();
}

void draw_frametime_overlay(void)
{
    if (!frametime_enabled())
        return;
    const bool detail = debug_display_frametime == 2;
    if (begin_corner_overlay("##ingame_frametime", ImVec2(1.0f, 0.5f)))
    {
        const struct FrametimeMeasurements &m = frametime_measurements;
        static const char *ft_names[TOTAL_FRAMETIME_KINDS] = { "Frame", "Logic", "Draw", "Sleep" };
        static const char *fr_names[TOTAL_FRAMERATE_KINDS] = { "Frame FPS", "Logic FPS", "Draw FPS" };
        if (ImGui::BeginTable("ft", detail ? 4 : 2, ImGuiTableFlags_SizingFixedFit))
        {
            for (int i = 0; i < TOTAL_FRAMETIME_KINDS; i++)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(ft_names[i]);
                ImGui::TableNextColumn(); ImGui::Text("%.3f ms", m.frametime_display[i]);
                if (detail)
                {
                    ImGui::TableNextColumn(); ImGui::Text("%.3f", m.frametime_get_min[i]);
                    ImGui::TableNextColumn(); ImGui::Text("%.3f", m.frametime_get_max[i]);
                }
            }
            for (int i = 0; i < TOTAL_FRAMERATE_KINDS; i++)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(fr_names[i]);
                ImGui::TableNextColumn(); ImGui::Text("%d", m.framerate_display[i]);
                if (detail)
                {
                    ImGui::TableNextColumn(); ImGui::Text("%d", m.framerate_min[i]);
                    ImGui::TableNextColumn(); ImGui::Text("%d", m.framerate_max[i]);
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void draw_network_stats_overlay(void)
{
    if (debug_display_network_stats == 0)
        return;
    if (begin_corner_overlay("##ingame_netstats", ImVec2(0.0f, 0.0f)))
    {
        const unsigned long ping = GetPing(my_player_number, my_player_number);
        const unsigned int in_kb10 = (GetDownloadRateBytesPerSecond() * 10) / 1024;
        const unsigned int out_kb10 = (GetUploadRateBytesPerSecond() * 10) / 1024;
        int32_t inc_wait, inc_turn, dec_wait, dec_sample;
        input_lag_get_stats(&inc_wait, &inc_turn, &dec_wait, &dec_sample);
        int64_t turn_ns = 0;
        if (kfx_sim_state.turns_per_second > 0)
        {
            turn_ns = 1000000000 / kfx_sim_state.turns_per_second + multiplayer_speed_adjustment_ns;
            if (turn_ns < 0) turn_ns = 0;
        }
        ImGui::Text("Full ping: %lums", ping);
        ImGui::Text("Half ping: %lums", ping / 2);
        ImGui::Text("Input lag: %d", kfx_net_state.input_lag_turns);
        ImGui::Text("Packet wait increase: %d/%dms in %dms", inc_wait, inc_turn, INPUT_LAG_INCREASE_SAMPLE_MS);
        ImGui::Text("Packet wait decrease: %d/%dms", dec_wait, dec_sample);
        ImGui::Text("Download: %u.%u KB/s", in_kb10 / 10, in_kb10 % 10);
        ImGui::Text("Upload: %u.%u KB/s", out_kb10 / 10, out_kb10 % 10);
        ImGui::Text("Congestion: %u bytes", GetClientDataInTransit());
        ImGui::Text("Loss rate: %u%%", GetPacketLoss(my_player_number, my_player_number));
        ImGui::Text("Lost packets: %u", GetClientPacketsLost());
        ImGui::Text("Stutter: %dms (avg %dms, max %dms)",
                    stutter_detection_current, stutter_detection_average, stutter_detection_max);
        ImGui::Text("Turn length: %" PRId64 "ns", turn_ns);
        ImGui::Text("Gameturn: %lu", (unsigned long)get_gameturn());
    }
    ImGui::End();
}

void draw_consolelog_overlay(void)
{
    if (!consolelog_enabled())
        return;
    const ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    if (ImGui::Begin("##ingame_consolelog", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav
                     | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing))
    {
        FeStylePushFont(FeFont_Body);
        const size_t start = (consoleLogArraySize > 21) ? consoleLogArraySize - 21 : 0;
        for (size_t i = start; i < consoleLogArraySize; i++)
            ImGui::TextUnformatted(consoleLogArray[i]);
        FeStylePopFont();
    }
    ImGui::End();
}

} // namespace

extern "C" void ingame_debug_overlays_frame(void)
{
    if (ingame_gui_use_classic_hud())
        return;
    draw_script_readouts();
    draw_gameturn_overlay();
    draw_frametime_overlay();
    draw_network_stats_overlay();
    draw_consolelog_overlay();
}
