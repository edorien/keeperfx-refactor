// kfx_platform: bflib_fileio.c, the thin fopen/fread/fwrite/fseek wrapper
// every other library's file I/O goes through. Unlike the fixture-file
// pattern kfx_config's value_util_test.cpp uses (a checked-in, read-only
// TOML file plus a configure_file()-baked absolute path), these tests
// need a *writable* scratch file -- so they create/delete their own
// temp file directly in the test binary's CWD rather than adding a new
// CMake fixture mechanism, cleaning up in the fixture's constructor (in
// case a prior crashed run left it behind) and destructor alike.
// KFX_BUILD_TESTS is native-Linux-only (docs/Architecture/testing-
// harness.md §2), so a bare POSIX filename is fine here.
//
// Deliberately a bare filename with no directory component, not an
// absolute /tmp/... path: found by testing (a real REQUIRE(h != nullptr)
// failure, not a hunch) that LbFileOpen(..., Lb_FILE_MODE_NEW) is
// actually broken for any path starting with '/' -- create_directory_for_
// file() does `strchr(fname, '/')`, finds the *leading* slash first, and
// calls `mkdir("")` for that empty first component, which fails with
// ENOENT (confirmed directly, not just read) and aborts the whole open
// before fopen() is ever reached. Not fixed here (same "record, don't
// fix outside scope" discipline as docs/refactor/todo/two-remaining-
// layering-violations.md) -- worth a real bug report, since every
// production LbFileOpen(..., Lb_FILE_MODE_NEW) caller in this codebase
// happens to use relative paths, so this has stayed latent.
//
// create_directory_for_file()/LbFileMakeFullPath() and the case-
// insensitive-fallback path inside LbFileOpen/LbFileLength/LbFileDelete
// (find_case_insensitive_file(), Linux/non-Windows only) aren't attempted
// here beyond that: the former needs a multi-level throwaway directory
// tree, the latter would need two same-named-but-cased files on disk to
// actually exercise the fallback branch, both more fixture setup than
// the straight-line open/read/write/seek/eof/length/delete lifecycle
// below.
#include <catch2/catch_test_macros.hpp>

#include "bflib_fileio.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {
const char *kTestFile = "kfx_platform_utest_fileio_test.bin";

struct ScratchFile {
    ScratchFile() { std::remove(kTestFile); }
    ~ScratchFile() { std::remove(kTestFile); }
};
}

TEST_CASE_METHOD(ScratchFile, "LbFileExists is false for a file that hasn't been created", "[kfx_platform][bflib_fileio]") {
    CHECK(LbFileExists(kTestFile) == 0);
}

TEST_CASE_METHOD(ScratchFile, "LbFileOpen in READ_ONLY mode fails when the file doesn't exist", "[kfx_platform][bflib_fileio]") {
    CHECK(LbFileOpen(kTestFile, Lb_FILE_MODE_READ_ONLY) == nullptr);
}

TEST_CASE_METHOD(ScratchFile, "LbFileOpen in NEW mode creates the file, and a full write/read/seek/close round-trip works", "[kfx_platform][bflib_fileio]") {
    TbFileHandle h = LbFileOpen(kTestFile, Lb_FILE_MODE_NEW);
    REQUIRE(h != nullptr);
    const char *payload = "hello, kfx";
    long written = LbFileWrite(h, payload, std::strlen(payload));
    CHECK(written == (long)std::strlen(payload));
    CHECK(LbFileClose(h) == 1);

    CHECK(LbFileExists(kTestFile) != 0);
    CHECK(LbFileLength(kTestFile) == (long)std::strlen(payload));

    TbFileHandle rh = LbFileOpen(kTestFile, Lb_FILE_MODE_READ_ONLY);
    REQUIRE(rh != nullptr);
    char buffer[32] = {0};
    int read_bytes = LbFileRead(rh, buffer, sizeof(buffer) - 1);
    CHECK(read_bytes == (int)std::strlen(payload));
    CHECK(std::strcmp(buffer, payload) == 0);
    CHECK(LbFileClose(rh) == 1);
}

