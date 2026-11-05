/* Verify real Mercs SNDS records pass the mixer cache/voice gates, including
 * startup607 and weapon IDs over the inherited512 limit. */
#include <assert.h>
#include <stdio.h>
#include "../src/sfx.c"
#include "../src/wtable.h"
#include "../src/wtable_titanium.h"
#include "../src/mercsfx_data.h"
#include "../src/mercsmotionaudio.h"
#include "fixtures/mercs_motion_audio_original.h"
#include "fixtures/mercs_torso_audio_original.h"
#include "fixtures/mercs_gameplay_audio_original.h"
#include "fixtures/mercs_audio_pan_original.h"

typedef struct { int frames; } music_counter;

static void count_music(void *user, float *output, int frames)
{
    music_counter *counter = user;
    (void)output;
    counter->frames += frames;
}

/* Exercise the actual SDL3 demand callback through a real stream, including
 * requests that cross its bounded mixing buffer. The expected levels come
 * directly from the PCM values and existing volume/soft-limit contract. */
static void stream_callback_tests(void)
{
    static const int request_frames[] = {1, 1023, 1024, 1025, 4097};
    SDL_AudioSpec spec;
    unsigned char mono[256];
    int16_t stereo[128];
    float output[4097 * 2], expected_left, expected_right;
    music_counter music = {0};
    sfx *s = calloc(1, sizeof *s);
    int format, sample;
    size_t request;
    double held_position;
    assert(s);
    SDL_zero(spec); spec.freq = 44100; spec.channels = 2; spec.format = SDL_AUDIO_F32;
    s->rate = spec.freq;
    s->stream = SDL_CreateAudioStream(&spec, &spec); assert(s->stream);
    assert(SDL_SetAudioStreamGetCallback(s->stream, audio_requested, s));
    memset(mono, 192, sizeof mono);
    for (sample = 0; sample < 64; ++sample) { stereo[sample * 2] = 8192; stereo[sample * 2 + 1] = -16384; }
    for (format = 0; format < 2; ++format) {
        if (!format) {
            assert(sfx_play_pcm(s, mono, (int)sizeof mono, 11025, 0.8f, 1) == 0);
            expected_left = expected_right = tanhf(0.5f * 0.8f * 0.6f);
        } else {
            assert(sfx_play_pcm_s16(s, stereo, 64, 2, 22050, 0.8f, 1) == 0);
            expected_left = tanhf(0.25f * 0.8f * 0.6f);
            expected_right = tanhf(-0.5f * 0.8f * 0.6f);
        }
        for (request = 0; request < sizeof request_frames / sizeof request_frames[0]; ++request) {
            int bytes = request_frames[request] * 2 * (int)sizeof(float);
            memset(output, 0, sizeof output);
            assert(SDL_GetAudioStreamData(s->stream, output, bytes) == bytes);
            for (sample = 0; sample < request_frames[request]; ++sample) {
                assert(fabsf(output[sample * 2] - expected_left) < 1e-6f);
                assert(fabsf(output[sample * 2 + 1] - expected_right) < 1e-6f);
            }
        }
        if (!format) sfx_stop_pcm(s, mono);
    }
    assert(SDL_ClearAudioStream(s->stream));
    held_position = s->v[0].pos;
    audio_requested(s, s->stream, 0, 8192);
    assert(s->v[0].pos == held_position && SDL_GetAudioStreamQueued(s->stream) == 0);
    sfx_set_music(s, count_music, &music);
    sfx_pause(s, 1);
    assert(SDL_GetAudioStreamData(s->stream, output, (int)sizeof output) == (int)sizeof output);
    for (sample = 0; sample < (int)(sizeof output / sizeof output[0]); ++sample) assert(output[sample] == 0.0f);
    assert(s->v[0].pos == held_position && music.frames == 0);
    sfx_pause(s, 0);
    assert(SDL_GetAudioStreamData(s->stream, output, 256 * 2 * (int)sizeof(float)) == 256 * 2 * (int)sizeof(float));
    assert(music.frames == 256);
    assert(fabsf(output[0] - expected_left) < 1e-6f && fabsf(output[1] - expected_right) < 1e-6f);
    sfx_destroy(s);
    puts("PASS: native SDL3 stream callback preserves mono/stereo levels across variable requests and game pause freezes loops/music.");
}

