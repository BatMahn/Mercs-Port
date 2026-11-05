#include "mercsoptions.h"
#include "mercsfile.h"
#include "datapath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

static int failure(char *error, size_t size, const char *name)
{
    if (error && size) snprintf(error, size, "Could not use %s: %s", name, strerror(errno));
    return -1;
}

uint32_t mercs_options_get(const mercs_options *o, int offset)
{
    const uint8_t *p;
    if (!o || offset < 0 || offset > MERCS_SND_BYTES - 4) return 0;
    p = o->snd + offset;
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

void mercs_options_set(mercs_options *o, int offset, uint32_t v)
{
    uint8_t *p;
    if (!o || offset < 0 || offset > MERCS_SND_BYTES - 4) return;
    p = o->snd + offset;
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

int mercs_options_volume(const mercs_options *o, int offset)
{
    int32_t v = (int32_t)mercs_options_get(o, offset);
    return v < 0 ? 0 : v > 65536 ? 65536 : (int)v;
}

void mercs_options_defaults(mercs_options *o)
{
    int i;
    memset(o, 0, sizeof *o);
    o->snd_size = MERCS_SND_BYTES; o->dif_size = MERCS_DIF_BYTES;
    for (i = 0; i <= 12; i += 4) mercs_options_set(o, i, 65536);
    mercs_options_set(o, 0x10, 15);
    for (i = 0x14; i <= 0x30; i += 4) mercs_options_set(o, i, 1);
    mercs_options_set(o, 0x34, 8);
    mercs_options_set(o, MO_TRANSITIONS, 1); mercs_options_set(o, MO_SUBTITLES, 1);
    o->dif[2] = o->dif[3] = o->dif[4] = o->dif[5] = 1;
}

static int read_record(uint8_t *bytes, size_t *length, const char *overlay,
                       const char *cd, const char *name, char *error, size_t size)
{
    char path[2048], relative[64];
    uint8_t raw[MERCS_OPTIONS_MAX + 1];
    size_t n;
    FILE *f;
    int found = overlay && overlay[0] && !dp_resolve_in(overlay, name, path, sizeof path);
    if (!found && cd && cd[0]) {
        snprintf(relative, sizeof relative, "MERCS/%s", name);
        found = !dp_resolve_in(cd, relative, path, sizeof path) ||
                !dp_resolve_in(cd, name, path, sizeof path);
    }
    if (!found) return 0;
    f = fopen(path, "rb");
    if (!f) return failure(error, size, name);
    n = fread(raw, 1, sizeof raw, f);
    if (ferror(f)) { int e = errno; fclose(f); errno = e ? e : EIO; return failure(error, size, name); }
    fclose(f);
    if (n > MERCS_OPTIONS_MAX) { errno = EFBIG; return failure(error, size, name); }
    memcpy(bytes, raw, n);
    if (n > *length) *length = n;
    return 0;
}

int mercs_options_load(mercs_options *o, const char *overlay, const char *cd,
                       char *error, size_t size)
{
    if (!o) { errno = EINVAL; return failure(error, size, "combat settings"); }
    mercs_options_defaults(o);
    if (error && size) error[0] = 0;
    if (read_record(o->snd, &o->snd_size, overlay, cd, "MW2SND.CFG", error, size) ||
        read_record(o->dif, &o->dif_size, overlay, cd, "MW2DIF.CFG", error, size)) return -1;
    return 0;
}


int mercs_options_save_sound(const mercs_options *o, const char *dir, char *error, size_t size)
{
    if (!o || o->snd_size < MERCS_SND_BYTES || o->snd_size > MERCS_OPTIONS_MAX) {
        errno = EINVAL; return failure(error, size, "MW2SND.CFG");
    }
    return mercs_atomic_write(dir, "MW2SND.CFG", o->snd, o->snd_size, error, size);
}

int mercs_options_save(const mercs_options *o, const char *dir, char *error, size_t size)
{
    if (!o || o->dif_size < MERCS_DIF_BYTES || o->dif_size > MERCS_OPTIONS_MAX) {
        errno = EINVAL; return failure(error, size, "MW2DIF.CFG");
    }
    if (mercs_options_save_sound(o, dir, error, size)) return -1;
    return mercs_atomic_write(dir, "MW2DIF.CFG", o->dif, o->dif_size, error, size);
}

int mercs_options_resolution(const mercs_options *o)
{
    const uint8_t *p = o->snd + MO_DRIVER;
    return !p[0] || p[0] == 'm' || p[0] == 'M' ? 0 : p[4] == '4' ? 1 : 2;
}

void mercs_options_set_resolution(mercs_options *o, int resolution)
{
    /* Original 0x412b4 uses strncpy(...,15), leaving the sixteenth byte alone. */
    const char *value = resolution == 1 ? "vesa480.dll" : resolution == 2 ? "vesa768.dll" : "";
    memset(o->snd + MO_DRIVER, 0, 15);
    memcpy(o->snd + MO_DRIVER, value, strlen(value));
}

int mercs_options_transition(const mercs_options *o, const char *movie)
{
    /* The original guards MLAND, MLAND2 and MTO. Intro, death/ejection and
     * campaign-story movies still play. Room door animations are separate. */
    return mercs_options_get(o, MO_TRANSITIONS) != 0 ||
           (strcasecmp(movie, "MLAND") && strcasecmp(movie, "MLAND2") && strcasecmp(movie, "MTO"));
}
