#include "pre_inc.h"
#include "frontgui_ingame_panel.h"

#include "frontgui_widgets.h"
#include "frontgui_style.h"
#include "frontgui_sprite_tex.h"      // FeGuiPanelTexture
#include "config_spritecolors.h"     // get_player_colored_icon_idx (event marker room/creature icons)
#include "frontgui_ingame_tabcontent.h" // ingame_tabcontent_draw
#include "frontgui_ingame_relief.h"    // relief::plateau / well / ring / ...
#include "renderer/RendererManager.h" // RendererImGuiEnabled

#include "globals.h"
#include "bflib_guibtns.h"            // struct GuiButton, LbBtnF_*
#include "sprites.h"                  // GPS_rpanel_*
#include "frontend.h"                 // active_buttons, BID_*, get_active_menu
#include "gui_frontmenu.h"            // menu_id_to_number, get_gui_button
#include "gui_frontbtns.h"            // get_gui_button, gui_set_menu_mode
#include "frontmenu_ingame_tabs.h"    // gui_zoom_in/out, gui_go_to_map, gui_turn_on_autopilot
#include "frontmenu_ingame_map.h"     // MapDiagonalLength, panel_map_draw_slabs / _overlay_things
#include "frontmenu_ingame_evnt.h"    // gui_open_event, gui_kill_event, get_my_event_button_index
#include "player_data.h"              // get_my_player, my_player_number, minimap_*
#include "dungeon_data.h"             // get_players_dungeon, total_money_owned
#include "map_events.h"               // kfx_sim_state.event[], event_button_info, EvF_BtnFalling
#include "kfx_sim_state.h"
#include "kfx_config_state.h"         // kfx_config_state.gui_blink_rate
#include "kfx_frontend_state.h"       // flash_button_index (tab spangle)
#include "bflib_video.h"              // scale_value_for_resolution_with_upp, TbPixel
#include "bflib_math.h"               // LbSinL / LbCosL / LbFPMath_TrigmBits
#include "engine_render.h"            // interpolate_synced
#include "custom_sprites.h"           // is_custom_icon, get_custom_icon_frame_count
#include "config_strings.h"           // GUIStr_MapN/S/E/W
#include "local_camera.h"             // get_local_camera

#include "post_inc.h"

#include <cfloat>  // FLT_MAX
#include <cmath>   // std::sin
#include <cstdio>
#include <cstring> // std::memset
#include <vector>
#include <imgui.h>