static int music_frames(sfx *s, const music_counter *counter)
{
    int frames;
    sfx_lock(s); frames = counter->frames; sfx_unlock(s);
    return frames;
}

static void wait_for_music(sfx *s, const music_counter *counter, int previous)
{
    Uint64 deadline = SDL_GetTicks() + 2000;
    while (music_frames(s, counter) <= previous && SDL_GetTicks() < deadline) SDL_Delay(5);
    assert(music_frames(s, counter) > previous);
}

/* Run two genuine logical playback devices. Pausing/destroying one must not
 * interrupt the other or release the caller's audio subsystem reference. */
static void stream_lifecycle_tests(void)
{
    sfx *first = sfx_create(NULL), *second = sfx_create(NULL);
    music_counter one = {0}, two = {0};
    SDL_AudioSpec input;
    int held, other;
    long callbacks;
    Uint64 deadline;
    assert(first && second);
    assert(SDL_GetAudioStreamFormat(first->stream, &input, NULL));
    assert(input.freq == 44100 && input.channels == 2 && input.format == SDL_AUDIO_F32);
    assert(sfx_rate(first) == input.freq && !SDL_AudioStreamDevicePaused(first->stream));
    /* Public lock callers may invoke another locking mixer API. */
    sfx_lock(first); sfx_lock(first);
    sfx_set_music(first, count_music, &one);
    sfx_unlock(first); sfx_unlock(first);
    sfx_set_music(second, count_music, &two);
    wait_for_music(first, &one, 0); wait_for_music(second, &two, 0);
    assert(SDL_PauseAudioStreamDevice(first->stream));
    held = music_frames(first, &one); other = music_frames(second, &two);
    wait_for_music(second, &two, other);
    assert(music_frames(first, &one) == held && SDL_AudioStreamDevicePaused(first->stream));
    assert(SDL_ResumeAudioStreamDevice(first->stream));
    wait_for_music(first, &one, held);
    sfx_pause(first, 1);
    held = music_frames(first, &one);
    sfx_lock(first); callbacks = first->callbacks; sfx_unlock(first);
    deadline = SDL_GetTicks() + 2000;
    for (;;) {
        int advanced;
        sfx_lock(first); advanced = first->callbacks > callbacks; sfx_unlock(first);
        if (advanced) break;
        assert(SDL_GetTicks() < deadline); SDL_Delay(5);
    }
    assert(!SDL_AudioStreamDevicePaused(first->stream) && music_frames(first, &one) == held);
    sfx_pause(first, 0); wait_for_music(first, &one, held);
    sfx_destroy(first);
    held = one.frames; other = music_frames(second, &two);
    wait_for_music(second, &two, other);
    assert(one.frames == held && (SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO));
    sfx_destroy(second);
    held = two.frames; SDL_Delay(30); assert(two.frames == held);
    puts("PASS: SDL3 callbacks run, recursive stream locking works, pause/resume isolates devices, and destruction stops callbacks while preserving caller audio ownership.");
}

