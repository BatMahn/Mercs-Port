/* Full CAR outputs come from both original executables, never from native
 * calculations. The transport pass also covers once-only event delivery. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msim.h"
#include "../tests/fixtures/result-audit/statistics_original_fixtures.h"

static msim sim;
static world_actor actor;
static combat_unit unit;
static unsigned char seen;

static void seed(uint8_t *p, int wrap)
{
    size_t i;
    for (i = 0; i < MERCS_CAR_BYTES; i++) p[i] = wrap ? 255 : (uint8_t)(i * 29 + 7);
}

static void compare(const char *name, const uint8_t *actual, const uint8_t *expected)
{
    size_t i;
    for (i = 0; i < MERCS_CAR_BYTES; i++) if (actual[i] != expected[i]) {
        fprintf(stderr, "%s CAR+0x%zx: native %02x original %02x\n",
                name, i, actual[i], expected[i]);
        abort();
    }
}

static void pure_producers(void)
{
    size_t i;
    int k;
    mercs_car_record car;
    for (i = 0; i < MERCS_START_FIXTURE_COUNT; i++) {
        const mercs_start_fixture *f = &mercs_start_fixtures[i];
        seed(car.bytes, f->wrap);
        for (k = 0; k < f->nu; k++) assert(!mercs_car_count_initial(&car, 0, f->units[k]));
        for (k = 0; k < f->nt; k++) assert(!mercs_car_count_initial(&car, 1, f->things[k]));
        compare(f->name, car.bytes, f->expected);
    }
    for (i = 0; i < MERCS_STATS_FIXTURE_COUNT; i++) {
        const mercs_stats_fixture *f = &mercs_stats_fixtures[i];
        const int *a = f->args;
        seed(car.bytes, f->wrap);
        if (f->kind == 1) {
            mercs_car_unit_loss e;
            memset(&e, 0, sizeof e);
            e.loss.is_player = a[0]; e.loss.actor_flags = (uint16_t)a[1];
            e.loss.invulnerable = a[2]; e.loss.controller_state = a[3];
            e.loss.hazardous_ejection = a[4]; e.unit_class = a[5];
            e.alliance = (uint8_t)a[6]; e.same_player_star = a[7]; e.killer_is_player = a[8];
            assert(!mercs_car_count_unit_loss(&car, &e));
            assert(e.loss.controller_state == f->state);
        } else if (f->kind == 2) {
            assert(!mercs_car_count_thing_loss(&car, (uint16_t)a[0], (uint8_t)a[1], a[2], a[3]));
        } else if (f->kind == 3) {
            assert(!mercs_car_count_unit_hit(&car, (uint8_t)a[0], a[1], a[2], a[3], a[4]));
        } else {
            assert(f->kind == 4);
            assert(!mercs_car_count_shot(&car, a[0], a[1]));
        }
        compare(f->name, car.bytes, f->expected);
    }
    printf("PASS: %zu complete CAR producer outputs match DOS and Windows originals\n",
           MERCS_START_FIXTURE_COUNT + MERCS_STATS_FIXTURE_COUNT);
}

static void transport(void)
{
    size_t i, n = 0, snapshots = 0;
    for (i = 0; i < MERCS_STATS_FIXTURE_COUNT; i++) {
        const mercs_stats_fixture *f = &mercs_stats_fixtures[i];
        const int *a = f->args;
        combat_unit *victim;
        mercs_car_record captured;
        uint8_t before[MERCS_CAR_BYTES];
        int rc;
        if (f->kind != 1) continue;
        memset(&sim, 0, sizeof sim); memset(&actor, 0, sizeof actor);
        memset(&unit, 0, sizeof unit); seen = 0;
        sim.actor_count = 1; sim.actors = &actor; sim.units = &unit; sim.msg_seen = &seen;
        sim.player_table = 3; actor.table = a[7] ? 3 : 4;
        actor.friendly = a[6] != 0; /* UI friendliness must not select counters. */
        actor.unit_class = sim.player_unit_class = a[5];
        actor.unit_flags = sim.player_unit_flags = (unsigned)a[1];
        actor.alliance = sim.player_alliance = a[6];
        victim = a[0] ? &sim.player_unit : &unit;
        victim->destroyed = 1; victim->invulnerable = a[2];
        victim->ejected = a[3] == 5 ? (a[4] ? 2 : 1) : 0;
        seed(sim.career, f->wrap); memcpy(before, sim.career, sizeof before);
        /* A late capture has no killer attribution. Its original fixtures
         * are therefore restricted to unknown-killer state4/state5 inputs. */
        if (!a[8] && a[3] != 7) {
            assert(!msim_capture_career(&sim, &captured));
            compare(f->name, captured.bytes, f->expected);
            compare("snapshot does not mutate live CAR", sim.career, before);
            assert(!msim_capture_career(&sim, &captured));
            compare("repeat snapshot", captured.bytes, f->expected);
            snapshots++;
        }
        rc = msim_record_unit_loss(&sim, a[0] ? -1 : 0, a[8] ? -1 : -2, a[3]);
        assert(rc == 0 || rc == 1);
        compare(f->name, sim.career, f->expected);
        assert(msim_record_unit_loss(&sim, a[0] ? -1 : 0, a[8] ? -1 : -2, a[3]) == 0);
        compare("repeated delivery", sim.career, f->expected);
        assert(!msim_capture_career(&sim, &captured));
        compare("capture after delivery", captured.bytes, f->expected);
        n++;
    }
    printf("PASS: %zu native loss events, duplicate guards and %zu immutable late snapshots\n", n, snapshots);
}

int main(void)
{
    pure_producers();
    transport();
    return 0;
}
