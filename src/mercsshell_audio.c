#include "mercsshell_audio.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSA_RESOURCE_COUNT 120

static unsigned wave_u16(const uint8_t *p) { return (unsigned)p[0] | (unsigned)p[1] << 8; }
static uint32_t wave_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

int mercs_shell_wave_decode(const uint8_t *data, size_t size, mercs_shell_wave *out)
{
    const uint8_t *fmt = NULL, *pcm = NULL;
    size_t pos, end, bytes = 0, count, i;
    unsigned channels, bits, align;
    uint32_t rate;
    mercs_shell_wave w = {0};
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!data || size < 12 || memcmp(data, "RIFF", 4) || memcmp(data + 8, "WAVE", 4)) return -1;
    if ((uint64_t)wave_u32(data + 4) + 8 > size) return -1;
    end = (size_t)wave_u32(data + 4) + 8;
    if (end < 12) return -1;
    for (pos = 12; pos < end;) {
        size_t n, padded;
        if (end - pos < 8) return -1;
        n = wave_u32(data + pos + 4); pos += 8;
        if (n > end - pos) return -1;
        if (!memcmp(data + pos - 8, "fmt ", 4)) {
            if (fmt || n < 16) return -1;
            fmt = data + pos;
        } else if (!memcmp(data + pos - 8, "data", 4)) {
            if (pcm) return -1;
            pcm = data + pos; bytes = n;
        }
        padded = n + (n & 1u);
        /* Original resources63/94 declare the odd data payload as the RIFF
         * end, leaving its padding byte outside that declared size. The full
         * payload is required; only this terminal padding may be omitted. */
        if (padded > end - pos) {
            if (n != end - pos) return -1;
            pos = end;
        } else pos += padded;
    }
    if (!fmt || !pcm || wave_u16(fmt) != 1) return -1;
    channels = wave_u16(fmt + 2); rate = wave_u32(fmt + 4);
    align = wave_u16(fmt + 12); bits = wave_u16(fmt + 14);
    if ((channels != 1 && channels != 2) || (bits != 8 && bits != 16) ||
        !rate || rate > 384000 || align != channels * (bits / 8) ||
        !bytes || bytes % align || bytes / align > (size_t)INT_MAX / channels) return -1;
    count = bytes / (bits / 8);
    if (count > SIZE_MAX / sizeof(int16_t)) return -1;
    w.samples = malloc(count * sizeof(int16_t));
    if (!w.samples) return -1;
    for (i = 0; i < count; i++) {
        int value;
        if (bits == 8) value = ((int)pcm[i] - 128) * 256;
        else {
            unsigned raw = wave_u16(pcm + i * 2);
            value = raw < 32768 ? (int)raw : (int)raw - 65536;
        }
        w.samples[i] = (int16_t)value;
    }
    w.frames = (int)(bytes / align); w.channels = (int)channels;
    w.rate = (int)rate; w.original_bits = (int)bits;
    *out = w;
    return 0;
}

void mercs_shell_wave_free(mercs_shell_wave *w)
{
    if (w) { free(w->samples); memset(w, 0, sizeof *w); }
}

struct mercs_shell_audio {
    sfx *mixer;
    const shelldb *database;
    mercs_shell_wave waves[MSA_RESOURCE_COUNT + 1];
    uint8_t attempted[MSA_RESOURCE_COUNT + 1];
    mercs_shell_audio_page page;
    int ambient, ambient_volume, gate, queued, queued_volume;
    int suspended, text_active;
    int idle_enabled, idle_resource;
    uint32_t random_state;
};

const mercs_shell_wave *mercs_shell_audio_wave(mercs_shell_audio *a, int resource)
{
    uint8_t *data = NULL;
    long size;
    if (!a || resource < 1 || resource > MSA_RESOURCE_COUNT) return NULL;
    if (!a->attempted[resource]) {
        a->attempted[resource] = 1;
        size = shelldb_entry(a->database, resource - 1, &data);
        if (size > 0) (void)mercs_shell_wave_decode(data, (size_t)size, &a->waves[resource]);
        free(data);
        if (!a->waves[resource].samples)
            fprintf(stderr, "mercs shell audio: DATABASE resource %d is absent or not supported PCM WAV\n", resource);
    }
    return a->waves[resource].samples ? &a->waves[resource] : NULL;
}