static void movement_audio(prj_archive *a, sfx *s)
{
    static const struct { int id; const char *name; } records[] = {
        {MERCS_SOUND_FOOTSTEP_OTHER, "NONFOOT"},
        {MERCS_SOUND_FOOTSTEP_MEDIUM, "MECFSMED"},
        {MERCS_SOUND_IMPACT_BUILDING, "MECBLDC1"},
        {MERCS_SOUND_LANDING_HARD, "MECMTNHD"},
        {MERCS_SOUND_LANDING_SOFT, "MECMTNSF"},
        {MERCS_SOUND_TORSO_SERVO, "TORSLOOP"},
        {MERCS_SOUND_MENU_MOVE, "MECGCLX1"},
        {MERCS_SOUND_MENU_OPEN, "MECMENU1"},
        {339, "STOP2WLK"}, {352, "WLK2STOP"}, {294, "RUN2WLK"},
        {348, "WALK2RUN"}, {293, "REV2STOP"}, {338, "STOP2REV"}
    };
    size_t i;
    int type = prj_find_type(a, "SNDS");
    assert(type >= 0);
    for (i = 0; i < sizeof MERCS_ORIGINAL_TORSO_CASES / sizeof MERCS_ORIGINAL_TORSO_CASES[0]; ++i) {
        int active = mercs_motion_torso_active(
            (float)MERCS_ORIGINAL_TORSO_CASES[i].demand / 65536.0f,
            (float)MERCS_ORIGINAL_TORSO_CASES[i].actual / 65536.0f,
            MERCS_ORIGINAL_TORSO_CASES[i].cockpit);
        assert((active ? MERCS_SOUND_TORSO_SERVO : -1) == MERCS_ORIGINAL_TORSO_CASES[i].sound);
    }
    /* The mistaken Mech2 servo index names real Mercs dialogue. Do not
     * "fix" the turning noise by suppressing that record globally. */
    assert(prj_find_id(a, type, "STAR3A1") == 316);
    for (i = 0; i < sizeof MERCS_ORIGINAL_GAIT_CASES / sizeof MERCS_ORIGINAL_GAIT_CASES[0]; ++i) {
        int state = MERCS_ORIGINAL_GAIT_CASES[i].previous;
        int sound = mercs_motion_gait_sound(&state, MERCS_ORIGINAL_GAIT_CASES[i].current,
                                           MERCS_ORIGINAL_GAIT_CASES[i].requested);
        assert(sound == MERCS_ORIGINAL_GAIT_CASES[i].sound);
        assert(state == MERCS_ORIGINAL_GAIT_CASES[i].next_state);
        assert(mercs_motion_gait_sound(&state, MERCS_ORIGINAL_GAIT_CASES[i].current,
                                       MERCS_ORIGINAL_GAIT_CASES[i].requested) == -1);
    }
    /* Freeze the asynchronous callback while checking each real movement clip
     * reaches an effects channel. Keep unrelated voices out of this fixture. */
    assert(SDL_PauseAudioStreamDevice(s->stream));
    for (i = 0; i < sizeof records / sizeof records[0]; ++i) {
        int channel, found = 0;
        assert(prj_find_id(a, type, records[i].name) == records[i].id);
        memset(s->v, 0, sizeof s->v);
        sfx_play_at3(s, records[i].id, 100, 50, 100, 0, 1);
        for (channel = 0; channel < SFX_VOICES; ++channel)
            if (s->v[channel].c && s->v[channel].index == records[i].id) {
                assert(!s->v[channel].is_voice);
                assert(s->v[channel].c->data && s->v[channel].c->len > 0);
                found = 1;
            }
        assert(found);
    }
    memset(s->v, 0, sizeof s->v);
    sfx_play_voice(s, 316, 1.0f);
    assert(s->v[0].c && s->v[0].is_voice && s->v[0].index == 316 && !s->v[0].loop);
    puts("PASS: 80 original gait decisions, 18 original player torso gates, 14 real Mercs movement/menu records and unchanged legitimate STAR3A1 speech.");
}

static int voices_for(const sfx *s, int id)
{
    int k, n = 0;
    for (k = 0; k < SFX_VOICES; ++k) if (s->v[k].c && s->v[k].index == id) ++n;
    return n;
}

