// kfx_platform: bflib_guibtns.c. check_if_pos_is_over_button() is a pure
// rectangle-hit-test over a bare struct GuiButton (fully defined in this
// same header, unlike most GUI structs which live one layer up) -- no
// fixture needed. setup_input_field() is pattern A on the module's own
// globals (backup_input_field/input_field_pos/lbInkey) plus the real
// LbLocTextStringLength() (bflib_string.c, already covered elsewhere,
// pure UTF-8 char counting). do_sound_menu_click()/do_sound_button_click()
// reach into real OpenAL playback (play_non_3d_sample*) and
// set_gui_click_sound_ids() only affects those through static globals
// with no way to read them back -- none of the three attempted here.
#include <catch2/catch_test_macros.hpp>

#include "bflib_guibtns.h"
#include "bflib_keybrd.h"

#include <cstring>

namespace {
struct ZeroedButton {
    ZeroedButton() { std::memset(&gbtn, 0, sizeof(gbtn)); }
    struct GuiButton gbtn;
};
}

TEST_CASE_METHOD(ZeroedButton, "check_if_pos_is_over_button is true for a position strictly inside the button's rectangle", "[kfx_platform][bflib_guibtns]") {
    gbtn.pos_x = 10;
    gbtn.pos_y = 20;
    gbtn.width = 30;
    gbtn.height = 40;
    CHECK(check_if_pos_is_over_button(&gbtn, 25, 35));
}

TEST_CASE_METHOD(ZeroedButton, "check_if_pos_is_over_button is true exactly at the top-left corner (inclusive)", "[kfx_platform][bflib_guibtns]") {
    gbtn.pos_x = 10;
    gbtn.pos_y = 20;
    gbtn.width = 30;
    gbtn.height = 40;
    CHECK(check_if_pos_is_over_button(&gbtn, 10, 20));
}

TEST_CASE_METHOD(ZeroedButton, "check_if_pos_is_over_button is false exactly at the bottom-right corner (exclusive)", "[kfx_platform][bflib_guibtns]") {
    gbtn.pos_x = 10;
    gbtn.pos_y = 20;
    gbtn.width = 30;
    gbtn.height = 40;
    CHECK_FALSE(check_if_pos_is_over_button(&gbtn, 10 + 30, 20 + 40));
}

TEST_CASE_METHOD(ZeroedButton, "check_if_pos_is_over_button is false for a position left of, above, right of, or below the rectangle", "[kfx_platform][bflib_guibtns]") {
    gbtn.pos_x = 10;
    gbtn.pos_y = 20;
    gbtn.width = 30;
    gbtn.height = 40;
    CHECK_FALSE(check_if_pos_is_over_button(&gbtn, 9, 35));   // left
    CHECK_FALSE(check_if_pos_is_over_button(&gbtn, 25, 19));  // above
    CHECK_FALSE(check_if_pos_is_over_button(&gbtn, 40, 35));  // right, exactly at pos_x + width
    CHECK_FALSE(check_if_pos_is_over_button(&gbtn, 25, 60));  // below, exactly at pos_y + height
}

TEST_CASE_METHOD(ZeroedButton, "setup_input_field copies the button's content string into backup_input_field and counts its length", "[kfx_platform][bflib_guibtns]") {
    char text[INPUT_FIELD_LEN] = "hello";
    gbtn.content.str = text;
    lbInkey = 42;
    setup_input_field(&gbtn, nullptr);
    CHECK(lbInkey == 0);
    CHECK(std::strcmp(backup_input_field, "hello") == 0);
    CHECK(input_field_pos == 5);
}

TEST_CASE_METHOD(ZeroedButton, "setup_input_field blanks the content when it matches empty_text", "[kfx_platform][bflib_guibtns]") {
    char text[INPUT_FIELD_LEN] = "Type here";
    gbtn.content.str = text;
    setup_input_field(&gbtn, "Type here");
    CHECK(text[0] == '\0');
}

TEST_CASE_METHOD(ZeroedButton, "setup_input_field leaves the content untouched when it doesn't match empty_text", "[kfx_platform][bflib_guibtns]") {
    char text[INPUT_FIELD_LEN] = "some value";
    gbtn.content.str = text;
    setup_input_field(&gbtn, "Type here");
    CHECK(std::strcmp(text, "some value") == 0);
}

TEST_CASE_METHOD(ZeroedButton, "setup_input_field is a safe no-op when content is a NULL pointer", "[kfx_platform][bflib_guibtns]") {
    gbtn.content.str = nullptr;
    setup_input_field(&gbtn, nullptr); // must not crash
    SUCCEED();
}
