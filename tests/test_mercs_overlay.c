/* Focused path/edition regression test. Creates only small synthetic fixtures
 * below the supplied empty directory; never needs an original game archive. */
#include "portcfg.h"
#include "datapath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;
static void check(int ok, const char *what)
{
    if (!ok) { fprintf(stderr, "FAIL: %s\n", what); failures++; }
}
static void put(const char *name, const void *data, size_t size)
{
    FILE *f = fopen(name, "wb");
    if (!f || fwrite(data, 1, size, f) != size || fclose(f) != 0) { perror(name); exit(2); }
}
static void text(const char *name, const char *data) { put(name, data, strlen(data)); }
static int exists(const char *name) { struct stat st; return stat(name, &st) == 0; }
static void w16(unsigned char *p, unsigned v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void w32(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
/* A checksummed PRJ containing one small, bounded 1.22 CEL. This exercises the
 * real archive reader used by availability checks without original game data. */
static void mercs16(const char *path)
{
    unsigned char p[212] = {0};
    unsigned char *d = p + 128, *cel = d + 62;
    unsigned i, sum = 0;
    memcpy(p, "PROJ", 4); w32(p + 4, sizeof p - 8); memcpy(p + 12, "DDIT", 4); w16(p + 24, 1);
    memcpy(p + 26, "CEL", 3); w32(p + 30, 50); w32(p + 34, 38); w32(p + 38, 88); w32(p + 42, 40);
    memcpy(p + 50, "INDX", 4); w32(p + 54, 30); memcpy(p + 62, "CEL", 3); w16(p + 66, 2); w16(p + 68, 1);
    w32(p + 80, 128); w32(p + 84, 84);
    memcpy(p + 88, "SYMB", 4); w32(p + 92, 32); memcpy(p + 100, "CEL", 3); w16(p + 104, 1); w16(p + 106, 1);
    memcpy(p + 110, "TEST", 4); w16(p + 126, 1);
    memcpy(d, "DATA", 4); w32(d + 4, 76); memcpy(d + 12, "CEL", 3); w16(d + 24, 1); memcpy(d + 30, "TEST", 4);
    memcpy(cel, "1.22", 4); w16(cel + 6, 1); w16(cel + 8, 2); w16(cel + 10, 2); cel[21] = 1;
    for (i = 12; i < 84; i++) sum += d[i];
    w32(d + 8, sum); put(path, p, sizeof p);
}
static void product(const char *value)
{
    char line[80];
    snprintf(line, sizeof line, "[Product Info]\nProductID=%s\n", value);
    text("game/cd/PRODUCT.INI", line);
}

int main(int argc, char **argv)
{
    portcfg c;
    char overlay[PORTCFG_PATH], want[PORTCFG_PATH];
    char m[PORTCFG_PATH], t[PORTCFG_PATH], k[PORTCFG_PATH], s[PORTCFG_PATH], f[PORTCFG_PATH];
    unsigned char header[26] = {'P','R','O','J',18,0,0,0,0,0,0,0,'D','D','I','T'};
    FILE *out;
    if (argc != 2 || chdir(argv[1]) != 0) return 2;
    check(mkdir("game", 0755) == 0 && mkdir("game/cd", 0755) == 0 &&
          mkdir("game/cd/MERCS", 0755) == 0, "fixture created");
    put("game/cd/MERCS/MW2.PRJ", header, sizeof header);
    text("game/cd/MERCS/SKYGND.PAR", "synthetic sky marker\n");
    text("game/cd/MERCS/ORIGINAL.CFG", "unchanged\n");
    product("Mercs3DFX");
    portcfg_defaults(&c);
    check(portcfg_resolve(&c, NULL, "game") == 0, "3dfx base recognized");
    check(!c.no3d, "3dfx base supplies 3D data without a 3d copy");
    check(portcfg_edition(&c, PORTCFG_ED_DOS, m,t,k,s,f) != 0, "3dfx base rejects DOS edition");
    check(portcfg_edition(&c, PORTCFG_ED_3DFX, m,t,k,s,f) != 0, "accelerated profile rejects header-only PRJ");
    mercs16("game/cd/MERCS/MW2.PRJ");
    check(portcfg_edition(&c, PORTCFG_ED_3DFX, m,t,k,s,f) == 0 &&
          strstr(m, "/cd/MERCS/MW2.PRJ") && strcmp(m,t) == 0 && strcmp(k,"mercs16") == 0 &&
          strstr(s, "/cd/MERCS/SKYGND.PAR") && !f[0], "3dfx base selects its archive and sky");
    check(portcfg_look(&c, PORTCFG_ED_3DFX, PORTCFG_TX_TITANIUM, m,t,k,s,f) == 0 &&
          strstr(m, "/cd/MERCS/MW2.PRJ") && strstr(s, "/cd/MERCS/SKYGND.PAR"),
          "legacy texture selection cannot mix edition data");
    product("MercsW95");
    portcfg_defaults(&c);
    check(portcfg_resolve(&c, NULL, "game") == 0 && c.no3d, "retail base remains DOS without 3D data");
    check(portcfg_edition(&c, PORTCFG_ED_DOS, m,t,k,s,f) == 0 && strcmp(k,"dos") == 0, "retail DOS accepted");
    check(portcfg_edition(&c, PORTCFG_ED_3DFX, m,t,k,s,f) != 0, "retail without 3D data rejects 3dfx");
    check(portcfg_edition(&c, PORTCFG_ED_D3D, m,t,k,s,f) != 0, "missing Titanium data disables Direct3D");
    check(mkdir("game/titanium", 0755) == 0, "Titanium fixture created");
    put("game/titanium/MW2.PRJ", header, sizeof header);
    text("game/titanium/SKYGND.PAR", "synthetic Titanium sky marker\n");
    portcfg_defaults(&c);
    check(portcfg_resolve(&c, NULL, "game") == 0 && c.no3d, "Titanium availability is independent of dedicated 3dfx data");
    check(portcfg_edition(&c, PORTCFG_ED_D3D, m,t,k,s,f) != 0, "Direct3D rejects archive without 1.2x CELs");
    mercs16("game/titanium/MW2.PRJ");
    check(portcfg_edition(&c, PORTCFG_ED_D3D, m,t,k,s,f) == 0 &&
          strstr(m, "/titanium/MW2.PRJ") && strcmp(m,t) == 0 && strcmp(k,"mercs16") == 0 &&
          strstr(s, "/titanium/SKYGND.PAR") && !f[0], "Direct3D selects its complete archive and matching sky");
    check(unlink("game/titanium/SKYGND.PAR") == 0 && portcfg_edition(&c, PORTCFG_ED_D3D, m,t,k,s,f) != 0,
          "Direct3D without its matching sky is unavailable");
    text("game/titanium/SKYGND.PAR", "synthetic Titanium sky marker\n");
    {
        int profiles[] = { PORTCFG_ED_DOS, PORTCFG_ED_D3D, PORTCFG_ED_3DFX }, j;
        setenv("MERCS_CONFIG", "profile-settings.cfg", 1);
        for (j = 0; j < 3; j++) {
            portcfg loaded;
            c.renderer = profiles[j]; c.texset = PORTCFG_TX_TITANIUM;
            check(portcfg_save(NULL, &c) == 0 && portcfg_load(NULL, &loaded) == 0 && loaded.renderer == profiles[j] &&
                  loaded.texset == PORTCFG_TX_ORIGINAL, "profile config roundtrip preserves renderer without a texture overlay");
            check(portcfg_mercs_parse(portcfg_mercs_name(profiles[j])) == profiles[j], "profile CLI/config token roundtrip");
        }
        check(portcfg_mercs_parse("d3d") == PORTCFG_ED_D3D && portcfg_mercs_parse("enhanced") == -1,
              "CLI alias is explicit and inherited mixed renderer is not offered");
    }

    check(portcfg_overlay_dir(&c,"retail",overlay,sizeof overlay,0) == 0 && !exists("game/install"),
          "inspection allows missing overlay without creating it");
    check(portcfg_overlay_dir(&c,"direct3d",overlay,sizeof overlay,0) == 0 && !exists("game/install"),
          "Direct3D inspection does not create an overlay");
    check(portcfg_overlay_dir(&c,"retail",overlay,sizeof overlay,0) == 0, "retail overlay restored for remaining checks");
    snprintf(want, sizeof want, "%s", overlay);
    snprintf(c.install, sizeof c.install, "/unrelated/install");
    check(portcfg_overlay_dir(&c,"retail",overlay,sizeof overlay,1) == 0 && strcmp(overlay,want) == 0 &&
          exists("game/install/retail"), "overlay remains beneath game, ignoring stale install override");
    check(portcfg_overlay_dir(&c,"direct3d",overlay,sizeof overlay,1) == 0 &&
          strcmp(overlay,want) != 0 && exists("game/install/direct3d"), "Direct3D owns a separate writable overlay");
    check(portcfg_overlay_dir(&c,"3dfx",overlay,sizeof overlay,1) == 0 &&
          strcmp(overlay,want) != 0 && exists("game/install/3dfx"), "3dfx owns a separate writable overlay");
    check(portcfg_overlay_dir(&c,"../cd",overlay,sizeof overlay,0) != 0, "edition path traversal rejected");
    check(dp_set_write_root("game/install/retail") == 0, "ordinary write root accepted");
    out = dp_fopen("new.cfg", "wb"); check(out != NULL, "ordinary overlay writable"); if (out) fclose(out);
    check(dp_set_write_root("game/install/retail/./") == 0, "ordinary root trailing dot/slash accepted");

#ifndef _WIN32
    check(symlink("game", "game-alias") == 0, "ancestor alias prepared");
    snprintf(c.game,sizeof c.game,"game-alias");
    check(portcfg_overlay_dir(&c,"retail",overlay,sizeof overlay,0) == 0 && strcmp(overlay,want) == 0,
          "canonical game root permits ancestor alias");
    check(dp_set_write_root("game-alias/install/retail") == 0, "write root permits an ancestor alias");
    check(symlink("cd/MERCS", "game/root-link") == 0, "root symlink prepared");
    check(dp_set_write_root("game/root-link") != 0 && dp_fopen("ORIGINAL.CFG","wb") == NULL,
          "symlink write root rejected and previous write state cleared");
    check(dp_set_write_root("game/root-link/") != 0, "symlink root trailing slash rejected");
    check(dp_set_write_root("game/root-link/./") != 0, "symlink root trailing dot rejected");
    check(rename("game/install/retail","game/install/good") == 0 &&
          symlink("../cd/MERCS","game/install/retail") == 0, "edition symlink prepared");
    check(portcfg_overlay_dir(&c,"retail",overlay,sizeof overlay,0) != 0 &&
          portcfg_overlay_dir(&c,"retail",overlay,sizeof overlay,1) != 0, "existing edition symlink rejected");
    check(unlink("game/install/retail") == 0 && rename("game/install","game/good-install") == 0 &&
          symlink("cd/MERCS","game/install") == 0, "install symlink prepared");
    check(portcfg_overlay_dir(&c,"retail",overlay,sizeof overlay,0) != 0 &&
          portcfg_overlay_dir(&c,"retail",overlay,sizeof overlay,1) != 0, "existing install symlink rejected");
    check(!exists("game/cd/MERCS/retail"), "rejection creates nothing in CD");
    check(unlink("game/install") == 0 && symlink("does-not-exist","game/install") == 0,
          "dangling install symlink prepared");
    check(portcfg_overlay_dir(&c,"retail",overlay,sizeof overlay,1) != 0, "dangling install symlink rejected");
#endif
    {
        char data[16] = {0};
        FILE *in = fopen("game/cd/MERCS/ORIGINAL.CFG","rb");
        check(in && fread(data,1,10,in) == 10 && strcmp(data,"unchanged\n") == 0, "CD sentinel unchanged");
        if (in) fclose(in);
    }
    printf("test_mercs_overlay: %s (%d failures)\n", failures ? "FAILED" : "ok", failures);
    return failures != 0;
}