static void positional_pan(sfx *s)
{
    size_t i;
    assert(SDL_PauseAudioStreamDevice(s->stream));
    sfx_set_world(s, 0, 182);
    for (i = 0; i < sizeof MERCS_ORIGINAL_PAN / sizeof MERCS_ORIGINAL_PAN[0]; ++i) {
        int p = MERCS_ORIGINAL_PAN[i].pan;
        float x = (float)MERCS_ORIGINAL_PAN[i].dx, z = (float)MERCS_ORIGINAL_PAN[i].dz;
        float yaw = (float)MERCS_ORIGINAL_PAN[i].yaw_q16 / 65536.0f;
        float left = p >= 63 ? 1.0f : (float)(2 * p) / 128.0f;
        float right = 127-p >= 63 ? 1.0f : (float)(2 * (127-p)) / 128.0f;
        float level;
        assert(bwd_angle_atan2((int32_t)x, (int32_t)z) == MERCS_ORIGINAL_PAN[i].bearing_q16);
        assert(sfx_pos_pan_engine(x, z, yaw) == p);
        memset(s->v, 0, sizeof s->v);
        sfx_play_at3(s, 243, x, 0, z, yaw, 0);
        assert(voices_for(s, 243) == 1);
        level = fmaxf(s->v[0].l, s->v[0].r);
        assert(level > 0 && fabsf(s->v[0].l - level * left) < 1e-6f &&
               fabsf(s->v[0].r - level * right) < 1e-6f);
    }
    assert(sfx_pos_pan_engine(NAN, 1, 0) == 64);
    assert(sfx_pos_pan_engine(1, INFINITY, 0) == 64);
    assert(sfx_pos_pan_engine(1, 1, NAN) == 64);
    memset(s->v, 0, sizeof s->v);
    puts("PASS: 1,134 original Mercs vector/pan cases retain fractional bearing and signed rounding through actual mixer channel levels.");
}