namespace {

// ---- deferred click dispatch -------------------------------------------
// The GMnu_MAIN handlers (gui_set_menu_mode, gui_open_event, ...) do
// turn_on/off_menu -- never from inside an ImGui window. Capture the small
// values they read off their GuiButton and replay through a stack button
// at the top of the next frame.
struct PendingBtn {
    void (*fn)(struct GuiButton *) = nullptr;
    unsigned short btype_value = 0;
    long content_lval = 0;
};
PendingBtn s_pending;

void request_btn(void (*fn)(struct GuiButton *), unsigned short btype_value, long content_lval)
{
    s_pending.fn = fn;
    s_pending.btype_value = btype_value;
    s_pending.content_lval = content_lval;
}

void apply_pending(void)
{
    if (s_pending.fn == nullptr)
        return;
    struct GuiButton gb;
    std::memset(&gb, 0, sizeof(gb));
    gb.btype_value = s_pending.btype_value;
    gb.content.lval = s_pending.content_lval;
    void (*fn)(struct GuiButton *) = s_pending.fn;
    s_pending.fn = nullptr;
    fn(&gb);
}

// ---- helpers ----------------------------------------------------------

const struct GuiButton *find_btn(int id)
{
    const struct GuiButton *b = get_gui_button(id);
    if (b == nullptr || (b->flags & LbBtnF_Active) == 0)
        return nullptr;
    return b;
}

// Screen rect of a legacy GMnu_MAIN button (create_menu already scaled its
// scr_pos_*). We only draw + hit-test in ImGui; the legacy sprite draw and
// input sweep are skipped for GMnu_MAIN (menu_is_migrated).
bool btn_rect(int id, ImVec2 *p0, ImVec2 *p1, bool *enabled)
{
    const struct GuiButton *b = find_btn(id);
    if (b == nullptr)
        return false;
    *p0 = ImVec2((float)b->scr_pos_x, (float)b->scr_pos_y);
    *p1 = ImVec2((float)(b->scr_pos_x + b->width), (float)(b->scr_pos_y + b->height));
    if (enabled != nullptr)
        *enabled = (b->flags & LbBtnF_Enabled) != 0;
    return true;
}

// A nav control (M / + / - / cpu) seated in a recessed triangular pocket
// at a corner of the minimap frame -- the right-angle vertex at
// (cx, cy), the legs running `dx`/`dy` (each +/-1) toward the panel
// interior, the hypotenuse parallel to the minimap's octagon facet. The
// caption sits by the vertex; a marble gap is left between the pocket and
// the panel edge by the caller. `sel` keeps it lit (autopilot toggle).
// Returns 1 on left-click.
int corner_nav(const char *id, const char *label, float cx, float cy, float dx, float dy,
               float leg, bool enabled, bool sel)
{
    if (leg < 22.0f) leg = 22.0f;
    const ImVec2 v0(cx, cy);
    const ImVec2 v1(cx, cy + dy * leg);
    const ImVec2 v2(cx + dx * leg, cy);

    ImDrawList *dl = ImGui::GetWindowDrawList();
    relief::well_tri(dl, v0, v1, v2);
    if (sel)
        dl->AddTriangle(v0, v1, v2, IM_COL32(210, 160, 70, 190), 1.5f);

    FeStylePushFont(FeFont_Subheading);
    const ImVec2 ls = ImGui::CalcTextSize(label);
    const float bw = ls.x + 8.0f, bh = ls.y + 6.0f;
    const float tx = (dx > 0.0f) ? cx + 2.0f : cx - 2.0f - bw;
    const float ty = (dy > 0.0f) ? cy + 2.0f : cy - 2.0f - bh;

    ImGui::SetCursorScreenPos(ImVec2(tx, ty));
    ImGui::PushID(id);
    const bool pressed = ImGui::InvisibleButton("b", ImVec2(bw, bh), ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    const ImU32 col = !enabled ? IM_COL32(180, 170, 150, 90)
                    : hovered  ? ImGui::GetColorU32(ImVec4(0.86f, 0.24f, 0.16f, 1.0f))
                    : sel      ? ImGui::GetColorU32(ImVec4(1.0f, 0.86f, 0.45f, 1.0f))
                               : ImGui::GetColorU32(ImVec4(0.95f, 0.88f, 0.7f, 1.0f));
    dl->AddText(ImVec2(tx + 4.0f, ty + 3.0f), col, label);
    FeStylePopFont();

    return (enabled && pressed) ? 1 : 0;
}

// An icon button occupying an explicit screen rect (a legacy button's
// slot). Panel-sprite icon centred in the rect; blood-red hover ring;
// One sidebar tab -- a rounded key on the shared recessed channel. The
// active tab takes the panel-face colour and drops downward into the
// content; inactive tabs sit halfway between face and recess, flat, with
// no side bevel (so no double lines between neighbours). Icon is a
// query-panel sprite (optionally keeper-coloured) or a vector glyph.
// Returns 1 on left-click.
int tab_button(const char *id, short spr, bool colorize, const char *glyph, ImU32 glyph_col,
               const ImVec2 &r0, const ImVec2 &r1, bool selected)
{
    const ImVec2 p0(r0.x + 1.5f, r0.y + 1.0f); // inset -> a gap between neighbours
    const ImVec2 p1(r1.x - 1.5f, r1.y);
    const ImVec2 sz(p1.x - p0.x, p1.y - p0.y);

    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID(id);
    const bool pressed = ImGui::InvisibleButton("b", sz, ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList *dl = ImGui::GetWindowDrawList();

    const float rnd = 15.0f; // upper corners only
    if (selected)
    {
        const ImVec2 e1(p1.x, p1.y + 8.0f); // drop through the channel into the content
        dl->AddRectFilled(p0, e1, relief::tab_fill(true), rnd, ImDrawFlags_RoundCornersTop);
        relief::bevel(dl, p0, e1, 2.0f, true, rnd, /*omit_bottom*/ true);
    }
    else
    {
        const ImU32 fill = hovered
            ? relief::mix(relief::tab_fill(false), relief::tab_fill(true), 0.5f)
            : relief::tab_fill(false);
        dl->AddRectFilled(p0, p1, fill, rnd, ImDrawFlags_RoundCornersTop);
        dl->AddLine(ImVec2(p0.x + 3.0f, p0.y + 1.0f), ImVec2(p1.x - 3.0f, p0.y + 1.0f),
                    ImGui::GetColorU32(relief::tones().hi, hovered ? 0.35f : 0.18f));
    }

    const ImVec2 ic(p0.x + sz.x * 0.5f, p0.y + sz.y * 0.5f);
    if (glyph != nullptr)
    {
        FeStylePushFont(FeFont_Heading);
        ImFont *f = ImGui::GetFont();
        const float fs = sz.y * 0.60f;
        const ImVec2 ts = f->CalcTextSizeA(fs, FLT_MAX, 0.0f, glyph);
        dl->AddText(f, fs, ImVec2(ic.x - ts.x * 0.5f, ic.y - ts.y * 0.5f), glyph_col, glyph);
        FeStylePopFont();
    }
    else
    {
        const short s = colorize ? get_player_colored_icon_idx(spr, my_player_number) : spr;
        int sw = 0, sh = 0;
        void *tex = FeGuiPanelTexture(s, &sw, &sh);
        if (tex != nullptr && sw > 0 && sh > 0)
        {
            float iw = sz.x * 0.76f, ih = sz.y * 0.76f;
            const float ar = (float)sw / (float)sh;
            if (iw / ih > ar) iw = ih * ar; else ih = iw / ar;
            dl->AddImage((ImTextureID)(intptr_t)tex, ImVec2(ic.x - iw * 0.5f, ic.y - ih * 0.5f),
                         ImVec2(ic.x + iw * 0.5f, ic.y + ih * 0.5f));
        }
    }
    ImGui::PopID();
    return pressed ? 1 : 0;
}

// ---- the panel ------------------------------------------------------

struct { long x, y, w, h; } s_menu_rect = {0, 0, 0, 0};

bool read_menu_rect(void)
{
    const MenuNumber n = menu_id_to_number(GMnu_MAIN);
    if (n < 0)
        return false;
    const struct GuiMenu *m = get_active_menu(n);
    if (m == nullptr || m->visual_state == 0)
        return false;
    s_menu_rect.x = m->pos_x;
    s_menu_rect.y = m->pos_y;
    s_menu_rect.w = m->width;
    s_menu_rect.h = m->height;
    return true;
}

void draw_background(void)
{
    // Procedural chrome over the whole panel column. Leave a transparent
    // gap over the minimap area (top-left of the panel, legacy raster) and
    // the tab-content area (lower part, legacy grids) so those show
    // through. Approximate the two holes from the legacy virtual grid
    // (minimap at ~(11,11) size ~104; tab content from y~230).
    const float x0 = (float)s_menu_rect.x;
    const float y0 = (float)s_menu_rect.y;
    const float x1 = x0 + (float)s_menu_rect.w;
    const float sy = (float)s_menu_rect.h / 400.0f;

    const float tab_y0 = y0 + 196.0f * sy; // above the tab-content grids

    ImDrawList *dl = ImGui::GetWindowDrawList();

    // The panel head (minimap + gold + tab strip) as one raised, mottled
    // stone face -- continuous with the tab-content face below; the tab
    // strip's own recessed channel is the divider there, so no groove.
    // A raised outer rim frames the whole column; one engraved groove above
    // the tab strip. The minimap texture (transparent outside its diamond)
    // composites on top of this.
    relief::face(dl, ImVec2(x0, y0), ImVec2(x1, tab_y0));
    relief::groove_h(dl, x0 + 4.0f, x1 - 4.0f, y0 + 150.0f * sy);
    relief::edge_frame(dl, ImVec2(x0, y0), ImVec2(x1, y0 + (float)s_menu_rect.h));
}

// ---- minimap (off-screen raster -> dynamic texture) ------------------
// Reuses the legacy panel_map_draw_slabs / _overlay_things verbatim, with
// the framebuffer redirected at a local buffer. Rendered at the same
// PanelMapX/Y the legacy path used (derived from player->minimap_pos_*),
// so front_input.c's mouse_is_over_panel_map() hit-test is unchanged and
// the ImGui::Image lands exactly where the legacy raster would have.
void *s_mm_tex = nullptr;
int s_mm_tex_dim = 0;
std::vector<TbPixel> s_mm_pixels;
long s_mm_diag = 0, s_mm_px = 0, s_mm_py = 0;

void render_minimap(void)
{
    const long mm_upp = (s_menu_rect.w * 16 + 140 / 2) / 140;
    if (mm_upp < 1)
        return;

    // Force one rebuild of the minimap's background-blend tables so
    // setup_background() re-samples our (transparent -> black) off-screen
    // buffer rather than whatever a prior session left cached -- otherwise
    // the fog-of-war / wall / gold classes stay blended toward a stale
    // background colour (was reddish, from the old near-black-red clear).
    static bool s_bg_reset = false;
    if (!s_bg_reset)
    {
        reset_panel_map_background_cache();
        s_bg_reset = true;
    }
    long mmzoom;
    if (16 / mm_upp < 3)
        mmzoom = local_info.minimap_zoom / scale_value_for_resolution_with_upp(2, mm_upp);
    else
        mmzoom = local_info.minimap_zoom;

    s_mm_px = scale_value_for_resolution_with_upp(local_info.minimap_pos_x, mm_upp);
    s_mm_py = scale_value_for_resolution_with_upp(local_info.minimap_pos_y, mm_upp);

    // Buffer generously covers [0 .. px + diamond]. Fixed slack (512) so a
    // stale MapDiagonalLength never makes it too small (which showed as
    // half-drawn rows). Capped at 1024.
    int dim = (int)(s_mm_px + 512);
    if (dim > 1024) dim = 1024;
    if (dim < 128) dim = 128;
    if (dim != s_mm_tex_dim || s_mm_tex == nullptr)
    {
        if (s_mm_tex != nullptr) RendererDestroyDynamicTexture(s_mm_tex);
        s_mm_tex = RendererCreateDynamicTexture(dim, dim);
        s_mm_tex_dim = dim;
    }
    if (s_mm_tex == nullptr)
        return;
    // Transparent clear -- outside the minimap diamond the panel's own
    // solid fill (draw_background) shows through, so the diamond's corners
    // read as panel rather than a black square. panel_map_draw_slabs draws
    // every in-diamond slab opaque (revealed or shadowed), so only the
    // out-of-diamond corners stay transparent.
    s_mm_pixels.assign((size_t)dim * (size_t)dim, TbPixel{0, 0, 0, 0});

    // panel_map_draw_slabs / setup_panel_colors resolve every slab colour
    // through RendererGetActivePalette() -- at ImGui present time that is
    // NOT the game's engine palette (same gotcha as the panel sprites), so
    // rock/lava/water/fog all decoded to the wrong (reddish) hues. Force
    // engine_palette for the capture, exactly like FeGuiPanelTexture.
    unsigned char prev_pal[PALETTE_SIZE];
    const bool forced_pal = (engine_palette != nullptr);
    if (forced_pal)
    {
        RendererPaletteGet(prev_pal);
        RendererPaletteSet(engine_palette);
    }
    TbPixel *prev = RendererSwapFramebufferTarget(s_mm_pixels.data(), dim, dim);
    panel_map_draw_slabs(local_info.minimap_pos_x, local_info.minimap_pos_y, mm_upp, mmzoom);
    panel_map_draw_overlay_things(mm_upp, mmzoom, local_info.minimap_zoom);
    RendererRestoreFramebufferTarget(prev);
    if (forced_pal)
        RendererPaletteSet(prev_pal);
    s_mm_diag = MapDiagonalLength;

    RendererUpdateDynamicTexture(s_mm_tex, s_mm_pixels.data(), dim, dim);
}

void draw_minimap_and_compass(void)
{
    if (s_mm_tex == nullptr || s_mm_diag <= 0 || s_mm_tex_dim <= 0)
        return;
    const float d = (float)s_mm_diag;
    const float dim = (float)s_mm_tex_dim;
    const ImVec2 org((float)(s_menu_rect.x + s_mm_px), (float)(s_menu_rect.y + s_mm_py));
    const ImVec2 uv0((float)s_mm_px / dim, (float)s_mm_py / dim);
    const ImVec2 uv1(((float)s_mm_px + d) / dim, ((float)s_mm_py + d) / dim);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 c(org.x + d * 0.5f, org.y + d * 0.5f);

    // Octagonal groove frame around the minimap recess -- 8 engraved
    // segments (top/bottom horizontals, left/right verticals, 4 corner
    // diagonals), each facing the circle centre, like the legacy panel's
    // plate divisions.
    {
        const float R = d * 0.5f + 3.0f;    // just outside the bezel
        const float k = R * 0.41421356f;    // regular-octagon half-side
        const ImVec2 N0(c.x - k, c.y - R), N1(c.x + k, c.y - R);
        const ImVec2 S0(c.x - k, c.y + R), S1(c.x + k, c.y + R);
        const ImVec2 W0(c.x - R, c.y - k), W1(c.x - R, c.y + k);
        const ImVec2 E0(c.x + R, c.y - k), E1(c.x + R, c.y + k);
        relief::groove(dl, N0, N1);
        relief::groove(dl, S0, S1);
        relief::groove(dl, W0, W1);
        relief::groove(dl, E0, E1);
        relief::groove(dl, W0, N0);
        relief::groove(dl, N1, E0);
        relief::groove(dl, W1, S0);
        relief::groove(dl, S1, E1);
    }

    // Deep circular recess + raised bronze bezel: the diamond map sits in
    // the well and the bezel laps over its points.
    relief::well_circle(dl, c, d * 0.5f + 2.0f);
    dl->AddImage((ImTextureID)(intptr_t)s_mm_tex, org, ImVec2(org.x + d, org.y + d), uv0, uv1);
    relief::ring(dl, c, d * 0.5f + 3.0f, d * 0.5f - 7.0f); // ~10px bezel

    // Compass -- N/S/E/W letters rotated by the camera angle around the
    // minimap centre (mirrors draw_overlay_compass()).
    const struct PlayerInfo *player = get_my_player();
    const struct Camera *cam = get_local_camera(get_player_active_camera((struct PlayerInfo *)player));
    if (cam == nullptr)
        return;
    const float r = d * 0.5f - 21.0f; // pulled in from the bezel toward the centre
    const long a = cam->rotation_angle_x;
    const float s = (float)LbSinL(a) / (float)(1 << LbFPMath_TrigmBits);
    const float co = (float)LbCosL(a) / (float)(1 << LbFPMath_TrigmBits);
    struct { const char *t; float dx, dy; } dirs[] = {
        { get_string(GUIStr_MapN), -s, -co },
        { get_string(GUIStr_MapS),  s,  co },
        { get_string(GUIStr_MapE),  co, -s },
        { get_string(GUIStr_MapW), -co,  s },
    };
    FeStylePushFont(FeFont_Body);
    for (const auto &dir : dirs)
    {
        const ImVec2 tsz = ImGui::CalcTextSize(dir.t);
        dl->AddText(ImVec2(c.x + dir.dx * r - tsz.x * 0.5f, c.y + dir.dy * r - tsz.y * 0.5f),
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.9f)), dir.t);
    }
    FeStylePopFont();
}

void draw_gold(void)
{
    const struct PlayerInfo *player = get_my_player();
    const struct Dungeon *dungeon = get_players_dungeon(player);
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%lld", (long long)dungeon->total_money_owned);

    FeStylePushFont(FeFont_Heading);
    const ImVec2 tsz = ImGui::CalcTextSize(buf);
    const float cx = (float)s_menu_rect.x + (float)s_menu_rect.w * 0.5f;
    const float cy = (float)s_menu_rect.y + (float)s_menu_rect.h * (67.0f / 200.0f);
    ImGui::GetWindowDrawList()->AddText(ImVec2(cx - tsz.x * 0.5f, cy),
        ImGui::GetColorU32(ImVec4(1.0f, 0.86f, 0.4f, 1.0f)), buf);
    FeStylePopFont();
}

void draw_tabs(void)
{
    // Icons from the query panel's vocabulary (not the legacy tab sprites,
    // which bake in their own frame): a pale-green "?" for info, the
    // keeper-coloured room / creature player-symbols, and the research /
    // workshop room icons for spells / manufacture.
    struct TabSpec {
        int id; MenuID target; short spr; bool colorize; const char *glyph; ImU32 glyph_col;
    };
    const TabSpec tabs[] = {
        { BID_INFO_TAB,   GMnu_QUERY,    0, false, "?", IM_COL32(168, 224, 168, 255) },
        { BID_ROOM_TAB,   GMnu_ROOM,     (short)GPS_plyrsym_symbol_room_red_std_a,   true,  nullptr, 0 },
        { BID_SPELL_TAB,  GMnu_SPELL,    (short)GPS_room_research_std_s,             false, nullptr, 0 },
        { BID_MNFCT_TAB,  GMnu_TRAP,     (short)GPS_room_workshop_std_s,             false, nullptr, 0 },
        { BID_CREATR_TAB, GMnu_CREATURE, (short)GPS_plyrsym_symbol_player_red_std_a, true,  nullptr, 0 },
    };
    // A tab "spangles" when it has an unseen new item -- mirrors
    // draw_menu_spangle(): flash_button_index, mapped to a tab id.
    const int flash_tab = (kfx_frontend_state.flash_button_index != 0)
        ? button_designation_to_tab_designation(kfx_frontend_state.flash_button_index) : 0;

    ImVec2 flash_p0(0, 0), flash_p1(0, 0);
    bool have_flash = false;

    // Recessed channel behind the whole tab row -- the plinths sit in it.
    // Span the panel's full inner width (x0+2 .. x1-2), the same extent the
    // tab-content well below uses, so their edges line up.
    const float chx0 = (float)s_menu_rect.x + 2.0f;
    const float chx1 = (float)(s_menu_rect.x + s_menu_rect.w) - 2.0f;
    float chy0 = FLT_MAX, chy1 = -FLT_MAX;
    for (const auto &t : tabs)
    {
        ImVec2 p0, p1; bool en = true;
        if (!btn_rect(t.id, &p0, &p1, &en))
            continue;
        chy0 = fminf(chy0, p0.y);
        chy1 = fmaxf(chy1, p1.y);
    }
    if (chy1 > chy0)
    {
        // Flat recess-tone fill, no bevel -- the tabs sit right at the
        // channel's left/right edges, so a sunken bevel there just peeked
        // out past the end tabs as a stray vertical line.
        const ImU32 recess = relief::mix(relief::tones().well_top, relief::tones().well_bot, 0.5f);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(chx0, chy0 - 3.0f), ImVec2(chx1, chy1 + 3.0f), recess, 3.0f);
    }

