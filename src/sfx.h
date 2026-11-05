/*
 * sfx.h - sound effects: the game's SNDS records mixed through SDL audio.
 *
 * SFLX SNDS records and disc .SFL files contain a14-byte header followed by
 * compressed blocks: magic, u32 packed size minus4, u32 block count, u16
 * samples per block. The original Mercs voice caller requests11025 Hz;
 * there is no rate field in SFLX. Records are addressed by their PRJ index.
 */
#ifndef MW2_SFX_H
#define MW2_SFX_H

#include "prj.h"

typedef struct sfx_s sfx;

/* Opens an SDL3 audio stream and its playback device (SDL_INIT_AUDIO must be
 * initialised). Mixes at 44100 Hz; SDL converts to the hardware format.
 * NULL if unavailable. Destruction closes only this stream's device. */
sfx *sfx_create(prj_archive *a);
void sfx_destroy(sfx *s);
/* Play SNDS record `index` (1-based PRJ index). volume 0..1, pan -1 (left) .. 1 (right). */
void sfx_play(sfx *s, int index, float volume, float pan);
/* Engine levels. Every sound, positional or not, reaches the mixer as an AIL volume 0..127 (0x100328e0 / DOS 0x409b0:
 * +0x468 = volume x the effects level 0x10000 >> 16) and pan 0..127 (+0x428; -1 = positional, 0x10032c10). The DOS
 * mixer (MW2.EXE's AIL, 0x79110) scales a channel by (volume + 1) / 128 and the pan by table 0xa42e0 T[i] = 2i (i <= 62)
 * else 128, / 128: the right channel by T[127 - pan], the left by T[pan] - a centred sound is at full level on both
 * sides. The port's scale is the positional one: level 100 = 0.8 (sfx_play_at_h). SFX_VOL(v): AIL volume v (clamped
 * at 127 = 1.0); SFX_V64 = volume 0x64 (0x10032b60 / DOS 0x40c50: edx = the volume, the first stack word the pan, then
 * the rate-table index - an unused argument of 0x100328e0 - and a channel priority 0x50 / 0x32, not a level).
 * DOSBox YELLSCN1 (SB16, each channel fitted on its own - DOSBox's right channel lags two samples, so a mono sum
 * cancels high frequencies, which had read as 0.42-0.62x and IREDMODE 1.1-1.6x): MECTURX2 (positional, 0 m) 0.785 /
 * 0.784, MECOMBEP 0.790 / 0.789, IREDMODE 0.70-0.78 (noisy), MECBSYRX (pan 0x2f) L 0.579 R 0.789 = 101/128 x 94/128 */
#define SFX_VOL(v) ((v) >= 127 ? 1.0f : (float)(v) * 0.008f)
#define SFX_V64 SFX_VOL(0x64)
/* an engine pan (0..127, 0x40 centre) as the port's pan (-1 left .. 1 right: the far side scaled by 1 - |pan|):
 * pan < 0x40 turns the left channel down (DOSBox: 0x2f right of centre), > 0x40 the right (0x4f: R 0.75) */
static inline float sfx_pan(int p)
{
    float l = p >= 63 ? 1.0f : (float)(2 * p) / 128.0f, r = 127 - p >= 63 ? 1.0f : (float)(2 * (127 - p)) / 128.0f;
    return l < 1.0f ? 1.0f - l : r < 1.0f ? r - 1.0f : 0.0f;
}
/* The EFFECTS and VOICE sliders (MW2SND.CFG +4 / +8, 16.16): every sound scaled by effects, voice lines (sfx_play_voice)
 * by voice instead. */
void sfx_set_gains(sfx *s, float effects, float voice);
void sfx_play_voice(sfx *s, int index, float volume);
int  sfx_voice_audible(sfx *s, int index);   /* the VOICE slider up and the clip present */
/* Play at a world position relative to the listener (cm; listener facing yaw degrees). */
void sfx_play_at(sfx *s, int index, float dx, float dz, float listener_yaw);
/* Mercs CLIM payload+4 signed sound range in meters (0: no range gate),
 * and original clock rate182 DOS /181 Windows for sound travel delay. */