static void gameplay_audio(prj_archive *a, sfx *s)
{
    static const struct { int id; const char *name; } names[] = {
        {MERCS_SOUND_ENEMY_FIRE,"ENEMYFIR"}, {MERCS_SOUND_INFRARED,"IREDMODE"},
        {MERCS_SOUND_JETTISON,"JETTISON"}, {MERCS_SOUND_EJECT,"MECBEJXX"},
        {MERCS_SOUND_MASC_FAILED,"MECBMAMA"}, {MERCS_SOUND_MASC_OFF,"MECBMAOF"},
        {MERCS_SOUND_MASC_ON,"MECBMAON"}, {MERCS_SOUND_SHUTDOWN_OVERRIDE,"MECBSOXX"},
        {MERCS_SOUND_SYSTEMS_READY,"MECBSYRX"}, {MERCS_SOUND_TRACK_ACQUIRED,"MECBYHLX"},
        {MERCS_SOUND_CRITICAL,"MECCHIN1"}, {MERCS_SOUND_DISPLAY,"MECHUDON"},
        {MERCS_SOUND_JET_FUEL,"MECJFUEL"}, {MERCS_SOUND_JET,"MECJJRXX"},
        {MERCS_SOUND_NAV_ARRIVAL,"MECNAVPT"}, {MERCS_SOUND_MAP,"MECOMBEP"},
        {MERCS_SOUND_OVERHEAT,"MECOVHW1"}, {MERCS_SOUND_ARMOR_BREACH,"MECRDWA1"},
        {MERCS_SOUND_SHUTDOWN,"MECSHTD1"}, {MERCS_SOUND_STARTUP,"MECTURX2"},
        {MERCS_SOUND_FIRE_GROUP,"MECWPTG1"}, {MERCS_SOUND_TARGET_LOCK,"MECXLOCK"},
        {MERCS_SOUND_INCOMING_TRACK,"MISLTRCK"}, {MERCS_SOUND_OTHER_STARTUP,"NONPSTRT"},
        {MERCS_SOUND_VIEW_ZOOM,"VIEWZOOM"}, {MERCS_SOUND_WEAPON_BUSY,"WEPNBUSY"}
    };
    unsigned char ambient[24][128];
    int16_t stereo[16] = {0};
    int type = prj_find_type(a, "SNDS"), k;
    size_t i;
    assert(SDL_PauseAudioStreamDevice(s->stream));
    for (i = 0; i < sizeof names / sizeof names[0]; ++i) {
        assert(prj_find_id(a, type, names[i].name) == names[i].id);
        memset(s->v, 0, sizeof s->v);
        sfx_play(s, names[i].id, SFX_V64, 0);
        assert(voices_for(s, names[i].id) == 1);
        assert(s->v[0].c->len > 0 && !s->v[0].is_voice && !s->v[0].loop);
    }
    for (i = 0; i < sizeof MERCS_ORIGINAL_JET_CASES / sizeof MERCS_ORIGINAL_JET_CASES[0]; ++i)
        assert(mercs_motion_jet_sound(MERCS_ORIGINAL_JET_CASES[i].dos,
            MERCS_ORIGINAL_JET_CASES[i].player, MERCS_ORIGINAL_JET_CASES[i].mounts,
            (float)MERCS_ORIGINAL_JET_CASES[i].fuel, (float)MERCS_ORIGINAL_JET_CASES[i].ticks)
            == MERCS_ORIGINAL_JET_CASES[i].sound);
    assert(sizeof SND_TABLE / sizeof SND_TABLE[0] == sizeof MERCS_ORIGINAL_SOUND_RULES / sizeof MERCS_ORIGINAL_SOUND_RULES[0]);
    for (i = 0; i < sizeof MERCS_ORIGINAL_SOUND_RULES / sizeof MERCS_ORIGINAL_SOUND_RULES[0]; ++i) {
        int priority, cap;
        snd_rule(MERCS_ORIGINAL_SOUND_RULES[i].id, &priority, &cap);
        assert(priority == (MERCS_ORIGINAL_SOUND_RULES[i].priority < 0 ? 50 : MERCS_ORIGINAL_SOUND_RULES[i].priority));
        assert(cap == MERCS_ORIGINAL_SOUND_RULES[i].cap);
        assert(SND_TABLE[i][0] == MERCS_ORIGINAL_SOUND_RULES[i].id && SND_TABLE[i][3] == MERCS_ORIGINAL_SOUND_RULES[i].rate);
    }
    for (i = 0; i < sizeof MERCS_ORIGINAL_WARNING_CHANCE / sizeof MERCS_ORIGINAL_WARNING_CHANCE[0]; ++i)
        assert(mercs_motion_enemy_fire_chance((unsigned)i) == MERCS_ORIGINAL_WARNING_CHANCE[i]);
    for (i = 0; i < sizeof MERCS_ORIGINAL_FORCED_LOCK / sizeof MERCS_ORIGINAL_FORCED_LOCK[0]; ++i)
        assert(mercs_motion_forced_lock(MERCS_ORIGINAL_FORCED_LOCK[i].control,
            MERCS_ORIGINAL_FORCED_LOCK[i].chance, MERCS_ORIGINAL_FORCED_LOCK[i].flags)
            == (MERCS_ORIGINAL_FORCED_LOCK[i].after != 0));
    for (i = 0; i < sizeof MERCS_ORIGINAL_POSITIONAL / sizeof MERCS_ORIGINAL_POSITIONAL[0]; ++i) {
        assert(positional_level(MERCS_ORIGINAL_POSITIONAL[i].distance,
            MERCS_ORIGINAL_POSITIONAL[i].limit, MERCS_ORIGINAL_POSITIONAL[i].half)
            == MERCS_ORIGINAL_POSITIONAL[i].volume);
        assert(positional_delay(MERCS_ORIGINAL_POSITIONAL[i].distance) == MERCS_ORIGINAL_POSITIONAL[i].delay);
        sfx_set_world(s, MERCS_ORIGINAL_POSITIONAL[i].limit, 182);
        memset(s->v, 0, sizeof s->v);
        sfx_play_at3(s, 243, 0, 0, (float)MERCS_ORIGINAL_POSITIONAL[i].distance, 0,
                    MERCS_ORIGINAL_POSITIONAL[i].half);
        assert(voices_for(s, 243) == (MERCS_ORIGINAL_POSITIONAL[i].volume > 1));
    }
    sfx_set_world(s, 0, 182);

    /* Reproduce the original allocator probes using actual decoded clips.
     * Eight ambient loops never consume the eight ordinary handles. */
    memset(s->v, 0, sizeof s->v); memset(ambient, 128, sizeof ambient);
    for (k = 0; k < 8; ++k) assert(sfx_play_pcm(s, ambient[k], 128, 11025, 0.3f, 1) == 0);
    for (k = 0; k < 8; ++k) sfx_play(s, 149, SFX_V64, 0);
    assert(voices_for(s, 149) == 8);
    sfx_play(s, 149, SFX_V64, 0); assert(voices_for(s, 149) == 8); /* equal priority cannot steal */
    sfx_play(s, 243, SFX_V64, 0); assert(!voices_for(s, 243)); /* priority0 cannot steal */
    sfx_play(s, 225, SFX_V64, 0); assert(s->v[15].index == 225 && voices_for(s, 149) == 7);
    sfx_play_voice(s, 607, SFX_V64); assert(s->v[15].index == 607 && s->v[15].is_voice);
    sfx_play(s, 225, SFX_V64, 0); assert(s->v[14].index == 225 && s->v[15].index == 607);
    for (k = 0; k < 8; ++k) assert(sfx_pcm_playing(s, ambient[k]));
    memset(s->v, 0, sizeof s->v);
    sfx_play(s, 347, SFX_V64, 0); sfx_play(s, 347, SFX_V64, 0);
    assert(voices_for(s, 347) == 1);
    sfx_play(s, 341, SFX_V64, 0); sfx_play(s, 341, SFX_V64, 0);
    assert(voices_for(s, 341) == 1);
    /* Saturated native borrowed-PCM storage fails cleanly. It must not
     * overwrite channel0, potentially replacing a live FMV/voice buffer. */
    memset(s->v, 0, sizeof s->v);
    for (k = 0; k < SFX_VOICES; ++k) assert(sfx_play_pcm(s, ambient[k], 128, 11025, 0.3f, 1) == 0);
    assert(sfx_play_pcm(s, ambient[0], 128, 11025, 1, 0) == -1);
    assert(sfx_play_pcm_s16(s, stereo, 8, 2, 22050, 1, 0) == -1);
    assert(s->v[0].loop && s->v[0].own.data == ambient[0]);
    memset(s->v, 0, sizeof s->v);
    puts("PASS: 26 gameplay records decode; 720 original jet cases, 45 sound rules, GPS6/forced-lock cases, shared voice/effect priority and isolated ambient handles agree.");
}