    for (const auto &t : tabs)
    {
        ImVec2 p0, p1; bool en = true;
        if (!btn_rect(t.id, &p0, &p1, &en))
            continue;
        const bool sel = menu_is_active(t.target);
        char sid[16]; std::snprintf(sid, sizeof(sid), "tab%d", t.id);
        if (tab_button(sid, t.spr, t.colorize, t.glyph, t.glyph_col, p0, p1, sel) == 1)
            request_btn(&gui_set_menu_mode, (unsigned short)t.target, 0);

        if (t.id == flash_tab && !sel)
        {
            flash_p0 = p0; flash_p1 = p1; have_flash = true;
        }
    }

    // Spangle in a second pass so it's over every tab icon, never tucked
    // behind an adjacent one. Pulsing glow ring, corner spark dots.
    if (have_flash)
    {
        ImDrawList *dl = ImGui::GetWindowDrawList();
        const float t = (float)ImGui::GetTime();
        const float pulse = 0.45f + 0.45f * std::sin(t * 6.0f);
        dl->AddRect(flash_p0, flash_p1,
                    ImGui::GetColorU32(ImVec4(1.0f, 0.93f, 0.4f, pulse)), 2.0f, 0, 3.0f);
        const float rad = 2.0f + 1.5f * std::sin(t * 9.0f);
        const ImU32 spark = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 0.75f, 0.9f));
        dl->AddCircleFilled(flash_p0, rad, spark);
        dl->AddCircleFilled(ImVec2(flash_p1.x, flash_p0.y), rad, spark);
        dl->AddCircleFilled(ImVec2(flash_p0.x, flash_p1.y), rad, spark);
        dl->AddCircleFilled(flash_p1, rad, spark);
    }
}

