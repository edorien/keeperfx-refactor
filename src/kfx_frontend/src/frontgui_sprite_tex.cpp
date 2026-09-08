#include "pre_inc.h"
#include "frontgui_sprite_tex.h"

#include "frontgui_widgets.h"
#include "frontgui_style.h"

#include "bflib_guibtns.h"                 // do_sound_menu_click
#include "bflib_sprite.h"                  // struct TbSprite
#include "bflib_vidraw.h"                  // LbSpriteDrawImmediate
#include "bflib_video.h"                   // TbPixel, LbScreen*GraphicsWindow, TbGraphicsWindow
#include "renderer/RendererManager.h"      // RendererSwapFramebufferTarget, dynamic textures, draw-flag state
#include "custom_sprites.h"                // get_button_sprite, get_panel_sprite
#include "kfx_sim_state.h"                 // engine_palette (game palette for the sprite decode)

#include <imgui_internal.h>                // GImGui->NavCursorVisible -- same use as frontgui_widgets.cpp
#include "post_inc.h"

#include <map>
#include <vector>

namespace {

struct CachedSprite {
    void *texture = nullptr;   // RendererCreateDynamicTexture handle, or nullptr while unbuilt
    int   width   = 0;
    int   height  = 0;
    bool  built   = false;     // a successful render happened -- stop retrying
};

std::map<short, CachedSprite> s_button_cache;
std::map<short, CachedSprite> s_panel_cache;

// Renders one classic button sprite into an RGBA buffer and uploads it to
// a fresh dynamic texture. Same sequence as build_cursor_pixels()
// (frontgui_style.cpp): save the ambient draw-flag/colour state (held off
// lbDisplay, left however the last engine draw set it), force a plain
// draw, redirect the framebuffer target at a local buffer, blit, restore.
// Palette is deliberately NOT overridden here (unlike the cursor, which
// runs in menu context and must force frontend_palette): these sprites are
// only ever shown while gameplay is running, decoded against the same
// active engine palette the classic gui_area_no_anim_button() draw uses.
bool render_sprite(const struct TbSprite *spr, CachedSprite &out, bool force_engine_palette)
{
    const int w = spr->SWidth;
    const int h = spr->SHeight;
    if (w <= 0 || h <= 0)
        return false;

    std::vector<TbPixel> pixels((size_t)w * (size_t)h, TbPixel{0, 0, 0, 0});

    const unsigned short prev_flags = RendererGetDrawFlags();
    const unsigned char prev_colour = RendererGetDrawColour();
    RendererSetDrawFlags(0);

    // LbSpriteDrawImmediate decodes the sprite's paletted bytes through
    // RendererGetActivePalette() (LbDrawBufferSolid). At ImGui present
    // time that ambient palette isn't guaranteed to be the one a given
    // sheet was authored against -- found live, "the zoom/close icons are
    // all white": the GUI *panel* sprites came out as bright silhouettes
    // (the button sprites happen to decode fine ambiently). For the panel
    // path, force the game engine palette (same technique as the ImGui
    // cursor forcing frontend_palette, frontgui_style.cpp) and restore
    // whatever was active. engine_palette is null until a level's palette
    // loads -- bail and retry rather than cache a wrong decode.
    unsigned char prev_palette[PALETTE_SIZE];
    bool palette_forced = false;
    if (force_engine_palette)
    {
        if (engine_palette == nullptr)
            return false;
        RendererPaletteGet(prev_palette);
        RendererPaletteSet(engine_palette);
        palette_forced = true;
    }

    TbGraphicsWindow grwnd;
    LbScreenStoreGraphicsWindow(&grwnd);
    TbPixel *previous = RendererSwapFramebufferTarget(pixels.data(), w, h);
    LbScreenSetGraphicsWindow(0, 0, w, h);
    LbSpriteDrawImmediate(0, 0, spr);
    RendererRestoreFramebufferTarget(previous);
    RendererSetDrawFlags(prev_flags);
    RendererSetDrawColour(prev_colour);
    if (palette_forced)
        RendererPaletteSet(prev_palette);
    LbScreenLoadGraphicsWindow(&grwnd);

    void *tex = RendererCreateDynamicTexture(w, h);
    if (tex == nullptr)
        return false;
    RendererUpdateDynamicTexture(tex, pixels.data(), w, h);

    out.texture = tex;
    out.width   = w;
    out.height  = h;
    out.built   = true;
    return true;
}

// Hover/press/nav-focus highlight, gated on NavCursorVisible so the
// auto-focused first item is not lit at rest -- identical rule to
// frontgui_widgets.cpp's fe_button_highlighted().
bool item_highlighted()
{
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        return true;
    return ImGui::IsItemFocused() && GImGui->NavCursorVisible;
}

// Shared geometry for FeSpriteButton() / FeSpriteButtonWidth(). Assumes the
// body font is already pushed. `spr_w/spr_h` are the sprite's pixel size.
struct SpriteBtnGeom {
    float icon_w, icon_h;
    float gap;          // icon->label spacing (0 when no label)
    ImVec2 text_sz;
    ImVec2 box;
};

SpriteBtnGeom sprite_button_geom(int spr_w, int spr_h, const char *label, float icon_h_req)
{
    const ImGuiStyle &style = ImGui::GetStyle();
    SpriteBtnGeom g;
    g.icon_h = icon_h_req > 0.0f ? icon_h_req : ImGui::GetFontSize();
    g.icon_w = g.icon_h * (float)spr_w / (float)spr_h;

    const bool have_label = label != nullptr && label[0] != '\0';
    g.text_sz = have_label ? ImGui::CalcTextSize(label) : ImVec2(0, 0);
    g.gap = have_label ? style.ItemInnerSpacing.x * 2.0f : 0.0f;

    g.box = ImVec2(
        g.icon_w + g.gap + g.text_sz.x + style.FramePadding.x * 2.0f,
        (g.icon_h > g.text_sz.y ? g.icon_h : g.text_sz.y) + style.FramePadding.y * 2.0f);
    return g;
}

void *lookup(std::map<short, CachedSprite> &cache, short idx,
            const struct TbSprite *(*resolve)(short), bool force_engine_palette,
            int *out_w, int *out_h)
{
    CachedSprite &c = cache[idx];
    if (!c.built)
    {
        const struct TbSprite *spr = resolve(idx);
        if (spr != nullptr && spr->SWidth > 0 && spr->SHeight > 0)
        {
            c.width  = spr->SWidth;
            c.height = spr->SHeight;
            c.built  = render_sprite(spr, c, force_engine_palette); // false -> retried next call
        }
    }
    if (out_w != nullptr) *out_w = c.width;
    if (out_h != nullptr) *out_h = c.height;
    return c.texture;
}

} // namespace

