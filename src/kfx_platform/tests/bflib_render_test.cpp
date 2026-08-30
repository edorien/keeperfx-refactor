// kfx_platform: bflib_render.c's polyscans[] lifecycle -- the only part
// of this file with no real rasterization/rendering-surface dependency
// (draw_gpoly/trig, the actual 3D triangle fillers, aren't attempted
// here, matching the rest of the render/vidraw pipeline's "significant
// harness change" gap noted in docs/Architecture/testing-harness.md
// §10).
#include <catch2/catch_test_macros.hpp>

#include "bflib_render.h"

namespace {
struct RenderLifecycle {
    ~RenderLifecycle() { finish_bflib_render(); } // in case a test forgets
};
}

TEST_CASE_METHOD(RenderLifecycle, "setup_bflib_render allocates a zeroed polyscans buffer", "[kfx_platform][bflib_render]") {
    setup_bflib_render();
    REQUIRE(polyscans != nullptr);
    CHECK(polyscans[0].X == 0);
    CHECK(polyscans[4095].X == 0);
}

TEST_CASE_METHOD(RenderLifecycle, "reset_bflib_render re-zeroes polyscans without reallocating it", "[kfx_platform][bflib_render]") {
    setup_bflib_render();
    struct PolyPoint *original_ptr = polyscans;
    polyscans[0].X = 42;
    reset_bflib_render();
    CHECK(polyscans == original_ptr);
    CHECK(polyscans[0].X == 0);
}

TEST_CASE_METHOD(RenderLifecycle, "finish_bflib_render frees polyscans and nulls the pointer", "[kfx_platform][bflib_render]") {
    setup_bflib_render();
    finish_bflib_render();
    CHECK(polyscans == nullptr);
}
