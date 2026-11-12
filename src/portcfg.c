#include "portcfg.h"
#include "prj.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wformat-truncation"   /* paths are bounded by PORTCFG_PATH by design */
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <strings.h>
#ifdef _WIN32
#include <windows.h>
#endif

void portcfg_defaults(portcfg *c) { memset(c, 0, sizeof *c); c->mode = 0; c->aa = 4; c->gpu = PORTCFG_GPU_AUTO; c->textures_ati = 0; c->speed = 0.85f; c->renderer = PORTCFG_ED_DOS; c->videos = PORTCFG_VID_ORIGINAL; c->video_fit = PORTCFG_FIT_43; }

static int exists(const char *p) { struct stat st; return p && p[0] && stat(p, &st) == 0; }
static int is_dir(const char *p) { struct stat st; return p && p[0] && stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static void mkdirs(const char *path)
{
    char tmp[PORTCFG_PATH], *q;
    snprintf(tmp, sizeof tmp, "%s", path);
    for (q = tmp + 1; *q; q++) if (*q == '/' || *q == '\\') { char sep = *q; *q = 0; mkdir(tmp, 0755); *q = sep; }
    mkdir(tmp, 0755);
}
/* a file in a folder, matching the name case-insensitively (DOS names) */
static int find_ci(const char *dir, const char *name, char *out, size_t outlen)
{
    char p[PORTCFG_PATH * 3];
    char lower[256], upper[256];
    size_t i;
    for (i = 0; name[i] && i < 255; i++) { lower[i] = (char)(name[i] >= 'A' && name[i] <= 'Z' ? name[i] + 32 : name[i]); upper[i] = (char)(name[i] >= 'a' && name[i] <= 'z' ? name[i] - 32 : name[i]); }
    lower[i] = upper[i] = 0;
    snprintf(p, sizeof p, "%s/%s", dir, name); if (exists(p)) { snprintf(out, outlen, "%s", p); return 1; }
    snprintf(p, sizeof p, "%s/%s", dir, upper); if (exists(p)) { snprintf(out, outlen, "%s", p); return 1; }
    snprintf(p, sizeof p, "%s/%s", dir, lower); if (exists(p)) { snprintf(out, outlen, "%s", p); return 1; }
    return 0;
}

const char *portcfg_file(void)
{
    static char path[PORTCFG_PATH];
    const char *home = getenv("HOME"), *xdg = getenv("XDG_CONFIG_HOME");
    if (getenv("MERCS_CONFIG")) { snprintf(path, sizeof path, "%s", getenv("MERCS_CONFIG")); return path; }   /* tests */
#if defined(_WIN32)
    {   /* %APPDATA%\mercsport\mercsport.cfg */
        const char *appdata = getenv("APPDATA");
        if (!appdata || !appdata[0]) appdata = getenv("USERPROFILE");
        snprintf(path, sizeof path, "%s/mercsport/mercsport.cfg", appdata ? appdata : ".");
        (void)home; (void)xdg;
    }
#elif defined(__APPLE__)
    snprintf(path, sizeof path, "%s/Library/Application Support/mercsport/mercsport.cfg", home ? home : ".");
    (void)xdg;
#else
    if (xdg && xdg[0]) snprintf(path, sizeof path, "%s/mercsport/mercsport.cfg", xdg);
    else snprintf(path, sizeof path, "%s/.config/mercsport/mercsport.cfg", home ? home : ".");
#endif
    return path;
}

int portcfg_load(const char *unused, portcfg *c)
{
    char line[PORTCFG_PATH + 32];
    FILE *f;
    (void)unused;
    portcfg_defaults(c);
    if (!(f = fopen(portcfg_file(), "r"))) return -1;
    while (fgets(line, sizeof line, f)) {
        char *v = strchr(line, '=');
        if (!v || line[0] == '#') continue;
        *v++ = 0;
        v[strcspn(v, "\r\n")] = 0;
#define STR(k) else if (strcmp(line, #k) == 0) snprintf(c->k, sizeof c->k, "%s", v)
        if (strcmp(line, "resolution") == 0) {
            if (strcmp(v, "desktop") == 0) c->mode = 0;
            else if (strcmp(v, "window") == 0) c->mode = 1;
            else if (sscanf(v, "%dx%d", &c->w, &c->h) == 2 && c->w > 0 && c->h > 0) c->mode = 2;
        } else if (strcmp(line, "speed") == 0) {
            float sp = (float)atof(v);
            if (sp >= 0.5f && sp <= 1.5f) c->speed = sp;
        } else if (strcmp(line, "render") == 0) {
            int rw = 0, rh = 0;
            if (sscanf(v, "%dx%d", &rw, &rh) == 2 && ((rw == 320 && rh == 200) || (rw == 640 && rh == 480) || (rw == 1024 && rh == 768))) { c->render_w = rw; c->render_h = rh; }
            else c->render_w = c->render_h = 0;
        } else if (strcmp(line, "aa") == 0) {
            int a = atoi(v);
            c->aa = a >= 8 ? 8 : a >= 4 ? 4 : a >= 2 ? 2 : 0;
        } else if (strcmp(line, "gpu") == 0) {
            int gpu;
            for (gpu = 0; gpu < PORTCFG_GPU_COUNT; gpu++)
                if (!strcmp(v, PORTCFG_GPU_NAME[gpu])) c->gpu = gpu;
        } else if (strcmp(line, "system_shortcuts") == 0) c->system_shortcuts = strcmp(v, "allow") == 0;
        else if (strcmp(line, "hud_layout") == 0) c->hud_original = strcmp(v, "original") == 0;
        else if (strcmp(line, "dos_shading") == 0 || strcmp(line, "dos_sky") == 0) c->dos_sky_smooth = strcmp(v, "smooth") == 0;   /* (dos_sky=: its older name) */
        else if (strcmp(line, "textures_kind") == 0) c->textures_ati = strcmp(v, "3dfx") != 0;
        else if (strcmp(line, "texture_filter") == 0) {
            int filter;
            for (filter = 0; filter < PORTCFG_FILTER_COUNT; filter++)
                if (!strcmp(v, PORTCFG_FILTER_NAME[filter])) c->texture_filter = filter;
        }
        else if (strcmp(line, "videos") == 0) {
            int look;
            for (look = 0; look < PORTCFG_VID_COUNT; look++) if (strcmp(v, PORTCFG_VID_NAME[look]) == 0) c->videos = look;
        } else if (strcmp(line, "video_fit") == 0) {
            int fit;
            for (fit = 0; fit < PORTCFG_FIT_COUNT; fit++) if (strcmp(v, PORTCFG_FIT_NAME[fit]) == 0) c->video_fit = fit;
        } else if (strcmp(line, "salvage") == 0) {
            int k;
            for (k = 0; k < PORTCFG_SALVAGE_COUNT; k++) if (strcmp(v, PORTCFG_SALVAGE_NAME[k]) == 0) c->salvage = k;
        }
        else if (strcmp(line, "renderer") == 0 || strcmp(line, "edition") == 0) {   /* (edition=: the old single look) */
            int e;
            for (e = 0; e < PORTCFG_ED_COUNT; e++) if (strcmp(v, PORTCFG_ED_NAME[e]) == 0) c->renderer = e;
            if (strcmp(v, "d3d") == 0) c->renderer = PORTCFG_ED_D3D;
            if (strcmp(v, "titanium") == 0) { c->renderer = PORTCFG_ED_ENHANCED; c->texset = PORTCFG_TX_TITANIUM; }   /* migrated */
        } else if (strcmp(line, "textures") == 0 && (strcmp(v, "original") == 0 || strcmp(v, "titanium") == 0))
            c->texset = strcmp(v, "titanium") == 0 ? PORTCFG_TX_TITANIUM : PORTCFG_TX_ORIGINAL;
        STR(game); STR(install); STR(cd); STR(music); STR(ultrasnd); STR(mt32); STR(models); STR(textures); STR(skygnd); STR(skygnd_fog); STR(titanium);
#undef STR
    }
    fclose(f);
    /* A Mercs profile always uses its own data, including after loading an old
     * configuration that selected an independent Titanium texture overlay. */
    if (portcfg_mercs_name(c->renderer)) c->texset = PORTCFG_TX_ORIGINAL;
    return 0;
}

int portcfg_save(const char *unused, const portcfg *c)
{
    char dir[PORTCFG_PATH], *slash;
    FILE *f;
    (void)unused;
    snprintf(dir, sizeof dir, "%s", portcfg_file());
    if ((slash = strrchr(dir, '/'))) { *slash = 0; mkdirs(dir); }
    if (!(f = fopen(portcfg_file(), "w"))) return -1;
    fprintf(f, "# MechWarrior 2: Mercenaries port settings (see portcfg.h). Paths left out are taken from the game folder.\n");
    if (c->game[0]) fprintf(f, "game=%s\n", c->game);
#define OPT(k) if (c->k[0] && !c->game[0]) fprintf(f, #k "=%s\n", c->k)
    OPT(install); OPT(cd); OPT(music); OPT(ultrasnd); OPT(mt32); OPT(models); OPT(textures); OPT(skygnd); OPT(skygnd_fog); OPT(titanium);
#undef OPT
    if (c->mode == 0) fprintf(f, "resolution=desktop\n");
    else if (c->mode == 1) fprintf(f, "resolution=window\n");
    else fprintf(f, "resolution=%dx%d\n", c->w, c->h);
    fprintf(f, "aa=%d\n", c->aa);
    fprintf(f, "gpu=%s\n", PORTCFG_GPU_NAME[c->gpu >= 0 && c->gpu < PORTCFG_GPU_COUNT ? c->gpu : PORTCFG_GPU_AUTO]);
    if (c->render_w) fprintf(f, "render=%dx%d\n", c->render_w, c->render_h); else fprintf(f, "render=native\n");
    fprintf(f, "speed=%.2f\n", (double)(c->speed > 0 ? c->speed : 0.85f));
    fprintf(f, "renderer=%s\n", PORTCFG_ED_NAME[c->renderer >= 0 && c->renderer < PORTCFG_ED_COUNT ? c->renderer : 0]);
    fprintf(f, "textures=%s\n", PORTCFG_TX_NAME[!portcfg_mercs_name(c->renderer) && c->texset == PORTCFG_TX_TITANIUM ? 1 : 0]);
    fprintf(f, "texture_filter=%s\n", PORTCFG_FILTER_NAME[c->texture_filter >= 0 && c->texture_filter < PORTCFG_FILTER_COUNT ? c->texture_filter : PORTCFG_FILTER_ORIGINAL]);
    fprintf(f, "system_shortcuts=%s\n", c->system_shortcuts ? "allow" : "block");
    fprintf(f, "hud_layout=%s\n", c->hud_original ? "original" : "wide");
    fprintf(f, "dos_shading=%s\n", c->dos_sky_smooth ? "smooth" : "dithered");
    fprintf(f, "videos=%s\n", PORTCFG_VID_NAME[c->videos >= 0 && c->videos < PORTCFG_VID_COUNT ? c->videos : PORTCFG_VID_ORIGINAL]);
    fprintf(f, "video_fit=%s\n", PORTCFG_FIT_NAME[c->video_fit >= 0 && c->video_fit < PORTCFG_FIT_COUNT ? c->video_fit : PORTCFG_FIT_43]);
    fprintf(f, "salvage=%s\n", PORTCFG_SALVAGE_NAME[c->salvage >= 0 && c->salvage < PORTCFG_SALVAGE_COUNT ? c->salvage : PORTCFG_SALVAGE_AUTO]);
    fclose(f);
    return 0;
}

/* The MW2 Titanium archive contains Mercenaries scene names too. A familiar
 * symbol name or an install/ directory alone therefore cannot identify Mercs.
 * These product IDs are read from the supplied retail/3dfx PRODUCT.INI files. */
static int mercs_product_edition(const char *dir)
{
    char path[PORTCFG_PATH], line[512];
    FILE *f;
    int ok = 0;
    if (!find_ci(dir, "PRODUCT.INI", path, sizeof path) || !(f = fopen(path, "r"))) return 0;
    while (fgets(line, sizeof line, f)) {
        char *p = line, *eq, *end;
        while (*p == ' ' || *p == '\t') p++;
        if (!(eq = strchr(p, '='))) continue;
        *eq++ = 0;
        end = p + strlen(p);
        while (end > p && (end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
        while (*eq == ' ' || *eq == '\t') eq++;
        end = eq + strcspn(eq, "\r\n");
        while (end > eq && (end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = 0;
        if (strcasecmp(p, "ProductID") == 0) {
            if (strcasecmp(eq, "MercsW95") == 0) ok = PORTCFG_ED_DOS;
            else if (strcasecmp(eq, "Mercs3DFX") == 0) ok = PORTCFG_ED_3DFX;
        }
    }
    fclose(f);
    return ok;
}

static int mercs_archive(const char *dir)
{
    char path[PORTCFG_PATH];
    unsigned char h[26];
    struct stat st;
    FILE *f;
    uint32_t size;
    int ok;
    if (!find_ci(dir, "MW2.PRJ", path, sizeof path) || stat(path, &st) != 0 ||
        !S_ISREG(st.st_mode) || st.st_size < 26 || !(f = fopen(path, "rb"))) return 0;
    ok = fread(h, 1, sizeof h, f) == sizeof h;
    fclose(f);
    if (!ok || memcmp(h, "PROJ", 4) != 0 || memcmp(h + 12, "DDIT", 4) != 0) return 0;
    size = (uint32_t)h[4] | (uint32_t)h[5] << 8 | (uint32_t)h[6] << 16 | (uint32_t)h[7] << 24;
    return (uint64_t)size + 8 == (uint64_t)st.st_size;
}

int portcfg_is_game_dir(const char *dir)
{
    char cd[PORTCFG_PATH], data[PORTCFG_PATH];
    if (!is_dir(dir) || !find_ci(dir, "cd", cd, sizeof cd) || !mercs_product_edition(cd) ||
        !find_ci(cd, "MERCS", data, sizeof data)) return 0;
    return mercs_archive(data);
}

int portcfg_app_folder(const char *exe_dir, char *out, size_t n)
{
    /* SDL_GetBasePath() inside a macOS bundle: ".../X.app/Contents/Resources/" (or MacOS/); the folder holding X.app */
    const char *a = exe_dir ? strstr(exe_dir, ".app/Contents/") : NULL, *s;
    if (!a) return 0;
    for (s = a; s > exe_dir && s[-1] != '/'; s--) {}
    if (s - exe_dir <= 1) snprintf(out, n, "%s", s > exe_dir ? "/" : ".");   /* "/X.app/..." or a relative "X.app/..." */
    else snprintf(out, n, "%.*s", (int)(s - exe_dir - 1), exe_dir);
    return 1;
}

int portcfg_translocated(const char *exe_dir) { return exe_dir && strstr(exe_dir, "/AppTranslocation/") != NULL; }

int portcfg_game_in(const char *dir, char *out, size_t n)
{
    char p[PORTCFG_PATH + 16];
    const char *g = NULL;
    char *r;
    if (portcfg_is_game_dir(dir)) g = dir;
    else { snprintf(p, sizeof p, "%s/Mercs-game", dir); if (portcfg_is_game_dir(p)) g = p; }
    if (!g) return 0;
    r = realpath(g, NULL);
    snprintf(out, n, "%s", r ? r : g);
    free(r);
    return 1;
}

/* Check the two writable components only after resolving the game root. System
 * ancestors such as macOS /tmp -> /private/tmp are valid; install/ and its
 * edition child must be real directories, never links back into cd/. */
static int overlay_component(const char *path, int create)
{
    int missing;
#ifdef _WIN32
    DWORD a = GetFileAttributesA(path);
    missing = a == INVALID_FILE_ATTRIBUTES &&
              (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND);
    if (a != INVALID_FILE_ATTRIBUTES) {
        if (a & FILE_ATTRIBUTE_REPARSE_POINT) { errno = ELOOP; return -1; }
        if (!(a & FILE_ATTRIBUTE_DIRECTORY)) { errno = ENOTDIR; return -1; }
        return 0;
    }
    if (!missing) { errno = EACCES; return -1; }
#else
    struct stat st;
    if (lstat(path, &st) == 0) {
        if (S_ISLNK(st.st_mode)) { errno = ELOOP; return -1; }
        if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return -1; }
        return 0;
    }
    missing = errno == ENOENT;
    if (!missing) return -1;
#endif
    if (!create) return 0;
    if (mkdir(path, 0755) != 0 && errno != EEXIST) return -1;
    /* An existing entry, including one found after mkdir, must pass the same
     * check. Never treat EEXIST alone as proof of a writable directory. */
    return overlay_component(path, 0);
}

int portcfg_overlay_dir(const portcfg *c, const char *edition, char *out, size_t n, int create)
{
    char install[PORTCFG_PATH], leaf[PORTCFG_PATH];
    char *root;
    int k;
    if (!c || !c->game[0] || !edition || !out || !n ||
        (strcmp(edition, "retail") != 0 && strcmp(edition, "direct3d") != 0 && strcmp(edition, "3dfx") != 0)) {
        errno = EINVAL; return -1;
    }
    out[0] = 0;
#ifdef _WIN32
    {
        HANDLE h = CreateFileA(c->game, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        DWORD needed, got;
        if (h == INVALID_HANDLE_VALUE) { errno = ENOENT; return -1; }
        needed = GetFinalPathNameByHandleA(h, NULL, 0, FILE_NAME_NORMALIZED);
        root = needed ? malloc((size_t)needed + 1) : NULL;
        got = root ? GetFinalPathNameByHandleA(h, root, needed + 1, FILE_NAME_NORMALIZED) : 0;
        if (!got || got >= needed + 1) {
            free(root); CloseHandle(h); errno = ENAMETOOLONG; return -1;
        }
        CloseHandle(h);
        /* Use ordinary DOS/UNC paths for the existing CRT filesystem calls. */
        if (strncmp(root, "\\\\?\\UNC\\", 8) == 0) { memmove(root + 2, root + 8, strlen(root + 8) + 1); root[0] = root[1] = '\\'; }
        else if (strncmp(root, "\\\\?\\", 4) == 0) memmove(root, root + 4, strlen(root + 4) + 1);
    }
#else
    root = realpath(c->game, NULL);
    if (!root) return -1;
#endif
    k = snprintf(install, sizeof install, "%s/install", root);
    free(root);
    if (k < 0 || (size_t)k >= sizeof install) { errno = ENAMETOOLONG; return -1; }
    k = snprintf(leaf, sizeof leaf, "%s/%s", install, edition);
    if (k < 0 || (size_t)k >= sizeof leaf || (size_t)k >= n) { errno = ENAMETOOLONG; return -1; }
    if (overlay_component(install, create) != 0 || overlay_component(leaf, create) != 0) return -1;
    memcpy(out, leaf, (size_t)k + 1);
    return 0;
}

int portcfg_resolve(portcfg *c, const char *exe_dir, const char *hint)
{
    char p[PORTCFG_PATH * 2];
    const char *home = getenv("HOME");
#ifdef _WIN32
    if (!home || !home[0]) home = getenv("USERPROFILE");   /* %USERPROFILE%\Mercs-game */
#endif
    if (hint) {
        if (!portcfg_game_in(hint, c->game, sizeof c->game)) return -1;
        c->install[0] = c->cd[0] = c->music[0] = c->models[0] = c->textures[0] = 0;
        c->skygnd[0] = c->skygnd_fog[0] = c->titanium[0] = c->mt32[0] = c->ultrasnd[0] = 0;
    }
    if (c->game[0] && !portcfg_is_game_dir(c->game)) return -1;
    if (!c->game[0] && !c->install[0]) {   /* look for it */
        char cand[8][PORTCFG_PATH], app[PORTCFG_PATH];
        int i, k = 0;
        snprintf(cand[k++], PORTCFG_PATH, ".");
        snprintf(cand[k++], PORTCFG_PATH, "%sMercs-game", exe_dir ? exe_dir : "");
        snprintf(cand[k++], PORTCFG_PATH, "%s../Mercs-game", exe_dir ? exe_dir : "");
        if (portcfg_app_folder(exe_dir, app, sizeof app)) {   /* in a macOS bundle: next to the .app, not inside it */
            snprintf(cand[k++], PORTCFG_PATH, "%s/Mercs-game", app);
            snprintf(cand[k++], PORTCFG_PATH, "%s/../Mercs-game", app);
        }
        snprintf(cand[k++], PORTCFG_PATH, "%s/Mercs-game", home ? home : ".");
        snprintf(cand[k++], PORTCFG_PATH, "%s", exe_dir ? exe_dir : ".");
        if (portcfg_app_folder(exe_dir, app, sizeof app)) snprintf(cand[k++], PORTCFG_PATH, "%s", app);
        for (i = 0; i < k && !c->game[0]; i++)
            if (portcfg_is_game_dir(cand[i])) {
                char *r = realpath(cand[i], NULL);
                snprintf(c->game, sizeof c->game, "%s", r ? r : cand[i]);
                free(r);
            }
    }
    if (c->game[0]) {   /* the folder's parts, unless overridden */
        snprintf(p, sizeof p, "%s/install", c->game);
        if (!c->install[0]) snprintf(c->install, sizeof c->install, "%s", p);
        snprintf(p, sizeof p, "%s/cd", c->game);
        if (!c->cd[0] && is_dir(p)) snprintf(c->cd, sizeof c->cd, "%s", p);
        snprintf(p, sizeof p, "%s/music", c->game);
        if (!c->music[0] && is_dir(p)) snprintf(c->music, sizeof c->music, "%s", p);
        snprintf(p, sizeof p, "%s/ultrasnd", c->game);
        if (!c->ultrasnd[0] && is_dir(p)) snprintf(c->ultrasnd, sizeof c->ultrasnd, "%s", p);
        snprintf(p, sizeof p, "%s/mt32", c->game);
        if (!c->mt32[0] && is_dir(p)) snprintf(c->mt32, sizeof c->mt32, "%s", p);
        snprintf(p, sizeof p, "%s/3d/models.prj", c->game);
        if (!c->models[0] && exists(p)) snprintf(c->models, sizeof c->models, "%s", p);
        snprintf(p, sizeof p, "%s/3d/textures.prj", c->game);
        if (!c->textures[0] && exists(p)) snprintf(c->textures, sizeof c->textures, "%s", p);
        snprintf(p, sizeof p, "%s/3d/skygnd.par", c->game);
        if (!c->skygnd[0] && exists(p)) snprintf(c->skygnd, sizeof c->skygnd, "%s", p);
        snprintf(p, sizeof p, "%s/3d/skygnd_pvr.par", c->game);
        if (!c->skygnd_fog[0] && exists(p)) snprintf(c->skygnd_fog, sizeof c->skygnd_fog, "%s", p);
        snprintf(p, sizeof p, "%s/3d/pvr/skygnd.par", c->game);   /* the PowerVR edition's own file: the fog densities */
        if (!c->skygnd_fog[0] && exists(p)) snprintf(c->skygnd_fog, sizeof c->skygnd_fog, "%s", p);
        snprintf(p, sizeof p, "%s/titanium", c->game);   /* the Titanium edition's 16-bit files */
        if (!c->titanium[0] && is_dir(p)) snprintf(c->titanium, sizeof c->titanium, "%s", p);
    }
    /* A 3dfx base disc already supplies the 3D models and sky; a second copy in
     * 3d/ is unnecessary. A retail base falls back to its DOS archive. */
    if (!c->models[0] && mercs_product_edition(c->cd) == PORTCFG_ED_3DFX) {
        snprintf(p, sizeof p, "%s/MERCS", c->cd);
        find_ci(p, "MW2.PRJ", c->models, sizeof c->models);
        if (!c->skygnd[0]) find_ci(p, "SKYGND.PAR", c->skygnd, sizeof c->skygnd);
    }
    if (!c->models[0] && c->install[0]) {
        if (!find_ci(c->install, "MW2.PRJ", c->models, sizeof c->models) && c->cd[0]) {
            snprintf(p, sizeof p, "%s/MERCS", c->cd);
            if (!find_ci(p, "MW2.PRJ", c->models, sizeof c->models)) find_ci(c->cd, "MW2.PRJ", c->models, sizeof c->models);
        }
    }
    if (!c->textures[0]) snprintf(c->textures, sizeof c->textures, "%s", c->models);
    {   /* the 3D archives present: a models archive that isn't the DOS one (the fallback above, or models= naming it) */
        char d[PORTCFG_PATH] = "";
        if (!c->install[0] || !find_ci(c->install, "MW2.PRJ", d, sizeof d)) {
            snprintf(p, sizeof p, "%s/MERCS", c->cd);
            if (!c->cd[0] || (!find_ci(p, "MW2.PRJ", d, sizeof d) && !find_ci(c->cd, "MW2.PRJ", d, sizeof d))) d[0] = 0;
        }
        c->no3d = !(c->models[0] && (strcmp(c->models, d) != 0 || mercs_product_edition(c->cd) == PORTCFG_ED_3DFX));
    }
    (void)errno;
    return c->game[0] && c->install[0] ? 0 : -1;
}

const char *const PORTCFG_ED_NAME[PORTCFG_ED_COUNT] = {"enhanced", "dos", "3dfx", "ati", "s3", "pvr", "mga", "rendition", "direct3d"};
const char *const PORTCFG_TX_NAME[PORTCFG_TX_COUNT] = {"original", "titanium"};
const char *const PORTCFG_FILTER_NAME[PORTCFG_FILTER_COUNT] = {"original", "nearest", "linear"};
const char *const PORTCFG_GPU_NAME[PORTCFG_GPU_COUNT] = {"auto", "d3d12", "vulkan"};
const char *const PORTCFG_VID_NAME[PORTCFG_VID_COUNT] = {"original", "smooth", "scanlines", "crt", "crt_curved"};
const char *const PORTCFG_FIT_NAME[PORTCFG_FIT_COUNT] = {"4:3", "fill", "stretch"};
const char *const PORTCFG_SALVAGE_NAME[PORTCFG_SALVAGE_COUNT] = {"auto", "contract", "dynamic"};
int portcfg_dynamic_salvage(const portcfg *c, int renderer)
{
    int k = c ? c->salvage : PORTCFG_SALVAGE_AUTO;
    if (k == PORTCFG_SALVAGE_DYNAMIC) return 1;
    if (k == PORTCFG_SALVAGE_CONTRACT) return 0;
    return renderer == PORTCFG_ED_D3D;
}

static void cp(char *dst, const char *src) { snprintf(dst, PORTCFG_PATH, "%s", src ? src : ""); }

const char *portcfg_mercs_name(int renderer)
{
    switch (renderer) {
    case PORTCFG_ED_DOS: return "dos";
    case PORTCFG_ED_D3D: return "direct3d";
    case PORTCFG_ED_3DFX: return "3dfx";
    default: return NULL;
    }
}

const char *portcfg_mercs_label(int renderer)
{
    switch (renderer) {
    case PORTCFG_ED_DOS: return "DOS";
    case PORTCFG_ED_D3D: return "Titanium Direct3D";
    case PORTCFG_ED_3DFX: return "3dfx";
    default: return NULL;
    }
}

const char *portcfg_mercs_overlay(int renderer)
{
    switch (renderer) {
    case PORTCFG_ED_DOS: return "retail";
    case PORTCFG_ED_D3D: return "direct3d";
    case PORTCFG_ED_3DFX: return "3dfx";
    default: return NULL;
    }
}

int portcfg_mercs_parse(const char *name)
{
    if (!name) return -1;
    if (strcmp(name, "dos") == 0) return PORTCFG_ED_DOS;
    if (strcmp(name, "direct3d") == 0 || strcmp(name, "d3d") == 0) return PORTCFG_ED_D3D;
    if (strcmp(name, "3dfx") == 0) return PORTCFG_ED_3DFX;
    return -1;
}

static unsigned profile_rd16(const uint8_t *p) { return (unsigned)p[0] | (unsigned)p[1] << 8; }

/* Edition availability needs the Mercs 1.2x CEL format, not just a similarly
 * named PRJ. Reuse the archive reader so bounds, record headers and checksums
 * receive the same validation as normal loads. Plain 8-bit CELs coexist in the
 * accelerated archive; finding one of those does not identify a DOS archive. */
static int mercs16_archive(const char *path)
{
    prj_archive *a;
    int type, i, ok = 0;
    if (!path[0] || !(a = prj_open(path, NULL, 0))) return 0;
    type = prj_find_type(a, "CEL");
    for (i = 0; i < prj_symbol_count(a, type); i++) {
        prj_record r;
        if (prj_read(a, type, prj_symbol_id(a, type, i), &r) != PRJ_OK) break;
        if (r.size >= 12 && memcmp(r.data, "1.2", 3) == 0) {
            size_t w = profile_rd16(r.data + 6), h = profile_rd16(r.data + 8);
            size_t colors = profile_rd16(r.data + 10), pixels = w * h;
            ok = colors > 1 && colors <= 256 && w && h &&
                 12 + colors * 4 <= r.size && pixels <= r.size - 12 - colors * 4;
        }
        prj_record_free(&r);
        if (ok) break;
    }
    prj_close(a);
    return ok;
}

static int profile_file(const char *path)
{
    struct stat st;
    return path[0] && stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

int portcfg_edition(const portcfg *c, int ed, char *models, char *textures, char *kind, char *sky, char *fog)
{
    char a[PORTCFG_PATH * 2], b[PORTCFG_PATH * 2];
    /* These are complete edition profiles. Their model/mission and texture
     * archives must stay paired: even equal IDs have differing original data.
     * The dedicated 3Dfx disc also uses 1.2x CELs, unlike base MW2's 3dfx art. */
    if (ed == PORTCFG_ED_D3D || ed == PORTCFG_ED_3DFX) {
        models[0] = textures[0] = kind[0] = sky[0] = fog[0] = 0;
        if (ed == PORTCFG_ED_D3D) {
            if (portcfg_titanium(c, a, b) != 0) return -1;
        } else {
            if (c->no3d) return -1;
            cp(a, c->models); cp(b, c->skygnd);
        }
        if (!profile_file(b) || !mercs16_archive(a)) return -1;
        cp(models, a); cp(textures, a); cp(kind, "mercs16"); cp(sky, b);
        return 0;
    }
    /* The base marker is an edition identity, not just a game-family check. */
    if (ed == PORTCFG_ED_DOS && mercs_product_edition(c->cd) == PORTCFG_ED_3DFX) return -1;
    /* the enhanced mix: the 3Dfx archive's models and data, the ATi archive's textures (the best-kept copy), the 3Dfx sky
     * file, the PowerVR fog */
    cp(models, c->models); cp(textures, c->textures); cp(kind, c->textures_ati ? "ati" : "3dfx"); cp(sky, c->skygnd); cp(fog, c->skygnd_fog);
    /* without the 3D editions' archives (no 3d/ folder) only the DOS look: the DOS archive drawn by a 3D look is bare
     * untextured geometry with no sky or ground (player report v1.0.1) */
    if (c->no3d && ed != PORTCFG_ED_DOS && ed != PORTCFG_ED_S3 && ed != PORTCFG_ED_MGA && ed != PORTCFG_ED_PVR) return -1;
    if (ed == PORTCFG_ED_ENHANCED) return 0;
    /* RENDITION (the Titanium plugin zredline.dll): the enhanced mix's archives for the models, data and the texture names
     * the Titanium archive lacks; available with the Titanium folder only */
    if (ed == PORTCFG_ED_RENDITION) return portcfg_titanium(c, NULL, NULL) == 0 ? 0 : -1;
    if (ed == PORTCFG_ED_DOS) {   /* original retail data, separate from the writable overlay */
        snprintf(b, sizeof b, "%s/MERCS", c->cd);
        if (!c->cd[0] || (!find_ci(b, "MW2.PRJ", a, sizeof a) && !find_ci(c->cd, "MW2.PRJ", a, sizeof a))) return -1;
        cp(models, a); cp(textures, a); cp(kind, "dos"); cp(sky, ""); cp(fog, "");
        return 0;
    }
    if (!c->game[0]) return -1;
    if (ed == PORTCFG_ED_ATI) {
        snprintf(a, sizeof a, "%s/3d/ati/skygnd.par", c->game);
        if (!c->textures[0] || !c->textures_ati) return -1;
        cp(models, c->textures); cp(kind, "ati"); cp(fog, "");
        if (exists(a)) cp(sky, a);
        return 0;
    }
    {
        const char *dir = ed == PORTCFG_ED_S3 ? "s3" : ed == PORTCFG_ED_MGA ? "mga" : "pvr";
        snprintf(a, sizeof a, "%s/3d/%s/MW2.PRJ", c->game, dir);
        snprintf(b, sizeof b, "%s/3d/%s/skygnd.par", c->game, dir);
    }
    if (!exists(a)) return -1;
    cp(models, a); cp(textures, a); cp(sky, exists(b) ? b : c->skygnd);
    /* the S3 edition's textures are the 3Dfx edition's, byte for byte; the Mystique's RGB555 with 0x0000 keyed */
    cp(kind, ed == PORTCFG_ED_S3 ? "3dfx" : ed == PORTCFG_ED_MGA ? "mga" : "pvr");
    cp(fog, ed == PORTCFG_ED_PVR && exists(b) ? b : "");
    return 0;
}

int portcfg_titanium(const portcfg *c, char *prj, char *sky)
{
    char a[PORTCFG_PATH], b[PORTCFG_PATH];
    if (!c->titanium[0] || !find_ci(c->titanium, "MW2.PRJ", a, sizeof a)) return -1;
    if (!find_ci(c->titanium, "SKYGND.PAR", b, sizeof b)) b[0] = 0;
    if (prj) cp(prj, a);
    if (sky) cp(sky, b);
    return 0;
}

int portcfg_texset_ok(const portcfg *c, int renderer, int texset)
{
    if (portcfg_mercs_name(renderer)) return texset == PORTCFG_TX_ORIGINAL;
    if (texset == PORTCFG_TX_ORIGINAL) return renderer != PORTCFG_ED_RENDITION;
    return texset == PORTCFG_TX_TITANIUM && renderer != PORTCFG_ED_DOS && portcfg_titanium(c, NULL, NULL) == 0;
}

int portcfg_texset_for(const portcfg *c, int renderer, int texset)
{
    if (portcfg_mercs_name(renderer)) return PORTCFG_TX_ORIGINAL;
    if (renderer == PORTCFG_ED_RENDITION) return PORTCFG_TX_TITANIUM;
    return portcfg_texset_ok(c, renderer, texset) ? texset : PORTCFG_TX_ORIGINAL;
}

int portcfg_look(const portcfg *c, int renderer, int texset, char *models, char *textures, char *kind, char *sky, char *fog)
{
    char a[PORTCFG_PATH];
    if (portcfg_mercs_name(renderer)) texset = PORTCFG_TX_ORIGINAL;
    if (renderer == PORTCFG_ED_RENDITION) texset = PORTCFG_TX_TITANIUM;   /* (its only set) */
    if (portcfg_edition(c, renderer, models, textures, kind, sky, fog) != 0 || !portcfg_texset_ok(c, renderer, texset)) return -1;
    if (texset == PORTCFG_TX_TITANIUM) {   /* the Titanium sky file (its sky dome, ground, fog colours and depths) */
        if (portcfg_titanium(c, NULL, a) != 0 || !models[0]) return -1;
        cp(sky, a); cp(fog, "");
    }
    return 0;
}
