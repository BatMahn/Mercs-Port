/* sfx.c - see sfx.h. */
#include "sfx.h"
#include "bwdplace.h"

#include <SDL3/SDL.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SFX_VOICES 24
#define SFX_CACHE  1200   /* Mercs sound-state capacity, MERCSW.DLL 0x1001d7a9..0x1001d7f8. */

typedef struct {
    unsigned char *data; int len; Uint32 last;
    /* Zero remains the original unsigned8 mono cache format. S16 len is frames. */
    int s16, channels; float ramp;
} clip;
typedef struct { const clip *c; double pos, step; float l, r; int loop, is_voice, index, prio; clip own; } voice;   /* pos < 0: a delayed start (sfx_play_at) */
/* Mercs sound rules: 46 original records at MERCSW.DLL 0x100ab0c8,
 * installed by 0x1001d7a0 into 1200 entries. A later duplicate wins (59).
 * {sound, priority, playing cap, rate}; -1 means the original default. */
static const short SND_TABLE[][4] = {
    {4, 80, -1, -1},
    {7, 80, 1, -1},
    {8, 80, 1, -1},
    {9, 80, -1, -1},
    {57, 80, -1, -1},
    {59, 80, -1, -1},
    {63, 80, -1, -1},
    {70, 80, -1, -1},
    {91, 100, -1, -1},
    {93, 80, -1, -1},
    {98, 80, -1, -1},
    {100, 80, -1, -1},
    {101, 80, -1, -1},
    {111, 80, -1, -1},
    {115, 80, -1, -1},
    {116, 80, -1, -1},
    {169, -1, 1, -1},
    {202, 80, -1, -1},
    {205, 80, 2, -1},
    {214, 100, 1, -1},
    {217, 80, -1, -1},
    {225, 100, -1, -1},
    {226, 80, 2, -1},
    {227, 80, 2, -1},
    {228, 80, 2, -1},
    {243, 0, -1, -1},
    {246, -1, -1, -1},
    {248, 100, 4, -1},
    {252, 80, -1, -1},
    {254, 80, -1, -1},
    {256, 80, 2, -1},
    {257, 80, -1, -1},
    {261, 80, -1, -1},
    {267, 80, 2, -1},
    {270, 80, -1, -1},
    {273, 80, -1, -1},
    {275, 80, -1, -1},
    {277, 80, -1, -1},
    {280, 80, -1, -1},
    {306, 80, 2, -1},
    {307, 80, 2, -1},
    {341, 0, 1, -1},
    {345, 80, -1, -1},
    {346, 80, -1, -1},
    {347, 10, 1, -1},
};
static void snd_rule(int index, int *prio, int *cap)
{
    size_t k;
    *prio = 0x32; *cap = -1;
    for (k = 0; k < sizeof SND_TABLE / sizeof SND_TABLE[0]; k++)
        if (SND_TABLE[k][0] == index) { if (SND_TABLE[k][1] != -1) *prio = SND_TABLE[k][1]; *cap = SND_TABLE[k][2]; return; }
}

struct sfx_s {
    float gain_fx, gain_voice;   /* sfx_set_gains; 0 = unset (full) */
    int   next_is_voice;         /* the next sfx_play_g is on the voice channel */
    long  plays, mixed_frames, callbacks;
    sfx_music_fn music; void *music_user;
    int   paused;                /* sfx_pause */
    prj_archive *a;
    SDL_AudioStream *stream;
    int    rate;
    int    positional_range_m, game_ticks_per_second;
    clip   cache[SFX_CACHE];
    voice  v[SFX_VOICES];
};

/* Mercs SFLX loader 0x10002d8f and decoder 0x100396a4 (retail Windows;
 * DOS decoder0x522d8 is byte-identical after workspace relocation).
 * Header: "SFLX", u32 packed length excluding magic, u32 block count, u16
 * output samples/block. Codecs0..5 are silence, repeat, 1/2/4-bit delta, raw.
 * Interpolation modes1/2 expand half/quarter blocks using integer averages IN
 * PLACE. The boundary reads the old buffer's next sample, and repeat uses the
 * expanded buffer. Mode3, like0, does not interpolate. Voice rate is11025 Hz
 * (0x10007afc); this format has no sample-rate field. */
