// kfx_platform: bflib_string.c, per docs/refactor/testing/comprehensive/
// stage-08-comprehensive-library-passes.md's kfx_platform row -- pure
// UTF-8-aware string functions, no extern state, in the LbSqrL/LbLerp
// vein stage-08 flagged as a reasonable next candidate.
//
// "a\xC3\xA9bc" is "aébc" with é encoded as the two-byte UTF-8 sequence
// 0xC3 0xA9 (U+00E9) -- 5 bytes, 4 logical characters. Used throughout to
// confirm these functions count/index by character, not by byte, which
// is their entire reason to exist over plain strlen()/strcpy().
#include <catch2/catch_test_macros.hpp>

#include "bflib_string.h"

#include <cstring>

TEST_CASE("LbLocTextStringLength counts logical characters, not bytes", "[kfx_platform][bflib_string]") {
    CHECK(LbLocTextStringLength("") == 0);
    CHECK(LbLocTextStringLength("abc") == 3);
    CHECK(LbLocTextStringLength("a\xC3\xA9""bc") == 4); // 5 bytes, 4 chars
}

TEST_CASE("LbLocTextStringSize counts bytes, including UTF-8 continuation bytes", "[kfx_platform][bflib_string]") {
    CHECK(LbLocTextStringSize("") == 0);
    CHECK(LbLocTextStringSize("abc") == 3);
    CHECK(LbLocTextStringSize("a\xC3\xA9""bc") == 5);
}

TEST_CASE("LbLocTextPosToLength converts a character position to a byte offset", "[kfx_platform][bflib_string]") {
    const char *s = "a\xC3\xA9""bc"; // a(1) + e-acute(2) + b(1) + c(1)
    CHECK(LbLocTextPosToLength(s, 0) == 0);
    CHECK(LbLocTextPosToLength(s, 1) == 1);   // start of the 2-byte char
    CHECK(LbLocTextPosToLength(s, 2) == 3);   // past the 2-byte char entirely, at 'b'
    CHECK(LbLocTextPosToLength(s, 4) == 5);   // one past the last char, at the terminator
}

TEST_CASE("LbLocTextPosToLength clamps a position beyond the string to its full byte size", "[kfx_platform][bflib_string]") {
    const char *s = "a\xC3\xA9""bc";
    CHECK(LbLocTextPosToLength(s, 100) == 5);
}

TEST_CASE("LbLocTextStringConcat appends up to maxlen source characters", "[kfx_platform][bflib_string]") {
    char buf[16] = "ab";
    LbLocTextStringConcat(buf, "cd", 2);
    CHECK(std::strcmp(buf, "abcd") == 0);
}

TEST_CASE("LbLocTextStringConcat truncates the appended portion at maxlen", "[kfx_platform][bflib_string]") {
    char buf[16] = "ab";
    LbLocTextStringConcat(buf, "cdef", 2);
    CHECK(std::strcmp(buf, "abcd") == 0);
}

TEST_CASE("LbLocTextStringInsert splices a string in at a character position", "[kfx_platform][bflib_string]") {
    char buf[16] = "abc";
    TbLocChar *result = LbLocTextStringInsert(buf, "XY", 1, sizeof(buf));
    REQUIRE(result == buf);
    CHECK(std::strcmp(buf, "aXYbc") == 0);
}

TEST_CASE("LbLocTextStringInsert splits a UTF-8 character position correctly, not by byte offset", "[kfx_platform][bflib_string]") {
    char buf[16] = "a\xC3\xA9""bc"; // aébc
    TbLocChar *result = LbLocTextStringInsert(buf, "X", 2, sizeof(buf)); // after e-acute, before 'b'
    REQUIRE(result == buf);
    CHECK(std::strcmp(buf, "a\xC3\xA9""Xbc") == 0); // aéXbc
}

TEST_CASE("LbLocTextStringInsert refuses to overflow maxlen", "[kfx_platform][bflib_string]") {
    char buf[16] = "abc";
    // slen(3) + clen(2) == 5, and the check is ">= maxlen" -- maxlen == 5
    // is exactly the rejection boundary, not just "too small".
    TbLocChar *result = LbLocTextStringInsert(buf, "XY", 1, 5);
    CHECK(result == nullptr);
    CHECK(std::strcmp(buf, "abc") == 0); // left untouched
}

TEST_CASE("LbLocTextStringDelete removes a character range from the middle", "[kfx_platform][bflib_string]") {
    char buf[16] = "abcde";
    TbLocChar *result = LbLocTextStringDelete(buf, 1, 2); // remove "bc"
    REQUIRE(result == buf);
    CHECK(std::strcmp(buf, "ade") == 0);
}

TEST_CASE("LbLocTextStringDelete clamps a count that runs past the end of the string", "[kfx_platform][bflib_string]") {
    char buf[16] = "abcde";
    TbLocChar *result = LbLocTextStringDelete(buf, 3, 100); // "delete to the end" via an oversized count
    REQUIRE(result == buf);
    CHECK(std::strcmp(buf, "abc") == 0);
}
