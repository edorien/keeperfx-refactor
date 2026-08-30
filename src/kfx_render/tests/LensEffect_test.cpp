// kfx_render: LensEffect.cpp (the base class) plus a fresh, never-Setup()
// instance of each concrete subclass whose constructor/Draw()/Cleanup()
// turned out to be safe without any real render/config/player state --
// confirmed by reading every one of their bodies first, not assumed:
// MistEffect/OverlayEffect/DisplacementEffect/FlyeyeEffect all guard
// their Cleanup() logic behind `m_current_lens >= 0` (or an internal
// nullptr-checked FreeLookupTable()), and their Draw() takes the same
// early-return path before ever touching the LensRenderContext*, so a
// nullptr ctx is safe to pass. PaletteEffect/LuaLensEffect's Setup()/
// Draw() reach into real player/lua state and aren't attempted here --
// this is exactly the "direct instantiation + assertion, no fixture
// needed" candidate docs/refactor/testing/comprehensive/
// stage-08-comprehensive-library-passes.md's kfx_render row already
// flagged.
#include <catch2/catch_test_macros.hpp>

#include "MistEffect.h"
#include "OverlayEffect.h"
#include "DisplacementEffect.h"
#include "FlyeyeEffect.h"

#include <string>

TEST_CASE("a fresh LensEffect subclass instance reports its type/name and defaults to enabled", "[kfx_render][LensEffect]") {
    MistEffect mist;
    CHECK(mist.GetType() == LensEffectType::Mist);
    CHECK(std::string(mist.GetName()) == "Mist");
    CHECK(mist.IsEnabled());
}

TEST_CASE("SetEnabled/IsEnabled round-trip on the base class", "[kfx_render][LensEffect]") {
    OverlayEffect overlay;
    overlay.SetEnabled(false);
    CHECK_FALSE(overlay.IsEnabled());
    overlay.SetEnabled(true);
    CHECK(overlay.IsEnabled());
}

TEST_CASE("each concrete subclass reports its own distinct type and name", "[kfx_render][LensEffect]") {
    MistEffect mist;
    OverlayEffect overlay;
    DisplacementEffect displacement;
    FlyeyeEffect flyeye;

    CHECK(mist.GetType() == LensEffectType::Mist);
    CHECK(overlay.GetType() == LensEffectType::Overlay);
    CHECK(displacement.GetType() == LensEffectType::Displacement);
    CHECK(flyeye.GetType() == LensEffectType::Flyeye);

    CHECK(std::string(overlay.GetName()) == "Overlay");
    CHECK(std::string(displacement.GetName()) == "Displacement");
    CHECK(std::string(flyeye.GetName()) == "Flyeye");
}

TEST_CASE("Draw on a fresh (never Setup) instance returns false without touching the render context", "[kfx_render][LensEffect]") {
    MistEffect mist;
    OverlayEffect overlay;
    DisplacementEffect displacement;
    FlyeyeEffect flyeye;

    // nullptr is safe here specifically because m_current_lens starts at
    // -1 and every one of these Draw() overrides checks that before
    // dereferencing ctx -- confirmed by reading each body.
    CHECK_FALSE(mist.Draw(nullptr));
    CHECK_FALSE(overlay.Draw(nullptr));
    CHECK_FALSE(displacement.Draw(nullptr));
    CHECK_FALSE(flyeye.Draw(nullptr));
}

TEST_CASE("destroying a fresh (never Setup) instance is a safe no-op Cleanup", "[kfx_render][LensEffect]") {
    { MistEffect mist; }
    { OverlayEffect overlay; }
    { DisplacementEffect displacement; }
    { FlyeyeEffect flyeye; }
    SUCCEED();
}

TEST_CASE("LoadAssetWithFallback rejects a NULL or empty filename", "[kfx_render][LensEffect]") {
    MistEffect mist;
    unsigned char buffer[16];
    CHECK_FALSE(mist.LoadAssetWithFallback(nullptr, buffer, sizeof(buffer), nullptr));
    CHECK_FALSE(mist.LoadAssetWithFallback("", buffer, sizeof(buffer), nullptr));
}

TEST_CASE("LoadAssetWithFallback rejects a NULL or zero-sized buffer", "[kfx_render][LensEffect]") {
    MistEffect mist;
    unsigned char buffer[16];
    CHECK_FALSE(mist.LoadAssetWithFallback("texture.dat", nullptr, sizeof(buffer), nullptr));
    CHECK_FALSE(mist.LoadAssetWithFallback("texture.dat", buffer, 0, nullptr));
}
