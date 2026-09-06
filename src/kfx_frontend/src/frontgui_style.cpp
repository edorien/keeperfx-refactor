#include "pre_inc.h"
#include "frontgui_style.h"
#include "globals.h"       // FGrp_FxData
#include "config.h"        // prepare_file_path
#include "config_keeperfx.h" // keeperfx_ui_config.ui_font_scale_pct
#include "bflib_fileio.h"  // LbFileExists
#include "renderer/RendererManager.h" // ImGuiCursorImage, RendererSwapFramebufferTarget/RestoreFramebufferTarget
#include "bflib_video.h"   // TbGraphicsWindow, LbScreen{Store,Load,Set}GraphicsWindow
#include "bflib_vidraw.h"  // LbSpriteDrawImmediate
#include "bflib_sprite.h"  // struct TbSprite
#include "sprites.h"       // GFS_cursor_horny
#include "custom_sprites.h" // get_frontend_sprite
#include "vidmode.h"       // frontend_sprite (readiness check)
#include "vidfade.h"        // frontend_palette (the stable, un-faded target palette)
#include "gui_draw.h"       // frontend_background
#include <imgui.h>
#include <cstring>
#include <vector>
#include "post_inc.h"

namespace {
    ImFont* s_font_heavy = nullptr; // Exocet Heavy, or Cinzel-Black.ttf
    ImFont* s_font_light = nullptr; // Exocet Light, or Cinzel-Regular.ttf
    bool s_initialised = false;
    bool s_using_exocet = false;

    // fname resolves through FGrp_FxData (prepare_file_path), which means
    // mod/override directories work for free via the same mechanism
    // custom_sprites.c/lua_base.c already use (§4.1). Returns false (path
    // left untouched) if the resolved file doesn't exist.
    bool resolve_fxdata_font(char *out, size_t out_size, const char *fname)
    {
        char *resolved = prepare_file_path(FGrp_FxData, fname);
        if (resolved == nullptr || !LbFileExists(resolved))
            return false;
        std::strncpy(out, resolved, out_size - 1);
        out[out_size - 1] = '\0';
        return true;
    }

    void load_fonts()
    {
        ImGuiIO &io = ImGui::GetIO();
        ImFontAtlas *atlas = io.Fonts;

        char heavy_path[512];
        char light_path[512];
        // §4.1: Exocet Heavy/Light are the named files in the DK2 Windows
        // theme's install; the launcher/installer copies them into fxdata/
        // out of the user's own DK2 install when present. Both must
        // resolve for Exocet to be used at all -- a partial pair falls
        // back to Cinzel entirely rather than mixing faces.
        bool have_exocet = resolve_fxdata_font(heavy_path, sizeof(heavy_path), "EXH_____.TTF")
                         && resolve_fxdata_font(light_path, sizeof(light_path), "EXL_____.TTF");

        if (!have_exocet)
        {
            // §4.1 point 1: this is the common case (Exocet ships in a
            // DK2-era extras bundle most installs don't have), so it must
            // look deliberate. Static instances, not the variable font --
            // stb_truetype has no variable-axis support, so a named
            // instance is the correct (and simpler) choice regardless.
            resolve_fxdata_font(heavy_path, sizeof(heavy_path), "Cinzel/static/Cinzel-Black.ttf");
            resolve_fxdata_font(light_path, sizeof(light_path), "Cinzel/static/Cinzel-Regular.ttf");
        }

        // Reference size: rebuilt every PushFont call at whatever pixel
        // size the caller asks for (§4.3), this is just the atlas's
        // nominal/legacy size, mostly unused.
        const float reference_size = 24.0f;
        ImFontConfig cfg;
        cfg.SizePixels = reference_size;
        // TODO(§4.2): merge GNU Unifont / WenQuanYi Zen Hei (already used
        // by tools/fxfontmaker for the legacy bitmap fonts, but only as
        // .hex/.bdf there -- ImGui needs actual TTF/OTF releases of each)
        // as MergeMode fallback fonts here, behind the display faces, for
        // CJK/Cyrillic coverage. Deferred out of this pass; until it
        // lands, non-Latin text falls back to ImGui's built-in tofu boxes
        // rather than crashing or silently vanishing.
        const ImWchar *ranges = atlas->GetGlyphRangesDefault();

        s_font_heavy = atlas->AddFontFromFileTTF(heavy_path, reference_size, &cfg, ranges);
        s_font_light = atlas->AddFontFromFileTTF(light_path, reference_size, &cfg, ranges);
        s_using_exocet = have_exocet && s_font_heavy != nullptr && s_font_light != nullptr;

        // Fall through to ImGui's built-in default font rather than a null
        // ImFont* (PushFont(nullptr, size) means "keep current font", not
        // "no font") if a face genuinely failed to load (corrupt file,
        // read error after the existence check above).
        if (s_font_heavy == nullptr || s_font_light == nullptr)
        {
            ImFont *fallback = atlas->AddFontDefault();
            if (s_font_heavy == nullptr) s_font_heavy = fallback;
            if (s_font_light == nullptr) s_font_light = fallback;
        }
    }