int sfx_decode_sflx(const uint8_t *rec, size_t len, unsigned char **out);
static int sflx_decode(const uint8_t *rec, size_t len, unsigned char **out);
int sfx_decode_sflx(const uint8_t *rec, size_t len, unsigned char **out) { return sflx_decode(rec, len, out); }
static int sflx_decode(const uint8_t *rec, size_t len, unsigned char **out)
{
    uint32_t nb, b;
    unsigned bs, n, i;
    size_t p = 14, o = 0;
    int prev = 0;
    unsigned char *dst, buf[2049], blk[2048];
    if (!out) return -1;
    *out = NULL;
    if (!rec || len < 14 || memcmp(rec, "SFLX", 4)) return -1;
    nb = (uint32_t)rec[8] | (uint32_t)rec[9] << 8 | (uint32_t)rec[10] << 16 | (uint32_t)rec[11] << 24;
    bs = (unsigned)rec[12] | (unsigned)rec[13] << 8;
    if (bs == 0 || bs > 2048 || nb > 1000000) return -1;
    dst = malloc((size_t)nb * bs + 1);
    if (!dst) return -1;
    memset(buf, 0, sizeof buf);   /* original static decoder workspace */
    for (b = 0; b < nb; b++) {
        if (p >= len) goto malformed;
        int h = rec[p++], m = h >> 6, c = h & 15;
        n = m == 1 ? bs >> 1 : m == 2 ? bs >> 2 : bs;
        if (!n || ((m == 1 || m == 2) && n * (1u << m) != bs)) goto malformed;
        if (c == 0) { memset(buf, 0x80, n); prev = 0; }
        else if (c == 1) { /* the previous block's samples again */ }
        else if (c >= 2 && c <= 4) {
            int k = c == 2 ? 2 : c == 3 ? 4 : 16, bits = c == 2 ? 1 : c == 3 ? 2 : 4, t[16], j;
            if (p + (size_t)k > len) goto malformed;
            for (j = 0; j < k; j++) t[j] = rec[p + (size_t)j] * 2 - 128;
            p += (size_t)k;
            for (i = 0; i < n;) {
                unsigned byte;
                int q;
                if (p >= len) goto malformed;
                byte = rec[p++];
                for (q = 0; q < 8 / bits && i < n; q++) {
                    prev += t[byte & (unsigned)(k - 1)];
                    byte >>= bits;
                    prev = prev > 127 ? 127 : prev < -127 ? -127 : prev;
                    buf[i++] = (unsigned char)(prev + 128);
                }
            }
        } else if (c == 5) {
            if (p + n > len) goto malformed;
            memcpy(buf, rec + p, n); p += n; prev = buf[n - 1] - 128;
        } else goto malformed;
        if (m == 1 || m == 2) {   /* 0x100398e3 / 0x1003993b */
            for (i = 0; i < n; i++) {
                unsigned a0 = buf[i], a1 = buf[i + 1], mid = (a0 + a1) >> 1;
                if (m == 1) { blk[i * 2] = (unsigned char)a0; blk[i * 2 + 1] = (unsigned char)mid; }
                else {
                    blk[i * 4] = (unsigned char)a0;
                    blk[i * 4 + 1] = (unsigned char)((a0 + mid) >> 1);
                    blk[i * 4 + 2] = (unsigned char)mid;
                    blk[i * 4 + 3] = (unsigned char)((mid + a1) >> 1);
                }
            }
            memcpy(buf, blk, bs);
        }
        memcpy(dst + o, buf, bs);
        o += bs;
    }
    *out = dst;
    return (int)o;
malformed:
    free(dst);
    return -1;
}

