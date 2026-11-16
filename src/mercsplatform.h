#ifndef MERCS_PLATFORM_H
#define MERCS_PLATFORM_H
/* The operating system the build runs on, for the originals' "Flee to DOS" (the DOS shell) and "Flee to Windows" (the
 * 3D editions' in-game menu): the port names its own platform instead. */
#if defined(_WIN32)
#define MERCS_PLATFORM_TITLE "Windows"
#define MERCS_PLATFORM_UPPER "WINDOWS"
#elif defined(__APPLE__)
#define MERCS_PLATFORM_TITLE "macOS"
#define MERCS_PLATFORM_UPPER "MACOS"
#else
#define MERCS_PLATFORM_TITLE "Linux"
#define MERCS_PLATFORM_UPPER "LINUX"
#endif
#endif
