#include "pre_inc.h"
#include "renderer/RendererSoftware.h"
#include "bflib_video.h"       // PALETTE_COLORS, lbWindow, SDL, vsync_enabled
#include "bflib_vidsurface.h"  // lbDrawSurface
#include "bflib_mouse.h"       // LbMouseOnBeginSwap/EndSwap (software cursor around present)
#include <SDL3_image/SDL_image.h> // IMG_SavePNG (screenshots)
#include "post_inc.h"

bool RendererSoftware::Init()
{
    return true;
}

void RendererSoftware::Shutdown()
{
    destroy_present_target();
}

void RendererSoftware::SetDisplayPalette(const unsigned char* rgb8)
{
    // Vestigial: the draw surface is RGBA32 now, not an indexed surface with
    // its own SDL palette to push colours into. The game's authoritative
    // palette (LbPaletteGetReadonly()/RendererGetActivePalette()) is tracked
    // independently of the surface's pixel format and is what source-asset
    // bytes actually get resolved against; this callback has nothing left
    // to do for the software backend.
    (void)rgb8;
}

void RendererSoftware::ClearScreen(unsigned char colour)
{
    if (lbDrawSurface == NULL)
        return;
    // colour is a palette index (every caller passes a literal like 0 or
    // 144) -- resolve it through the game's own palette, then map it to
    // the draw surface's own (RGBA32) pixel format for the fill.
    TbPixel px = resolve_indexed_pixel(colour, LbPaletteGetReadonly());
    Uint32 mapped = SDL_MapSurfaceRGBA(lbDrawSurface, px.r, px.g, px.b, px.a);
    if (!SDL_FillSurfaceRect(lbDrawSurface, NULL, mapped))
        ERRORLOG("Error while clearing screen: %s", SDL_GetError());
}

bool RendererSoftware::ensure_present_target()
{
    if (m_renderer != nullptr && SDL_GetRenderWindow(m_renderer) != lbWindow)
        destroy_present_target();
    if (m_renderer == nullptr)
    {
        m_renderer = SDL_CreateRenderer(lbWindow, NULL);
        if (m_renderer == nullptr)
        {
            ERRORLOG("SDL_CreateRenderer failed: %s", SDL_GetError());
            return false;
        }
    }

    const int want_vsync = vsync_enabled ? 1 : 0;
    if (m_vsync != want_vsync)
    {
        SDL_SetRenderVSync(m_renderer, want_vsync);
        m_vsync = want_vsync;
    }

    if (m_texture == nullptr || m_tex_w != lbDrawSurface->w || m_tex_h != lbDrawSurface->h)
    {
        if (m_texture != nullptr) { SDL_DestroyTexture(m_texture); m_texture = nullptr; }
        m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32,
                                      SDL_TEXTUREACCESS_STREAMING, lbDrawSurface->w, lbDrawSurface->h);
        if (m_texture == nullptr)
        {
            ERRORLOG("SDL_CreateTexture failed: %s", SDL_GetError());
            return false;
        }
        SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_NEAREST); // crisp pixels
        m_tex_w = lbDrawSurface->w;
        m_tex_h = lbDrawSurface->h;
    }
    return true;
}

void RendererSoftware::destroy_present_target()
{
    if (m_texture != nullptr) { SDL_DestroyTexture(m_texture); m_texture = nullptr; }
    if (m_renderer != nullptr) { SDL_DestroyRenderer(m_renderer); m_renderer = nullptr; }
    m_tex_w = 0;
    m_tex_h = 0;
    m_vsync = -1;
}

unsigned char* RendererSoftware::LockFramebuffer(TbBytePitch* out_pitch)
{
    if (lbDrawSurface == NULL || !SDL_LockSurface(lbDrawSurface))
        return nullptr;
    if (out_pitch != nullptr)
        *out_pitch = TbBytePitch{ lbDrawSurface->pitch };
    return static_cast<unsigned char*>(lbDrawSurface->pixels);
}

void RendererSoftware::UnlockFramebuffer()
{
    if (lbDrawSurface != NULL)
        SDL_UnlockSurface(lbDrawSurface);
}

bool RendererSoftware::ScheduleScreenshot(const char* path, int fmt)
{
    if (lbDrawSurface == NULL)
        return false;
    bool ok;
    switch (fmt)
    {
        case 1:  ok = IMG_SavePNG(lbDrawSurface, path); break;
        case 2:  ok = SDL_SaveBMP(lbDrawSurface, path); break;
        default: return false;
    }
    if (!ok)
        ERRORLOG("Screenshot save failed (%s): %s", path, SDL_GetError());
    return ok;
}

void RendererSoftware::PresentFrame()
{
    if (lbDrawSurface == NULL || !ensure_present_target())
        return;
    LbMouseOnBeginSwap();
    // The draw surface is already RGBA32 -- the same format m_texture was
    // created with -- so presentation is a direct upload now, no more
    // INDEX8->RGBA blit through a locked texture surface.
    if (!SDL_UpdateTexture(m_texture, NULL, lbDrawSurface->pixels, lbDrawSurface->pitch))
    {
        ERRORLOG("Present texture update failed: %s", SDL_GetError());
        LbMouseOnEndSwap();
        return;
    }
    SDL_RenderClear(m_renderer);
    SDL_RenderTexture(m_renderer, m_texture, NULL, NULL);
    SDL_RenderPresent(m_renderer);
    LbMouseOnEndSwap();
}