mercs_shell_audio *mercs_shell_audio_create(sfx *mixer, const shelldb *database)
{
    mercs_shell_audio *a;
    if (!database) return NULL;
    a = calloc(1, sizeof *a);
    if (a) { a->mixer = mixer; a->database = database; a->page = MSA_PAGE_NONE; a->random_state = 1; }
    return a;
}

int mercs_shell_audio_resource_playing(mercs_shell_audio *a, int resource)
{
    return a && resource > 0 && resource <= MSA_RESOURCE_COUNT &&
           sfx_pcm_playing(a->mixer, a->waves[resource].samples);
}

static void msa_stop(mercs_shell_audio *a, int resource)
{
    if (a && resource > 0 && resource <= MSA_RESOURCE_COUNT)
        sfx_stop_pcm(a->mixer, (const unsigned char *)a->waves[resource].samples);
}

/* One original AIL sample object per currently used resource. A repeated play
 * restarts that sample rather than overlapping copies of a button or line. */
static int msa_play(mercs_shell_audio *a, int resource, int volume, int loop)
{
    const mercs_shell_wave *w;
    if (!a || !a->mixer || a->suspended) return 0;
    w = mercs_shell_audio_wave(a, resource);
    if (!w) return 0;
    msa_stop(a, resource);
    if (sfx_play_pcm_s16(a->mixer, w->samples, w->frames, w->channels, w->rate,
                         SFX_VOL(volume), loop) != 0) return 0;
    if (getenv("MERCS_SHELL_AUDIO_TRACE"))
        fprintf(stderr, "SHELLAUDIO resource=%d index=%d volume=%d loop=%d frames=%d rate=%d channels=%d bits=%d\n",
                resource, resource - 1, volume, loop, w->frames, w->rate, w->channels, w->original_bits);
    return 1;
}

static void msa_stop_owned(mercs_shell_audio *a)
{
    int i;
    if (!a) return;
    for (i = 1; i <= MSA_RESOURCE_COUNT; i++) msa_stop(a, i);
    a->gate = a->queued = 0;
}

void mercs_shell_audio_leave(mercs_shell_audio *a)
{
    if (!a) return;
    msa_stop_owned(a);
    a->ambient = a->ambient_volume = a->text_active = a->suspended = 0;
    a->idle_enabled = a->idle_resource = 0;
    a->page = MSA_PAGE_NONE;
}

void mercs_shell_audio_destroy(mercs_shell_audio *a)
{
    int i;
    if (!a) return;
    mercs_shell_audio_leave(a);
    for (i = 1; i <= MSA_RESOURCE_COUNT; i++) mercs_shell_wave_free(&a->waves[i]);
    free(a);
}

