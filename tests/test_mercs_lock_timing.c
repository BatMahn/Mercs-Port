/* Original-backed lock state sequences through the complete native consumer.
 * Declared equal-center units only: no scene, driver or career execution.
 * The source override permits a private negative control without replacing
 * shared production files or executables. */
#define _POSIX_C_SOURCE 200809L
#ifndef MERCS_LOCK_TIMING_SOURCE
#define MERCS_LOCK_TIMING_SOURCE "../src/msim.c"
#endif
#include MERCS_LOCK_TIMING_SOURCE
#include "../tests/fixtures/lock-timing/original-lock-timing-fixtures.h"

static void timing_unit(combat_unit *u, int human)
{
    int i;
    memset(u, 0, sizeof *u);
    u->human = human; u->unit_class = 1;
    u->armor_mult = u->speed_gain = 1;
    for (i = 0; i < 8; i++) u->armor[i] = u->internal[i] = 100;
    u->lock_target = -2;
}

int main(int argc, char **argv)
{
    unsigned flag_checks = 0, timer_checks = 0, mismatches = 0, profile_checks = 0;
    unsigned inexact_tick_conversions = 0;
    float max_tick_error = 0;
    size_t q;
    int profile;
    if (argc != 1) { fprintf(stderr, "usage: %s\n", argv[0]); return 2; }
    for (profile = COMBAT_PROFILE_DOS; profile <= COMBAT_PROFILE_TITANIUM; profile++) {
        combat_set_weapon_profile(profile);
        if (combat_lock_acquire_ticks() != (profile == COMBAT_PROFILE_DOS ? 364.0f : 362.0f)) mismatches++;
        profile_checks++;
    }
    printf("{\"sequences\":[\n");
    for (q = 0; q < sizeof original_lock_timing_sequences / sizeof original_lock_timing_sequences[0]; q++) {
        const original_lock_timing_sequence *seq = &original_lock_timing_sequences[q];
        msim *s = calloc(1, sizeof *s);
        world_actor *ac = calloc(1, sizeof *ac);
        combat_unit actor_unit;
        ai_mind mind;
        int armed = 1;
        float height = 1325, center, radius = 680;
        unsigned row, bad = 0;
        if (!s || !ac) { free(s); free(ac); return 2; }
        combat_set_weapon_profile(seq->dos ? COMBAT_PROFILE_DOS : COMBAT_PROFILE_RETAIL);
        timing_unit(&s->player_unit, 1); timing_unit(&actor_unit, 0);
        memset(&mind, 0, sizeof mind);
        s->actors = ac; s->actor_count = 1; s->units = &actor_unit; s->armed = &armed;
        s->height = &height; s->centre_h = &center; s->radius = &radius;
        s->player_radius = 680; s->player_unit_class = 1; s->player_victim = -2;
        s->no_player_muzzle = 1; s->rng = 1; s->difficulty = 1;
        s->player_target = s->player_lock_target = s->player_sel_w = -1;
        ac->unit_class = 1; ac->table = 1;
        if (seq->source_ai) {
            center = (float)seq->source_center;
            s->player_centre_h = (float)seq->target_center;
            s->player[2] = (float)seq->target_z;
            ac->alliance = 1; ac->gps_control = 2; ac->ai_gps6 = 0;
            s->minds = &mind; s->ai_ok = 1; mind.tgt = -1;
            actor_unit.weapon_count = 1; actor_unit.ai_sel = 0;
            actor_unit.weapons[0].weapon = 37; actor_unit.weapons[0].state = CW_READY;
            actor_unit.weapons[0].ammo = -1;
            s->nav_count = 1; s->navs[0].z = (float)seq->target_z;
            s->navs[0].y = (float)seq->target_center; s->navs[0].radius = 10000;
        } else {
            center = (float)seq->target_center;
            s->player_centre_h = (float)seq->source_center;
            ac->mech.origin[2] = seq->target_z; ac->alliance = 0; ac->friendly = 1;
            s->player_unit.weapon_count = 1;
            s->player_unit.weapons[0].weapon = 37;
            s->player_unit.weapons[0].state = CW_READY; s->player_unit.weapons[0].ammo = -1;
        }
        printf("%s{\"dos\":%d,\"source_ai\":%d,\"name\":\"%s\",\"steps\":%u,\"failures\":[",
               q ? ",\n" : "", seq->dos, seq->source_ai, seq->name, seq->count);
        for (row = 0; row < seq->count; row++) {
            const original_lock_timing_step *r = &seq->steps[row];
            float dt = (float)r->ticks / COMBAT_TICKS_PER_S, timer;
            float tick_error = fabsf(dt * COMBAT_TICKS_PER_S - (float)r->ticks);
            unsigned flags;
            int flag_bad, timer_bad;
            /* The native consumer takes seconds, so e.g. 6 and 100 ticks do
             * not round-trip exactly through its unchanged float conversion.
             * Bound and report that error; do not normalize the consumer or
             * claim parity for arbitrary fractional scheduling. */
            if (tick_error > 0.0001f) {
                fprintf(stderr, "native tick conversion error too large: %d %.9g\n", r->ticks, tick_error); return 2;
            }
            if (tick_error) inexact_tick_conversions++;
            if (tick_error > max_tick_error) max_tick_error = tick_error;
            if (seq->source_ai) {
                actor_unit.ai_sel = r->selected;
                ac->mech.heading = (float)r->yaw_q16 / 65536.0f;
                s->player_unit.narc_beacon = r->narc;
                ac->alliance = 1; mind.tgt = -1;
                if (r->target_code == 0x100) {
                    mind.tgt = -2; mind.state = AI_TARGET; mind.node_tgt = 0x10000;
                } else if (!r->target_code) {
                    ac->alliance = 2; mind.tgt = -2; mind.node_tgt = 0;
                }
            } else {
                s->player_sel_w = r->selected;
                s->player_lock_target = r->target_code == 0x201 ? 0 :
                    r->target_code == 0x100 ? MSIM_TGT_NAV(0) : -1;
                s->player_aim_yaw = (float)r->yaw_q16 / 65536.0f;
                actor_unit.narc_beacon = r->narc;
            }
            combat_step(s, dt);
            flags = (unsigned)(seq->source_ai ? mind.lock : s->player_lock) & 0x80c0u;
            timer = seq->source_ai ? mind.lock_ticks : s->player_lock_ticks;
            flag_bad = flags != r->flags;
            timer_bad = fabsf(timer - (float)r->timer) > 0.0001f;
            flag_checks++; timer_checks++;
            mismatches += (unsigned)(flag_bad + timer_bad);
            if (flag_bad || timer_bad)
                printf("%s{\"step\":%u,\"name\":\"%s\",\"flags\":%u,\"expected_flags\":%u,"
                       "\"timer\":%.9g,\"expected_timer\":%d}", bad++ ? "," : "", row, r->name,
                       flags, r->flags, timer, r->timer);
        }
        printf("],\"failed_steps\":%u}", bad);
        free(ac); free(s);
    }
    printf("\n],\"profile_checks\":%u,\"flag_checks\":%u,\"timer_checks\":%u,"
           "\"inexact_tick_conversions\":%u,\"max_tick_error\":%.9g,\"timer_tolerance\":0.0001,"
           "\"mismatches\":%u,\"pass_all\":%s}\n",
           profile_checks, flag_checks, timer_checks, inexact_tick_conversions, max_tick_error,
           mismatches, mismatches ? "false" : "true");
    return mismatches ? 1 : 0;
}