void draw_nav_buttons(void)
{
    // M / + / - / cpu seated in recessed triangular pockets at the four
    // corners of the minimap frame: a 12px marble gap from the panel edge,
    // and each pocket's hypotenuse held 12px clear of the minimap's octagon
    // facet (leg length solved per corner). cpu (autopilot) stays lit.
    const float GAP = 12.0f;
    const float ccx = (float)(s_menu_rect.x + s_mm_px) + (float)s_mm_diag * 0.5f;
    const float ccy = (float)(s_menu_rect.y + s_mm_py) + (float)s_mm_diag * 0.5f;
    const float octR = (float)s_mm_diag * 0.5f + 3.0f;      // minimap octagon radius
    const float clr  = octR + 12.0f;                        // octagon radius + hyp clearance

    const float xl = (float)s_menu_rect.x + GAP;
    const float xr = (float)(s_menu_rect.x + s_menu_rect.w) - GAP;
    const float yt = (float)s_menu_rect.y + GAP;
    const float yb = ccy + octR;                            // aligned with the octagon's base
    // leg = D - sqrt(2)*clr, D = dot((corner->centre), inward dir)
    auto leg_for = [&](float px, float py, float sx, float sy) {
        return sx * (ccx - px) + sy * (ccy - py) - 1.41421356f * clr;
    };

    if (corner_nav("navM", "M", xl, yt, +1.0f, +1.0f, leg_for(xl, yt, +1, +1), true, false) == 1)
        request_btn(&gui_go_to_map, 0, 0);
    if (corner_nav("navP", "+", xr, yt, -1.0f, +1.0f, leg_for(xr, yt, -1, +1), true, false) == 1)
        request_btn(&gui_zoom_in, 0, 0);
    if (corner_nav("navN", "-", xr, yb, -1.0f, -1.0f, leg_for(xr, yb, -1, -1), true, false) == 1)
        request_btn(&gui_zoom_out, 0, 0);

    const struct GuiButton *asst = find_btn(BID_ASSIST);
    const bool asst_en = asst == nullptr || (asst->flags & LbBtnF_Enabled) != 0;
    const struct Dungeon *d = get_players_dungeon(get_my_player());
    const bool asst_on = d != nullptr && (d->computer_enabled & 0x01) != 0;
    if (corner_nav("navCPU", "cpu", xl, yb, +1.0f, -1.0f, leg_for(xl, yb, +1, -1), asst_en, asst_on) == 1)
        request_btn(&gui_turn_on_autopilot, 0, 0);
}

