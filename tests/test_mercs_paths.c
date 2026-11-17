/* Integration checks for the actual supplied game roots and isolated writes.
 * Usage: test_mercs_paths MERCS_GAME MW2_GAME GBL_GAME EMPTY_TEST_DIRECTORY */
#include "portcfg.h"
#include "datapath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;
static void check(int ok, const char *name) { if (!ok) { fprintf(stderr, "FAIL: %s\n", name); failures++; } }
static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (!f || fputs(text, f) == EOF || fclose(f) != 0) { perror(path); exit(2); }
}
static int is_text(const char *path, const char *want)
{
    char b[256]; size_t n; FILE *f = fopen(path, "rb");
    if (!f) return 0;
    n = fread(b, 1, sizeof b, f); fclose(f);
    return n == strlen(want) && memcmp(b, want, n) == 0;
}
static void write_overlay(const char *path, const char *mode, const char *text)
{
    FILE *f = dp_fopen(path, mode);
    check(f != NULL, path);
    if (f) { check(fputs(text, f) != EOF, "overlay write"); check(fclose(f) == 0, "overlay close"); }
}

int main(int argc, char **argv)
{
    char install[2048], cd[2048], source[2048], target[2048], cfg[2048];
    portcfg c;
    FILE *f;
    if (argc != 5) return 2;
    check(portcfg_is_game_dir(argv[1]), "Mercenaries product/data accepted");
    check(!portcfg_is_game_dir(argv[2]), "original MW2 folder refused");
    check(!portcfg_is_game_dir(argv[3]), "GBL folder refused");
    portcfg_defaults(&c);
    check(portcfg_resolve(&c, NULL, argv[1]) == 0, "Mercs folder resolves");
    check(strstr(c.cd, "/cd") != NULL && strstr(c.install, "/install") != NULL, "folder convention retained");
    check(c.renderer == PORTCFG_ED_DOS, "retail default is explicit");
    check(portcfg_resolve(&c, NULL, argv[2]) != 0, "bad explicit folder does not silently reuse remembered Mercs");
    snprintf(install, sizeof install, "%s/install", argv[4]);
    snprintf(cd, sizeof cd, "%s/cd", argv[4]);
    check(mkdir(install, 0755) == 0 && mkdir(cd, 0755) == 0, "test overlay created");
    check(!portcfg_is_game_dir(argv[4]), "empty install/cd directories are insufficient identity");
    snprintf(cfg, sizeof cfg, "%s/mercs.cfg", argv[4]);
    setenv("MW2_CONFIG", "/unchanged/mw2.cfg", 1);
    setenv("MERCS_CONFIG", cfg, 1);
    check(strcmp(portcfg_file(), cfg) == 0, "own configuration namespace");
    portcfg_defaults(&c);
    check(portcfg_resolve(&c, NULL, argv[1]) == 0 && portcfg_save(NULL, &c) == 0, "separate config written");
    memset(&c, 0, sizeof c);
    check(portcfg_load(NULL, &c) == 0 && portcfg_is_game_dir(c.game), "separate config roundtrip");

    snprintf(source, sizeof source, "%s/cd/SETTINGS.CFG", argv[4]);
    write_text(source, "original");
    dp_clear_roots(); dp_add_root(install); dp_add_root(cd);
    f = dp_fopen("settings.cfg", "rb"); check(f != NULL, "case-insensitive CD reads"); if (f) fclose(f);
    check(dp_fopen("settings.cfg", "wb") == NULL, "read roots alone never authorize writes");
    check(dp_set_write_root(install) == 0, "explicit writable overlay");
    write_overlay("settings.cfg", "wb", "changed");
    snprintf(target, sizeof target, "%s/install/settings.cfg", argv[4]);
    check(is_text(source, "original") && is_text(target, "changed"), "truncating write preserves original CD file");
    snprintf(source, sizeof source, "%s/cd/APPEND.CFG", argv[4]); write_text(source, "base");
    write_overlay("append.cfg", "ab", "+append");
    snprintf(target, sizeof target, "%s/install/append.cfg", argv[4]);
    check(is_text(source, "base") && is_text(target, "base+append"), "append copies fallback into overlay");
    snprintf(source, sizeof source, "%s/cd/UPDATE.CFG", argv[4]); write_text(source, "base");
    write_overlay("update.cfg", "r+b", "B");
    snprintf(target, sizeof target, "%s/install/update.cfg", argv[4]);
    check(is_text(source, "base") && is_text(target, "Base"), "r+ copies fallback into overlay");
    write_overlay("C:\\MEK\\Custom.MEK", "wb", "custom");
    snprintf(target, sizeof target, "%s/install/MEK/Custom.MEK", argv[4]);
    check(is_text(target, "custom"), "DOS paths and new subdirectories remain inside overlay");
    check(dp_fopen("../outside.cfg", "wb") == NULL, "parent traversal rejected");
#ifndef _WIN32
    snprintf(target, sizeof target, "%s/install/escape", argv[4]);
    check(symlink(cd, target) == 0, "symlink test prepared");
    check(dp_fopen("escape/SETTINGS.CFG", "wb") == NULL, "overlay symlink cannot redirect a write into CD");
#endif
    dp_clear_roots(); dp_add_root(cd);
    check(dp_fopen("SETTINGS.CFG", "wb") == NULL, "clearing roots also clears writable state");
#ifndef _WIN32
    {   /* a game folder copied from Windows, where case never mattered: "CD" holding "Mercs" (GitHub: Fedora refused a
         * folder that worked on Windows - "The requested DOS data is missing or invalid") */
        char mixed[2048], mixed_cd[2056], from[2048], to[2080], models[2048], tex[2048], kind[64], sky[2048], fog[2048];
        const char *const parts[] = {"PRODUCT.INI", "MERCS"};
        int k, have;
        snprintf(mixed, sizeof mixed, "%s/mixed", argv[4]);
        snprintf(mixed_cd, sizeof mixed_cd, "%s/CD", mixed);
        check(mkdir(mixed, 0755) == 0 && mkdir(mixed_cd, 0755) == 0, "mixed-case folder created");
        for (k = 0; k < 2; k++) {
            snprintf(from, sizeof from, "%s/cd/%s", argv[1], parts[k]);
            snprintf(to, sizeof to, "%s/%s", mixed_cd, k ? "Mercs" : "Product.ini");
            check(symlink(from, to) == 0, "mixed-case entry linked");
        }
        check(portcfg_is_game_dir(mixed), "mixed-case game folder accepted");
        portcfg_defaults(&c);
        check(portcfg_resolve(&c, NULL, mixed) == 0, "mixed-case game folder resolves");
        have = portcfg_edition(&c, PORTCFG_ED_DOS, models, tex, kind, sky, fog) == 0;
        check(have && strstr(models, "/CD/Mercs/") != NULL, "DOS archive found through CD/Mercs");
        dp_clear_roots(); setenv("MERCS_CD_DIR", c.cd, 1); setenv("MERCS_INSTALL_DIR", install, 1); dp_add_roots_from_env();
        check(dp_resolve("GAMEKEY.MAP", from, sizeof from) == 0 && strstr(from, "/CD/Mercs/") != NULL, "game files found through CD/Mercs");
        unsetenv("MERCS_CD_DIR"); unsetenv("MERCS_INSTALL_DIR"); dp_clear_roots();
    }
#endif
    printf("test_mercs_paths: %s (%d failures)\n", failures ? "FAILED" : "ok", failures);
    return failures != 0;
}
