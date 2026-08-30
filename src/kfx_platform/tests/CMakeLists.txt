# docs/refactor/testing/stage-01-framework-and-scaffold.md: one Catch2 test
# binary per src/kfx_*/ library. Only ever add_subdirectory()'d when
# KFX_BUILD_TESTS is ON (see ../CMakeLists.txt), so Catch2 and CTest are
# already available by the time this file is processed.
#
# kfx_test_main: a small shared library, not specific to kfx_platform's own
# tests, that every *_utest target links (added first, before any other
# kfx_*/tests/ subdirectory, so later ones can reference it by name --
# CMake target names are global to the whole build). Every kfx_*_utest
# transitively links kfx_platform (directly, or via another library that
# depends on it), and kfx_platform/src/platform/PlatformLinux.cpp
# physically owns the process main() (by design -- see
# docs/refactor/todo/check-layering-symbol-level-blind-spot.md's item 3).
# kfx_test_main.cpp forwards that main()'s kfxmain() call into Catch2's
# session runner, so every *_utest target links plain Catch2::Catch2 (via
# this library's PUBLIC dependency) instead of Catch2::Catch2WithMain,
# which would define its own `main` that never actually gets used (see
# stage-01's note on how that failure mode manifested at link time).
add_library(kfx_test_main STATIC kfx_test_main.cpp)
target_link_libraries(kfx_test_main PUBLIC Catch2::Catch2)
# Only needs platform.h's kfxmain() declaration, not kfx_platform's
# compiled objects (those come from whichever *_utest target links
# kfx_platform itself) -- an include dir, not a library link, avoids
# pulling kfx_platform's objects in twice (once here, once directly) and
# risking a multiple-definition link error.
target_include_directories(kfx_test_main PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")

file(GLOB KFX_PLATFORM_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM KFX_PLATFORM_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/kfx_test_main.cpp")

add_executable(kfx_platform_utest ${KFX_PLATFORM_TEST_SOURCES})
target_link_libraries(kfx_platform_utest PRIVATE
    kfx_platform
    kfx_bfdebug_std
    kfx_common_opts
    kfx_test_main)

include(CTest)
include(Catch)
# TEST_PREFIX: catch_discover_tests() otherwise names each ctest entry after
# the bare Catch2 TEST_CASE description alone, with no per-library
# namespacing -- `ctest -R kfx_platform` (the workflow stage-01 documents)
# needs this prefix present to match anything.
catch_discover_tests(kfx_platform_utest TEST_PREFIX "kfx_platform.")