// Event markers -- fully ImGui-drawn (the legacy sprites didn't sit with
// the rest of the HUD): a rounded body + a coloured glyph or one of the
// room/spell/trap/creature tab icons, pulsing gold while the event is
// unread, steady gold while it's the open one.
struct EvStyle {
    const char *glyph;
    short icon_spr;
    ImU32 col;
    bool grad_top;
    bool colorize;     // remap the icon to my keeper colour (room / player symbols)
    bool button_spr;   // icon is a GBS_ (FeSpriteTexture) not a GPS_ (FeGuiPanelTexture)
};

EvStyle event_style(int kind)
{
    // Same icons the query panel uses: room / player symbols
    // (get_player_colored_icon_idx), research + workshop room icons; the
    // crossed-swords fight symbol for combat / attacks.
    switch (kind)
    {
    case EvKind_NewRoomResrch: case EvKind_RoomTakenOver:
    case EvKind_TreasureRoomFull: case EvKind_NeedTreasureRoom:
        return { nullptr, (short)GPS_plyrsym_symbol_room_red_std_a, 0, false, true, false };
    case EvKind_NewCreature: case EvKind_CreatrScavenged: case EvKind_CreaturePayday:
    case EvKind_CreatrIsAnnoyed: case EvKind_CreatrHungry: case EvKind_PrisonerStarving:
        return { nullptr, (short)GPS_plyrsym_symbol_player_red_std_a, 0, false, true, false };
    case EvKind_NewSpellResrch: case EvKind_SpellPickedUp:
        return { nullptr, (short)GPS_room_research_std_s, 0, false, false, false };
    case EvKind_NewTrap: case EvKind_NewDoor:
    case EvKind_TrapCrateFound: case EvKind_DoorCrateFound:
        return { nullptr, (short)GPS_room_workshop_std_s, 0, false, false, false };
    case EvKind_HeartAttacked: case EvKind_EnemyFight: case EvKind_FriendlyFight:
    case EvKind_Breach: case EvKind_RoomUnderAttack: case EvKind_RoomLost:
    case EvKind_AlarmTriggered:
        return { nullptr, (short)GBS_guisymbols_sym_fight, 0, false, false, true };
    case EvKind_Objective:
        return { "?", 0, IM_COL32(120, 220, 110, 255), false, false, false };
    case EvKind_Information:
        return { "i", 0, IM_COL32(120, 175, 240, 255), false, false, false };
    case EvKind_QuickInformation:
        return { "i", 0, IM_COL32(120, 220, 120, 255), false, false, false };
    default:
        return { "!", 0, IM_COL32(150, 195, 255, 255), true, false, false };
    }
}