int sfx_decode(prj_archive *a, int index, unsigned char **out)
{
    prj_record r;
    int n;
    *out = NULL;
    if (!a) return -1;
    {
        int t = prj_find_type(a, "SNDS");
        if (t < 0 || prj_read(a, t, index, &r) != PRJ_OK) return -1;
    }
    if (r.size < 14 || memcmp(r.data, "SFLX", 4) != 0) { prj_record_free(&r); return -1; }
    n = sflx_decode(r.data, r.size, out);
    prj_record_free(&r);
    return n;
}

static void mix(void *user, Uint8 *stream, int len)
{
    sfx *s = user;
    float *o = (float *)stream;
    int frames = len / (int)(2 * sizeof(float)), f, k;
    memset(stream, 0, (size_t)len);
    s->callbacks++;
    if (s->paused) return;   /* everything held where it was */
    if (s->music) s->music(s->music_user, o, frames);
    for (k = 0; k < SFX_VOICES; k++) {
        voice *vc = &s->v[k];
        if (!vc->c) continue;
        for (f = 0; f < frames; f++) {
            int i, n = vc->c->len;
            if (vc->pos < 0) { vc->pos += vc->step; continue; }   /* not started yet: the sound's travel time */
            i = (int)vc->pos;
            float smp, right, fr, a, b, env = 1.0f;
            if (i >= n) {
                if (vc->loop) { vc->pos = 0; i = 0; }
                else { vc->c = NULL; break; }
            }
            /* linear interpolation between the 11 kHz samples (nearest-sample stepping aliased into a grit), and a
             * 2.5 ms ramp at both ends of a one-shot (a clip that starts or stops off zero clicks) */
            fr = (float)(vc->pos - (double)i);
            if (vc->c->s16) {
                const int16_t *p = (const int16_t *)vc->c->data;
                int ch = vc->c->channels, next = i + 1 < n ? i + 1 : vc->loop ? 0 : -1;
                a = (float)p[i * ch] / 32768.0f;
                b = next >= 0 ? (float)p[next * ch] / 32768.0f : 0.0f;
                smp = a + (b - a) * fr;
                if (ch == 2) {
                    a = (float)p[i * 2 + 1] / 32768.0f;
                    b = next >= 0 ? (float)p[next * 2 + 1] / 32768.0f : 0.0f;
                    right = a + (b - a) * fr;
                } else right = smp;
            } else {
                a = ((float)vc->c->data[i] - 128.0f) / 128.0f;
                b = i + 1 < n ? ((float)vc->c->data[i + 1] - 128.0f) / 128.0f : (vc->loop ? ((float)vc->c->data[0] - 128.0f) / 128.0f : 0.0f);
                smp = a + (b - a) * fr;
                right = smp;
            }
            if (!vc->loop) {
                float ramp = vc->c->s16 ? vc->c->ramp : 28.0f;   /* old path: 2.5 ms at 11025 Hz */
                if (vc->pos < ramp) env = (float)vc->pos / ramp;
                if ((double)n - vc->pos < ramp) env = (float)(((double)n - vc->pos) / ramp);
            }
            smp *= env;
            right *= env;
            s->mixed_frames++;
            o[f * 2] += smp * vc->l;
            o[f * 2 + 1] += right * vc->r;
            vc->pos += vc->step;
        }
    }
    for (f = 0; f < frames * 2; f++) o[f] = tanhf(o[f] * 0.6f);   /* headroom + soft limit: overlapping full-scale samples */
}

/* SDL3 asks for bytes in this stream's input format. Keep the original mixer
 * at 44.1 kHz stereo float and let the stream convert to the current hardware
 * format. Requests vary in size (and may be zero); bounded frame-aligned
 * chunks avoid allocating a device-sized buffer in the audio callback.
 * SDL holds the stream lock throughout this callback. */
