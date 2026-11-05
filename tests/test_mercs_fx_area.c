/* Probe the real native area_damage caller, capturing its downstream damage
 * calls just as the original-routine harness does. Compare emitted values
 * with independently executed Mercs x86 fixtures, not duplicate formulas. */
#include "combat.h"
static void capture_damage(combat_unit *unit, float amount, int location, unsigned *rng);
#define combat_damage_at capture_damage
#include "../src/msim.c"
#undef combat_damage_at
#include <assert.h>
#include "../tests/fixtures/result-audit/original-fx-area-fixtures.h"
#include "../tests/fixtures/result-audit/original-fx-lifecycle.h"
#include "../tests/fixtures/combat-completion/original-wreck.h"

static int locations[32], count;
static long amounts[32];
typedef struct { int calls, ticks, damage_seen; fx_world visual; } tick_capture;
static void capture_tick(void *user, int ticks)
{
    tick_capture *c = user;
    c->calls++; c->ticks += ticks; c->damage_seen = count;
    fx_update(&c->visual, ticks);
}
static void capture_damage(combat_unit *unit, float amount, int location, unsigned *rng)
{
    (void)unit; (void)rng;
    if (count < 32) { locations[count] = location; amounts[count++] = lroundf(amount * 65536.0f); }
}
int main(int argc, char **argv)
{
    msim *s;
    int type, state, dt, i, elapsed;
    if (argc == 1) {
        int fixture;
        for (fixture = 0; fixture < MERCS_ORIGINAL_FX_AREA_FIXTURE_COUNT; fixture++) {
            const mercs_original_fx_area_fixture *f = &mercs_original_fx_area_fixtures[fixture];
            s = calloc(1, sizeof *s); assert(s); count = 0;
            fx_set_timing(0); s->now = 1000; s->splash = 1;
            s->player_radius = (float)f->controller_radius;
            s->player[0] = (float)f->position[0]; s->player_unit.y = (float)f->position[1];
            s->player[2] = (float)f->position[2]; s->player_unit.heat = 10;
            s->player_unit.destroyed = f->state == 4 || f->state == 5;
            s->player_unit.intact_out = f->state == 5;
            s->area_count = 1; s->area[0].type = f->type; s->area[0].r = (float)f->effect_radius;
            s->area[0].duration = fx_duration(f->type);
            s->area[0].remaining = s->area[0].duration - f->elapsed_ticks;
            assert(fx_damage_q16(f->type) == f->damage_coefficient);
            assert(fx_heat_q16(f->type) == f->heat_coefficient);
            area_damage(s, f->delta_ticks);
            if (count != f->damage_call_count) { fprintf(stderr, "%s: call count\n", f->name); return 1; }
            for (i = 0; i < count; i++) {
                if (locations[i] != i + 1 || amounts[i] != f->damage_q16[i]) {
                    fprintf(stderr, "%s: location %d raw damage %ld, expected %d\n", f->name, locations[i], amounts[i], f->damage_q16[i]); return 1;
                }
            }
            assert(lroundf((s->player_unit.heat - 10.0f) * 65536.0f) == f->heat_added_q16);
            free(s);
        }
        puts("PASS: 30 native area-effect fixtures match both original retail engines' captured damage calls and heat");
        for (fixture = 0; fixture < (int)(sizeof original_wreck_areas / sizeof original_wreck_areas[0]); fixture++) {
            const original_wreck_area_case *f = &original_wreck_areas[fixture];
            s = calloc(1, sizeof *s); assert(s); count = 0;
            s->splash = 1; s->player_radius = 100;
            s->player_unit.destroyed = f->state == 4 || f->state == 5;
            s->player_unit.intact_out = f->state == 5;
            s->player_unit.body_detached = (f->flags & 0x100) != 0;
            s->area_count = 1; s->area[0].type = 10; s->area[0].r = 40;
            s->area[0].duration = s->area[0].remaining = fx_duration(10);
            area_damage(s, 6);
            assert(count == f->calls && lroundf(s->player_unit.heat * 65536) == f->heat);
            free(s);
        }
        puts("PASS: original wreck flag0x100 excludes the emptied controller from damage and heat in every state");
        for (fixture = 0; fixture < (int)(sizeof original_fx_creation / sizeof original_fx_creation[0]); fixture++) {
            const fx_creation_fixture *f = &original_fx_creation[fixture];
            const float origin[3] = {0, 0, 0};
            s = calloc(1, sizeof *s); assert(s); fx_set_timing(f->dos);
            s->fx_delta_ticks = f->delta;
            effect(s, f->type, origin, 0);
            assert(s->area_count == (fx_damage_q16(f->type) != 0));
            if (s->area_count) assert(s->area[0].duration == f->duration && s->area[0].remaining == f->remaining);
            free(s);
        }
        for (fixture = 0; fixture < (int)(sizeof original_fx_updates / sizeof original_fx_updates[0]); fixture++) {
            const fx_update_fixture *f = &original_fx_updates[fixture];
            s = calloc(1, sizeof *s); assert(s); count = 0; fx_set_timing(f->dos);
            s->splash = f->splash; s->player_radius = 60; s->player_unit.heat = 10;
            s->player[0] = (float)f->position[0]; s->player_unit.y = (float)f->position[1];
            s->player[2] = (float)f->position[2];
            s->area_count = f->active && fx_damage_q16(f->type) != 0;
            s->area[0].type = f->type; s->area[0].r = 40;
            s->area[0].duration = f->duration; s->area[0].remaining = f->remaining;
            if (f->unit_calls) assert(fx_area_radius(f->type, 40, f->duration - f->remaining) == f->radius);
            area_damage(s, f->delta);
            assert(count == f->damage_count);
            for (i = 0; i < count; i++) assert(locations[i] == i + 1 && amounts[i] == f->damage[i]);
            assert(lroundf((s->player_unit.heat - 10.0f) * 65536.0f) == f->heat);
            if (fx_damage_q16(f->type)) {
                assert(s->area_count == f->active_after);
                if (s->area_count) assert(s->area[0].remaining == f->remaining_after);
            }
            if (!s->area_count) { count = 0; area_damage(s, f->delta); assert(count == 0); }
            free(s);
        }
        puts("PASS: original DOS/Windows lifecycle captures match native creation, final-step damage, signed radius and expiry");
        {
            /* Declared empty-world integration fixture: exercise the real step
             * boundary, callback order, pause, and carried sub-tick time. */
            tick_capture observed = {0};
            s = calloc(1, sizeof *s); assert(s); count = 0; fx_set_timing(1);
            s->player_target = s->player_lock_target = s->player_sel_w = -1;
            s->player_radius = 60; s->splash = 1;
            s->on_effect_tick = capture_tick; s->shot_user = &observed;
            s->area_count = 1; s->area[0].type = 3; s->area[0].r = 40;
            s->area[0].duration = fx_duration(3); s->area[0].remaining = 1;
            observed.visual.e[0].alive = 1; observed.visual.e[0].remaining = 1;
            msim_step(s, 40);
            assert(s->fx_delta_ticks == 7 && s->area_count == 0 && count == 8);
            assert(observed.calls == 1 && observed.ticks == 7 && observed.damage_seen == 8);
            assert(observed.visual.e[0].alive == 0 && observed.visual.e[0].remaining == 0);
            msim_step(s, 0);
            assert(observed.calls == 1 && observed.ticks == 7 && count == 8);
            for (i = 0; i < 20; i++) msim_step(s, 1);
            assert(s->now == 60 && observed.calls == 21 && observed.ticks == 10);
            assert(count == 8); /* removed effects never damage again */
            free(s);
        }
        puts("PASS: integer FX clock carries fractional ticks; one late callback per step, none while paused");
        return 0;
    }
    if (argc != 11) {
        fprintf(stderr, "usage: %s type state ticks mech-radius effect-radius x y z splash elapsed\n", argv[0]);
        return 2;
    }
    s = calloc(1, sizeof *s); if (!s) return 1;
    type = atoi(argv[1]); state = atoi(argv[2]); dt = atoi(argv[3]); elapsed = atoi(argv[10]);
    if (type < 0 || type >= FX_TYPES || dt < 0 || dt > 1000) { free(s); return 2; }
    fx_set_timing(0); s->now = 1000; s->splash = atoi(argv[9]);
    s->player_radius = (float)atoi(argv[4]);
    s->player[0] = (float)atoi(argv[6]); s->player_unit.y = (float)atoi(argv[7]);
    s->player[2] = (float)atoi(argv[8]);
    s->player_unit.heat = 10; s->player_unit.destroyed = state == 4 || state == 5;
    s->player_unit.intact_out = state == 5;
    s->area_count = 1; s->area[0].type = type; s->area[0].r = (float)atoi(argv[5]);
    s->area[0].duration = fx_duration(type); s->area[0].remaining = s->area[0].duration - elapsed;
    area_damage(s, dt);
    printf("{\"damage_calls\":[");
    for (i = 0; i < count; i++) printf("%s{\"damage_q16\":%ld,\"location\":%d}",
                                    i ? "," : "", amounts[i], locations[i]);
    printf("],\"heat_added_q16\":%ld}\n", lroundf((s->player_unit.heat - 10.0f) * 65536.0f));
    free(s); return 0;
}
