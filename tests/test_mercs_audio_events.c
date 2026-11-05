/* Native producer integration. Include the actual front end, intercept only
 * its two audio-driver submission boundaries, and execute the real damage,
 * player-message, jet-mount and guided-fire consumers. SDL3 stays native.
 * The separate mixer fixture verifies decoded original audio and priorities. */
#include <assert.h>
#include "../src/sfx.h"
static void record_sound(sfx *s, int id, float level, float pan);
static void record_position(sfx *s, int id, float dx, float dy, float dz, float yaw, int half);
#define sfx_play record_sound
#define sfx_play_at3 record_position
#define main mercs_sim_main
#include "../tools/glview.c"
#undef main
#undef sfx_play_at3
#undef sfx_play
#include "fixtures/mercs_gameplay_audio_original.h"

static struct { int id, positional; float p[3], level, pan; } audio[64];
static int audio_count;
static void record_sound(sfx *s, int id, float level, float pan)
{
    (void)s;
    assert(audio_count < 64);
    audio[audio_count].id = id; audio[audio_count].positional = 0;
    audio[audio_count].level = level; audio[audio_count++].pan = pan;
}
static void record_position(sfx *s, int id, float dx, float dy, float dz, float yaw, int half)
{
    (void)s; (void)yaw; (void)half;
    assert(audio_count < 64);
    audio[audio_count].id = id; audio[audio_count].positional = 1;
    audio[audio_count].p[0] = dx; audio[audio_count].p[1] = dy; audio[audio_count++].p[2] = dz;
}
static void simulator_sound(void *user, int id, const float position[3])
{
    (void)user;
    if (position) record_position(NULL, id, position[0], position[1], position[2], 0, 0);
    else record_sound(NULL, id, SFX_V64, 0);
}

static void jet_events(void)
{
    msim *s = calloc(1, sizeof *s);
    mech3d m = {0};
    size_t i;
    assert(s); s->on_sound = simulator_sound;
    m.origin[0] = 1000; m.origin[2] = 3000;
    m.mount_local[6][0] = 1000; m.mount_local[6][1] = 2000; m.mount_local[6][2] = 3000;
    m.mount_local[7][0] = 2000; m.mount_local[7][1] = 3000; m.mount_local[7][2] = 4000;
    s->player_unit.y = 2000;
    for (i = 0; i < sizeof MERCS_ORIGINAL_JET_CASES / sizeof MERCS_ORIGINAL_JET_CASES[0]; ++i) {
        int expected = MERCS_ORIGINAL_JET_CASES[i].sound;
        combat_jet_refill_dos = MERCS_ORIGINAL_JET_CASES[i].dos;
        m.mount_local_ok = (unsigned)MERCS_ORIGINAL_JET_CASES[i].mounts << 6;
        s->player_unit.human = MERCS_ORIGINAL_JET_CASES[i].player;
        s->player_unit.jet_fuel = (float)MERCS_ORIGINAL_JET_CASES[i].fuel;
        s->player_unit.jet_on = 1; audio_count = 0;
        msim_jet_audio(s, &m, &s->player_unit, (float)MERCS_ORIGINAL_JET_CASES[i].ticks);
        assert(audio_count == (expected >= 0));
        if (expected >= 0) {
            int last = (MERCS_ORIGINAL_JET_CASES[i].mounts & 2) ? 7 : 6;
            assert(audio[0].id == expected && audio[0].positional == (expected == 243));
            if (expected == 243) {
                assert(audio[0].p[0] == (last == 7 ? 3000 : 2000));
                assert(audio[0].p[1] == (last == 7 ? 5000 : 4000));
                assert(audio[0].p[2] == (last == 7 ? 7000 : 6000));
            }
        }
        s->player_unit.jet_on = 0; audio_count = 0;
        msim_jet_audio(s, &m, &s->player_unit, (float)MERCS_ORIGINAL_JET_CASES[i].ticks);
        assert(!audio_count);
    }
    free(s);
    puts("PASS: 720 actual jet-mount callbacks match executed DOS/Windows originals, plus720 inactive-jet cases.");
}