    // A KeeperFX-authentic dark bronze/parchment/blood-red palette. Placeholder
    // values (§5.1: procedural treatment is an accepted stand-in through
    // Phase E) rather than sampled from front.pal -- revisit once the
    // wrapper layer's look is being reviewed against real chrome art rather
    // than proven out on its own.
    void apply_colours()
    {
        ImGuiStyle &style = ImGui::GetStyle();
        ImVec4 *c = style.Colors;

        const ImVec4 parchment      (0.90f, 0.82f, 0.66f, 1.00f);
        const ImVec4 parchment_dim  (0.68f, 0.60f, 0.46f, 1.00f);
        const ImVec4 bronze         (0.62f, 0.48f, 0.30f, 1.00f);
        const ImVec4 bronze_bright  (0.80f, 0.64f, 0.40f, 1.00f);
        // Legacy in-game message frames (draw_round_slab64k, gui_draw.c) are
        // a *translucent* mid-brown box (Lb_SPRITE_TRANSPAR4/8 over palette
        // brown) under a lighter, embossed edge -- not the near-opaque
        // near-black these were. The backdrop, and the land/3D view behind an
        // in-menu box, are meant to read through the fill.
        const ImVec4 brown_bg       (0.17f, 0.11f, 0.07f, 0.70f);
        const ImVec4 brown_panel    (0.20f, 0.13f, 0.085f, 0.80f);
        const ImVec4 brown_frame    (0.11f, 0.075f, 0.055f, 0.85f);
        // Window/panel border: a warm tan a shade lighter than the fill, held
        // slightly translucent so it reads as a raised edge rather than a
        // hard outline. fe_inset_bevel() (frontgui_widgets.cpp) adds the
        // matching two-tone highlight/shadow inside it.
        const ImVec4 edge_hi        (0.72f, 0.58f, 0.38f, 0.80f);
        const ImVec4 blood          (0.50f, 0.09f, 0.08f, 1.00f);
        const ImVec4 blood_hover    (0.68f, 0.15f, 0.11f, 1.00f);
        const ImVec4 blood_active   (0.82f, 0.22f, 0.13f, 1.00f);

        c[ImGuiCol_Text]                  = parchment;
        c[ImGuiCol_TextDisabled]          = parchment_dim;
        c[ImGuiCol_WindowBg]              = brown_bg;
        c[ImGuiCol_ChildBg]               = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_PopupBg]               = brown_panel;
        c[ImGuiCol_Border]                = edge_hi;
        c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0.55f);
        c[ImGuiCol_FrameBg]               = brown_frame;
        c[ImGuiCol_FrameBgHovered]        = ImVec4(0.20f, 0.14f, 0.10f, 0.96f);
        c[ImGuiCol_FrameBgActive]         = ImVec4(0.25f, 0.17f, 0.12f, 1.00f);
        c[ImGuiCol_TitleBg]               = brown_panel;
        c[ImGuiCol_TitleBgActive]         = brown_panel;
        c[ImGuiCol_TitleBgCollapsed]      = brown_panel;
        c[ImGuiCol_MenuBarBg]             = brown_panel;
        c[ImGuiCol_ScrollbarBg]           = ImVec4(0.06f, 0.04f, 0.03f, 0.60f);
        c[ImGuiCol_ScrollbarGrab]         = bronze;
        c[ImGuiCol_ScrollbarGrabHovered]  = bronze_bright;
        c[ImGuiCol_ScrollbarGrabActive]   = blood_hover;
        c[ImGuiCol_CheckMark]             = blood_active;
        c[ImGuiCol_SliderGrab]            = bronze_bright;
        c[ImGuiCol_SliderGrabActive]      = blood_active;
        // Menu buttons are text-only here: FeButton/FeNavButton/FeIconButton
        // (frontgui_widgets.cpp) draw just the label and redden it on
        // hover/focus -- the legacy highlighted-caption look -- with no
        // filled rectangle. These stay defined (transparent) so a stray
        // ImGui::Button() degrades to an invisible-framed text button rather
        // than the default blue.
        c[ImGuiCol_Button]                = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ButtonHovered]         = ImVec4(parchment.x, parchment.y, parchment.z, 0.06f);
        c[ImGuiCol_ButtonActive]          = ImVec4(parchment.x, parchment.y, parchment.z, 0.12f);
        // List-row selection/hover: a warm bronze wash rather than a red
        // slab, to match the de-emphasised chrome above.
        c[ImGuiCol_Header]                = ImVec4(bronze.x, bronze.y, bronze.z, 0.45f);
        c[ImGuiCol_HeaderHovered]         = ImVec4(bronze_bright.x, bronze_bright.y, bronze_bright.z, 0.55f);
        c[ImGuiCol_HeaderActive]          = ImVec4(bronze_bright.x, bronze_bright.y, bronze_bright.z, 0.75f);
        c[ImGuiCol_Separator]             = bronze;
        c[ImGuiCol_SeparatorHovered]      = bronze_bright;
        c[ImGuiCol_SeparatorActive]       = blood_active;
        c[ImGuiCol_ResizeGrip]            = ImVec4(bronze.x, bronze.y, bronze.z, 0.40f);
        c[ImGuiCol_ResizeGripHovered]     = bronze_bright;
        c[ImGuiCol_ResizeGripActive]      = blood_active;
        // Tabs follow the buttons: no red fill. The active tab reads from the
        // bronze overline (kept) plus a slightly lighter panel fill; hover is
        // a faint parchment tint.
        c[ImGuiCol_Tab]                   = brown_frame;
        c[ImGuiCol_TabHovered]            = ImVec4(parchment.x, parchment.y, parchment.z, 0.10f);
        c[ImGuiCol_TabSelected]           = brown_panel;
        c[ImGuiCol_TabSelectedOverline]   = bronze_bright;
        c[ImGuiCol_TabDimmed]             = brown_frame;
        c[ImGuiCol_TabDimmedSelected]     = brown_panel;
        c[ImGuiCol_NavCursor]             = bronze_bright;
        c[ImGuiCol_NavWindowingHighlight] = bronze_bright;
        c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0, 0, 0, 0.5f);
    }

    // Sizing derives from font metrics and window scale, never literal
    // pixels (§5.2's sizing-contract rule), but ImGuiStyle's own fields
    // are absolute pixels set once here -- FeStylePushFont's caller-side
    // size scaling is what actually keeps geometry proportional across
    // resolutions; these are just modest, resolution-independent defaults
    // for corner rounding / border thickness that look right whether the
    // frame is 640x480 or 4K (§5.1: chrome ornament is drawn "at modest
    // thickness rather than scaling it proportionally with the panel").
    void apply_metrics()
    {
        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowRounding    = 4.0f;
        style.ChildRounding     = 4.0f;
        style.FrameRounding     = 3.0f;
        style.PopupRounding     = 4.0f;
        // Classic list scrollbar: a chunky bar with a fully-rounded grab, so
        // a short grab reads as the legacy rounded (lozenge) position
        // indicator rather than a thin rectangle. ScrollbarRounding at half
        // the bar width gives pill ends; GrabMinSize keeps it lozenge-shaped
        // even when the list only just overflows.
        style.ScrollbarSize     = 18.0f;
        style.ScrollbarRounding = 9.0f;
        style.GrabMinSize       = 16.0f;
        style.GrabRounding      = 3.0f;
        style.TabRounding       = 3.0f;
        style.WindowBorderSize  = 1.5f;
        style.ChildBorderSize   = 1.0f;
        style.PopupBorderSize   = 1.5f;
        style.FrameBorderSize   = 1.0f;
        style.WindowPadding     = ImVec2(12.0f, 12.0f);
        style.FramePadding      = ImVec2(8.0f, 5.0f);
        style.ItemSpacing       = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);
    }
}