void mercs_shell_audio_enter(mercs_shell_audio *a, mercs_shell_audio_page page,
                            int registered, mercs_shell_audio_page previous)
{
    if (!a) return;
    mercs_shell_audio_leave(a); a->page = page; a->idle_enabled = 1;
    /* Values are original constructor 0x6c150 resource/0x6c6ec volume,
     * 0x6c580 sets repeat forever, 0x6c5c0 starts the sample. */
    switch (page) {
    case MSA_PAGE_MENU: a->ambient = 48; a->ambient_volume = 30; break;
    case MSA_PAGE_REGISTER: case MSA_PAGE_HALL: case MSA_PAGE_NEWS:
        a->ambient = 62; a->ambient_volume = 5; break;
    case MSA_PAGE_OFFICE: case MSA_PAGE_COMPUTER: case MSA_PAGE_CONTRACTS:
        a->ambient = 60; a->ambient_volume = 30; break;
    case MSA_PAGE_DROPSHIP: case MSA_PAGE_COMMAND: case MSA_PAGE_BRIEFING: case MSA_PAGE_DEBRIEF:
        a->ambient = 63; a->ambient_volume = 30; break;
    case MSA_PAGE_ROSTER: a->ambient = 95; a->ambient_volume = 30; break;
    case MSA_PAGE_SAVE: a->ambient = 104; a->ambient_volume = 50; break;
    case MSA_PAGE_REPAIR: a->ambient = 54; a->ambient_volume = 17; break;
    case MSA_PAGE_FINANCE: case MSA_PAGE_DROP_FINANCE:
    case MSA_PAGE_INVENTORY: case MSA_PAGE_DROP_INVENTORY: case MSA_PAGE_PERSONNEL:
        a->ambient = 60; a->ambient_volume = 30; break;
    case MSA_PAGE_FACTORY: a->ambient = 58; a->ambient_volume = 20; break;
    case MSA_PAGE_GROUPS: a->ambient = 54; a->ambient_volume = 50; break;
    case MSA_PAGE_INSTANT: case MSA_PAGE_INSTANT_PARAMS:
        a->ambient = 54; a->ambient_volume = 30; break;
    case MSA_PAGE_MECHLAB: a->ambient = 101; a->ambient_volume = 10; break;
    case MSA_PAGE_TRADING: a->ambient = 58; a->ambient_volume = 40; break;
    case MSA_PAGE_MECHBAY: case MSA_PAGE_MECHOPS:
        a->ambient = 57; a->ambient_volume = 10; break;
    default: break;
    }
    /* In the hall constructor its first-registration greeting precedes loop62. */
    if (page == MSA_PAGE_HALL && !registered) (void)msa_play(a, 68, 50, 0);
    if (page == MSA_PAGE_CONTRACTS) (void)msa_play(a, 72, 50, 0);        /* 0x2b61b, before loop60 */
    if (page == MSA_PAGE_FINANCE || page == MSA_PAGE_DROP_FINANCE)
        (void)msa_play(a, page == MSA_PAGE_FINANCE ? 73 : 50, 50, 0);
    if (page == MSA_PAGE_INVENTORY || page == MSA_PAGE_DROP_INVENTORY) (void)msa_play(a, 76, 50, 0);
    if (page == MSA_PAGE_PERSONNEL) (void)msa_play(a, 74, 50, 0);
    if (page == MSA_PAGE_CREW && previous == MSA_PAGE_MECHBAY) (void)msa_play(a, 120, 50, 0); /*0x40193*/
    if (a->ambient) (void)msa_play(a, a->ambient, a->ambient_volume, 1);
    if (page == MSA_PAGE_REGISTER) (void)msa_play(a, 79, 50, 0);             /* 0x6355d */
    else if (page == MSA_PAGE_HALL) {
        if (previous == MSA_PAGE_OFFICE) (void)msa_play(a, 66, 30, 0);    /* 0x6d174 */
    } else if (page == MSA_PAGE_OFFICE) {
        if (previous == MSA_PAGE_HALL) (void)msa_play(a, 66, 40, 0);      /* 0x60aee */
        else if (previous == MSA_PAGE_MECHBAY) (void)msa_play(a, 66, 25, 0); /* 0x60adb */
    } else if (page == MSA_PAGE_SAVE) (void)msa_play(a, 77, 50, 0);        /* 0x6af31 */
}