TEST_CASE_METHOD(ScratchFile, "LbFileSeek/LbFilePosition/LbFileEof track the file position through a read", "[kfx_platform][bflib_fileio]") {
    TbFileHandle h = LbFileOpen(kTestFile, Lb_FILE_MODE_NEW);
    REQUIRE(h != nullptr);
    LbFileWrite(h, "0123456789", 10);
    LbFileClose(h);

    TbFileHandle rh = LbFileOpen(kTestFile, Lb_FILE_MODE_READ_ONLY);
    REQUIRE(rh != nullptr);
    CHECK(LbFilePosition(rh) == 0);
    CHECK_FALSE(LbFileEof(rh));

    CHECK(LbFileSeek(rh, 5, Lb_FILE_SEEK_BEGINNING) == 0);
    CHECK(LbFilePosition(rh) == 5);

    char buffer[8] = {0};
    LbFileRead(rh, buffer, 5); // reads "56789", lands exactly at EOF
    CHECK(LbFileEof(rh));

    LbFileClose(rh);
}

TEST_CASE_METHOD(ScratchFile, "LbFileLengthHandle reports the file's total size without disturbing the current position", "[kfx_platform][bflib_fileio]") {
    TbFileHandle h = LbFileOpen(kTestFile, Lb_FILE_MODE_NEW);
    REQUIRE(h != nullptr);
    LbFileWrite(h, "0123456789", 10);
    LbFileSeek(h, 3, Lb_FILE_SEEK_BEGINNING);
    CHECK(LbFileLengthHandle(h) == 10);
    CHECK(LbFilePosition(h) == 3); // restored after probing the end
    LbFileClose(h);
}

TEST_CASE_METHOD(ScratchFile, "LbFileFlush succeeds on a valid handle", "[kfx_platform][bflib_fileio]") {
    TbFileHandle h = LbFileOpen(kTestFile, Lb_FILE_MODE_NEW);
    REQUIRE(h != nullptr);
    LbFileWrite(h, "x", 1);
    CHECK(LbFileFlush(h) != 0);
    LbFileClose(h);
}

TEST_CASE_METHOD(ScratchFile, "LbFileOpen in OLD mode opens an existing file for read+write", "[kfx_platform][bflib_fileio]") {
    TbFileHandle h = LbFileOpen(kTestFile, Lb_FILE_MODE_NEW);
    LbFileWrite(h, "abc", 3);
    LbFileClose(h);

    TbFileHandle oh = LbFileOpen(kTestFile, Lb_FILE_MODE_OLD);
    REQUIRE(oh != nullptr);
    LbFileSeek(oh, 0, Lb_FILE_SEEK_END);
    long written = LbFileWrite(oh, "def", 3);
    CHECK(written == 3);
    LbFileClose(oh);
    CHECK(LbFileLength(kTestFile) == 6);
}

TEST_CASE_METHOD(ScratchFile, "LbFileDelete removes an existing file and reports failure for a missing one", "[kfx_platform][bflib_fileio]") {
    TbFileHandle h = LbFileOpen(kTestFile, Lb_FILE_MODE_NEW);
    LbFileClose(h);
    CHECK(LbFileExists(kTestFile) != 0);
    CHECK(LbFileDelete(kTestFile) == 1);
    CHECK(LbFileExists(kTestFile) == 0);
    CHECK(LbFileDelete(kTestFile) == -1); // already gone
}

TEST_CASE("LbFileLength returns -1 for a nonexistent file", "[kfx_platform][bflib_fileio]") {
    CHECK(LbFileLength("/tmp/kfx_platform_utest_definitely_missing_file.bin") == -1);
}

TEST_CASE("LbDirectoryCurrent fills the buffer with an absolute path", "[kfx_platform][bflib_fileio]") {
    char buf[1024];
    CHECK(LbDirectoryCurrent(buf, sizeof(buf)) == 1);
    CHECK(buf[0] == '/');
}