void FeStyleInit()
{
    load_fonts();
    apply_colours();
    apply_metrics();

    // §5.2: "the wrappers still set ImGui's nav flags uniformly, because
    // consistency is the point of the layer and because uniform focus
    // handling is what keyboard navigation needs anyway." Gamepad is
    // explicitly not a target (same section) -- keyboard only.
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    s_initialised = true;
}

void FeStyleEnsureInit()
{
    if (!s_initialised)
        FeStyleInit();
}

void FeStylePushFont(FeFontRole role)
{
    FeStyleEnsureInit();

    // §4.3: derive from actual window height, not a fixed reference --
    // io.DisplaySize is set by ImGui_ImplSDL3_NewFrame from the real
    // window pixel size every frame, so this re-derives on resolution
    // change for free, no rebuild needed (1.92's dynamic font system
    // rasterizes glyphs on demand at whatever size PushFont asks for).
    const float display_h = ImGui::GetIO().DisplaySize.y;
    float body_px = display_h / 32.0f;
    if (body_px < 11.0f) body_px = 11.0f;
    if (body_px > 96.0f) body_px = 96.0f;

    // UI_FONT_SCALE (keeperfx.cfg, KeeperFX-only): the resolution-derived
    // size above is a reasonable default, not a perfect fit for every
    // display/preference -- this multiplier is the user's own correction on
    // top of it, applied after the clamp so a user asking for 200% at a low
    // resolution isn't silently capped by the 96px ceiling meant for the
    // *unscaled* formula.
    body_px *= (float)keeperfx_ui_config.ui_font_scale_pct / 100.0f;

    ImFont *font = s_font_light;
    float size = body_px;
    switch (role)
    {
        case FeFont_Heading:    font = s_font_heavy; size = body_px * 1.9f;  break;
        case FeFont_Subheading: font = s_font_heavy; size = body_px * 1.3f;  break;
        case FeFont_Body:       font = s_font_light; size = body_px;         break;
        case FeFont_Caption:    font = s_font_light; size = body_px * 0.8f;  break;
        default: break;
    }
    // Found live: text visibly softened/blurred, most noticeably at
    // smaller UI_FONT_SCALE percentages. body_px (display_h/32, further
    // scaled by UI_FONT_SCALE) is essentially never an integer, and every
    // per-role multiplier above compounds that -- 1.92's dynamic font
    // system rasterizes a fresh glyph bitmap at whatever size is asked
    // for, but a fractional pixel size still means a fractional-pixel
    // baseline, which forces the renderer to blend each glyph across two
    // rows of pixels vertically instead of drawing it on a single crisp
    // one. Rounding to the nearest whole pixel here (after every
    // role-specific multiplier, not just on body_px, since 1.9x/1.3x/0.8x
    // of an already-rounded value isn't itself an integer either) is the
    // standard fix for this class of softness.
    size = (float)(int)(size + 0.5f);
    ImGui::PushFont(font, size);
}