static void data_driven_audio(sfx *s)
{
    unsigned char seen[SFX_CACHE] = {0};
    int row, id, unique = 0;
    /* The extraction probe independently compares the retail66 weapon and
     * DOS/Windows36 effect sound fields with the original binary tables.
     * Check every referenced clip in each actual edition archive as well. */
    for (row = 0; row < SIM_WEAPON_COUNT; ++row) {
        id = sim_weapons[row].sound; if (id > 0) { assert(id < SFX_CACHE); seen[id] = 1; }
        id = sim_weapons_titanium[row].sound; if (id > 0) { assert(id < SFX_CACHE); seen[id] = 1; }
    }
    for (row = 0; row < 36; ++row) {
        id = mercs_fx_windows[row][2]; if (id > 0) { assert(id < SFX_CACHE); seen[id] = 1; }
    }
    for (id = 1; id < SFX_CACHE; ++id) if (seen[id]) {
        memset(s->v, 0, sizeof s->v);
        sfx_play(s, id, SFX_V64, 0);
        assert(voices_for(s, id) == 1 && s->v[0].c->len > 0);
        ++unique;
    }
    memset(s->v, 0, sizeof s->v);
    printf("PASS: all66 retail and66 Titanium weapon rows plus36 effects resolve to%d playable original sound clips.\n", unique);
}

