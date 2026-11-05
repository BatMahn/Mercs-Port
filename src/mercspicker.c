/* Folder selection follows tools/mw2shell.c in the mature MW2 foundation:
 * AppleScript on macOS, the existing native Windows helper, zenity/kdialog on
 * Linux. Picker results never become shell command text. */
#include "mercspicker.h"
#include "wincompat.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#define PICK_PROMPT "Select your MechWarrior 2 Mercenaries game folder (Mercs-game)"
#define APP_TITLE "MechWarrior 2: Mercenaries"

static int path_copy(char *out, size_t n, const char *path)
{
    size_t k = path ? strlen(path) : 0;
    if (!out || !n || !k || k >= n) { errno = k >= n ? ENAMETOOLONG : EINVAL; return -1; }
    memcpy(out, path, k + 1);
    return 0;
}

int mercs_executable_path(const char *argv0, char *out, size_t n)
{
    char *absolute;
    if (!out || !n) { errno = EINVAL; return -1; }
    out[0] = 0;
#ifdef _WIN32
    mw2_exe_path(out, n);
    if (out[0]) return 0;
#elif defined(__APPLE__)
    {
        uint32_t size = 0;
        char *raw;
        (void)_NSGetExecutablePath(NULL, &size);
        if (size && (raw = malloc(size))) {
            if (_NSGetExecutablePath(raw, &size) == 0) {
                absolute = realpath(raw, NULL);
                if (absolute) {
                    int rc = path_copy(out, n, absolute);
                    free(absolute); free(raw); return rc;
                }
            }
            free(raw);
        }
    }
#elif defined(__linux__)
    {
        ssize_t k = readlink("/proc/self/exe", out, n - 1);
        if (k > 0 && (size_t)k < n - 1) { out[k] = 0; return 0; }
        out[0] = 0;
    }
#endif
    if (argv0 && (strchr(argv0, '/') || strchr(argv0, '\\')) && (absolute = realpath(argv0, NULL))) {
        int rc = path_copy(out, n, absolute);
        free(absolute); return rc;
    }
    if (argv0 && argv0[0]) {
        const char *base = SDL_GetBasePath();
        if (base) {
            const char *name = strrchr(argv0, '/');
            char *joined;
            size_t size;
            name = name ? name + 1 : argv0;
            size = strlen(base) + strlen(name) + 1;
            joined = malloc(size);
            if (joined) {
                snprintf(joined, size, "%s%s", base, name);
                absolute = realpath(joined, NULL);
                free(joined);
                if (absolute) {
                    int rc = path_copy(out, n, absolute);
                    free(absolute);  return rc;
                }
            }
            
        }
    }
    errno = ENOENT;
    return -1;
}

int mercs_gui_error(const char *message)
{
    fprintf(stderr, "%s: %s\n", APP_TITLE, message);
    return SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, APP_TITLE, message, NULL) ? 0 : -1;
}

#ifndef _WIN32
/* 1 selected, 0 cancelled, -1 command unavailable/no display/invalid output. */
static int run_picker(const char *cmd, char *out, size_t n)
{
    char line[PORTCFG_PATH], discard[256];
    int status, too_long = 0;
    size_t k;
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return -1;
    if (!fgets(line, sizeof line, pipe)) line[0] = 0;
    else if (!strchr(line, '\n') && !feof(pipe)) too_long = 1;
    while (fgets(discard, sizeof discard, pipe)) {}   /* drain before waiting */
    status = pclose(pipe);
    if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) == 127 || WEXITSTATUS(status) == 255) return -1;
    if (WEXITSTATUS(status) != 0) return 0;
    if (too_long) { errno = ENAMETOOLONG; return -1; }
    line[strcspn(line, "\r\n")] = 0;
    for (k = strlen(line); k > 1 && line[k - 1] == '/'; k--) line[k - 1] = 0;
    if (!line[0]) return 0;
    return path_copy(out, n, line) == 0 ? 1 : -1;
}
#endif

static int pick_folder(char *out, size_t n)
{
#ifdef _WIN32
    return mw2_pick_folder(PICK_PROMPT, out, n);
#else
    static const char *const commands[] = {
#ifdef __APPLE__
        "/usr/bin/osascript -e 'POSIX path of (choose folder with prompt \"" PICK_PROMPT "\")' 2>/dev/null",
#else
        "command -v zenity >/dev/null 2>&1 || exit 127; zenity --file-selection --directory --title='" PICK_PROMPT "' 2>/dev/null",
        "command -v kdialog >/dev/null 2>&1 || exit 127; kdialog --getexistingdirectory \"$HOME\" --title '" PICK_PROMPT "' 2>/dev/null",
#endif
    };
    size_t i;
    const char *override = getenv("MERCS_PICKER");   /* development: stand-in chooser command */
    if (override) return run_picker(override, out, n);
    for (i = 0; i < sizeof commands / sizeof commands[0]; i++) {
        int rc = run_picker(commands[i], out, n);
        if (rc >= 0) return rc;
    }
    return -1;
#endif
}

int mercs_pick_game_folder(portcfg *cfg, const char *exe_dir)
{
    char selected[PORTCFG_PATH], game[PORTCFG_PATH], message[PORTCFG_PATH + 500];
    if (!cfg) { errno = EINVAL; return -1; }
    if (portcfg_translocated(exe_dir))
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, APP_TITLE,
            "macOS is running this application from a temporary copy. Choose your Mercs-game folder; "
            "the game will remember it.\n\nMoving the application with Finder also lets it find a "
            "Mercs-game folder placed next to the application.", NULL);
    for (;;) {
        portcfg chosen = *cfg;
        int rc = pick_folder(selected, sizeof selected);
        if (rc != 1) return rc;
        if (portcfg_game_in(selected, game, sizeof game) && portcfg_resolve(&chosen, exe_dir, game) == 0) {
            *cfg = chosen;
            return 1;
        }
        snprintf(message, sizeof message,
            "%s is not a Mercenaries game folder.\n\n"
            "Choose Mercs-game (the folder containing cd and install), or its parent folder. "
            "The cd folder must contain the original Mercenaries PRODUCT.INI and MERCS/MW2.PRJ.\n\n"
            "Click OK to choose again, or Cancel in the folder chooser to quit.", selected);
        if (mercs_gui_error(message) != 0) return -1;
    }
}