void *FeSpriteTexture(short sprite_idx, int *out_w, int *out_h)
{
    return lookup(s_button_cache, sprite_idx, &get_button_sprite, false, out_w, out_h);
}

void *FeGuiPanelTexture(short sprite_idx, int *out_w, int *out_h)
{
    // Panel sprites need the game engine palette forced (see render_sprite).
    return lookup(s_panel_cache, sprite_idx, &get_panel_sprite, true, out_w, out_h);
}

namespace {

// Shared body for FeSpriteButton() / FeGuiPanelButton(): the texture is
// already resolved. `label` (if any) draws beside the icon; `fallback` is
// the caption shown as a plain text button until the texture is ready
// (defaults to `label`, then `str_id`).
bool sprite_button_body(const char *str_id, void *tex, int spr_w, int spr_h,
                        const char *label, const char *fallback, float icon_h)
{
    if (tex == nullptr || spr_w <= 0 || spr_h <= 0)
    {
        const char *cap = (fallback != nullptr && fallback[0] != '\0') ? fallback
                        : (label != nullptr && label[0] != '\0') ? label : str_id;
        return FeButton(cap);
    }

    const ImGuiStyle &style = ImGui::GetStyle();

    FeStylePushFont(FeFont_Body);
    const SpriteBtnGeom g = sprite_button_geom(spr_w, spr_h, label, icon_h);
    const bool have_label = g.gap > 0.0f || (label != nullptr && label[0] != '\0');

    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(str_id, g.box, ImGuiButtonFlags_EnableNav);
    const bool hot = item_highlighted();

    ImDrawList *dl = ImGui::GetWindowDrawList();

    if (hot)
    {
        // Soft warm wash on hover -- no hard border (the blood-red ring
        // read as harsh around the message-box / quit-modal icons).
        dl->AddRectFilled(p0, ImVec2(p0.x + g.box.x, p0.y + g.box.y),
                          IM_COL32(255, 235, 190, 40), 2.0f);
    }

    // Global-alpha aware: BeginDisabled() lowers style.Alpha, so the icon
    // dims with the rest of a disabled row.
    const ImU32 tint = ImGui::GetColorU32(ImVec4(1, 1, 1, 1));
    const ImVec2 icon_p0(p0.x + style.FramePadding.x, p0.y + (g.box.y - g.icon_h) * 0.5f);
    dl->AddImage((ImTextureID)(intptr_t)tex, icon_p0,
                 ImVec2(icon_p0.x + g.icon_w, icon_p0.y + g.icon_h),
                 ImVec2(0, 0), ImVec2(1, 1), tint);

    if (have_label)
    {
        const ImU32 col = hot ? ImGui::GetColorU32(ImVec4(1.0f, 0.92f, 0.72f, 1.0f))
                              : ImGui::GetColorU32(ImGuiCol_Text);
        dl->AddText(ImVec2(icon_p0.x + g.icon_w + g.gap, p0.y + (g.box.y - g.text_sz.y) * 0.5f),
                    col, label);
    }
    FeStylePopFont();

    if (pressed)
        do_sound_menu_click();
    return pressed;
}

} // namespace

bool FeSpriteButton(const char *str_id, short sprite_idx, const char *label, float icon_h)
{
    int w = 0, h = 0;
    void *tex = FeSpriteTexture(sprite_idx, &w, &h);
    return sprite_button_body(str_id, tex, w, h, label, nullptr, icon_h);
}

bool FeGuiPanelButton(const char *str_id, short sprite_idx, const char *label, float icon_h)
{
    int w = 0, h = 0;
    void *tex = FeGuiPanelTexture(sprite_idx, &w, &h);
    return sprite_button_body(str_id, tex, w, h, label, nullptr, icon_h);
}

bool FeGuiPanelIconButton(const char *str_id, short sprite_idx, const char *fallback_label, float icon_h)
{
    int w = 0, h = 0;
    void *tex = FeGuiPanelTexture(sprite_idx, &w, &h);
    return sprite_button_body(str_id, tex, w, h, nullptr, fallback_label, icon_h);
}

float FeSpriteButtonWidth(short sprite_idx, const char *label, float icon_h)
{
    int spr_w = 0, spr_h = 0;
    FeSpriteTexture(sprite_idx, &spr_w, &spr_h);
    if (spr_w <= 0 || spr_h <= 0)
        return 0.0f;
    FeStylePushFont(FeFont_Body);
    const float w = sprite_button_geom(spr_w, spr_h, label, icon_h).box.x;
    FeStylePopFont();
    return w;
}
