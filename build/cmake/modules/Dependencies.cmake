# Dependencies.cmake - third-party libraries, per platform. Mirrors the hand
# Makefiles: Windows/MinGW uses prebuilt kfx-deps mingw32 static libs + SDL3 dev
# tarballs (../Makefile); Linux uses system/pkg-config libs + a few prebuilt
# lin64 static libs (../linux.mk). Dep URLs/tags track those Makefiles.
#
# Targets are defined here; kfx_link_dependencies(<target>) links them onto the
# game executables (called from BuildTargets, once they exist).

set(KFX_DEPS_BASE "https://github.com/dkfans/kfx-deps/releases/download")

# Downloaded deps live under the BUILD dir, not the source tree, so Windows and
# Linux builds in one checkout get their own deps and never collide.
set(D "${CMAKE_BINARY_DIR}/deps")
set(KFX_CENTITOML_SRC "${CMAKE_SOURCE_DIR}/deps/centitoml")

# kfx_fetch(<dir> <url>): download + extract into <builddir>/deps/<dir>/ once.
function(kfx_fetch dir url)
    set(_tgz "${D}/${dir}.tar.gz")
    set(_dest "${D}/${dir}")
    if(NOT EXISTS "${_dest}")
        file(MAKE_DIRECTORY "${_dest}")
        if(NOT EXISTS "${_tgz}")
            message(STATUS "Downloading dep: ${dir}  <-  ${url}")
            file(DOWNLOAD "${url}" "${_tgz}" SHOW_PROGRESS STATUS _st)
            list(GET _st 0 _code)
            if(NOT _code EQUAL 0)
                message(FATAL_ERROR "Failed to download ${url}: ${_st}")
            endif()
        endif()
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xzf "${_tgz}"
            WORKING_DIRECTORY "${_dest}"
            RESULT_VARIABLE _rc)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "Failed to extract ${_tgz}")
        endif()
    endif()
endfunction()

macro(kfx_imported name lib incdir)
    add_library(${name} STATIC IMPORTED GLOBAL)
    set_target_properties(${name} PROPERTIES
        IMPORTED_LOCATION "${lib}"
        INTERFACE_INCLUDE_DIRECTORIES "${incdir}")
endmacro()