void FeStylePopFont()
{
    ImGui::PopFont();
}

bool FeStyleUsingExocet()
{
    return s_using_exocet;
}

namespace {
    std::vector<TbPixel> s_cursor_pixels;
    int s_cursor_w = 0, s_cursor_h = 0;
    int s_cursor_hotspot_x = 0, s_cursor_hotspot_y = 0;
    bool s_cursor_build_attempted = false;

    // Renders GFS_cursor_horny into an off-screen RGBA buffer once, via the
    // same RendererSwapFramebufferTarget seam thing_creature.c's eye-lens
    // effect uses for its own off-screen render target. The RLE-encoded
    // sprite data (bflib_sprite.h) leaves untouched pixels at whatever the
    // buffer was pre-filled with -- zeroed (transparent) here -- so the
    // result is a correct RGBA cutout with no extra alpha work needed.
    //
    // Tried MousePG_Arrow (pointer_sprites, the same plain arrow shipped
    // for the in-game cursor) instead of this, twice: first loading a
    // duplicate copy of the same art straight from a PNG file (found live
    // to need its own fxdata/ copy for no reason, since the art is already
    // shipped here), then the shipped sprite itself -- which turned out to
    // have two separate, real problems of its own: pointer_sprites is only
    // ever loaded once actual gameplay starts (fixed independently,
    // frontend_load_data()), and even once loaded, its sprite data decodes
    // correctly only against the game's own *active* engine palette
    // (LbSpriteDrawUsingScalingUpDataSolidLR's RendererGetActivePalette()
    // call, bflib_mspointer.cpp -- confirmed by reading the classic
    // in-game cursor's own draw path), not frontend_palette -- found live
    // as a "tiny & black" cursor once loading was fixed, since this
    // function forces frontend_palette for the decode. Reverted to
    // GFS_cursor_horny per the user's own call: it's a genuine frontend
    // asset (frontend_sprite, loaded by frontend_load_data() -- always
    // available in menu contexts) correctly decoded against
    // frontend_palette already, with none of MousePG_Arrow's baggage --
    // simpler to get the hotspot right on this one sprite than to keep
    // fighting two unrelated problems on the other.
    bool build_cursor_pixels()
    {
        if (s_cursor_build_attempted)
            return !s_cursor_pixels.empty();

        // Found live (a real, wrong sprite got permanently cached): this
        // can run as early as the very first ImGui frame, before
        // frontend_load_data()/frontend_load_data_from_cd() has populated
        // frontend_sprite -- get_frontend_sprite() doesn't itself
        // distinguish "not loaded yet" from "loaded, sprite index empty",
        // it just indexes whatever's there. set_pointer_graphic_menu()
        // (vidmode.c) checks frontend_sprite directly for the same reason
        // before calling get_frontend_sprite(GFS_cursor_horny) for the
        // game's own real cursor -- mirror that check, and don't latch
        // "attempted" until sprites actually exist, so this keeps retrying
        // on later frames instead of caching a premature, wrong result.
        if (frontend_sprite == NULL)
            return false;

        // Found live (a real capture briefly landed before front.pal was
        // loaded and stuck forever): frontend_palette (vidfade.h) is the
        // stable, un-faded target the frontend's own on-screen rendering
        // is ultimately driven from -- it's only genuinely all-zero before
        // frontend.cpp's FeSt_INITIAL exit loads front.pal into it, which
        // happens well before Land View/Torture/NetLand View can ever be
        // reached. Treat that the same way frontend_sprite==NULL is
        // treated -- "not ready yet" -- and keep retrying instead of
        // latching a black cursor permanently.
        bool palette_all_zero = true;
        for (int i = 0; i < PALETTE_SIZE; i++)
        {
            if (frontend_palette[i] != 0) { palette_all_zero = false; break; }
        }
        if (palette_all_zero)
            return false;

        const struct TbSprite *spr = get_frontend_sprite(GFS_cursor_horny);
        if (spr == nullptr || spr->SWidth == 0 || spr->SHeight == 0)
            return false;
        s_cursor_build_attempted = true;

        s_cursor_w = spr->SWidth;
        s_cursor_h = spr->SHeight;
        // (0,0) -- what set_pointer_graphic_menu() (vidmode.c) itself uses
        // for this sprite -- turned out visibly wrong live ("alignment
        // issue... difficult to select things"). The user identified the
        // actual image directly (52x40, the gauntlet's grip roughly
        // centred rather than anchored to a corner) and gave the correct
        // hotspot: (26, 18).
        s_cursor_hotspot_x = 26;
        s_cursor_hotspot_y = 18;
        s_cursor_pixels.assign((size_t)s_cursor_w * (size_t)s_cursor_h, TbPixel{0, 0, 0, 0});

        // RendererGetDrawFlags()/RendererGetDrawColour() are ambient global
        // state (RendererManager.h's own comment: "held off lbDisplay"),
        // left however the last sprite/text draw call set them -- possibly
        // a REMAP or transparency flag with a colour table index that means
        // nothing here. Force a plain, unflagged draw and restore whatever
        // was ambient before, same reasoning as the graphics-window
        // save/restore below.
        unsigned short prev_flags = RendererGetDrawFlags();
        unsigned char prev_colour = RendererGetDrawColour();
        RendererSetDrawFlags(0);

        // Sprite draws decode through whatever palette is currently active
        // (LbDrawBufferSolid's own RendererGetActivePalette() call), not
        // frontend_palette directly -- and the active palette is exactly
        // what fade_in()/fade_out() (vidfade.c) transiently scale down
        // during a fade-to/from-black transition around state changes, the
        // same transitions that make this capture possible in the first
        // place (found live: landed mid-fade once, producing a
        // significantly-too-dark cursor instead of an outright-black one,
        // so the all-zero guard above didn't catch it). Force the stable,
        // un-faded frontend_palette as active for just this draw and
        // restore whatever was actually active afterward.
        unsigned char prev_palette[PALETTE_SIZE] = {0};
        RendererPaletteGet(prev_palette);
        RendererPaletteSet(frontend_palette);

        TbGraphicsWindow grwnd;
        LbScreenStoreGraphicsWindow(&grwnd);
        TbPixel *previous = RendererSwapFramebufferTarget(s_cursor_pixels.data(), s_cursor_w, s_cursor_h);
        LbScreenSetGraphicsWindow(0, 0, s_cursor_w, s_cursor_h);
        LbSpriteDrawImmediate(0, 0, spr);
        RendererRestoreFramebufferTarget(previous);
        RendererSetDrawFlags(prev_flags);
        RendererSetDrawColour(prev_colour);
        RendererPaletteSet(prev_palette);
        LbScreenLoadGraphicsWindow(&grwnd);

        return true;
    }
}