static void SDLCALL audio_requested(void *user, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
    float output[1024 * 2];
    const int frame_bytes = 2 * (int)sizeof(float);
    (void)total_amount;
    while (additional_amount > 0) {
        int frames = additional_amount / frame_bytes + (additional_amount % frame_bytes != 0);
        int bytes;
        if (frames > 1024) frames = 1024;
        bytes = frames * frame_bytes;
        mix(user, (Uint8 *)output, bytes);
        if (!SDL_PutAudioStreamData(stream, output, bytes)) return;
        additional_amount -= bytes;
    }
}

sfx *sfx_create(prj_archive *a)
{
    SDL_AudioSpec want;
    sfx *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->a = a;
    SDL_zero(want);
    want.freq = 44100; want.format = SDL_AUDIO_F32; want.channels = 2;
    s->rate = want.freq;
    s->game_ticks_per_second = 182;
    s->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want, audio_requested, s);
    if (!s->stream) { free(s); return NULL; }
    if (!SDL_ResumeAudioStreamDevice(s->stream)) {
        SDL_DestroyAudioStream(s->stream);
        free(s);
        return NULL;
    }
    return s;
}

void sfx_destroy(sfx *s)
{
    int i;
    if (!s) return;
    /* OpenAudioDeviceStream transfers device ownership to the stream. Stop
     * its callback before touching statistics or releasing samples. */
    SDL_DestroyAudioStream(s->stream);
    if (getenv("MERCS_SFX_TRACE")) fprintf(stderr, "sfx: %ld plays, %ld callbacks, %ld frames mixed\n", s->plays, s->callbacks, s->mixed_frames);
    for (i = 0; i < SFX_CACHE; i++) free(s->cache[i].data);
    free(s);
}

static void sfx_play_g(sfx *s, int index, float volume, float pan);
static double s_delay;   /* the next sfx_play_g starts this many seconds late (sfx_play_at) */
void sfx_set_gains(sfx *s, float effects, float voice)
{
    if (!s) return;
    s->gain_fx = effects <= 0 ? -1.0f : effects;   /* -1: silent */
    s->gain_voice = voice <= 0 ? -1.0f : voice;
}
static float gain_of(float g) { return g == 0 ? 1.0f : g < 0 ? 0.0f : g; }
/* a voice line would be heard: the VOICE slider above 0 and the clip present (engine 0x10031960 case 1: voices on
 * while the volume is non-zero; 0x10031440 posts the line's text only when it could not play) */
int sfx_voice_audible(sfx *s, int index)
{
    clip *c;
    if (!s || index <= 0 || index >= SFX_CACHE || gain_of(s->gain_voice) <= 0.01f) return 0;
    c = &s->cache[index];
    if (!c->data) c->len = sfx_decode(s->a, index, &c->data);
    return c->data && c->len > 0;
}
void sfx_play(sfx *s, int index, float volume, float pan) { if (s) sfx_play_g(s, index, volume * gain_of(s->gain_fx), pan); }
void sfx_play_voice(sfx *s, int index, float volume) { if (s) { s->next_is_voice = 1; sfx_play_g(s, index, volume * gain_of(s->gain_voice), 0.0f); s->next_is_voice = 0; } }
double sfx_log_time = -1;   /* TEST ONLY (MERCS_SFX_LOG): the caller's clock (s) printed with each play */
/* TEST ONLY (MERCS_SFX_SIMCLOCK=1): the voice channel counts as busy for each line's length on that clock (sfx_log_time),
 * not while the audio device plays it - headless runs go faster than real time, so voice order is checkable */
