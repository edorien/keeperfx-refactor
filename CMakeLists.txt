cmake_minimum_required(VERSION 3.20)

# Do not allow building in root
if (CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "Do not build in the root. Create a bin directory and remove ${CMAKE_SOURCE_DIR}/CMakeCache.txt")
endif ()

project(keeperfx C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" describe --always OUTPUT_VARIABLE COMMIT_ID OUTPUT_STRIP_TRAILING_WHITESPACE)

file(STRINGS "${CMAKE_SOURCE_DIR}/build/make/version.mk" _kfx_version_lines)
foreach(_line IN LISTS _kfx_version_lines)
    if(_line MATCHES "^(VER_MAJOR|VER_MINOR|VER_RELEASE|VER_BUILD)=([0-9]+)")
        set(${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
    endif()
endforeach()
foreach(_v VER_MAJOR VER_MINOR VER_RELEASE VER_BUILD)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "version.mk is missing ${_v}")
    endif()
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/build/make/version.mk")

if(NOT DEFINED BUILD_NUMBER)
    set(BUILD_NUMBER "${VER_BUILD}")
endif()
set(PACKAGE_SUFFIX ""  CACHE STRING "Distribution suffix (e.g. Alpha, Release)")
set(VER_STRING     "${VER_MAJOR}.${VER_MINOR}.${VER_RELEASE}.${BUILD_NUMBER} ${PACKAGE_SUFFIX}")
set(GIT_REVISION   "${COMMIT_ID}")

set(KEEPERFX_VER_DEFS_H_IN ${CMAKE_SOURCE_DIR}/ver_defs.h.in)
set(KEEPERFX_VER_DEFS_H_OUT ${CMAKE_SOURCE_DIR}/ver_defs.h)
configure_file(${KEEPERFX_VER_DEFS_H_IN} ${KEEPERFX_VER_DEFS_H_OUT})

# build/cmake/modules/: brought in from upstream's SDL3 migration (merge of
# origin/master's SDL2->SDL3 work, see git log). Dependencies.cmake defines
# the kfx_sdl3 INTERFACE target (SDL3/SDL3_image/SDL3_mixer, prebuilt on
# Windows, pkg-config-or-FetchContent on Linux) plus every other external
# dependency (curl, luajit, ffmpeg, openal, ...) and the kfx_link_dependencies()
# function used below. BuildTargets.cmake (the rest of that module set) is
# NOT included here -- it assumes upstream's flat src/*.c(pp) GLOB_RECURSE
# single-target build, which this repo's src/kfx_*/ OBJECT-library split
# (below) replaces; only the dependency-acquisition half of upstream's
# module set applies here.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/build/cmake/modules)
include(Helpers)
include(Platforms)
include(Dependencies)

# Non-recursive: main.cpp is the only source file directly under src/ --
# every kfx_* library now lives in its own src/kfx_*/ subdirectory
# (unified alongside main.cpp/ftests/ rather than a separate top-level
# libs/ tree), and each owns its own file(GLOB ...) in its own
# CMakeLists.txt (add_subdirectory()'d below). A recursive glob here
# would double-pick-up every one of those files into the executable
# targets directly, alongside their real OBJECT-library compilation --
# silently duplicating every translation unit and failing at link time
# with "multiple definition" for every single symbol.
file(GLOB KEEPERFX_SOURCES_C "src/*.c")
file(GLOB KEEPERFX_SOURCES_CXX "src/*.cpp")
# src/ftests/ is the one subdirectory under src/ that isn't a kfx_*
# library (functional-test scaffolding, deliberately kept out of any
# library) -- it needs its own scoped recursive glob since it nests a
# tests/ subdirectory of its own.
file(GLOB_RECURSE KEEPERFX_FTESTS_C "src/ftests/*.c")
file(GLOB_RECURSE KEEPERFX_FTESTS_CXX "src/ftests/*.cpp")
list(APPEND KEEPERFX_SOURCES_C ${KEEPERFX_FTESTS_C})
list(APPEND KEEPERFX_SOURCES_CXX ${KEEPERFX_FTESTS_CXX})
# The platform-exclusive OS-integration files (windows.cpp/steam_api.cpp/
# cdrom.cpp/linux.cpp) moved to src/kfx_platform/src/ in stage 3 -- see
# src/kfx_platform/CMakeLists.txt for their exclusion logic now. The new
# src/kfx_platform/src/kfx/platform/ classes from upstream's SDL3
# migration moved there too.

message(STATUS "We are using ${CMAKE_CXX_COMPILER_ID}")

# ---------------------------------------------------------------------------
# docs/refactor/ split src/ into ~9 kfx_* libraries by ownership (see
# docs/refactor/stage-01-object-libraries.md for how this started, as an
# OBJECT-library grouping proven against real files before anything
# physically moved). Every kfx_* library is now its own physically-extracted
# directory under src/ (alongside main.cpp/ftests/, not a separate top-level
# libs/ tree), each with a CMakeLists.txt that globs its own
# sources directly -- there's no per-file stem list left in this file to
# keep in sync with anything. What's left in KFX_SOURCES_REMAINING below is
# exactly src/main.cpp and the src/ftests/*.c functional-test scaffolding,
# both deliberately never migrated into a library.
#
# keeperfx and keeperfx_hvlog need genuinely different BFDEBUG_LEVEL
# compiled into nearly every translation unit (globals.h's SYNCDBG/
# WARNDBG/etc macros key off it, and globals.h is included almost
# everywhere) -- not just linked differently. OBJECT library sources are
# compiled once according to that library's own settings, so sharing one
# set of kfx_* libraries between both executables would silently drop
# keeperfx_hvlog's enhanced debug logging across the whole codebase. Each
# library is therefore built twice, once per BFDEBUG_LEVEL -- exactly the
# same total compile work the old single GLOB_RECURSE approach already
# did (every source file was already compiled twice, once per
# add_executable call).
# ---------------------------------------------------------------------------

# Kept identical to Makefile's WARNFLAGS (line 448) -- this list had drifted
# out of sync (missing -Wno-format-truncation/-Wno-maybe-uninitialized),
# which only surfaces as build failures on GCC 13, i.e. exactly the
# compiler CI's g++-mingw-w64-i686 install actually provides.
set(WARNFLAGS -Wall -W -Wshadow -Wno-sign-compare -Wno-unused-parameter -Wno-maybe-uninitialized -Wno-strict-aliasing -Wno-unknown-pragmas -Wno-format-truncation -Werror)
set(GNU_COMPILER_FLAG -march=x86-64 -fno-omit-frame-pointer -fmessage-length=0)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wimplicit")

# Compile settings shared by every kfx_* library and by keeperfx/
# keeperfx_hvlog's own directly-owned sources (KFX_SOURCES_REMAINING) --
# an INTERFACE library so there's exactly one place to change a flag,
# instead of ~20 target_compile_options calls drifting apart. Also carries
# every external dependency (SDL3 + the rest of Dependencies.cmake's
# targets) for the same reason: OBJECT library sources need these include
# directories to compile at all, not just at final-link time. Individual
# *_static/PkgConfig::* targets propagate their own
# INTERFACE_INCLUDE_DIRECTORIES/COMPILE_DEFINITIONS (e.g. curl_static's
# CURL_STATICLIB) through this same INTERFACE chain -- linking them here
# doesn't duplicate anything at the final executable link, since OBJECT
# libraries don't perform a real link step; kfx_link_dependencies() below
# is what actually links them onto keeperfx/keeperfx_hvlog.
add_library(kfx_common_opts INTERFACE)
target_compile_options(kfx_common_opts INTERFACE ${WARNFLAGS} ${GNU_COMPILER_FLAG})
target_link_libraries(kfx_common_opts INTERFACE kfx_sdl3)
# Individual kfx_* OBJECT libraries #include headers from these dependencies
# directly (lua.h, AL/al.h, curl/curl.h, toml.h, json.h, minizip/unzip.h,
# enet6/enet.h, astronomy.h, ...), not just the final keeperfx/keeperfx_hvlog
# executables -- so their INTERFACE_INCLUDE_DIRECTORIES need to reach every
# OBJECT library's own compilation via this same INTERFACE chain, mirroring
# kfx_link_dependencies() (Dependencies.cmake) but with INTERFACE instead of
# PRIVATE linkage since kfx_common_opts has no compiled sources of its own.
if(WIN32)
    target_link_libraries(kfx_common_opts INTERFACE
        libavformat_static libavcodec_static libswresample_static libavutil_static
        openal_static astronomy_static enet6_static miniupnpc_static natpmp_static
        curl_static spng_static centijson_static minizip_static zlib_static
        luajit_static)
else()
    target_link_libraries(kfx_common_opts INTERFACE
        kfx_ffmpeg kfx_openal kfx_luajit
        kfx_spng kfx_minizip PkgConfig::ZLIB
        astronomy_static centijson_static enet6_static curl_static
        kfx_miniupnpc kfx_natpmp)
endif()
target_link_libraries(kfx_common_opts INTERFACE centitoml)
# kfx_platform (stage 3) physically moved out of directly under src/ into
# its own include/ directory; every other kfx_* component and both
# executables still need to find its headers (e.g. #include
# "bflib_basics.h"), and src/kfx_platform/src/*.c* files still have a
# handful of acknowledged residual upward includes back into src/ (see
# docs/refactor/stage-02-decouple-bflib.md) that stage 2 didn't fully
# resolve -- so this is deliberately bidirectional rather than a one-way
# include dir on the kfx_platform target itself, which sibling OBJECT
# libraries (only linked together at the final executable, not to each
# other) wouldn't inherit.
target_include_directories(kfx_common_opts INTERFACE
    "${CMAKE_SOURCE_DIR}/src/kfx_platform/include"
    "${CMAKE_SOURCE_DIR}/src/kfx_config/include"
    "${CMAKE_SOURCE_DIR}/src/kfx_pathfinding/include"
    "${CMAKE_SOURCE_DIR}/src/kfx_sim/include"
    "${CMAKE_SOURCE_DIR}/src/kfx_render/include"
    "${CMAKE_SOURCE_DIR}/src/kfx_net/include"
    "${CMAKE_SOURCE_DIR}/src/kfx_game/include"
    "${CMAKE_SOURCE_DIR}/src/kfx_frontend/include"
    "${CMAKE_SOURCE_DIR}/src/kfx_script/include"
    "${CMAKE_SOURCE_DIR}/src/kfx_apploop/include"
    "${CMAKE_SOURCE_DIR}/src"
    # ver_defs.h is configure_file()'d to the repo root (see above), not
    # under src/ -- this was a pre-existing latent gap (predates the
    # libs/->src/ move) that a stray leftover src/ver_defs.h from the
    # Makefile build happened to mask until now.
    "${CMAKE_SOURCE_DIR}")
if(UNIX AND NOT APPLE)
    # Same suppression linux.mk already carries for this GCC/glibc combo
    # (not needed by the mingw-w64 cross-compiler, hence kept Linux-only):
    # abs(long) vs abs(int) call sites throughout ariadne/bflib_sound.
    target_compile_options(kfx_common_opts INTERFACE $<$<COMPILE_LANGUAGE:C>:-Wno-absolute-value>)
endif()

# The two BFDEBUG_LEVEL variants (see block comment above).
add_library(kfx_bfdebug_std INTERFACE)
target_compile_definitions(kfx_bfdebug_std INTERFACE BFDEBUG_LEVEL=0)
add_library(kfx_bfdebug_hvlog INTERFACE)
target_compile_definitions(kfx_bfdebug_hvlog INTERFACE BFDEBUG_LEVEL=10)

set(KFX_SOURCES_REMAINING ${KEEPERFX_SOURCES_C} ${KEEPERFX_SOURCES_CXX})
set(KFX_OBJECT_LIBS_STD "")
set(KFX_OBJECT_LIBS_HVLOG "")

# Every kfx_* component is now a real, physically-extracted library target
# -- see src/kfx_*/CMakeLists.txt -- so nothing here subtracts from
# KFX_SOURCES_REMAINING anymore; it stays exactly what the glob above found
# (main.cpp + src/ftests/*, both intentionally kept out of any library).
# Stage 1 through 11's kfx_add_component() function, which incrementally
# carved libraries out of KFX_SOURCES_REMAINING by regex-matching stem
# lists before their physical src/kfx_*/ directory existed, is gone now
# that every component has one -- each src/kfx_*/CMakeLists.txt appends
# itself to KFX_OBJECT_LIBS_STD/_HVLOG directly, so all of them must come
# before those lists are consumed below.
add_subdirectory(src/kfx_platform)
add_subdirectory(src/kfx_config)
add_subdirectory(src/kfx_pathfinding)
add_subdirectory(src/kfx_sim)
add_subdirectory(src/kfx_render)
add_subdirectory(src/kfx_net)
add_subdirectory(src/kfx_game)
add_subdirectory(src/kfx_frontend)
add_subdirectory(src/kfx_script)
add_subdirectory(src/kfx_apploop)

# Each src/kfx_*/CMakeLists.txt globs its own library directory directly --
# see docs/refactor/stage-0{6,7,8,9,10,11}-*.md for the per-stage history
# of how each one got there incrementally (kfx_sim/kfx_render one prefix
# cluster at a time, kfx_net/kfx_game/kfx_frontend the same but with their
# CMakeLists.txt created early to give their *_state.h/.c a home ahead of
# the first cluster, kfx_platform/kfx_config/kfx_script in a single pass).
# Physically unified under src/ (rather than a separate top-level libs/
# tree) after the SDL3 merge, so a single "everything the app builds from
# lives under src/" root matches upstream's own layout too.

message(STATUS "kfx: ${KFX_OBJECT_LIBS_STD} grouped; ${KFX_SOURCES_REMAINING} left ungrouped (app entry point + test scaffolding)")

# Add two executable targets: keeperfx and keeperfx_hvlog. Each owns
# whatever's left in KFX_SOURCES_REMAINING directly (main.cpp, ftests/*)
# and links in every kfx_* OBJECT library's matching BFDEBUG_LEVEL variant.
#
# Order matters: the default linker on both platforms (ld.bfd -- confirmed
# on both native Linux and mingw-w64) resolves static-library symbols in a
# single left-to-right pass. kfx_common_opts carries kfx_sdl3 -- if it's
# listed before the OBJECT libraries that need SDL3 symbols, the linker
# would already have moved past it by the time the .o files referencing
# them are reached. Object libraries needing symbols must come before the
# static/shared libraries providing them; kfx_link_dependencies() (called
# below, after the OBJECT libraries) carries the rest of Dependencies.cmake's
# targets for exactly this reason.
add_executable(keeperfx ${KFX_SOURCES_REMAINING})
target_link_libraries(keeperfx PRIVATE ${KFX_OBJECT_LIBS_STD} kfx_bfdebug_std kfx_common_opts)

add_executable(keeperfx_hvlog ${KFX_SOURCES_REMAINING})
target_link_libraries(keeperfx_hvlog PRIVATE ${KFX_OBJECT_LIBS_HVLOG} kfx_bfdebug_hvlog kfx_common_opts)

# kfx_link_dependencies() (Dependencies.cmake) links every external
# dependency other than SDL3 (already on kfx_common_opts above) onto the
# final executables -- curl/luajit/ffmpeg/openal/enet6/astronomy/spng/
# zlib/minizip/centijson/miniupnpc/libnatpmp, plus (Windows) the
# RESCAN-grouped static-archive linking their circular refs need.
kfx_link_dependencies(keeperfx)
kfx_link_dependencies(keeperfx_hvlog)

# centitoml (Dependencies.cmake) is itself an OBJECT library, wrapped
# inside kfx_sdl3/kfx_link_dependencies' INTERFACE indirection. Its
# compiled object doesn't reliably make it through two levels of
# INTERFACE-library indirection into the final link (its include
# directories propagate fine for compilation -- kfx_config's toml_parse()
# callers compile clean -- but the object file itself doesn't), so link it
# directly here too, the same way the pre-Stage-1 CMakeLists.txt always did.
target_link_libraries(keeperfx PRIVATE centitoml)
target_link_libraries(keeperfx_hvlog PRIVATE centitoml)

if(WIN32)
    target_sources(keeperfx PRIVATE "res/keeperfx_stdres.rc")
    target_sources(keeperfx_hvlog PRIVATE "res/keeperfx_stdres.rc")
endif()

if(WIN32)
    # The default bfd linker in MinGW is extremely slow. LLVM linker (LLD) is
    # much much faster -- but isn't guaranteed to be installed alongside
    # g++-mingw-w64-i686 (it isn't, in a plain apt install), so this is a
    # speed optimization, not a hard requirement.
    find_program(KFX_LLD_LINKER NAMES ld.lld i686-w64-mingw32-ld.lld)
    if(KFX_LLD_LINKER)
        set_property(TARGET keeperfx PROPERTY LINKER_TYPE LLD)
        set_property(TARGET keeperfx_hvlog PROPERTY LINKER_TYPE LLD)
    endif()

    set(GNU_LINK_FLAG -mwindows -Wl,--enable-auto-import)
    target_link_options(keeperfx PRIVATE ${GNU_LINK_FLAG} -Wl,-Map,keeperfx.map)
    target_link_options(keeperfx_hvlog PRIVATE ${GNU_LINK_FLAG} -Wl,-Map,keeperfx_hvlog.map)
    target_link_libraries (keeperfx PUBLIC -static stdc++ winpthread -dynamic)
    target_link_libraries (keeperfx_hvlog PUBLIC -static stdc++ winpthread -dynamic)

    # System libraries -- kept identical to and in the same relative
    # position as Makefile's LINKLIB tail (`-lwinmm -lmingw32 -limagehlp
    # -lws2_32 -ldbghelp -lbcrypt -lole32 -luuid`, line ~391). This has to
    # be listed *after* everything else that might need winsock symbols
    # (e.g. libcurl_static's recvfrom/gethostname): ld.bfd is single-pass,
    # and enet_static already links ws2_32 too, but only for enet's own
    # needs at the point it's processed -- libcurl's later, different
    # winsock symbol needs go unresolved unless ws2_32 is scanned again
    # after it. bcrypt is also linked by Dependencies.cmake's openal_static
    # block; relinking it here is harmless.
    target_link_libraries(keeperfx PRIVATE winmm mingw32 imagehlp ws2_32 dbghelp bcrypt ole32 uuid)
    target_link_libraries(keeperfx_hvlog PRIVATE winmm mingw32 imagehlp ws2_32 dbghelp bcrypt ole32 uuid)
elseif(UNIX AND NOT APPLE)
    target_link_options(keeperfx PRIVATE -rdynamic -Wl,-Map,keeperfx.map)
    target_link_options(keeperfx_hvlog PRIVATE -rdynamic -Wl,-Map,keeperfx_hvlog.map)
    target_link_libraries(keeperfx PRIVATE m dl)
    target_link_libraries(keeperfx_hvlog PRIVATE m dl)
    # Any SDL3-family libs built from source (Dependencies.cmake's Linux
    # fallback when the system doesn't have libsdl3-dev) get installed
    # alongside the binary -- see that file's install(DIRECTORY ...) calls.
    # Let the *installed* copy find them there instead of needing
    # LD_LIBRARY_PATH, without touching the build tree's own absolute RPATH
    # into _deps/ (CMake only applies INSTALL_RPATH at `cmake --install`
    # time; running straight out of the build dir is unaffected).
    set_target_properties(keeperfx keeperfx_hvlog PROPERTIES
        INSTALL_RPATH "$ORIGIN")
endif()

# tools/ builds a host-native icon-conversion utility (png2ico) used to
# generate the Windows resource icon; unrelated to keeperfx/keeperfx_hvlog
# linking, so its position here doesn't matter.
add_subdirectory(tools)

# CPack-based release packaging (build/cmake/modules/Packaging.cmake, from
# upstream's SDL3 migration) -- produces the same kind of release archive
# as `make package`, taking the keeperfx binary from this CMake build and
# the game data from `make pkg-assemble` (see Packaging.cmake's header
# comment for the exact command sequence). Replaces the old
# Makefile-passthrough `package` custom target, which built its own
# separate copies of the binaries via Make's own object directory instead
# of reusing this CMake build tree's keeperfx/keeperfx_hvlog -- CPack's
# install()-based approach also correctly bundles SDL3's runtime DLLs on
# Windows (see Dependencies.cmake's install(FILES ...) calls), which the
# old target had no equivalent for.
include(Packaging)

# Optional self-contained Linux/Steam Deck bundle (-DKFX_DECK_BUNDLE=ON).
include(DeckBundle)
