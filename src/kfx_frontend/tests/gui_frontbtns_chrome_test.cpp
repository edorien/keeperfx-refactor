// kfx_frontend: frontend_button_chrome_repeat_count() (gui_frontbtns.c),
// the pure arithmetic behind frontend_draw_button_chrome_flexible()'s
// width-driven middle-piece tiling -- see docs/refactor/gui (the
// flexible-width/icon/grid button primitives plan). Rendering itself
// (frontend_draw_button_chrome_flexible/frontend_draw_button_icon) needs
// real sprites and isn't covered here.
#include <catch2/catch_test_macros.hpp>

#include "gui_frontbtns.h"

TEST_CASE("frontend_button_chrome_repeat_count is zero when the middle piece has no width", "[kfx_frontend][gui_frontbtns]") {
    CHECK(frontend_button_chrome_repeat_count(371, 20, 20, 0) == 0);
}

TEST_CASE("frontend_button_chrome_repeat_count exactly fits the available width", "[kfx_frontend][gui_frontbtns]") {
    // left(20) + mid(20)*3 + right(20) == 100
    CHECK(frontend_button_chrome_repeat_count(100, 20, 20, 20) == 3);
}

TEST_CASE("frontend_button_chrome_repeat_count rounds to the nearest whole tile", "[kfx_frontend][gui_frontbtns]") {
    // avail = 100 - 20 - 20 = 60; 60/40 = 1.5 -> rounds to 2
    CHECK(frontend_button_chrome_repeat_count(100, 20, 20, 40) == 2);
}

TEST_CASE("frontend_button_chrome_repeat_count never goes negative when the caps alone exceed the target width", "[kfx_frontend][gui_frontbtns]") {
    CHECK(frontend_button_chrome_repeat_count(30, 20, 20, 20) == 0);
}

TEST_CASE("frontend_button_chrome_repeat_count scales linearly with target width", "[kfx_frontend][gui_frontbtns]") {
    // Not a claim about real sprite dimensions (unknown without the game's
    // data files) -- just that N extra mid_w-sized increments of width
    // produce N extra repeats, the property frontend_draw_button_chrome_
    // flexible's tiling depends on.
    CHECK(frontend_button_chrome_repeat_count(40, 20, 20, 20) == 0);
    CHECK(frontend_button_chrome_repeat_count(60, 20, 20, 20) == 1);
    CHECK(frontend_button_chrome_repeat_count(80, 20, 20, 20) == 2);
}