typedef struct { uint8_t resource, volume, blocking, if_idle, before, after; } msa_cue;
static const msa_cue cue_table[MSA_CUE_COUNT] = {
    [MSA_CUE_CREW_SELECT]         = {53,50,1,0,0,0}, /*0x40342*/
    [MSA_CUE_CREW_BIO_EXIT]       = {56,50,1,0,0,0}, /*0x3ffd2*/
    [MSA_CUE_INSTANT_SELECT]       = {52,50,0,0,0,0},
    [MSA_CUE_INSTANT_EXIT]         = {56,50,1,0,0,0},
    [MSA_CUE_INSTANT_PARAMS]       = {52,50,1,0,0,0},
    [MSA_CUE_INSTANT_PARAM_SELECT] = {52,50,0,0,0,0},
    [MSA_CUE_INSTANT_PARAM_EXIT]   = {56,30,1,0,0,0},
    [MSA_CUE_MENU_COMMANDER]     = {81,50,1,0,52,0}, /* 0x6e080/6e0ad */
    [MSA_CUE_MENU_MERCENARY]     = {80,50,1,0,52,0}, /* 0x6dfcc/6dff9 */
    [MSA_CUE_MENU_MERCNET]       = {52,50,1,0,0,53}, /* 0x6e153/6e164: sequential */
    [MSA_CUE_MENU_INSTANT]       = {75,50,1,0,52,0}, /* 0x6df6c/6df99 */
    [MSA_CUE_MENU_OPTIONS]       = {53,50,1,0,52,0}, /* 0x6e186/6e1b3 */
    [MSA_CUE_REGISTER_ACCEPT]    = {56,50,1,0,0,0},  /* 0x638dd */
    [MSA_CUE_HALL_TERMINAL]      = {83,20,1,0,0,0},  /* 0x6d399 */
    [MSA_CUE_HALL_OFFICE]        = {66,30,0,0,0,0},  /* 0x6d446: while door animates */
    [MSA_CUE_HALL_BOARD]         = {67,40,0,0,0,0},  /* 0x6d2c2/6d361 */
    [MSA_CUE_HALL_UNREGISTERED]  = {69,50,0,0,0,0},  /* 0x6d215/6d3e1 */
    [MSA_CUE_HALL_NEED_CONTRACT] = {70,40,0,1,0,0},  /* 0x6d2e4/6d2f2 */
    [MSA_CUE_HALL_NO_MECH]       = {71,50,0,0,0,0},  /* 0x6d253 */
    [MSA_CUE_NEWS_EXIT]          = {56,50,1,0,0,0},  /* 0x5df10 */
    [MSA_CUE_OFFICE_COMPUTER]    = {61,55,1,0,0,0},  /* 0x60b78 */
    [MSA_CUE_OFFICE_STARPORT]    = {66,40,0,0,0,0},  /* 0x60c80 */
    [MSA_CUE_OFFICE_MECHBAY]     = {66,25,0,0,0,0},  /* 0x60d6e */
    [MSA_CUE_COMPUTER_SELECT]    = {52,50,1,0,0,0},  /* 0x2ac64/2acb9/2ad0e/2ad63 */
    [MSA_CUE_COMPUTER_EXIT]      = {56,50,1,0,0,0},  /* 0x2adaf */
    [MSA_CUE_CONTRACT_SELECT]    = {52,50,0,0,0,0},  /* 0x2bde2/2be9c */
    [MSA_CUE_CONTRACT_ACCEPT]    = {56,50,1,0,0,0},  /* 0x2bc47 */
    [MSA_CUE_CONTRACT_EXIT]      = {56,50,1,0,0,0},  /* 0x2bbe9 */
    [MSA_CUE_CABIN_COMMAND]      = {61,40,1,0,0,0},  /* 0x3df2c */
    [MSA_CUE_COMMAND_SELECT]     = {52,30,1,0,0,0},  /* 0x3d64e and subsequent actions */
    [MSA_CUE_COMMAND_EXIT]       = {56,30,1,0,0,0},  /* 0x3d5e7 */
    [MSA_CUE_ROSTER_SELECT]      = {52,20,0,0,0,0},  /* 0x3f72c/3f7c6 */
    [MSA_CUE_ROSTER_ROW]         = {51,20,0,0,0,0},  /* 0x3e4c3/3e396 */
    [MSA_CUE_ROSTER_EXIT]        = {56,30,1,0,0,0},  /* 0x3f910; failed gate uses async play */
    [MSA_CUE_BRIEFING_LAUNCH]    = {52,5,1,0,0,0},   /* 0x62b1c; constructor default5 */
    [MSA_CUE_BRIEFING_EXIT]      = {56,50,1,0,0,0},  /* 0x62958 */
    [MSA_CUE_DEBRIEF_SELECT]     = {52,50,1,0,0,0},  /* 0x3c4b3/515/57f/5ff */
    [MSA_CUE_SAVE_SELECT]       = {0,0,0,0,0,0},    /* original list selection is silent */
    [MSA_CUE_SAVE_LOAD]         = {78,50,1,0,0,0},  /* 0x6b1bf..1dc, AFTER load succeeds */
    [MSA_CUE_SAVE_WRITE]        = {82,50,1,0,0,0},  /* 0x6b2eb..308, AFTER write succeeds */
    [MSA_CUE_SAVE_EXIT]         = {0,0,0,0,0,0},    /* 0x6b373..3cc: no sound call */
    [MSA_CUE_REPAIR_SELECT]     = {52,5,0,0,0,0},   /* 0x66ec9 etc., constructor default5 */
    [MSA_CUE_REPAIR_CHANGE]     = {50,40,0,0,0,0},  /* location change0x64f32/657dc */
    [MSA_CUE_REPAIR_EXIT]       = {56,40,1,0,0,0},  /* 0x66e23 */
    [MSA_CUE_REPAIR_ROW]        = {51,5,0,0,0,0},   /* critical/group selection0x658da/659df */
    [MSA_CUE_REPAIR_APPLY]      = {102,5,0,0,0,0},  /* successful repair0x64421/647a2/64829/66fe2 */
    [MSA_CUE_ROSTER_REJECT]     = {56,30,0,0,0,0},  /* 0x3f891/3f8cb, before the rejection dialog */
    [MSA_CUE_LEDGER_EXIT]       = {56,30,1,0,0,0},
    [MSA_CUE_PERSONNEL_SELECT]  = {52,50,0,0,0,0},
    [MSA_CUE_PERSONNEL_EXIT]    = {56,30,1,0,0,0},
    [MSA_CUE_INVENTORY_CATEGORY]= {52,50,0,0,0,0},
    [MSA_CUE_INVENTORY_MECH]    = {51,20,0,0,0,0},
    [MSA_CUE_INVENTORY_EXIT]    = {56,30,1,0,0,0},
    [MSA_CUE_TRADING_SELECT]    = {52,50,0,0,0,0},
    [MSA_CUE_TRADING_EXIT]      = {56,40,1,0,0,0},
    [MSA_CUE_MECHBAY_OPS]      = {61,40,1,0,0,0},
    [MSA_CUE_MECHBAY_OFFICE]   = {66,25,1,0,0,0},
    [MSA_CUE_MECHOPS_SELECT]   = {52,50,1,0,0,0},
    [MSA_CUE_MECHOPS_EXIT]     = {56,50,1,0,0,0},
    [MSA_CUE_FACTORY_SELECT]   = {52,20,0,0,0,0},
    [MSA_CUE_FACTORY_MECH]     = {51,20,0,0,0,0},
    [MSA_CUE_FACTORY_EXIT]     = {56,55,1,0,0,0},
    [MSA_CUE_GROUPS_SELECT]    = {52,5,0,0,0,0},
    [MSA_CUE_GROUPS_DEFAULT]   = {52,5,1,0,0,0},
    [MSA_CUE_GROUPS_EXIT]      = {56,50,1,0,0,0},
    /* DOS0x35964 constructors: resource is fourth (ECX) argument, volume
     * follows at0x6c6ec. Completion0x30d9c waits for102 before committing. */
    [MSA_CUE_MECHLAB_SELECT]   = {52,50,0,0,0,0},
    [MSA_CUE_MECHLAB_ROW]      = {51,50,0,0,0,0},
    [MSA_CUE_MECHLAB_CHANGE]   = {50,40,0,0,0,0},
    [MSA_CUE_MECHLAB_APPLY]    = {102,50,1,0,0,0},
    [MSA_CUE_MECHLAB_EXIT]     = {56,40,1,0,0,0},
};

