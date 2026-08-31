// kfx_platform: bflib_cpu.c -- cpu_get_type/_family/_model/_stepping are
// pure bit-field decoders over a caller-supplied struct CPU_INFO, no
// real CPUID needed to exercise them (unlike cpu_detect() itself, which
// issues real `cpuid` instructions -- given a light smoke test below
// since it's a benign, always-available real dependency with no crash
// risk, not the "significant harness change" kind this project declines
// elsewhere).
#include <catch2/catch_test_macros.hpp>

#include "bflib_cpu.h"

#include <cstring>

TEST_CASE("cpu_get_type/_family/_model/_stepping default to OEM/486/0/0 when feature_intl is zero", "[kfx_platform][bflib_cpu]") {
    struct CPU_INFO cpu;
    std::memset(&cpu, 0, sizeof(cpu));

    CHECK(cpu_get_type(&cpu) == CPUID_TYPE_OEM);
    CHECK(cpu_get_family(&cpu) == CPUID_FAMILY_486);
    CHECK(cpu_get_model(&cpu) == 0);
    CHECK(cpu_get_stepping(&cpu) == 0);
}

TEST_CASE("cpu_get_family/_model apply the extended-model formula for family 6 (Kaby-Lake-shaped signature)", "[kfx_platform][bflib_cpu]") {
    struct CPU_INFO cpu;
    std::memset(&cpu, 0, sizeof(cpu));
    // type=0, ext_model=0x8 (bits 19:16), family=6 (bits 11:8),
    // base_model=0xE (bits 7:4), stepping=9 (bits 3:0) -- a real Kaby
    // Lake-shaped signature: extended model (0x8<<4)+0xE = 0x8E = 142.
    cpu.feature_intl = 0x806E9;

    CHECK(cpu_get_type(&cpu) == 0);
    CHECK(cpu_get_family(&cpu) == 6);
    CHECK(cpu_get_model(&cpu) == 0x8E);
    CHECK(cpu_get_stepping(&cpu) == 9);
}

TEST_CASE("cpu_get_family/_model apply the extended-family/extended-model formula for family 15", "[kfx_platform][bflib_cpu]") {
    struct CPU_INFO cpu;
    std::memset(&cpu, 0, sizeof(cpu));
    // type=1, ext_family=0 (bits 27:20, so combined family stays exactly
    // 15), ext_model=2 (bits 19:16), base_family=15 (bits 11:8),
    // base_model=3 (bits 7:4), stepping=5 (bits 3:0) -- extended model
    // (2<<4)+3 = 0x23 = 35.
    cpu.feature_intl = 0x21F35;

    CHECK(cpu_get_type(&cpu) == 1);
    CHECK(cpu_get_family(&cpu) == 15);
    CHECK(cpu_get_model(&cpu) == 0x23);
    CHECK(cpu_get_stepping(&cpu) == 5);
}

TEST_CASE("cpu_get_family/_model take the plain, non-extended path for a family that isn't 6 or 15", "[kfx_platform][bflib_cpu]") {
    struct CPU_INFO cpu;
    std::memset(&cpu, 0, sizeof(cpu));
    // family=2, model=5, stepping=1, type=0 -- real (nonzero) feature_intl,
    // exercising the "!= 0" branches in cpu_get_type/_family rather than
    // their zero-defaults, and cpu_get_model's plain non-extended else.
    cpu.feature_intl = 0x251;

    CHECK(cpu_get_type(&cpu) == 0);
    CHECK(cpu_get_family(&cpu) == 2);
    CHECK(cpu_get_model(&cpu) == 5);
    CHECK(cpu_get_stepping(&cpu) == 1);
}

TEST_CASE("cpu_detect fills a non-empty, null-terminated vendor string without crashing", "[kfx_platform][bflib_cpu]") {
    struct CPU_INFO cpu;
    std::memset(&cpu, 0xAA, sizeof(cpu)); // poison, so a no-op detect would fail this
    cpu_detect(&cpu);

    CHECK(std::strlen(cpu.vendor) > 0);
    CHECK(std::strlen(cpu.vendor) <= 12); // vendor[13] is always null-terminated within the 12-char CPUID vendor string
}
