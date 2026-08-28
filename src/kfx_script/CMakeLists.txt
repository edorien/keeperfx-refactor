# Stage 11 of docs/refactor/: kfx_script is physically extracted out of
# src/ in one pass (see docs/refactor/stage-11-kfx-script.md), mirroring
# kfx_platform/kfx_config's single-pass approach (stages 3/4) rather than
# kfx_sim/kfx_render/kfx_net/kfx_game/kfx_frontend's incremental-cluster
# one -- unlike those, this cluster needs no struct-Game field migration
# (kfx_script is allowed to depend on everything below it; "no upward
# includes" isn't the right test for this layer per the stage doc), so
# there's no reason to stage it. This file globs src/kfx_script/src/
# directly, same shape as every other library's CMakeLists.txt.

file(GLOB KFX_SCRIPT_SOURCES_C "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB KFX_SCRIPT_SOURCES_CXX "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
set(KFX_SCRIPT_SOURCES ${KFX_SCRIPT_SOURCES_C} ${KFX_SCRIPT_SOURCES_CXX})

add_library(kfx_script OBJECT ${KFX_SCRIPT_SOURCES})
target_link_libraries(kfx_script PRIVATE kfx_common_opts kfx_bfdebug_std)
target_include_directories(kfx_script PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(kfx_script_hvlog OBJECT ${KFX_SCRIPT_SOURCES})
target_link_libraries(kfx_script_hvlog PRIVATE kfx_common_opts kfx_bfdebug_hvlog)
target_include_directories(kfx_script_hvlog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

list(APPEND KFX_OBJECT_LIBS_STD kfx_script)
set(KFX_OBJECT_LIBS_STD ${KFX_OBJECT_LIBS_STD} PARENT_SCOPE)
list(APPEND KFX_OBJECT_LIBS_HVLOG kfx_script_hvlog)
set(KFX_OBJECT_LIBS_HVLOG ${KFX_OBJECT_LIBS_HVLOG} PARENT_SCOPE)