if(WIN32)
    # --- SDL3 (prebuilt MinGW dev tarballs; each wraps <name>-<ver>/i686-w64-mingw32/*)
    # SDL_net is intentionally absent: api.c now uses a native Winsock socket
    # layer (SDL3_net is not reliably packaged). See docs SDL3-MIGRATION notes.
    set(SDL3_VER      3.4.12)
    set(SDL3_MIX_VER  3.2.4)
    set(SDL3_IMG_VER  3.4.4)

    kfx_fetch(sdl3       "https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_VER}/SDL3-devel-${SDL3_VER}-mingw.tar.gz")
    kfx_fetch(sdl3_mixer "https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL3_MIX_VER}/SDL3_mixer-devel-${SDL3_MIX_VER}-mingw.tar.gz")
    kfx_fetch(sdl3_image "https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL3_IMG_VER}/SDL3_image-devel-${SDL3_IMG_VER}-mingw.tar.gz")

    set(SDL3_PREFIX      "${D}/sdl3/SDL3-${SDL3_VER}/i686-w64-mingw32")
    set(SDL3_MIX_PREFIX  "${D}/sdl3_mixer/SDL3_mixer-${SDL3_MIX_VER}/i686-w64-mingw32")
    set(SDL3_IMG_PREFIX  "${D}/sdl3_image/SDL3_image-${SDL3_IMG_VER}/i686-w64-mingw32")

    add_library(kfx_sdl3 INTERFACE)
    # SDL3 headers live under include/SDL3/*.h; code uses <SDL3/SDL.h>,
    # <SDL3_mixer/SDL_mixer.h>, <SDL3_image/SDL_image.h>.
    target_include_directories(kfx_sdl3 INTERFACE
        "${SDL3_PREFIX}/include"
        "${SDL3_MIX_PREFIX}/include"
        "${SDL3_IMG_PREFIX}/include")
    target_link_libraries(kfx_sdl3 INTERFACE
        "${SDL3_PREFIX}/lib/libSDL3.dll.a"
        "${SDL3_MIX_PREFIX}/lib/libSDL3_mixer.dll.a"
        "${SDL3_IMG_PREFIX}/lib/libSDL3_image.dll.a")

    # SDL3 links dynamically, so ship its runtime DLLs (CPack picks these up).
    # COMPONENT runtime -- see Packaging.cmake's keeperfx install(TARGETS ...)
    # comment for why.
    install(FILES
        "${SDL3_PREFIX}/bin/SDL3.dll"
        "${SDL3_MIX_PREFIX}/bin/SDL3_mixer.dll"
        "${SDL3_IMG_PREFIX}/bin/SDL3_image.dll"
        DESTINATION . COMPONENT runtime)

    # --- Static libs from kfx-deps (mirror ../Makefile URLs/tags)
    kfx_fetch(enet6      "${KFX_DEPS_BASE}/20260212/enet6-mingw32.tar.gz")
    kfx_fetch(zlib       "${KFX_DEPS_BASE}/initial/zlib-mingw32.tar.gz")
    kfx_fetch(spng       "${KFX_DEPS_BASE}/initial/spng-mingw32.tar.gz")
    kfx_fetch(astronomy  "${KFX_DEPS_BASE}/astronomy_fix/astronomy-mingw32.tar.gz")
    kfx_fetch(centijson  "${KFX_DEPS_BASE}/initial/centijson-mingw32.tar.gz")
    kfx_fetch(ffmpeg     "${KFX_DEPS_BASE}/initial/ffmpeg-mingw32.tar.gz")
    kfx_fetch(openal     "${KFX_DEPS_BASE}/2024-11-14/openal-mingw32.tar.gz")
    kfx_fetch(luajit     "${KFX_DEPS_BASE}/20250418/luajit-mingw32.tar.gz")
    kfx_fetch(miniupnpc  "${KFX_DEPS_BASE}/20260102/miniupnpc-mingw32.tar.gz")
    kfx_fetch(libnatpmp  "${KFX_DEPS_BASE}/20260102/libnatpmp-mingw32.tar.gz")
    kfx_fetch(libcurl    "${KFX_DEPS_BASE}/20260310/libcurl-mingw32.tar.gz")

    kfx_imported(enet6_static      "${D}/enet6/lib/libenet6.a"       "${D}/enet6/include")
    target_link_libraries(enet6_static INTERFACE ws2_32 winmm)
    kfx_imported(spng_static       "${D}/spng/libspng.a"             "${D}/spng/include")
    kfx_imported(centijson_static  "${D}/centijson/libjson.a"        "${D}/centijson/include")
    kfx_imported(astronomy_static  "${D}/astronomy/libastronomy.a"   "${D}/astronomy/include")
    kfx_imported(zlib_static       "${D}/zlib/libz.a"                "${D}/zlib/include")
    kfx_imported(minizip_static    "${D}/zlib/libminizip.a"          "${D}/zlib/include")
    target_link_libraries(minizip_static INTERFACE zlib_static)
    kfx_imported(openal_static     "${D}/openal/libOpenAL32.a"       "${D}/openal/include")
    target_link_libraries(openal_static INTERFACE winmm ole32 uuid)
    kfx_imported(luajit_static     "${D}/luajit/lib/libluajit.a"     "${D}/luajit/include")
    kfx_imported(miniupnpc_static  "${D}/miniupnpc/libminiupnpc.a"   "${D}/miniupnpc/include")
    target_link_libraries(miniupnpc_static INTERFACE ws2_32 iphlpapi)
    kfx_imported(natpmp_static     "${D}/libnatpmp/libnatpmp.a"      "${D}/libnatpmp/include")
    target_link_libraries(natpmp_static INTERFACE ws2_32 iphlpapi)
    kfx_imported(curl_static       "${D}/libcurl/lib/libcurl.a"      "${D}/libcurl/include")
    target_compile_definitions(curl_static INTERFACE CURL_STATICLIB)
    target_link_libraries(curl_static INTERFACE zlib_static wldap32 crypt32 secur32 bcrypt ws2_32 iphlpapi)

    kfx_imported(libavcodec_static     "${D}/ffmpeg/libavcodec/libavcodec.a"         "${D}/ffmpeg")
    kfx_imported(libavformat_static    "${D}/ffmpeg/libavformat/libavformat.a"       "${D}/ffmpeg")
    kfx_imported(libavutil_static      "${D}/ffmpeg/libavutil/libavutil.a"           "${D}/ffmpeg")
    kfx_imported(libswresample_static  "${D}/ffmpeg/libswresample/libswresample.a"   "${D}/ffmpeg")

    add_library(centitoml OBJECT "${KFX_CENTITOML_SRC}/toml_api.c")
    target_link_libraries(centitoml PUBLIC centijson_static)
    target_include_directories(centitoml INTERFACE "${KFX_CENTITOML_SRC}")

