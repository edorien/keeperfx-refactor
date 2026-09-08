#include "pre_inc.h"
#include "frontgui_ingame_text.h"

#include "frontgui_widgets.h"
#include "frontgui_style.h"
#include "frontgui_sprite_tex.h" // FeGuiPanelTexture -- message-queue icons
#include "renderer/RendererManager.h" // RendererImGuiEnabled

#include "globals.h"
#include "gui_topmsg.h"       // onscreen_msg_text, onscreen_banner_visible
#include "gui_msgs.h"         // message_icon_spridx
#include "gui_tooltips.h"     // tool_tip_box, TTip_Visible
#include "frontend.h"         // status_panel_width
#include "player_data.h"      // get_my_player, my_player_number
#include "config_strings.h"   // GUIStr_PausedMsg
#include "kfx_sim_state.h"    // operation_flags GOF_*, system_flags GSF_*, messages[]
#include "packets.h"          // unpausing_in_progress

#include "post_inc.h"

#include <cfloat> // FLT_MAX
#include <cstdio> // std::snprintf
#include <cstring> // memcpy
#include <strings.h> // strncasecmp
#include <imgui.h>

namespace {

// The tooltip strings carry a trailing control legend ("... LMB pick up
// creature. RMB zoom.", "... LMB toggle.") baked into the language data.
// It read as a hint in the classic scrolling tooltip; in the ImGui one it
// is just clutter (user's call). Cut from the first LMB/RMB/MMB token to
// the end, trimming the sentence gap before it. The acronyms are kept
// verbatim across translations, so this is language-agnostic enough.
const char *tooltip_without_control_hint(const char *src, char *buf, size_t n)
{
    const char *cut = nullptr;
    for (const char *p = src; *p != '\0'; p++)
    {
        const bool at_word_start = (p == src) || (p[-1] == ' ');
        if (at_word_start
            && (strncasecmp(p, "LMB", 3) == 0
             || strncasecmp(p, "RMB", 3) == 0
             || strncasecmp(p, "MMB", 3) == 0))
        {
            cut = p;
            break;
        }
    }
    if (cut == nullptr)
        return src;
    // Also drop a trailing ". " / ", " that separated the legend.
    while (cut > src && (cut[-1] == ' ' || cut[-1] == '.' || cut[-1] == ','))
        cut--;
    size_t len = (size_t)(cut - src);
    if (len >= n) len = n - 1;
    std::memcpy(buf, src, len);
    buf[len] = '\0';
    return buf;
}

constexpr ImGuiWindowFlags kFlags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav
    | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
    | ImGuiWindowFlags_AlwaysAutoResize;

// Centred caption pinned near the top of the engine view -- the "Paused"
// plate (engine_redraw.c) as an ImGui window with a translucent ground.
void draw_paused_caption(void)
{
    const TbBool paused = (kfx_sim_state.operation_flags & GOF_Paused) != 0
                       && (kfx_sim_state.operation_flags & GOF_WorldInfluence) == 0
                       && !unpausing_in_progress;
    if (!paused)
        return;

    const ImGuiIO &io = ImGui::GetIO();
    // Centre within the 3D viewport (inset by the status panel on the
    // left) like the legacy draw, falling back to full-width centring.
    const float left = (float)local_info.engine_window_x;
    const float cx = left + (io.DisplaySize.x - left) * 0.5f;
    ImGui::SetNextWindowPos(ImVec2(cx, 14.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.62f);
    if (ImGui::Begin("##ingame_paused", nullptr, kFlags))
    {
        FeStylePushFont(FeFont_Heading);
        ImGui::TextUnformatted(get_string(GUIStr_PausedMsg));
        FeStylePopFont();
    }
    ImGui::End();
}

// The transient warning banner + out-of-sync lines (gui_topmsg.c).
void draw_onscreen_banner(void)
{
    const bool oos  = (kfx_sim_state.system_flags & GSF_NetGameNoSync) != 0;
    const bool seed = (kfx_sim_state.system_flags & GSF_NetSeedNoSync) != 0;
    const bool banner = onscreen_banner_visible() && onscreen_msg_text[0] != '\0';
    if (!banner && !oos && !seed)
        return;

    const ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(banner ? 0.55f : 0.0f);
    if (ImGui::Begin("##ingame_onscreen_banner", nullptr, kFlags))
    {
        FeStylePushFont(FeFont_Body);
        if (banner)
            ImGui::TextUnformatted(onscreen_msg_text);
        if (oos || seed)
        {
            const ImU32 red = ImGui::GetColorU32(ImVec4(0.90f, 0.20f, 0.15f, 1.0f));
            if (oos)  { ImGui::PushStyleColor(ImGuiCol_Text, red); ImGui::TextUnformatted("OUT OF SYNC");      ImGui::PopStyleColor(); }
            if (seed) { ImGui::PushStyleColor(ImGuiCol_Text, red); ImGui::TextUnformatted("SEED OUT OF SYNC"); ImGui::PopStyleColor(); }
        }
        FeStylePopFont();
    }
    ImGui::End();
}

// The multiplayer chat input line -- ">text_" echoed top-left while the
// player is typing (get_players_message_inputs, front_input.c). Input is
// unchanged; only this echo moved off engine_redraw.c.
void draw_mp_chat_line(void)
{
    const struct PlayerInfo *player = get_my_player();
    if (player == nullptr || (player->allocflags & PlaF_NewMPMessage) == 0)
        return;
    char text[sizeof(player->mp_message_text) + 4];
    std::snprintf(text, sizeof(text), ">%s_", player->mp_message_text);

    ImGui::SetNextWindowPos(ImVec2((float)status_panel_width + 10.0f, 6.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    if (ImGui::Begin("##ingame_mp_chat", nullptr, kFlags))
    {
        FeStylePushFont(FeFont_Body);
        ImGui::TextUnformatted(text);
        FeStylePopFont();
    }
    ImGui::End();
}

// The scrolling player-message queue (gui_msgs.c::message_draw) -- creature
// taunts, DISPLAY_MESSAGE, event text -- down the top-left, just right of
// the status panel. Same kfx_sim_state.messages[] source and target-idx
// filter; icon logic reused via message_icon_spridx().
void draw_message_queue(void)
{
    if (kfx_sim_state.active_messages_count == 0)
        return;

    const float left = (float)status_panel_width + 8.0f;
    ImGui::SetNextWindowPos(ImVec2(left, 28.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(ImGui::GetIO().DisplaySize.x * 0.55f, FLT_MAX));
    ImGui::SetNextWindowBgAlpha(0.0f); // text-on-3D, no plate (matches legacy)
    if (ImGui::Begin("##ingame_message_queue", nullptr, kFlags))
    {
        FeStylePushFont(FeFont_Body);
        const float icon_h = ImGui::GetFontSize() * 1.5f;
        for (int i = 0; i < kfx_sim_state.active_messages_count; i++)
        {
            const struct GuiMessage *m = &kfx_sim_state.messages[i];
            if (m->target_idx != my_player_number && m->target_idx != -1)
                continue;

            const short icon = message_icon_spridx(i);
            if (icon >= 0)
            {
                int sw = 0, sh = 0;
                void *tex = FeGuiPanelTexture(icon, &sw, &sh);
                if (tex != nullptr && sw > 0 && sh > 0)
                {
                    const float w = icon_h * (float)sw / (float)sh;
                    ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(w, icon_h));
                    ImGui::SameLine();
                }
            }
            // Vertically align the text to the icon row; wrap long lines
            // (get_string content may contain '%', so never Text()).
            ImGui::AlignTextToFramePadding();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetIO().DisplaySize.x * 0.42f);
            ImGui::TextUnformatted(m->text);
            ImGui::PopTextWrapPos();
        }
        FeStylePopFont();
    }
    ImGui::End();
}

// Context tooltip (gui_tooltips.c). setup_*_tooltips() still runs in the
// input path and fills tool_tip_box (text, TTip_Visible, the hover delay);
// only the final render moves here. Long tooltips wrap to a max width
// rather than the legacy vertical scroll.
void draw_tooltip_overlay(void)
{
    if ((tool_tip_box.flags & TTip_Visible) == 0 || tool_tip_box.text[0] == '\0')
        return;

    const ImVec2 m = ImGui::GetMousePos();
    const ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(m.x + 14.0f, m.y + 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    if (ImGui::Begin("##ingame_tooltip", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav
                     | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
                     | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_Tooltip))
    {
        FeStylePushFont(FeFont_Body);
        ImGui::PushTextWrapPos(io.DisplaySize.x * 0.28f);
        char trimmed[TOOLTIP_MAX_LEN];
        ImGui::TextUnformatted(tooltip_without_control_hint(tool_tip_box.text, trimmed, sizeof(trimmed)));
        ImGui::PopTextWrapPos();
        FeStylePopFont();
    }
    ImGui::End();
}

} // namespace

extern "C" void ingame_text_overlays_frame(void)
{
    if (!RendererImGuiEnabled())
        return;
    draw_onscreen_banner();
    draw_paused_caption();
    draw_mp_chat_line();
    draw_message_queue();
}

extern "C" void ingame_tooltip_frame(void)
{
    if (!RendererImGuiEnabled())
        return;
    draw_tooltip_overlay();
}