int mercs_shell_audio_trigger(mercs_shell_audio *a, mercs_shell_audio_cue cue)
{
    const msa_cue *c;
    if (!a || a->suspended || cue < 0 || cue >= MSA_CUE_COUNT) return 0;
    /* Original blocking calls do not process another shell click. */
    if (mercs_shell_audio_busy(a)) return 1;
    c = &cue_table[cue];
    if (!c->resource || (c->if_idle && mercs_shell_audio_resource_playing(a, c->resource))) return 0;
    if (c->before) (void)msa_play(a, c->before, 50, 0);
    if (msa_play(a, c->resource, c->volume, 0) && c->blocking) {
        a->gate = c->resource; a->queued = c->after; a->queued_volume = 50;
    }
    return a->gate != 0;
}

void mercs_shell_audio_tick(mercs_shell_audio *a)
{
    /* DOS tables0xc1260/0xc1e34/0xc0bd8. Six actual silent resource118
     * entries provide authored pauses. Never replace them with new ambience. */
    static const uint8_t idle_table[25] = {
        105,106,107,108,109,110,111,112,113,114,
        85,86,87,88,89,90,91,92,117,118,118,118,118,118,118
    };
    /* DOS0x4b110/0x4ba6c: the first23 entries belong to the room,
     * all29 to its operations screen. Entries118 are original silent WAVs. */
    static const uint8_t mech_idle[29] = {
        105,106,107,108,109,110,111,112,113,114,96,97,98,117,
        118,118,118,118,118,118,118,118,118,118,118,118,118,118,118
    };
    unsigned draw;
    if (!a || !a->mixer || a->suspended) return;
    if (a->gate) {
        if (mercs_shell_audio_resource_playing(a, a->gate)) return;
        a->gate = 0;
        if (a->queued) {
            int resource = a->queued;
            a->queued = 0;
            if (msa_play(a, resource, a->queued_volume, 0)) a->gate = resource;
        }
        return;
    }
    if (!a->idle_enabled ||
        (a->page != MSA_PAGE_REGISTER && a->page != MSA_PAGE_HALL && a->page != MSA_PAGE_NEWS &&
         a->page != MSA_PAGE_MECHBAY && a->page != MSA_PAGE_MECHOPS) ||
        (a->page == MSA_PAGE_HALL && mercs_shell_audio_resource_playing(a, 68)) ||
        mercs_shell_audio_resource_playing(a, a->idle_resource)) return;
    a->random_state = a->random_state * UINT32_C(0x41c64e6d) + UINT32_C(0x3039);
    draw = (a->random_state >> 16) & 0x7fff;
    if (a->page == MSA_PAGE_MECHBAY || a->page == MSA_PAGE_MECHOPS) {
        a->idle_resource = mech_idle[draw % (a->page == MSA_PAGE_MECHBAY ? 23u : 29u)];
        if (a->idle_resource != 1) (void)msa_play(a, a->idle_resource, a->idle_resource <= 9 ? 5 : 15, 0);
    } else {
        a->idle_resource = idle_table[draw % 25];
        (void)msa_play(a, a->idle_resource, 15, 0);
    }
}

