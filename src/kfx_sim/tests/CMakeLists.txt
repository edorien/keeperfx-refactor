# docs/refactor/testing/stage-04c-kfx-sim.md: links the whole kfx_sim
# OBJECT library, same shape as kfx_pathfinding_utest -- explicit
# centitoml link for the same reason (kfx_config is transitively linked
# and calls toml_parse()).
#
# kfx_packet_test_stubs: a small shared library (like kfx_platform/tests/'s
# kfx_test_main), defined here -- the first place it's needed -- so any
# later *_utest target that transitively links kfx_sim can reuse it
# instead of duplicating it. See packet_test_stubs.cpp for what it stubs
# and why.
add_library(kfx_packet_test_stubs STATIC packet_test_stubs.cpp)
target_include_directories(kfx_packet_test_stubs PRIVATE
    "${CMAKE_SOURCE_DIR}/src/kfx_sim/include")
# kfx_common_opts, not a bare kfx_platform/include add: packet_data.h
# transitively pulls in version.h -> ver_defs.h, which is only on the
# include path via kfx_common_opts's INTERFACE_INCLUDE_DIRECTORIES
# (ver_defs.h is configure_file()'d to the repo root, not under src/).
target_link_libraries(kfx_packet_test_stubs PRIVATE kfx_common_opts)

file(GLOB KFX_SIM_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM KFX_SIM_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/packet_test_stubs.cpp")

add_executable(kfx_sim_utest ${KFX_SIM_TEST_SOURCES})
target_link_libraries(kfx_sim_utest PRIVATE
    kfx_sim
    kfx_pathfinding
    kfx_config
    kfx_platform
    kfx_bfdebug_std
    kfx_common_opts
    centitoml
    kfx_packet_test_stubs
    kfx_test_main)

include(CTest)
include(Catch)
catch_discover_tests(kfx_sim_utest TEST_PREFIX "kfx_sim.")
