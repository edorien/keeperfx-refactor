# docs/refactor/stage-06a-ariadne-pathfinding-interface.md: kfx_pathfinding
# is the ariadne wallhug/pathfinding code extracted out of src/kfx_sim,
# mirroring src/kfx_config/CMakeLists.txt's approach exactly (see the
# comment there).

file(GLOB KFX_PATHFINDING_SOURCES_C "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB KFX_PATHFINDING_SOURCES_CXX "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
set(KFX_PATHFINDING_SOURCES ${KFX_PATHFINDING_SOURCES_C} ${KFX_PATHFINDING_SOURCES_CXX})

add_library(kfx_pathfinding OBJECT ${KFX_PATHFINDING_SOURCES})
target_link_libraries(kfx_pathfinding PRIVATE kfx_common_opts kfx_bfdebug_std)
target_include_directories(kfx_pathfinding PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(kfx_pathfinding_hvlog OBJECT ${KFX_PATHFINDING_SOURCES})
target_link_libraries(kfx_pathfinding_hvlog PRIVATE kfx_common_opts kfx_bfdebug_hvlog)
target_include_directories(kfx_pathfinding_hvlog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

list(APPEND KFX_OBJECT_LIBS_STD kfx_pathfinding)
set(KFX_OBJECT_LIBS_STD ${KFX_OBJECT_LIBS_STD} PARENT_SCOPE)
list(APPEND KFX_OBJECT_LIBS_HVLOG kfx_pathfinding_hvlog)
set(KFX_OBJECT_LIBS_HVLOG ${KFX_OBJECT_LIBS_HVLOG} PARENT_SCOPE)
