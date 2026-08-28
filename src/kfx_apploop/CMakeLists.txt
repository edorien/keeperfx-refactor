# Stage 12.5 of docs/refactor/: kfx_apploop holds the top-level game/
# frontend session loop (game_loop/wait_at_frontend/keeper_gameplay_loop/
# update() and the frame-pacing helpers around them), physically
# extracted out of src/main.cpp. Ranked above kfx_script (see
# scripts/check_layering.py) -- like kfx_script, it's allowed to depend
# on anything below it, so this globs src/kfx_apploop/src/ directly,
# same shape as kfx_script's own CMakeLists.txt (stage 11).

file(GLOB KFX_APPLOOP_SOURCES_C "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB KFX_APPLOOP_SOURCES_CXX "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
set(KFX_APPLOOP_SOURCES ${KFX_APPLOOP_SOURCES_C} ${KFX_APPLOOP_SOURCES_CXX})

add_library(kfx_apploop OBJECT ${KFX_APPLOOP_SOURCES})
target_link_libraries(kfx_apploop PRIVATE kfx_common_opts kfx_bfdebug_std)
target_include_directories(kfx_apploop PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(kfx_apploop_hvlog OBJECT ${KFX_APPLOOP_SOURCES})
target_link_libraries(kfx_apploop_hvlog PRIVATE kfx_common_opts kfx_bfdebug_hvlog)
target_include_directories(kfx_apploop_hvlog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

list(APPEND KFX_OBJECT_LIBS_STD kfx_apploop)
set(KFX_OBJECT_LIBS_STD ${KFX_OBJECT_LIBS_STD} PARENT_SCOPE)
list(APPEND KFX_OBJECT_LIBS_HVLOG kfx_apploop_hvlog)
set(KFX_OBJECT_LIBS_HVLOG ${KFX_OBJECT_LIBS_HVLOG} PARENT_SCOPE)
