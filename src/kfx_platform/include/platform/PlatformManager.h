#ifndef PLATFORM_MANAGER_H
#define PLATFORM_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

// OS information — facade over IPlatform::Get*.
const char * PlatformManager_GetOSVersion(void);
const void * PlatformManager_GetImageBase(void);
const char * PlatformManager_GetWineVersion(void);
const char * PlatformManager_GetWineHost(void);

int          PlatformManager_InitVideo(void);
int          PlatformManager_HasWindow(void);
int          PlatformManager_GetIsAppActive(void);
int          PlatformManager_OwnsDisplay(void);
int          PlatformManager_ForcesAllModesAvailable(void);

unsigned int PlatformManager_GetWindowFlags(void);
void         PlatformManager_GetWindowSize(int* out_w, int* out_h);
// Returns the window's SDL display ID (opaque), not a 0-based index.
int          PlatformManager_GetWindowDisplayIndex(void);
int          PlatformManager_GetNumVideoDisplays(void);
int          PlatformManager_GetDesktopDisplayMode(int display, int* out_w, int* out_h);
int          PlatformManager_GetDisplayBounds(int display, int* out_x, int* out_y, int* out_w, int* out_h);
int          PlatformManager_GetClosestDisplayMode(int display, int desired_w, int desired_h, int* out_w, int* out_h);
int          PlatformManager_SetWindowDisplayMode(int w, int h);
void         PlatformManager_SetWindowSize(int w, int h);
int          PlatformManager_SetWindowFullscreen(unsigned int flags);
void         PlatformManager_SetWindowBordered(int bordered);
void         PlatformManager_SetWindowPosition(int x, int y);
int          PlatformManager_CreateWindow(const char* title, int x, int y, int w, int h, unsigned int flags);
void         PlatformManager_WarpCursor(int x, int y);
int          PlatformManager_IsCursorInWindow(void);
int          PlatformManager_RecreateWindowForSoftwareRenderer(void);
int          PlatformManager_GetDisplayRefreshRate(void);
int          PlatformManager_GetFullscreenDisplayModeCount(int display);
int          PlatformManager_GetFullscreenDisplayModeAt(int display, int index, int* out_w, int* out_h);

// Lists the immediate sub-directory names of `path` into `out` (a flat
// buffer of `max` slots, `stride` bytes each; each name NUL-terminated and
// truncated to fit). Returns the number of names written. LbFileFindFirst
// deliberately drops directories on every platform, so this is separate.
int          PlatformManager_ListSubdirectories(const char* path, char* out, int stride, int max);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_MANAGER_H
