# Stage 10 of docs/refactor/: kfx_frontend is physically extracted out
# of src/ incrementally, one cluster at a time (see
# docs/refactor/stage-10-kfx-frontend.md), mirroring stage 6/7/8/9's
# kfx_sim/kfx_render/kfx_net/kfx_game approach. Created ahead of the
# physical-extraction clusters (stage 10.2+) to give kfx_frontend_state.h/.c
# a real home for the struct Game field-group migration (stage 10.1) --
# the root CMakeLists.txt's flat-src `kfx_frontend` component was renamed
# to `kfx_frontend_remaining` to free up the name for this real library,
# same as kfx_game_remaining/kfx_net_remaining/kfx_sim_remaining/
# kfx_render_remaining before it. This file globs src/kfx_frontend/src/
# directly, exactly like kfx_game's own CMakeLists.txt -- so later
# clusters just add more files here via `git mv` and shrink their stem
# list out of kfx_frontend_remaining; no changes needed in this file.

file(GLOB KFX_FRONTEND_SOURCES_C "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB KFX_FRONTEND_SOURCES_CXX "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
set(KFX_FRONTEND_SOURCES ${KFX_FRONTEND_SOURCES_C} ${KFX_FRONTEND_SOURCES_CXX})

add_library(kfx_frontend OBJECT ${KFX_FRONTEND_SOURCES})
target_link_libraries(kfx_frontend PRIVATE kfx_common_opts kfx_bfdebug_std)
target_include_directories(kfx_frontend PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(kfx_frontend_hvlog OBJECT ${KFX_FRONTEND_SOURCES})
target_link_libraries(kfx_frontend_hvlog PRIVATE kfx_common_opts kfx_bfdebug_hvlog)
target_include_directories(kfx_frontend_hvlog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

list(APPEND KFX_OBJECT_LIBS_STD kfx_frontend)
set(KFX_OBJECT_LIBS_STD ${KFX_OBJECT_LIBS_STD} PARENT_SCOPE)
list(APPEND KFX_OBJECT_LIBS_HVLOG kfx_frontend_hvlog)
set(KFX_OBJECT_LIBS_HVLOG ${KFX_OBJECT_LIBS_HVLOG} PARENT_SCOPE)