int mercs_shell_audio_busy(mercs_shell_audio *a)
{
    mercs_shell_audio_tick(a);
    return a && !a->suspended && a->gate != 0;
}

void mercs_shell_audio_suspend(mercs_shell_audio *a)
{
    if (!a || a->suspended) return;
    msa_stop_owned(a); a->suspended = 1;
}

void mercs_shell_audio_resume(mercs_shell_audio *a)
{
    if (!a || !a->suspended) return;
    a->suspended = 0;
    if (a->ambient) (void)msa_play(a, a->ambient, a->ambient_volume, 1);
    if (a->text_active) (void)msa_play(a, 49, 10, 1);
}

void mercs_shell_audio_text_reveal(mercs_shell_audio *a, int active)
{
    if (!a) return;
    active = active != 0;
    if (active == a->text_active) return;
    a->text_active = active;
    if (active) (void)msa_play(a, 49, 10, 1); else msa_stop(a, 49);
}

void mercs_shell_audio_seed(mercs_shell_audio *a, uint32_t seed)
{
    if (a) a->random_state = seed;
}

void mercs_shell_audio_idle_enabled(mercs_shell_audio *a, int enabled)
{
    if (!a) return;
    a->idle_enabled = enabled != 0;
    if (!a->idle_enabled) { msa_stop(a, a->idle_resource); a->idle_resource = 0; }
}