static void independent_pcm_gains(sfx *s)
{
    static const float gains[] = {0.0f, 0.375f, 1.0f};
    unsigned char pcm[256];
    float output[512];
    size_t effect, speech;
    int is_voice, sample;
    /* A constant half-scale PCM signal has an independently known output
     * after the start ramp. Exercise the real callback, including Effects=0
     * with audible speech, where the old inverse-effects adjustment made NaN. */
    memset(pcm, 192, sizeof pcm);
    assert(SDL_PauseAudioStreamDevice(s->stream));
    for (effect = 0; effect < sizeof gains / sizeof gains[0]; ++effect)
        for (speech = 0; speech < sizeof gains / sizeof gains[0]; ++speech)
            for (is_voice = 0; is_voice <= 1; ++is_voice) {
                float level = gains[is_voice ? speech : effect];
                float expected = tanhf(0.5f * 0.8f * level * 0.6f);
                memset(s->v, 0, sizeof s->v);
                sfx_set_gains(s, gains[effect], gains[speech]);
                if (is_voice) sfx_play_pcm_voice(s, pcm, (int)sizeof pcm, 0.8f);
                else assert(sfx_play_pcm(s, pcm, (int)sizeof pcm, 11025, 0.8f, 0) == 0);
                assert(s->v[0].is_voice == is_voice);
                assert(isfinite(s->v[0].l) && isfinite(s->v[0].r));
                assert(fabsf(s->v[0].l - 0.8f * level) < 1e-6f);
                assert(fabsf(s->v[0].r - 0.8f * level) < 1e-6f);
                /* The device is paused; only this deterministic callback
                 * advances the selected channel. Its rate is set explicitly. */
                s->v[0].step = 1.0;
                mix(s, (Uint8 *)output, (int)sizeof output);
                for (sample = 0; sample < (int)(sizeof output / sizeof output[0]); ++sample) {
                    assert(isfinite(output[sample]));
                    if (level == 0.0f) assert(output[sample] == 0.0f);
                }
                assert(fabsf(output[128] - expected) < 1e-6f);
                assert(fabsf(output[129] - expected) < 1e-6f);
            }
    memset(s->v, 0, sizeof s->v);
    sfx_set_gains(s, 1.0f, 1.0f);
    puts("PASS: 18 PCM mixer cases keep effects/voice gains independent, finite and correctly scaled at zero, fractional and full levels.");
}

int main(int argc,char **argv)
{
    char error[256];
    prj_archive *a;
    sfx *s;
    const int weapons[]={19,22,7,23};
    int i;
    assert(argc==2);
    a=prj_open(argv[1],error,sizeof error);
    if (!a) { fprintf(stderr,"%s\n",error); return 1; }
    assert(SDL_Init(SDL_INIT_AUDIO));
    s=sfx_create(a); assert(s);
    sfx_play_voice(s,607,0.9f);
    assert(s->cache[607].data && s->cache[607].len>0);
    assert(sfx_voice_busy(s));
    sfx_play(s,225,0.7f,0);
    assert(s->cache[225].data && s->cache[225].len>0);
    for (i=0;i<4;i++) {
        int id=sim_weapons[weapons[i]].sound;
        sfx_play(s,id,0.7f,0);
        assert(s->cache[id].data && s->cache[id].len>0);
        printf("weapon%d sound%d decoded%d samples\n",weapons[i],id,s->cache[id].len);
    }
    assert(s->plays==6);
    movement_audio(a,s);
    gameplay_audio(a,s);
    positional_pan(s);
    data_driven_audio(s);
    independent_pcm_gains(s);
    sfx_destroy(s);
    stream_callback_tests(); stream_lifecycle_tests();
    SDL_Quit(); prj_close(a);
    puts("PASS: original startup voice607, critical alarm225 and all four Kodiak weapon sounds decode and reach the native mixer.");
    return 0;
}