void sfx_set_world(sfx *s, int range_m, int ticks_per_second);
/* The same; half=1 halves the original integer level. Mercs DOS0x51640 /
 * Windows0x10003686 apply CLIM and attenuation before that halving. Sound
 * travel uses round(distance_cm*360/65536) ticks at the selected engine rate. */
void sfx_play_at_h(sfx *s, int index, float dx, float dz, float listener_yaw, int half);
/* Mercs DOS0x51790 / Windows0x100037f0 accept the caller's camera-relative
 * vector. The caller preserves each event's position-minus-camera or mirrored
 * camera-minus-position convention. Volume/delay use 3D distance; panning uses
 * the horizontal bearing and the listener's current yaw. */
void sfx_play_at3(sfx *s, int index, float dx, float dy, float dz, float listener_yaw, int half);
/* Mercs DOS0x517a4 / Windows0x1000381c: original Q16 bearing and Q29 sine,
 * signed rounding, clamp15..111; then conversion to the native mixer pan. */
int   sfx_pos_pan_engine(float dx, float dz, float listener_yaw);
float sfx_pos_pan(float dx, float dz, float listener_yaw);
/* Play caller-owned unsigned 8-bit mono PCM at `rate` Hz (data must stay valid while playing).
 * loop = 1 repeats until sfx_stop_loops. Returns0 on success, -1 if unavailable
 * or invalid. A full pool never replaces another caller's live PCM buffer. */
int  sfx_play_pcm(sfx *s, const unsigned char *pcm, int len, int rate, float volume, int loop);
/* Caller-owned native-endian signed 16-bit PCM, interleaved for stereo.
 * `frames` counts complete mono/stereo frames, not bytes or scalar samples.
 * The existing unsigned8 simulation path remains available; shell WAV/Smacker
 * callers can preserve their original signed16 precision and channel count. */
int  sfx_play_pcm_s16(sfx *s, const int16_t *pcm, int frames, int channels,
                      int rate, float volume, int loop);
/* True while any voice plays this caller-owned 8- or 16-bit buffer. */
int  sfx_pcm_playing(sfx *s, const void *pcm);
void sfx_stop_loops(sfx *s);
/* the in-game menu's pause (engine 0x10031f30 / 0x10031f50): one-shot effects end, voice, loops and music hold */
void sfx_pause(sfx *s, int on);
/* 1 while a voice is playing sound `index` */
int  sfx_playing(sfx *s, int index);
/* 1 while a voice-channel sound (sfx_play_voice) plays */
int  sfx_voice_busy(sfx *s);
/* stop the voice-channel sound `index` (the end sequence's flush cutting a message line, 0x10031640) */
void sfx_stop_voice(sfx *s, int index);
/* decode an SFLX sound (a SNDS record or a disc .SFL file) to unsigned 8-bit at 11025 Hz; free(*out). Bytes, or -1 */
int  sfx_decode_sflx(const unsigned char *rec, size_t len, unsigned char **out);
/* play decoded 8-bit PCM on the voice channel (counts for sfx_voice_busy); the buffer must stay valid */
void sfx_play_pcm_voice(sfx *s, const unsigned char *pcm, int len, float volume);
/* stop every voice playing this buffer (sfx_play_pcm) */
void sfx_stop_pcm(sfx *s, const unsigned char *pcm);
/* set the volume (EFFECTS-scaled) and pan of every voice playing this buffer (a positional loop) */
void sfx_pcm_level(sfx *s, const unsigned char *pcm, float volume, float pan);
/* set the playback rate (Hz) of every voice playing this buffer (AIL_set_sample_playback_rate) */
void sfx_pcm_rate(sfx *s, const unsigned char *pcm, int rate);
/* Music: called from the audio thread to add `frames` stereo float frames into out. */
typedef void (*sfx_music_fn)(void *user, float *out, int frames);
void sfx_set_music(sfx *s, sfx_music_fn fn, void *user);
int  sfx_rate(const sfx *s);
void sfx_lock(sfx *s);
void sfx_unlock(sfx *s);
/* Decode a record to 8-bit unsigned samples (for tests). Returns sample count, or -1. */
int  sfx_decode(prj_archive *a, int index, unsigned char **out);

/* TEST ONLY: with MERCS_SFX_LOG set every play is printed (stderr) with this clock (seconds, set by the caller) */
extern double sfx_log_time;

#endif
