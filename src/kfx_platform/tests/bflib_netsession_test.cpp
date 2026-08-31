// kfx_platform: bflib_netsession.c -- net_copy_name_string is the only
// function in this file, a pure bounded string copy with a
// memset-first-then-conditionally-copy shape.
#include <catch2/catch_test_macros.hpp>

#include "bflib_netsession.h"

#include <cstring>

TEST_CASE("net_copy_name_string copies src into dst, null-terminated", "[kfx_platform][bflib_netsession]") {
    char dst[32];
    std::memset(dst, 0xAA, sizeof(dst));
    net_copy_name_string(dst, "hello", sizeof(dst));
    CHECK(std::strcmp(dst, "hello") == 0);
}

TEST_CASE("net_copy_name_string zero-fills the whole buffer first, clearing any trailing garbage", "[kfx_platform][bflib_netsession]") {
    char dst[16];
    std::memset(dst, 0xAA, sizeof(dst));
    net_copy_name_string(dst, "hi", sizeof(dst));
    CHECK(std::strcmp(dst, "hi") == 0);
    for (size_t i = 3; i < sizeof(dst); i++) {
        CHECK(dst[i] == 0);
    }
}

TEST_CASE("net_copy_name_string leaves dst zeroed when src is NULL", "[kfx_platform][bflib_netsession]") {
    char dst[16];
    std::memset(dst, 0xAA, sizeof(dst));
    net_copy_name_string(dst, nullptr, sizeof(dst));
    for (size_t i = 0; i < sizeof(dst); i++) {
        CHECK(dst[i] == 0);
    }
}

TEST_CASE("net_copy_name_string truncates a src longer than max_len", "[kfx_platform][bflib_netsession]") {
    char dst[6];
    net_copy_name_string(dst, "abcdefghij", sizeof(dst));
    CHECK(std::strlen(dst) == 5); // snprintf null-terminates within max_len
    CHECK(std::strcmp(dst, "abcde") == 0);
}