TbBool FeStyleGetCursorImage(struct ImGuiCursorImage *out)
{
    if (out == nullptr || !build_cursor_pixels())
        return 0;
    out->rgba = s_cursor_pixels.data();
    out->width = s_cursor_w;
    out->height = s_cursor_h;
    out->hotspot_x = s_cursor_hotspot_x;
    out->hotspot_y = s_cursor_hotspot_y;
    return 1;
}

namespace {
    void *s_menu_backdrop_texture = nullptr;
    bool s_menu_backdrop_build_attempted = false;
    const int MENU_BACKDROP_W = 640;
    const int MENU_BACKDROP_H = 480;

    // docs/refactor/renderer/05-imgui-owned-menu-backdrop.md Phase A.
    // frontend_background is a flat 640x480 indexed (VGA-palette) bitmap,
    // loaded once at frontend_load_data() time and never touched again --
    // unlike the land-preview panel's own dynamic texture (frontgui_screens.cpp,
    // rebuilt every frame it's visible), this decodes and uploads exactly
    // once, ever, then just hands back the same cached handle.
    bool build_menu_backdrop_texture()
    {
        if (s_menu_backdrop_build_attempted)
            return s_menu_backdrop_texture != nullptr;

        // Same readiness gating as build_cursor_pixels() above, and for the
        // same reason: this can run as early as the very first ImGui frame,
        // before frontend_load_data() has populated frontend_background or
        // FeSt_INITIAL's own exit has loaded a real palette into
        // frontend_palette. Keep retrying instead of latching a black/wrong
        // capture permanently.
        if (frontend_background == NULL)
            return false;
        bool palette_all_zero = true;
        for (int i = 0; i < PALETTE_SIZE; i++)
        {
            if (frontend_palette[i] != 0) { palette_all_zero = false; break; }
        }
        if (palette_all_zero)
            return false;
        s_menu_backdrop_build_attempted = true;

        // frontend_background's index 0 is a normal opaque background
        // colour, not "transparent" the way a sprite's index 0 is --
        // resolve_indexed_pixel() (bflib_video.h), not expand_indexed_pixel().
        std::vector<TbPixel> pixels((size_t)MENU_BACKDROP_W * (size_t)MENU_BACKDROP_H);
        for (size_t i = 0; i < pixels.size(); i++)
            pixels[i] = resolve_indexed_pixel(frontend_background[i], frontend_palette);

        s_menu_backdrop_texture = RendererCreateDynamicTexture(MENU_BACKDROP_W, MENU_BACKDROP_H);
        if (s_menu_backdrop_texture == nullptr)
            return false;
        RendererUpdateDynamicTexture(s_menu_backdrop_texture, pixels.data(), MENU_BACKDROP_W, MENU_BACKDROP_H);
        return true;
    }
}

void *FeStyleGetMenuBackdropTexture(int *out_w, int *out_h)
{
    if (!build_menu_backdrop_texture())
        return nullptr;
    if (out_w) *out_w = MENU_BACKDROP_W;
    if (out_h) *out_h = MENU_BACKDROP_H;
    return s_menu_backdrop_texture;
}