static void warning_events(void)
{
    world_actor actor = {0};
    combat_unit unit = {0};
    ai_mind mind = {0};
    int armed = 1, edition, state;
    g_actors = &actor; g_actor_count = 1; g_sim.actor_count = 1;
    g_sim.actors = &actor; g_sim.units = &unit; g_sim.armed = &armed; g_sim.minds = &mind;
    g_sim_ok = 1; g_world = 1; g_cockpit = 0; g_camode = 1;
    mind.targets_player = 1; mind.state = AI_ATTACK;
    actor.mech.origin[2] = 1000;
    for (edition = 0; edition < 2; ++edition) {
        g_edition = edition ? PORTCFG_ED_D3D : PORTCFG_ED_DOS;
        memset(&g_sim.player_unit, 0, sizeof g_sim.player_unit);
        g_tracking_next_tick = g_tracking_last_tick = -1;
        g_sim.now = 0; audio_count = 0; player_messages(); assert(!audio_count);
        g_sim.now = 4999; player_messages(); assert(!audio_count);
        g_sim.now = 5000; player_messages(); assert(audio_count == 1 && audio[0].id == 278);
        assert(audio[0].level == SFX_V64 && !audio[0].positional);
        g_sim.now = 9999; player_messages(); assert(audio_count == 1);
        g_sim.now = 10000; player_messages(); assert(audio_count == 2);
        /* Distance is three-dimensional, even with the cockpit/HUD hidden. */
        actor.mech.origin[2] = 0; unit.y = 150001;
        g_sim.now = 15000; player_messages(); assert(audio_count == 2);
        unit.y = 150000; g_sim.now = 20000; player_messages(); assert(audio_count == 3);
        unit.y = 0; actor.mech.origin[2] = 1000;
        /* A new mission restarts the original five-second initial delay. */
        g_sim.now = 0; audio_count = 0; player_messages(); assert(!audio_count);
        g_sim.now = 4999; player_messages(); assert(!audio_count);
        for (state = 0; state < 3; ++state) {
            g_sim.player_unit.shutdown = state == 0;
            g_sim.player_unit.destroyed = state == 1;
            g_sim.player_unit.intact_out = state == 2;
            g_sim.now = 5000 + state * 5000; player_messages(); assert(!audio_count);
        }
    }
    g_sim.actor_count = g_actor_count = 0;
    g_sim.actors = NULL; g_sim.units = NULL; g_sim.armed = NULL; g_sim.minds = NULL; g_actors = NULL;

    combat_damage_live = 1;
    for (state = 0; state < 5; ++state) {
        combat_unit *p = &g_sim.player_unit;
        memset(p, 0, sizeof *p); p->human = 1; p->armor[0] = 1; p->internal[0] = 100;
        p->offline = state == 1; p->shutdown = state == 2;
        p->destroyed = state == 3; p->intact_out = state == 4;
        assert(player_systems_online() == (state == 0));
        audio_count = 0;
        combat_damage_at(p, 1, 1, &g_sim.rng);
        player_messages(); assert(audio_count == (state == 0));
        if (state == 0) assert(audio[0].id == 257 && !audio[0].positional);
        assert(!p->breach_sound);
        combat_damage_at(p, 1, 1, &g_sim.rng); player_messages();
        assert(audio_count == (state == 0));
    }
    puts("PASS: native incoming-track delay/cadence, 3D range and player gates; breach producer, HUD-independent consumption and state2 input gate.");
}

static void guided_fire_events(void)
{
    msim *s = calloc(1, sizeof *s);
    world_actor actor = {0}; combat_unit unit = {0}; ai_mind mind = {0};
    int behavior, target, n;
    assert(s); s->actors = &actor; s->units = &unit; s->minds = &mind; s->actor_count = 1;
    s->on_sound = simulator_sound;
    for (behavior = 0; behavior < 7; ++behavior)
        for (target = -1; target <= 0; ++target)
            for (n = 0; n < 100; ++n) {
                unsigned seed, next, roll;
                /* Select seeds spanning every integer original RNG outcome.
                 * The branch oracle is the executed original 0x100234a9. */
                for (seed = 0;; ++seed) {
                    next = seed * 1103515245u + 12345u;
                    roll = ((next >> 16) & 0x7fffu) % 100u;
                    if (roll == (unsigned)n) break;
                }
                actor.ai_gps6 = (uint8_t)behavior; actor.gps_control = 2;
                s->rng = seed; mind.lock = 0; unit.lock_target = -2; audio_count = 0;
                {
                    int expected = n < MERCS_ORIGINAL_WARNING_CHANCE[behavior];
                    assert(msim_enemy_fire_notice(s, 0, target) == expected);
                    assert(audio_count == (expected && target == -1));
                    assert(mind.lock == (expected ? 0x80 : 0));
                    assert(unit.lock_target == (expected ? target : -2));
                    assert(s->rng == next); /* exactly one roll */
                    if (audio_count) assert(audio[0].id == 92 && !audio[0].positional);
                }
            }
    free(s);
    puts("PASS: 1,400 native guided-fire probability/target events use one roll for original GPS6 audio and forced lock.");
}

int main(void)
{
    assert(SDL_Init(SDL_INIT_AUDIO)); g_sfx = sfx_create(NULL); assert(g_sfx);
    jet_events(); warning_events(); guided_fire_events();
    sfx_destroy(g_sfx); g_sfx = NULL; SDL_Quit();
    puts("PASS: actual native audio producers; no original or user save files modified.");
    return 0;
}