void ev_glyph(ImDrawList *dl, const ImVec2 &p0, const ImVec2 &sz, const char *g, ImU32 col, bool grad_top)
{
    FeStylePushFont(FeFont_Heading);
    ImFont *font = ImGui::GetFont();
    const float fs = sz.y * 0.82f;
    const ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, g);
    const ImVec2 tp(p0.x + (sz.x - ts.x) * 0.5f, p0.y + (sz.y - ts.y) * 0.5f);
    dl->AddText(font, fs, tp, col, g);
    if (grad_top)   // fade toward white over the top of the glyph
    {
        dl->PushClipRect(p0, ImVec2(p0.x + sz.x, p0.y + sz.y * 0.42f), true);
        dl->AddText(font, fs, tp, IM_COL32(235, 245, 255, 255), g);
        dl->PopClipRect();
    }
    FeStylePopFont();
}

void draw_event_markers(void)
{
    // Anchor the column to slot 0's legacy rect, then stack the markers at a
    // fixed pitch. The legacy per-slot rects scale 30 virtual px up to
    // ~55-80 screen px -- far taller than the ~square markers we draw -- so
    // keying each marker to the vertical centre of its own oversized slot
    // left a big gap between them. Each slot still owns a fixed y (an empty
    // slot leaves a gap, as in the legacy layout); a run of populated slots
    // now reads as a contiguous stack.
    const struct GuiButton *b0 = find_btn(BID_MSG_EV01);
    if (b0 == nullptr)
        return;
    const float edge   = (float)(s_menu_rect.x + s_menu_rect.w);
    const float mx     = edge + 3.0f;
    const float mw     = 40.0f;
    const float mh     = 40.0f;
    const float pitch  = mh + 1.0f;                                // 1px hairline, no floaty gap
    const float base_y = (float)b0->scr_pos_y + (float)b0->height; // bottom of slot 0

    for (int slot = 0; slot < 13; slot++)
    {
        const struct GuiButton *b = find_btn(BID_MSG_EV01 + slot);
        if (b == nullptr)
            continue;
        const EventIndex evidx = get_my_event_button_index(slot);
        if (evidx == 0)
            continue;
        const struct Event *ev = &kfx_sim_state.event[evidx];

        float my = base_y - (float)(slot + 1) * pitch;
        if (flag_is_set(ev->flags, EvF_BtnFalling))
            my = interpolate_synced(my - pitch, my);
        const ImVec2 p0(mx, my);
        const ImVec2 p1(mx + mw, my + mh);
        const ImVec2 sz(mw, mh);

        char sid[16]; std::snprintf(sid, sizeof(sid), "ev%d", slot);
        ImGui::SetCursorScreenPos(p0);
        ImGui::PushID(sid);
        const bool pressed = ImGui::InvisibleButton("e", sz,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool hovered = ImGui::IsItemHovered();
        const bool rclick  = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        ImGui::PopID();

        ImDrawList *dl = ImGui::GetWindowDrawList();
        const bool sel    = (evidx == my_visible_event_idx);
        const bool unread = !(my_event_button_state[evidx] & EvBtnS_Read);
        const EvStyle st  = event_style(ev->kind);

        dl->AddRectFilled(p0, p1, IM_COL32(30, 22, 13, 240), 5.0f, ImDrawFlags_RoundCornersRight);
        relief::bevel(dl, p0, p1, 1.5f, true); // slim raised edge -- reads as a token, not a sticker

        // Content pulled ~2px toward the sidebar (the marker mostly sits
        // off-panel), sized to nearly fill the cell.
        const ImVec2 c0(p0.x + 1.0f, p0.y + 1.0f);
        const ImVec2 csz(sz.x - 4.0f, sz.y - 2.0f);
        if (st.icon_spr != 0)
        {
            const short spr = (st.colorize && !st.button_spr)
                ? get_player_colored_icon_idx(st.icon_spr, my_player_number) : st.icon_spr;
            int w = 0, h = 0;
            void *tex = st.button_spr ? FeSpriteTexture(spr, &w, &h)
                                      : FeGuiPanelTexture(spr, &w, &h);
            if (tex != nullptr && w > 0 && h > 0)
            {
                float iw = csz.x, ih = csz.y;
                const float ar = (float)w / (float)h;
                if (iw / ih > ar) iw = ih * ar; else ih = iw / ar;
                const ImVec2 i0(c0.x + (csz.x - iw) * 0.5f, c0.y + (csz.y - ih) * 0.5f);
                dl->AddImage((ImTextureID)(intptr_t)tex, i0, ImVec2(i0.x + iw, i0.y + ih));
            }
        }
        else
        {
            ev_glyph(dl, c0, csz, st.glyph, st.col, st.grad_top);
        }

        ImU32 bcol = IM_COL32(90, 70, 42, 200);
        float bth = 1.0f;
        if (sel) { bcol = IM_COL32(255, 219, 102, 255); bth = 2.0f; }
        else if (unread)
        {
            const float pulse = 0.35f + 0.45f * (float)std::sin(ImGui::GetTime() * 7.0);
            bcol = ImGui::GetColorU32(ImVec4(1.0f, 0.9f, 0.5f, pulse));
            bth = 2.0f;
        }
        if (hovered) { bcol = IM_COL32(220, 62, 40, 255); bth = 2.0f; }
        dl->AddRect(p0, p1, bcol, 5.0f, ImDrawFlags_RoundCornersRight, bth);

        if (pressed)      request_btn(&gui_open_event, 0, slot);
        else if (rclick)  request_btn(&gui_kill_event, 0, slot);
    }
}

} // namespace

