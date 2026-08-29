// kfx_platform: bflib_video.c's DK-value scaling functions, per
// docs/refactor/testing/comprehensive/stage-08-comprehensive-library-
// passes.md's kfx_platform row -- the third and last known
// VideoScaleCallbacks consumer, flagged as still open there
// ("landed for two of the three known callback consumers").
//
// scale_value_for_resolution_with_upp takes units_per_px as a direct
// argument, no callback at all. scale_value_landview reads
// units_per_pixel_landview, a bare kfx_platform extern -- pattern A.
// Everything else (scale_value_by_horizontal_resolution/_vertical_
// resolution, scale_ui_value, scale_fixed_DK_value, scale_value_menu)
// reads its own field off video_scale_callbacks->get_video_scale_values()
// -- pattern B, kfx_platform's third callback-struct test (after
// GetGameTurnFunc and EmulateIntegerOverflowFunc in bflib_basics_test.cpp).
//
// 16 is "native resolution" for every units_per_pixel_* field (640x400/
// 640x480 at 1:1, per each field's own doc comment) -- scaling by 16
// is documented as the identity case, used throughout as the easy-to-
// verify baseline, with one doubled-upp case per function to confirm the
// field is actually being read, not just defaulting to identity.
#include <catch2/catch_test_macros.hpp>

#include "bflib_video.h"

#include <cstring>

TEST_CASE("scale_value_for_resolution_with_upp is the identity at units_per_px 16", "[kfx_platform][bflib_video]") {
    CHECK(scale_value_for_resolution_with_upp(100, 16) == 100);
}

TEST_CASE("scale_value_for_resolution_with_upp scales linearly with units_per_px", "[kfx_platform][bflib_video]") {
    CHECK(scale_value_for_resolution_with_upp(10, 32) == 20); // double upp -> double output
}

TEST_CASE("scale_value_for_resolution_with_upp floors at 1, never returning 0 or negative", "[kfx_platform][bflib_video]") {
    CHECK(scale_value_for_resolution_with_upp(0, 16) == 1);
    CHECK(scale_value_for_resolution_with_upp(-100, 16) == 1);
}

namespace {
struct ResetLandview {
    ResetLandview() { units_per_pixel_landview = 16; }
};
}

TEST_CASE_METHOD(ResetLandview, "scale_value_landview is the identity at units_per_pixel_landview 16", "[kfx_platform][bflib_video]") {
    CHECK(scale_value_landview(100) == 100);
}

TEST_CASE_METHOD(ResetLandview, "scale_value_landview scales with units_per_pixel_landview", "[kfx_platform][bflib_video]") {
    units_per_pixel_landview = 32;
    CHECK(scale_value_landview(10) == 20);
}

namespace {
struct VideoScaleValues g_fake_scale_values{};
const struct VideoScaleValues *fake_get_video_scale_values(void) { return &g_fake_scale_values; }

struct VideoScaleCallbacksFixture {
    struct VideoScaleCallbacks callbacks{};
    VideoScaleCallbacksFixture() {
        std::memset(&g_fake_scale_values, 0, sizeof(g_fake_scale_values));
        g_fake_scale_values.units_per_pixel_width = 16;
        g_fake_scale_values.units_per_pixel_height = 16;
        g_fake_scale_values.units_per_pixel_ui = 16;
        g_fake_scale_values.units_per_pixel_best = 16;
        g_fake_scale_values.units_per_pixel_menu = 16;
        callbacks.get_video_scale_values = fake_get_video_scale_values;
        set_video_scale_callbacks(&callbacks);
    }
    ~VideoScaleCallbacksFixture() { set_video_scale_callbacks(nullptr); } // restores the default no-op table
};
}

TEST_CASE_METHOD(VideoScaleCallbacksFixture, "scale_value_by_horizontal_resolution reads units_per_pixel_width through the callback", "[kfx_platform][bflib_video]") {
    CHECK(scale_value_by_horizontal_resolution(100) == 100); // identity at upp 16
    g_fake_scale_values.units_per_pixel_width = 32;
    CHECK(scale_value_by_horizontal_resolution(10) == 20);
}

TEST_CASE_METHOD(VideoScaleCallbacksFixture, "scale_value_by_vertical_resolution reads units_per_pixel_height through the callback", "[kfx_platform][bflib_video]") {
    CHECK(scale_value_by_vertical_resolution(100) == 100);
    g_fake_scale_values.units_per_pixel_height = 32;
    CHECK(scale_value_by_vertical_resolution(10) == 20);
}

TEST_CASE_METHOD(VideoScaleCallbacksFixture, "scale_ui_value reads units_per_pixel_ui through the callback", "[kfx_platform][bflib_video]") {
    CHECK(scale_ui_value(100) == 100);
    g_fake_scale_values.units_per_pixel_ui = 32;
    CHECK(scale_ui_value(10) == 20);
}

TEST_CASE_METHOD(VideoScaleCallbacksFixture, "scale_fixed_DK_value reads units_per_pixel_best through the callback", "[kfx_platform][bflib_video]") {
    CHECK(scale_fixed_DK_value(100) == 100);
    g_fake_scale_values.units_per_pixel_best = 32;
    CHECK(scale_fixed_DK_value(10) == 20);
}

TEST_CASE_METHOD(VideoScaleCallbacksFixture, "scale_value_menu reads units_per_pixel_menu through the callback", "[kfx_platform][bflib_video]") {
    CHECK(scale_value_menu(100) == 100);
    g_fake_scale_values.units_per_pixel_menu = 32;
    CHECK(scale_value_menu(10) == 20);
}
