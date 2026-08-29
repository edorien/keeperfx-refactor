# docs/refactor/testing/stage-04d-kfx-render.md: links the whole
# kfx_render OBJECT library, same shape as kfx_sim_utest (see
# ../../kfx_sim/tests/CMakeLists.txt). Explicit centitoml link for the
# same transitive-kfx_config reason.
#
# kfx_render_utest transitively links kfx_sim, which needs the same
# get_packet/set_packet_action/etc. accepted-residual stubs
# kfx_sim_utest does -- reuses kfx_sim/tests/'s kfx_packet_test_stubs
# rather than duplicating it. Does NOT need kfx_sim_test_stubs.cpp's
# creature_table_add[] stub: kfx_render's own custom_sprites.c (linked
# here as part of the real kfx_render library) provides the real
# definition -- linking that stub too would be a duplicate-definition
# error.
file(GLOB KFX_RENDER_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(kfx_render_utest ${KFX_RENDER_TEST_SOURCES})
target_link_libraries(kfx_render_utest PRIVATE
    kfx_render
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
catch_discover_tests(kfx_render_utest TEST_PREFIX "kfx_render.")