else()
    find_package(PkgConfig REQUIRED)

    add_library(kfx_sdl3 INTERFACE)
    pkg_check_modules(SDL3       IMPORTED_TARGET sdl3)
    pkg_check_modules(SDL3_image IMPORTED_TARGET sdl3-image)
    if(NOT SDL3_image_FOUND)
        pkg_check_modules(SDL3_image IMPORTED_TARGET SDL3_image)
    endif()
    pkg_check_modules(SDL3_mixer IMPORTED_TARGET sdl3-mixer)
    if(NOT SDL3_mixer_FOUND)
        pkg_check_modules(SDL3_mixer IMPORTED_TARGET SDL3_mixer)
    endif()
    if(SDL3_FOUND AND SDL3_image_FOUND AND SDL3_mixer_FOUND)
        message(STATUS "SDL3: using system libraries (pkg-config)")
        target_link_libraries(kfx_sdl3 INTERFACE
            PkgConfig::SDL3 PkgConfig::SDL3_image PkgConfig::SDL3_mixer)
    else()
        message(STATUS "SDL3: system libraries not found; building from source (FetchContent)")
        include(FetchContent)
        set(SDL3_VER      3.4.12)
        set(SDL3_MIX_VER  3.2.4)
        set(SDL3_IMG_VER  3.4.4)
        # Shared libs, no tests/examples. Use system decoder libraries rather than
        # vendored ones: the release source tarballs do not bundle the external/
        # decoder submodules, so VENDORED would fail. Distros that hit this path
        # need libpng + the ogg/vorbis/flac/mpg123 -dev packages (see CI).
        set(SDL_TEST_LIBRARY   OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES       OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_SAMPLES   OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_VENDORED  OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_SAMPLES   OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_VENDORED  OFF CACHE BOOL "" FORCE)
        # Wayland-only: most distros have dropped X11 by default, and building
        # the X11 backend in pulls a chain of X11 dev headers (Xcursor, Xrandr,
        # Xfixes, ...) for a session type fewer and fewer users actually run.
        # SDL3 still auto-detects Wayland vs X11 at runtime; this only removes
        # the X11 backend from the build, so a build here can no longer fall
        # back to XWayland/X11 sessions.
        set(SDL_X11            OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(SDL3
            URL "https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_VER}/SDL3-${SDL3_VER}.tar.gz")
        FetchContent_Declare(SDL3_image
            URL "https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL3_IMG_VER}/SDL3_image-${SDL3_IMG_VER}.tar.gz")
        FetchContent_Declare(SDL3_mixer
            URL "https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL3_MIX_VER}/SDL3_mixer-${SDL3_MIX_VER}.tar.gz")
        FetchContent_MakeAvailable(SDL3 SDL3_image SDL3_mixer)
        target_link_libraries(kfx_sdl3 INTERFACE
            SDL3::SDL3 SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer)

        # Built from source, so not on the system's library path -- ship the
        # .so files next to the installed binary (mirrors the WIN32 branch's
        # SDL3*.dll install(FILES ...) above). Matches keeperfx/
        # keeperfx_hvlog's INSTALL_RPATH "$ORIGIN" (CMakeLists.txt).
        #
        # install(CODE ...) + a glob at install time (not install(DIRECTORY
        # ... FILES_MATCHING)): the .so files don't exist yet at configure
        # time (built later, during `cmake --build`), and install(DIRECTORY
        # ... FILES_MATCHING) recreates every subdirectory it recurses
        # through even when nothing inside matches -- these binary dirs also
        # hold CMakeFiles/, generated headers, docs, wayland protocol XML,
        # etc., which then showed up as empty clutter alongside the libs.
        # Only the top-level *.so* files are the actual runtime libraries.
        install(CODE "
            file(GLOB _kfx_sdl3_runtime_libs
                \"${sdl3_BINARY_DIR}/*.so*\"
                \"${sdl3_image_BINARY_DIR}/*.so*\"
                \"${sdl3_mixer_BINARY_DIR}/*.so*\")
            file(INSTALL \${_kfx_sdl3_runtime_libs} DESTINATION \"\${CMAKE_INSTALL_PREFIX}\")
        " COMPONENT runtime)
    endif()

    # ffmpeg: always built from source here, unlike openal/spng/miniupnpc/
    # natpmp below -- a system ffmpeg is the problem, present or not. Distro
    # ffmpeg packages are built with every optional codec/protocol/font-
    # rendering feature enabled, dynamically linking 100+ transitive shared
    # libraries (X11, cairo, pango, fontconfig, Kerberos, video codecs
    # nothing here uses, ...), none of which build-package.sh's dist/
    # install bundles -- making a keeperfx linked against system ffmpeg
    # non-portable to any machine that doesn't happen to already have a
    # closely-compatible ffmpeg (and its own huge dependency chain)
    # installed. bflib_fmvids.cpp (the only ffmpeg caller, see its
    # #includes) only ever plays KeeperFX's own bundled .smk (Smacker)
    # cutscenes via avformat/avcodec/avutil/swresample -- no swscale, no
    # other container/codec -- so a minimal --disable-everything static
    # build covers the actual need with zero runtime library dependencies
    # of its own, fully solving the portability gap rather than just
    # matching what upstream's Makefile happened to do.
    #
    # ExternalProject_Add, not FetchContent: ffmpeg's build is its own
    # hand-written ./configure + make, not CMake, so there's no
    # add_subdirectory() to pull in the way SDL3/openal/spng do.
    include(ExternalProject)
    include(ProcessorCount)
    ProcessorCount(KFX_FFMPEG_NPROC)
    if(KFX_FFMPEG_NPROC EQUAL 0)
        set(KFX_FFMPEG_NPROC 4)
    endif()
    set(FFMPEG_INSTALL_DIR "${CMAKE_BINARY_DIR}/_deps/ffmpeg-install")
    file(MAKE_DIRECTORY "${FFMPEG_INSTALL_DIR}/include")
    ExternalProject_Add(ffmpeg_build
        URL "https://ffmpeg.org/releases/ffmpeg-7.1.tar.xz"
        PREFIX "${CMAKE_BINARY_DIR}/_deps/ffmpeg"
        CONFIGURE_COMMAND <SOURCE_DIR>/configure
            --prefix=${FFMPEG_INSTALL_DIR}
            --disable-shared --enable-static --enable-pic
            --disable-programs --disable-doc --disable-avdevice --disable-postproc
            --disable-network --disable-x86asm
            --disable-avfilter --disable-swscale
            --disable-everything --disable-autodetect
            --enable-avformat --enable-avcodec --enable-avutil --enable-swresample
            --enable-demuxer=smacker
            --enable-decoder=smacker,smackaud
            --enable-protocol=file
        BUILD_COMMAND make -j${KFX_FFMPEG_NPROC}
        INSTALL_COMMAND make install
        BUILD_BYPRODUCTS
            "${FFMPEG_INSTALL_DIR}/lib/libavformat.a"
            "${FFMPEG_INSTALL_DIR}/lib/libavcodec.a"
            "${FFMPEG_INSTALL_DIR}/lib/libavutil.a"
            "${FFMPEG_INSTALL_DIR}/lib/libswresample.a"
    )
    foreach(_lib avformat avcodec avutil swresample)
        add_library(kfx_ffmpeg_${_lib} STATIC IMPORTED GLOBAL)
        set_target_properties(kfx_ffmpeg_${_lib} PROPERTIES
            IMPORTED_LOCATION "${FFMPEG_INSTALL_DIR}/lib/lib${_lib}.a"
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INSTALL_DIR}/include")
        add_dependencies(kfx_ffmpeg_${_lib} ffmpeg_build)
    endforeach()
    # avformat/avcodec/avutil/swresample have circular internal refs (same
    # reasoning as the Windows static-archive RESCAN group in
    # kfx_link_dependencies() below) -- link as a group so ld.bfd's
    # single-pass symbol resolution doesn't matter.
    add_library(kfx_ffmpeg INTERFACE)
    target_link_libraries(kfx_ffmpeg INTERFACE
        "$<LINK_GROUP:RESCAN,kfx_ffmpeg_avformat,kfx_ffmpeg_avcodec,kfx_ffmpeg_swresample,kfx_ffmpeg_avutil>"
        m pthread)

    pkg_check_modules(ZLIB       REQUIRED IMPORTED_TARGET zlib)

    # --- openal / luajit / spng / minizip / miniupnpc / natpmp: system
    # pkg-config first, else build from source (FetchContent), same
    # two-tier shape as SDL3 above. Each gets its own kfx_<name> INTERFACE
    # target so kfx_link_dependencies() below doesn't care which path was
    # taken. These previously were plain `pkg_check_modules(... REQUIRED
    # ...)` with no fallback (miniupnpc/natpmp had no check at all -- bare
    # linker names, silently assuming the system already had them), despite
    # the stage-13-era claim that this file "already covers native Linux
    # builds itself" -- it only did for SDL3. Fixed here so `KFX_OS=linux
    # ./build-cmake.sh` doesn't require ~20 apt `-dev` packages that aren't
    # actually needed to produce a working binary.

    # openal: OpenAL-soft has a normal CMake build.
    pkg_check_modules(OPENAL IMPORTED_TARGET openal)
    add_library(kfx_openal INTERFACE)
    if(OPENAL_FOUND)
        message(STATUS "openal: using system library (pkg-config)")
        target_link_libraries(kfx_openal INTERFACE PkgConfig::OPENAL)
    else()
        message(STATUS "openal: system library not found; building from source (FetchContent)")
        set(LIBTYPE        STATIC CACHE STRING "" FORCE)
        set(ALSOFT_UTILS    OFF CACHE BOOL "" FORCE)
        set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(ALSOFT_TESTS    OFF CACHE BOOL "" FORCE)
        set(ALSOFT_INSTALL  OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(openal_soft
            URL "https://github.com/kcat/openal-soft/releases/download/1.24.3/openal-soft-1.24.3.tar.bz2")
        FetchContent_MakeAvailable(openal_soft)
        target_link_libraries(kfx_openal INTERFACE OpenAL::OpenAL)
    endif()

    # spng: libspng has a normal CMake build.
    pkg_check_modules(SPNG IMPORTED_TARGET spng)
    add_library(kfx_spng INTERFACE)
    if(SPNG_FOUND)
        message(STATUS "spng: using system library (pkg-config)")
        target_link_libraries(kfx_spng INTERFACE PkgConfig::SPNG)
    else()
        message(STATUS "spng: system library not found; building from source (FetchContent)")
        set(SPNG_SHARED    OFF CACHE BOOL "" FORCE)
        set(SPNG_STATIC    ON  CACHE BOOL "" FORCE)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(spng
            URL "https://github.com/randy408/libspng/archive/refs/tags/v0.7.4.tar.gz")
        FetchContent_MakeAvailable(spng)
        target_link_libraries(kfx_spng INTERFACE spng_static)
    endif()

    # minizip: the classic zlib contrib/minizip API (<minizip/unzip.h>,
    # unzOpen/unzGoToFilePos64/... -- matches what custom_zip.c/
    # custom_sprites.c actually call and how Debian's libminizip-dev lays
    # out headers), not minizip-ng's different API. No CMake build of its
    # own upstream (it's two files inside zlib's source tree) -- compile
    # them directly against the system zlib already found above. Nothing
    # in this codebase writes zips, so zip.c is intentionally omitted.
    pkg_check_modules(MINIZIP IMPORTED_TARGET minizip)
    add_library(kfx_minizip INTERFACE)
    if(MINIZIP_FOUND)
        message(STATUS "minizip: using system library (pkg-config)")
        target_link_libraries(kfx_minizip INTERFACE PkgConfig::MINIZIP)
    else()
        message(STATUS "minizip: system library not found; building contrib/minizip from zlib source")
        FetchContent_Declare(zlib_minizip_src
            URL "https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz")
        FetchContent_Populate(zlib_minizip_src)
        add_library(minizip_from_source STATIC
            "${zlib_minizip_src_SOURCE_DIR}/contrib/minizip/ioapi.c"
            "${zlib_minizip_src_SOURCE_DIR}/contrib/minizip/unzip.c")
        target_link_libraries(minizip_from_source PUBLIC PkgConfig::ZLIB)
        # <minizip/unzip.h> needs a directory literally named "minizip" on
        # the include path -- contrib/minizip's own folder already has that
        # name, its parent just isn't laid out like an install prefix.
        # Symlink one instead of copying (same trick used for miniupnpc
        # below).
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/minizip_shim")
        file(CREATE_LINK "${zlib_minizip_src_SOURCE_DIR}/contrib/minizip" "${CMAKE_BINARY_DIR}/minizip_shim/minizip" SYMBOLIC)
        target_include_directories(kfx_minizip INTERFACE "${CMAKE_BINARY_DIR}/minizip_shim")
        target_link_libraries(kfx_minizip INTERFACE minizip_from_source)
    endif()

    # luajit: no CMake build upstream at all -- it's a hand-written
    # Makefile (bootstraps its own minilua/buildvm, generates lj_bcdef.h
    # etc.). Shell out to it via ExternalProject rather than reimplementing
    # any of that; import the resulting static lib afterward. `amalg`
    # (amalgamated build) keeps this to one translation unit.
    pkg_check_modules(LUAJIT IMPORTED_TARGET luajit)
    add_library(kfx_luajit INTERFACE)
    if(LUAJIT_FOUND)
        message(STATUS "luajit: using system library (pkg-config)")
        target_link_libraries(kfx_luajit INTERFACE PkgConfig::LUAJIT)
    else()
        message(STATUS "luajit: system library not found; building from source (its own Makefile, via ExternalProject)")
        include(ExternalProject)
        FetchContent_Declare(luajit_src
            URL "https://github.com/openresty/luajit2/archive/refs/tags/v2.1-20260724.tar.gz")
        FetchContent_Populate(luajit_src)
        ExternalProject_Add(luajit_build
            SOURCE_DIR        "${luajit_src_SOURCE_DIR}"
            CONFIGURE_COMMAND ""
            BUILD_IN_SOURCE   1
            BUILD_COMMAND     make -j${CMAKE_BUILD_PARALLEL_LEVEL} amalg CC=${CMAKE_C_COMPILER} BUILDMODE=static
            INSTALL_COMMAND   ""
            BUILD_BYPRODUCTS  "${luajit_src_SOURCE_DIR}/src/libluajit.a")
        add_library(luajit_from_source STATIC IMPORTED GLOBAL)
        set_target_properties(luajit_from_source PROPERTIES
            IMPORTED_LOCATION             "${luajit_src_SOURCE_DIR}/src/libluajit.a"
            INTERFACE_INCLUDE_DIRECTORIES "${luajit_src_SOURCE_DIR}/src"
            INTERFACE_LINK_LIBRARIES      "m;dl")
        add_dependencies(luajit_from_source luajit_build)
        target_link_libraries(kfx_luajit INTERFACE luajit_from_source)
    endif()

    # miniupnpc: part of the miniupnp monorepo; its miniupnpc/ subdir has
    # its own CMakeLists.txt (SOURCE_SUBDIR points straight at it).
    pkg_check_modules(MINIUPNPC IMPORTED_TARGET miniupnpc)
    add_library(kfx_miniupnpc INTERFACE)
    if(MINIUPNPC_FOUND)
        message(STATUS "miniupnpc: using system library (pkg-config)")
        target_link_libraries(kfx_miniupnpc INTERFACE PkgConfig::MINIUPNPC)
    else()
        message(STATUS "miniupnpc: system library not found; building from source (FetchContent)")
        set(UPNPC_BUILD_SHARED OFF CACHE BOOL "" FORCE)
        set(UPNPC_BUILD_STATIC ON  CACHE BOOL "" FORCE)
        set(UPNPC_BUILD_TESTS  OFF CACHE BOOL "" FORCE)
        set(UPNPC_BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(miniupnp_src
            URL "https://github.com/miniupnp/miniupnp/archive/refs/tags/miniupnpc_2_3_3.tar.gz"
            SOURCE_SUBDIR miniupnpc)
        FetchContent_MakeAvailable(miniupnp_src)
        # <miniupnpc/miniupnpc.h> needs a directory literally named
        # "miniupnpc" on the include path; upstream's own generated
        # include/ dir (a flat copy for its in-tree consumers) isn't named
        # that -- symlink one.
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/miniupnpc_shim")
        file(CREATE_LINK "${miniupnp_src_SOURCE_DIR}/miniupnpc/include" "${CMAKE_BINARY_DIR}/miniupnpc_shim/miniupnpc" SYMBOLIC)
        target_include_directories(kfx_miniupnpc INTERFACE "${CMAKE_BINARY_DIR}/miniupnpc_shim")
        target_link_libraries(kfx_miniupnpc INTERFACE libminiupnpc-static)
    endif()

    # libnatpmp: untagged upstream (no releases) -- pinned to a commit
    # instead of a tag for reproducibility. Has its own CMakeLists.txt
    # (target `natpmp`); EXCLUDE_FROM_ALL skips building its two sample
    # CLI tools since nothing here needs them.
    pkg_check_modules(NATPMP IMPORTED_TARGET libnatpmp)
    if(NOT NATPMP_FOUND)
        pkg_check_modules(NATPMP IMPORTED_TARGET natpmp)
    endif()
    add_library(kfx_natpmp INTERFACE)
    if(NATPMP_FOUND)
        message(STATUS "natpmp: using system library (pkg-config)")
        target_link_libraries(kfx_natpmp INTERFACE PkgConfig::NATPMP)
    else()
        message(STATUS "natpmp: system library not found; building from source (FetchContent)")
        FetchContent_Declare(natpmp_src
            URL "https://github.com/miniupnp/libnatpmp/archive/134fc89e2781e154e40042641f4d8bcbe42579f1.tar.gz")
        FetchContent_GetProperties(natpmp_src)
        if(NOT natpmp_src_POPULATED)
            FetchContent_Populate(natpmp_src)
            # Its own CMakeLists.txt does `option(BUILD_SHARED_LIBS ... OFF)`,
            # which is a no-op once some earlier FetchContent'd project (SDL3)
            # has already cached BUILD_SHARED_LIBS ON -- force it back off for
            # this one subdirectory so natpmp links in statically like every
            # other kfx_natpmp/kfx_* fallback here, not as a co-shipped .so.
            set(BUILD_SHARED_LIBS OFF)
            add_subdirectory("${natpmp_src_SOURCE_DIR}" "${natpmp_src_BINARY_DIR}" EXCLUDE_FROM_ALL)
        endif()
        target_link_libraries(kfx_natpmp INTERFACE natpmp)
    endif()

    # Not reliably packaged; use the prebuilt lin64 static libs (as linux.mk does).
    kfx_fetch(astronomy "${KFX_DEPS_BASE}/20250418/astronomy-lin64.tar.gz")
    kfx_fetch(centijson "${KFX_DEPS_BASE}/20250418/centijson-lin64.tar.gz")
    kfx_fetch(enet6     "${KFX_DEPS_BASE}/20260213/enet6-lin64.tar.gz")
    kfx_fetch(libcurl   "${KFX_DEPS_BASE}/20260310/libcurl-lin64.tar.gz")

    kfx_imported(astronomy_static "${D}/astronomy/libastronomy.a" "${D}/astronomy/include")
    kfx_imported(centijson_static "${D}/centijson/libjson.a"      "${D}/centijson/include")
    kfx_imported(enet6_static     "${D}/enet6/libenet6.a"         "${D}/enet6/include")
    kfx_imported(curl_static      "${D}/libcurl/lib/libcurl.a"    "${D}/libcurl/include")
    target_link_libraries(curl_static INTERFACE ssl crypto zstd)

    add_library(centitoml OBJECT "${KFX_CENTITOML_SRC}/toml_api.c")
    target_link_libraries(centitoml PUBLIC centijson_static)
    target_include_directories(centitoml INTERFACE "${KFX_CENTITOML_SRC}")
endif()

# --- Dear ImGui (docs/refactor/renderer/04-imgui-gui-foundation.md, Phase A)
# Vendored at deps/imgui/ (upstream ocornut/imgui v1.92.7, MIT) -- an in-tree
# source dep like deps/centitoml above, not a fetched binary; see that
# directory's git history for how it was pulled in. Only the two SDL3
# backends are compiled (imgui_impl_sdl3 + imgui_impl_sdlrenderer3) since
# kfx_platform's window/renderer are exactly those types (RendererSoftware.h
# / WindowSystemSDL.h) -- see §3.1/§3.2 of the plan doc. imgui_demo.cpp is
# included too (Phase A's imgui_demo proof, §7); it costs nothing in a
# release build if ImGui::ShowDemoWindow() is never called.
#
# One OBJECT library shared between the std/hvlog kfx_platform variants --
# kfx_common_opts is a single INTERFACE target linked into both (see
# centitoml just above for the identical reasoning): ImGui doesn't touch
# BFDEBUG_LEVEL or any bflib header, so nothing differs between the two
# builds and compiling it twice would be pure waste.
set(KFX_IMGUI_SRC "${CMAKE_SOURCE_DIR}/deps/imgui")
add_library(imgui OBJECT
    "${KFX_IMGUI_SRC}/imgui.cpp"
    "${KFX_IMGUI_SRC}/imgui_draw.cpp"
    "${KFX_IMGUI_SRC}/imgui_tables.cpp"
    "${KFX_IMGUI_SRC}/imgui_widgets.cpp"
    "${KFX_IMGUI_SRC}/imgui_demo.cpp"
    "${KFX_IMGUI_SRC}/backends/imgui_impl_sdl3.cpp"
    "${KFX_IMGUI_SRC}/backends/imgui_impl_sdlrenderer3.cpp")
target_include_directories(imgui PUBLIC "${KFX_IMGUI_SRC}" "${KFX_IMGUI_SRC}/backends")
target_link_libraries(imgui PUBLIC kfx_sdl3)

# Link every dependency onto TARGET.
function(kfx_link_dependencies TARGET)
    if(WIN32)
        # Static archives have circular refs (curl<->zlib, ffmpeg internals), so
        # link them in a group (RESCAN == --start-group/--end-group).
        set(_static
            libavformat_static libavcodec_static libswresample_static libavutil_static
            openal_static astronomy_static enet6_static miniupnpc_static natpmp_static
            curl_static spng_static centijson_static minizip_static zlib_static
            luajit_static)
        target_link_libraries(${TARGET} PRIVATE
            kfx_sdl3 "$<LINK_GROUP:RESCAN,${_static}>" centitoml)
    else()
        target_link_libraries(${TARGET} PRIVATE
            kfx_sdl3
            kfx_ffmpeg kfx_openal kfx_luajit
            kfx_spng kfx_minizip PkgConfig::ZLIB
            astronomy_static centijson_static enet6_static curl_static
            centitoml
            kfx_miniupnpc kfx_natpmp dl)
    endif()
endfunction()