static double s_sim_voice_end = -1;
static void sim_voice(int samples) { if (getenv("MERCS_SFX_SIMCLOCK")) s_sim_voice_end = sfx_log_time + samples / 11025.0; }
static void sfx_log(const char *what, int index, float volume, int voice, float pan)
{
    char pn[32] = "";
    if (!getenv("MERCS_SFX_LOG")) return;
    if (pan != 0.0f) snprintf(pn, sizeof pn, " pan %.2f", (double)pan);   /* off centre only */
    if (s_delay > 0) fprintf(stderr, "SFXLOG %.3f %s %d vol %.2f delay %.3f%s\n", sfx_log_time, what, index, (double)volume, s_delay, pn);
    else fprintf(stderr, "SFXLOG %.3f %s %d vol %.2f%s%s\n", sfx_log_time, what, index, (double)volume, voice ? " voice" : "", pn);
}
/* Mercs0x10002d8f scans eight ordinary sample handles. Spoken SFLX shares
 * those handles, carries flag0x100, and cannot be stolen. Object ambience
 * owns a separate eight handles (0x1000393d scans8..15). Native PCM loops
 * must therefore neither use this budget nor become steal candidates. */
static int effect_channel(sfx *s, int priority, int speech)
{
    int k, free_channel = -1, busy = 0, steal = -1;
    int requested = priority | (speech ? 0x100 : 0);
    for (k = 0; k < SFX_VOICES; k++) {
        const voice *v = &s->v[k];
        if (!v->c) { if (free_channel < 0) free_channel = k; continue; }
        if (v->loop) continue;
        busy++;
        if (v->is_voice) { if (speech) return -1; continue; }
        if (v->prio < requested) steal = k;
    }
    return busy < 8 ? free_channel : steal;
}
static void sfx_play_g(sfx *s, int index, float volume, float pan)
{
    clip *c;
    int k, best = 0;
    if (s && index > 0) sfx_log("snd", index, volume, s->next_is_voice, pan);   /* TEST ONLY */
    if (!s || index <= 0 || index >= SFX_CACHE || volume <= 0.01f) return;
    c = &s->cache[index];
    if (!c->data) { c->len = sfx_decode(s->a, index, &c->data); if (c->len <= 0) return; }
    s->plays++;
    SDL_LockAudioStream(s->stream);
    {   /* Mercs0x10002ba7/0x10002d8f: table cap, first free handle,
         * otherwise the last eligible lower-priority handle. */
        int prio, cap, same = 0;
        snd_rule(index, &prio, &cap);
        for (k = 0; k < SFX_VOICES; k++) if (s->v[k].c && s->v[k].index == index) same++;
        if (cap >= 0 && same >= cap) { SDL_UnlockAudioStream(s->stream); return; }
        best = effect_channel(s, prio, s->next_is_voice);
        if (best < 0) { SDL_UnlockAudioStream(s->stream); return; }
        s->v[best].index = index; s->v[best].prio = prio;
    }
    if (pan < -1) pan = -1;
    if (pan > 1) pan = 1;
    s->v[best].c = c;
    s->v[best].loop = 0;
    if (s->next_is_voice) sim_voice(c->len);   /* TEST ONLY */
    s->v[best].is_voice = s->next_is_voice; s->next_is_voice = 0;
    s->v[best].pos = -s_delay * 11025.0;   /* the handle is taken now, the sample starts after the delay (0x100328e0 +0x3a8) */
    s->v[best].step = 11025.0 / (double)s->rate;    /* engine rate: 11025 Hz */
    s->v[best].l = volume * (pan <= 0 ? 1.0f : 1.0f - pan);
    s->v[best].r = volume * (pan >= 0 ? 1.0f : 1.0f + pan);
    if (getenv("MERCS_SFX_LOG"))
        fprintf(stderr, "SFXMIX %.3f snd %d slot %d priority %d voice %d\n",
                sfx_log_time, index, best, s->v[best].prio, s->v[best].is_voice);
    SDL_UnlockAudioStream(s->stream);
}

void sfx_play_at(sfx *s, int index, float dx, float dz, float listener_yaw) { sfx_play_at3(s, index, dx, 0.0f, dz, listener_yaw, 0); }
void sfx_play_at_h(sfx *s, int index, float dx, float dz, float listener_yaw, int half) { sfx_play_at3(s, index, dx, 0.0f, dz, listener_yaw, half); }
/* Mercs DOS0x517a4 / Windows0x1000381c: Q16 vector bearing, original
 * interpolated Q29 sine, arithmetic >>23, then clamp to15..111. The inherited
 * floating Mech2 path discarded fractional bearing and truncated negative
 * sine toward zero. Keep Mercs' exact arithmetic after converting the native
 * coordinates to integer centimetres and the native listener angle to Q16. */
