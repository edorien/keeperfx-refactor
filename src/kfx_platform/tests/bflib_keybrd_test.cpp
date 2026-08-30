// kfx_platform: bflib_keybrd.c's keyboardControl() -- the actual key-state
// state machine, called from the real SDL event handler
// (bflib_inputctrl.cpp, still untested/OS-bound). Pattern A on the
// module's own extern globals (lbKeyOn[]/lbInkey); lbInkeyFlags/lbIInkey/
// lbIInkeyFlags are static (module-private, no accessor) so their effects
// are only checked indirectly via lbKeyOn[code]'s post-modifier-OR at the
// end of the function. LbIKeyboardOpen/LbIKeyboardClose call the real
// init_inputcontrol() (SDL keyboard init) and aren't attempted here.
#include <catch2/catch_test_macros.hpp>

#include "bflib_keybrd.h"

#include <cstring>

namespace {
struct ResetKeyState {
    ResetKeyState() {
        std::memset(lbKeyOn, 0, sizeof(lbKeyOn));
        lbInkey = 0;
    }
};
}

TEST_CASE_METHOD(ResetKeyState, "keyboardControl KEYDOWN sets lbKeyOn for the pressed code and updates lbInkey", "[kfx_platform][bflib_keybrd]") {
    keyboardControl(KActn_KEYDOWN, KC_A, KMod_DONTCARE, 0);
    CHECK(lbKeyOn[KC_A] != 0);
    CHECK(lbInkey == KC_A);
}

TEST_CASE_METHOD(ResetKeyState, "keyboardControl KEYUP clears lbKeyOn for the released code", "[kfx_platform][bflib_keybrd]") {
    keyboardControl(KActn_KEYDOWN, KC_A, KMod_DONTCARE, 0);
    keyboardControl(KActn_KEYUP, KC_A, KMod_DONTCARE, 0);
    CHECK(lbKeyOn[KC_A] == 0);
}

TEST_CASE_METHOD(ResetKeyState, "keyboardControl with KMod_DONTCARE leaves modifier keys' lbKeyOn state untouched", "[kfx_platform][bflib_keybrd]") {
    lbKeyOn[KC_LSHIFT] = 1;
    keyboardControl(KActn_KEYDOWN, KC_A, KMod_DONTCARE, 0);
    CHECK(lbKeyOn[KC_LSHIFT] == 1); // not cleared, since modifiers == KMod_DONTCARE skips the whole block
}

TEST_CASE_METHOD(ResetKeyState, "keyboardControl with an explicit KMod_SHIFT synthesizes KC_LSHIFT when neither shift key is already down", "[kfx_platform][bflib_keybrd]") {
    keyboardControl(KActn_KEYDOWN, KC_A, KMod_SHIFT, 0);
    CHECK(lbKeyOn[KC_LSHIFT] != 0);
}

TEST_CASE_METHOD(ResetKeyState, "keyboardControl with an explicit KMod_SHIFT doesn't override an already-down KC_RSHIFT", "[kfx_platform][bflib_keybrd]") {
    lbKeyOn[KC_RSHIFT] = 1;
    keyboardControl(KActn_KEYDOWN, KC_A, KMod_SHIFT, 0);
    CHECK(lbKeyOn[KC_RSHIFT] == 1);
    CHECK(lbKeyOn[KC_LSHIFT] == 0); // only synthesized when NEITHER shift key is down
}

TEST_CASE_METHOD(ResetKeyState, "keyboardControl with modifiers set but KMod_SHIFT absent clears both shift keys", "[kfx_platform][bflib_keybrd]") {
    lbKeyOn[KC_LSHIFT] = 1;
    lbKeyOn[KC_RSHIFT] = 1;
    keyboardControl(KActn_KEYDOWN, KC_A, KMod_CONTROL, 0); // modifiers != DONTCARE, but SHIFT bit unset
    CHECK(lbKeyOn[KC_LSHIFT] == 0);
    CHECK(lbKeyOn[KC_RSHIFT] == 0);
}

TEST_CASE_METHOD(ResetKeyState, "keyboardControl folds the resolved modifier flags into lbKeyOn for the pressed key itself", "[kfx_platform][bflib_keybrd]") {
    keyboardControl(KActn_KEYDOWN, KC_A, KMod_SHIFT, 0);
    // lbKeyOn[code] was set to 1 by the KEYDOWN branch, then OR'd with the
    // resolved lbInkeyFlags (KMod_SHIFT, 0x10) since lbKeyOn[code] != 0.
    CHECK((lbKeyOn[KC_A] & KMod_SHIFT) != 0);
}

TEST_CASE_METHOD(ResetKeyState, "keyboardControl KEYUP with an unrecognized action value falls through to the default (KEYUP) branch", "[kfx_platform][bflib_keybrd]") {
    keyboardControl(KActn_KEYDOWN, KC_A, KMod_DONTCARE, 0);
    keyboardControl(99, KC_A, KMod_DONTCARE, 0); // not KActn_KEYDOWN or KActn_KEYUP
    CHECK(lbKeyOn[KC_A] == 0);
}
