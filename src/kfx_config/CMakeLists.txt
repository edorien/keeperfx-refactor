# Stage 4 of docs/refactor/: kfx_config is the second library physically
# extracted out of src/ (see docs/refactor/stage-04-kfx-config.md), mirroring
# src/kfx_platform/CMakeLists.txt's approach exactly (see the comment
# there). Unlike kfx_platform, none of its files are platform-exclusive, so
# there's no exclusion step here.

file(GLOB KFX_CONFIG_SOURCES_C "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB KFX_CONFIG_SOURCES_CXX "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
set(KFX_CONFIG_SOURCES ${KFX_CONFIG_SOURCES_C} ${KFX_CONFIG_SOURCES_CXX})

add_library(kfx_config OBJECT ${KFX_CONFIG_SOURCES})
target_link_libraries(kfx_config PRIVATE kfx_common_opts kfx_bfdebug_std)
target_include_directories(kfx_config PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(kfx_config_hvlog OBJECT ${KFX_CONFIG_SOURCES})
target_link_libraries(kfx_config_hvlog PRIVATE kfx_common_opts kfx_bfdebug_hvlog)
target_include_directories(kfx_config_hvlog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

list(APPEND KFX_OBJECT_LIBS_STD kfx_config)
set(KFX_OBJECT_LIBS_STD ${KFX_OBJECT_LIBS_STD} PARENT_SCOPE)
list(APPEND KFX_OBJECT_LIBS_HVLOG kfx_config_hvlog)
set(KFX_OBJECT_LIBS_HVLOG ${KFX_OBJECT_LIBS_HVLOG} PARENT_SCOPE)

# docs/refactor/testing/stage-01-framework-and-scaffold.md: opt-in Catch2
# unit tests, gated the same way as the FetchContent(Catch2) block itself.
if(KFX_BUILD_TESTS)
    add_subdirectory(tests)
endif()