int sfx_pos_pan_engine(float dx, float dz, float listener_yaw)
{
    int32_t bearing, yaw, wave;
    double angle;
    int offset, p;
    if (!isfinite(dx) || !isfinite(dz) || !isfinite(listener_yaw) ||
        dx <= (float)INT_MIN || dx >= (float)INT_MAX ||
        dz <= (float)INT_MIN || dz >= (float)INT_MAX) return 64;
    bearing = bwd_angle_atan2((int32_t)dx, (int32_t)dz);
    angle = (double)listener_yaw * 65536.0;
    if (angle < INT_MIN || angle > INT_MAX)
        angle = fmod((double)listener_yaw, 360.0) * 65536.0;
    yaw = (int32_t)llrint(angle);
    wave = bwd_angle_sin((int32_t)((uint32_t)yaw - (uint32_t)bearing));
    offset = wave / 0x800000;
    if (wave < 0 && ((uint32_t)wave & 0x7fffffu)) --offset;
    p = 64 + offset;
    return p < 15 ? 15 : p > 111 ? 111 : p;
}
float sfx_pos_pan(float dx, float dz, float listener_yaw) { return sfx_pan(sfx_pos_pan_engine(dx, dz, listener_yaw)); }
void sfx_set_world(sfx *s, int range_m, int ticks_per_second)
{
    if (!s) return;
    s->positional_range_m = range_m;
    s->game_ticks_per_second = ticks_per_second == 181 ? 181 : 182;
}
/* Executed Mercs DOS0x51640 / Windows0x10003686: the wrapper first
 * divides centimeters by100 for the signed CLIM gate, then the attenuation
 * helper0x10003cf8 divides that value by100 again. Ambient tasks call that
 * helper directly with centimeters, so their falloff is different. */
static int positional_level(int centimeters, int range_m, int half)
{
    int meters = centimeters / 100, distance, volume;
    if (centimeters < 0 || (range_m != 0 && meters >= range_m)) return -1;
    if (meters > 50000) return -1;
    distance = meters / 100;
    volume = 100 - distance * distance * 100 / 250000;
    if (volume <= 0) return -1;
    return half ? volume >> 1 : volume;
}
static int positional_delay(int centimeters)
{ return (int)(((int64_t)centimeters * 360 + 32768) >> 16); }
void sfx_play_at3(sfx *s, int index, float dx, float dy, float dz, float listener_yaw, int half)
{
    float d3 = sqrtf(dx * dx + dy * dy + dz * dz);
    int centimeters, level, ticks, hz;
    if (!s || !isfinite(d3) || d3 < 0 || d3 >= (float)INT_MAX) return;
    centimeters = (int)d3;
    level = positional_level(centimeters, s->positional_range_m, half);
    if (level < 0) return;
    ticks = positional_delay(centimeters);
    hz = s->game_ticks_per_second == 181 ? 181 : 182;
    s_delay = ticks > hz / 30 ? (double)ticks / hz : 0.0;
    sfx_play(s, index, SFX_VOL(level), sfx_pos_pan(dx, dz, listener_yaw));
    s_delay = 0;
}

/* Volume is already scaled for the selected channel. Applying that channel's
 * gain directly also keeps external SFL speech independent when Effects=0. */
