/* Test the observer at real simulator launch/move sites, with the renderer
 * disabled. The fixture kernel is separately compared to original code. */
#include <assert.h>
#include <stdio.h>
#include "mercsparticles.h"
#include "../src/msim.c"

typedef struct {
    msim *sim;
    mercs_particles particles;
    int calls, kind;
    float origin[3], delta[3];
} particle_capture;
static particle_capture cap;
static void particle_event(void *user, int kind, const float origin[3], const float delta[3])
{
    particle_capture *c = user;
    int j; int32_t p[3], d[3];
    c->calls++; c->kind = kind;
    memcpy(c->origin, origin, sizeof c->origin);
    memcpy(c->delta, delta, sizeof c->delta);
    for (j = 0; j < 3; ++j) { p[j] = (int32_t)origin[j]; d[j] = (int32_t)delta[j]; }
    mercs_particles_emit(&c->particles, kind, p, d, sim_ticks(c->sim), NULL, 0);
}
static void same_sim(msim *on, const msim *off)
{
    on->on_particle = NULL; on->shot_user = NULL;
    assert(!memcmp(on, off, sizeof *on));
    on->on_particle = particle_event; on->shot_user = &cap;
}
int main(void)
{
    msim *on = calloc(1, sizeof *on), *off = calloc(1, sizeof *off);
    const float from[3] = {1000, 2000, 3000}, to[3] = {-9000, 2000, 3000};
    int w, j, launch_events = 0, move_events = 0;
    assert(on && off);
    for (w = 0; w < SIM_WEAPON_COUNT; ++w) {
        float before[3];
        int kind = combat_weapon_kind(w), missile = kind == 3 || kind == 4 || kind == 9 || kind == 10 || kind == 12;
        memset(on, 0, sizeof *on); memset(off, 0, sizeof *off);
        on->player_unit.destroyed = off->player_unit.destroyed = 1; /* no unrelated player fire */
        on->rng = off->rng = 12345;
        on->planet.gravity_g = off->planet.gravity_g = 1;
        memset(&cap, 0, sizeof cap); cap.sim = on;
        mercs_particles_init(&cap.particles, MERCS_PARTICLES_DOS, 1);
        on->on_particle = particle_event; on->shot_user = &cap;
        spawn_shot_h(on, w, -1, from, to, 0, -2);
        spawn_shot_h(off, w, -1, from, to, 0, -2);
        same_sim(on, off);
        assert(on->shot_count == 1 && cap.calls == (kind == 5));
        if (kind == 5) {
            /* Table speed is cm per 182-Hz tick. For the axis-aligned launch,
             * the original producer is at from + 2*velocity, with NO dt. */
            assert(cap.kind == MERCS_PARTICLE_BALLISTIC);
            assert(cap.delta[0] == -combat_weapon_speed(w));
            assert(cap.delta[1] == 0 && cap.delta[2] == 0);
            assert(cap.origin[0] == from[0] - 2 * combat_weapon_speed(w));
            assert(cap.origin[1] == from[1] && cap.origin[2] == from[2]);
            if (w == 43) assert(cap.origin[0] == 400 && cap.delta[0] == -300);
            ++launch_events;
        }
        before[0] = on->shots[0].x; before[1] = on->shots[0].y; before[2] = on->shots[0].z;
        cap.calls = 0;
        combat_step(on, 0.033f); combat_step(off, 0.033f);
        mercs_particles_step(&cap.particles, 6);
        same_sim(on, off);
        assert(on->shot_count == 1 && cap.calls == missile);
        if (missile) {
            float after[3] = {on->shots[0].x, on->shots[0].y, on->shots[0].z};
            assert(cap.kind == MERCS_PARTICLE_MISSILE);
            for (j = 0; j < 3; ++j) {
                assert(cap.origin[j] == before[j]);
                assert(cap.delta[j] == after[j] - before[j]);
            }
            assert(cap.delta[0] < -combat_weapon_speed(w)); /* the full frame, not one tick */
            ++move_events;
        }
        cap.calls = 0; on->shots[0].life = off->shots[0].life = 0;
        combat_step(on, 0.033f); combat_step(off, 0.033f);
        same_sim(on, off);
        assert(!on->shot_count && !cap.calls); /* an expired missile emits nothing */
    }
    for (w = 0; w < 8; ++w) {
        const int intervals[] = {1, 2, 5, 16, 33, 34, 100, 333};
        int ms = intervals[w], total = 0;
        off->now = 0;
        for (j = 0; j < 1000; ++j) {
            total += msim_fx_ticks_for_step(off, ms);
            off->now += ms;
            assert(total == sim_ticks(off));
        }
    }
    printf("PASS: all 66 weapon observers, %d ballistic launches, %d missile frame moves; entire simulator/RNG state unchanged; 8000 accumulated-clock steps\n", launch_events, move_events);
    free(on); free(off);
    return 0;
}