void ingame_panel_frame(void)
{
    apply_pending();

    if (!RendererImGuiEnabled())
        return;
    // Tab / Ctrl+Tab (toggle_gui -> GOF_ShowGui) hides the whole sidebar.
    // set_menu_visible_off leaves GMnu_MAIN's visual_state non-zero, so
    // read_menu_rect() alone won't catch this.
    if ((kfx_sim_state.operation_flags & GOF_ShowGui) == 0)
        return;
    // The parchment / overhead map is a full-screen view -- no sidebar over it.
    const struct PlayerInfo *lp = get_my_player();
    if (lp != nullptr && lp->view_mode == PVM_ParchmentView)
        return;
    if (!read_menu_rect())
        return;
    if (s_menu_rect.w <= 0 || s_menu_rect.h <= 0)
        return;

    // Framebuffer-swap raster -- must run before any ImGui window is open.
    render_minimap();

    // Window == the panel strip (+ a right margin for event markers that
    // overhang the 140-grid edge). Sized to the panel so io.WantCaptureMouse
    // -- hence busy_doing_gui via ingame_imgui_wants_mouse() -- is only set
    // while the pointer is actually over the sidebar, not the 3D view.
    ImGui::SetNextWindowPos(ImVec2((float)s_menu_rect.x, (float)s_menu_rect.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)s_menu_rect.w + 44.0f, (float)s_menu_rect.h), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##IngameSidebar", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings
                 | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavInputs
                 | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    draw_background();
    draw_minimap_and_compass();
    draw_gold();
    draw_tabs();
    // Phase 5: the active tab's body, in this same window (no z-order seam).
    ingame_tabcontent_draw((float)s_menu_rect.x, (float)s_menu_rect.y,
                           (float)s_menu_rect.w, (float)s_menu_rect.h);
    draw_nav_buttons();
    draw_event_markers();

    ImGui::End();
}