static int sfx_play_pcm_g(sfx *s, const unsigned char *pcm, int len, int rate, float volume, int loop)
{
    int k, best = -1;
    if (s && pcm) sfx_log("pcm", len, volume, s->next_is_voice, 0.0f);   /* TEST ONLY */
    if (!s || !pcm || len <= 0 || rate <= 0) return -1;
    SDL_LockAudioStream(s->stream);
    if (s->next_is_voice) best = effect_channel(s, 0x32, 1);
    else for (k = 0; k < SFX_VOICES; k++) if (!s->v[k].c) { best = k; break; }
    if (best < 0) { SDL_UnlockAudioStream(s->stream); return -1; }
    s->v[best].index = -1; s->v[best].prio = 0x32; s->v[best].is_voice = 0;
    s->v[best].own.data = (unsigned char *)pcm;   /* not owned: never freed here */
    s->v[best].own.len = len;
    s->v[best].own.s16 = 0; s->v[best].own.channels = 1; s->v[best].own.ramp = 28.0f;
    s->v[best].c = &s->v[best].own;
    s->v[best].pos = 0;
    s->v[best].step = (double)rate / (double)s->rate;
    s->v[best].l = s->v[best].r = volume;
    s->v[best].loop = loop;
    s->v[best].is_voice = s->next_is_voice; s->next_is_voice = 0;
    SDL_UnlockAudioStream(s->stream);
    return 0;
}

int sfx_play_pcm(sfx *s, const unsigned char *pcm, int len, int rate, float volume, int loop)
{
    if (s) volume *= gain_of(s->gain_fx);   /* the EFFECTS slider */
    return sfx_play_pcm_g(s, pcm, len, rate, volume, loop);
}

int sfx_play_pcm_s16(sfx *s, const int16_t *pcm, int frames, int channels,
                     int rate, float volume, int loop)
{
    int k, best = -1;
    if (!s || !pcm || frames <= 0 || (channels != 1 && channels != 2) ||
        frames > INT_MAX / channels || rate <= 0) return -1;
    volume *= gain_of(s->gain_fx);
    sfx_log("pcm16", frames, volume, 0, 0.0f);
    SDL_LockAudioStream(s->stream);
    for (k = 0; k < SFX_VOICES; k++) if (!s->v[k].c) { best = k; break; }
    if (best < 0) { SDL_UnlockAudioStream(s->stream); return -1; }
    s->v[best].index = -1; s->v[best].prio = 0x32; s->v[best].is_voice = 0;
    s->v[best].own.data = (unsigned char *)pcm;   /* borrowed, never freed by sfx */
    s->v[best].own.len = frames;
    s->v[best].own.s16 = 1; s->v[best].own.channels = channels;
    s->v[best].own.ramp = (float)rate * 0.0025f;
    s->v[best].c = &s->v[best].own;
    s->v[best].pos = 0; s->v[best].step = (double)rate / (double)s->rate;
    s->v[best].l = s->v[best].r = volume; s->v[best].loop = loop != 0;
    SDL_UnlockAudioStream(s->stream);
    return 0;
}

int sfx_pcm_playing(sfx *s, const void *pcm)
{
    int k, on = 0;
    if (!s || !pcm) return 0;
    SDL_LockAudioStream(s->stream);
    for (k = 0; k < SFX_VOICES; k++)
        if (s->v[k].c == &s->v[k].own && s->v[k].own.data == pcm) on = 1;
    SDL_UnlockAudioStream(s->stream);
    return on;
}

void sfx_play_pcm_voice(sfx *s, const unsigned char *pcm, int len, float volume)
{
    if (!s) return;
    volume *= gain_of(s->gain_voice);   /* the VOICE slider, independent of EFFECTS */
    s->next_is_voice = 1;
    sim_voice(len);   /* TEST ONLY */
    sfx_play_pcm_g(s, pcm, len, 11025, volume, 0);
    s->next_is_voice = 0;
}

int sfx_voice_busy(sfx *s)
{
    int k, on = 0;
    if (!s) return 0;
    SDL_LockAudioStream(s->stream);
    for (k = 0; k < SFX_VOICES; k++) if (s->v[k].c && s->v[k].is_voice) on = 1;
    SDL_UnlockAudioStream(s->stream);
    if (getenv("MERCS_SFX_SIMCLOCK")) return sfx_log_time >= 0 && sfx_log_time < s_sim_voice_end;   /* TEST ONLY */
    return on;
}

void sfx_stop_voice(sfx *s, int index)
{
    int k;
    if (!s) return;
    SDL_LockAudioStream(s->stream);
    for (k = 0; k < SFX_VOICES; k++) if (s->v[k].c && s->v[k].is_voice && s->v[k].index == index) s->v[k].c = NULL;
    SDL_UnlockAudioStream(s->stream);
    s_sim_voice_end = -1;   /* TEST ONLY clock */
}

int sfx_playing(sfx *s, int index)
{
    int k, on = 0;
    if (!s || index <= 0 || index >= SFX_CACHE) return 0;
    SDL_LockAudioStream(s->stream);
    for (k = 0; k < SFX_VOICES; k++) if (s->v[k].c == &s->cache[index]) on = 1;
    SDL_UnlockAudioStream(s->stream);
    return on;
}

void sfx_stop_pcm(sfx *s, const unsigned char *pcm)
{
    int k;
    if (!s || !pcm) return;
    SDL_LockAudioStream(s->stream);
    for (k = 0; k < SFX_VOICES; k++) if (s->v[k].c == &s->v[k].own && s->v[k].own.data == pcm) s->v[k].c = NULL;
    SDL_UnlockAudioStream(s->stream);
}

void sfx_pcm_level(sfx *s, const unsigned char *pcm, float volume, float pan)
{
    int k;
    if (!s || !pcm) return;
    volume *= gain_of(s->gain_fx);   /* the EFFECTS slider, as sfx_play_pcm */
    if (pan < -1) pan = -1;
    if (pan > 1) pan = 1;
    SDL_LockAudioStream(s->stream);
    for (k = 0; k < SFX_VOICES; k++)
        if (s->v[k].c == &s->v[k].own && s->v[k].own.data == pcm) {
            s->v[k].l = volume * (pan <= 0 ? 1.0f : 1.0f - pan);
            s->v[k].r = volume * (pan >= 0 ? 1.0f : 1.0f + pan);
        }
    SDL_UnlockAudioStream(s->stream);
}

void sfx_pcm_rate(sfx *s, const unsigned char *pcm, int rate)
{
    int k;
    if (!s || !pcm || rate <= 0) return;
    SDL_LockAudioStream(s->stream);
    for (k = 0; k < SFX_VOICES; k++)
        if (s->v[k].c == &s->v[k].own && s->v[k].own.data == pcm) s->v[k].step = (double)rate / (double)s->rate;
    SDL_UnlockAudioStream(s->stream);
}

void sfx_stop_loops(sfx *s)
{
    int k;
    if (!s) return;
    SDL_LockAudioStream(s->stream);
    for (k = 0; k < SFX_VOICES; k++) if (s->v[k].loop) { s->v[k].c = NULL; s->v[k].loop = 0; }
    SDL_UnlockAudioStream(s->stream);
}

void sfx_pause(sfx *s, int on)
{
    int k;
    if (!s) return;
    SDL_LockAudioStream(s->stream);
    if (on && !s->paused)   /* engine 0x10031f30: the effect samples end (0x10032c70), the voice and the music pause */
        for (k = 0; k < SFX_VOICES; k++) if (s->v[k].c && !s->v[k].loop && !s->v[k].is_voice) s->v[k].c = NULL;
    s->paused = on != 0;
    SDL_UnlockAudioStream(s->stream);
}

void sfx_set_music(sfx *s, sfx_music_fn fn, void *user)
{
    if (!s) return;
    SDL_LockAudioStream(s->stream);
    s->music = fn; s->music_user = user;
    SDL_UnlockAudioStream(s->stream);
}
int sfx_rate(const sfx *s) { return s ? s->rate : 44100; }
void sfx_lock(sfx *s) { if (s) SDL_LockAudioStream(s->stream); }
void sfx_unlock(sfx *s) { if (s) SDL_UnlockAudioStream(s->stream); }
