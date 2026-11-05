/* Table and dispatcher probe for independent original-executable comparison.
 * tools/research/verify_mercs_effects_original.py reads original tables and
 * runs original dispatch instructions; expected outputs are not copied here. */
#include "fx.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../tests/fixtures/result-audit/original-fx-lifecycle.h"

int main(int argc, char **argv)
{
    int i, j;
    const int bits[4] = {FX_HIT_UNIT, FX_HIT_STRUCT, FX_HIT_GROUND, FX_HIT_PLAYER};
    if (argc == 2 && !strcmp(argv[1], "--dump")) {
        printf("{\"tables\":[");
        for (i = 0; i < FX_TYPES; i++) {
            int dos, win;
            fx_set_timing(1); dos = fx_duration(i);
            fx_set_timing(0); win = fx_duration(i);
            printf("%s[%d,%d,%d,%d,%d,%d]", i ? "," : "", dos, win,
                   fx_sound(i), fx_light(i), fx_damage_q16(i), fx_heat_q16(i));
        }
        printf("],\"dispatch\":[");
        for (i = 0; i < FX_TYPES; i++) for (j = 0; j < 16; j++) {
            int k, flags = 0;
            for (k = 0; k < 4; k++) if (j & (1 << k)) flags |= bits[k];
            printf("%s[%d,%d,%d]", i || j ? "," : "", i, flags, fx_impact_type(i, flags));
        }
        puts("]}");
        return 0;
    }
    {
        fx_defs *d = calloc(1, sizeof *d);
        fx_world w = {0};
        const float origin[3] = {0, 0, 0};
        assert(d);
        for (i = 0; i < FX_TYPES; i++) d->slots[i] = 4;
        fx_set_timing(1); fx_detail_low = 0;
        for (i = 0; i < 3; i++) assert(fx_spawn(&w, d, 3, origin, 0, 0) >= 0);
        assert(fx_spawn(&w, d, 3, origin, 0, 0) == -1);
        fx_update(&w, fx_duration(3));
        assert(fx_spawn(&w, d, 3, origin, fx_duration(3), 0) >= 0);
        memset(&w, 0, sizeof w);
        /* Original flag2 exempts nuclear effects from the ordinary near cap. */
        for (i = 0; i < 4; i++) assert(fx_spawn(&w, d, 22, origin, 0, 0) >= 0);
        assert(fx_spawn(&w, d, 22, origin, 0, 0) == -1); /* actual pool still bounds it */
        memset(&w, 0, sizeof w);
        assert(fx_spawn(&w, d, 34, origin, 0, 0) >= 0);
        assert(fx_spawn(&w, d, 35, origin, 0, 0) >= 0);
        assert(fx_impact_type(36, 0) == -1 && fx_impact_type(-1, 0) == -1);
        assert(fx_duration(36) == 0 && fx_sound(36) == -1);
        for (i = 0; i < 100; i++) assert((fx_sound_roll(9, i) >= 0) == (i < 10));
        assert(fx_area_radius(35, 999, 3) == 72 && fx_area_radius(3, 999, 3) == 999);
        for (i = 0; i < (int)(sizeof original_fx_creation / sizeof original_fx_creation[0]); i++) {
            const fx_creation_fixture *f = &original_fx_creation[i];
            int at;
            memset(&w, 0, sizeof w); fx_set_timing(f->dos);
            at = fx_spawn(&w, d, f->type, origin, 1000, f->delta);
            assert(at >= 0 && w.e[at].remaining == f->remaining);
            fx_update(&w, f->delta);
            assert(w.e[at].alive && w.e[at].remaining == f->duration);
        }
        for (i = 0; i < (int)(sizeof original_fx_updates / sizeof original_fx_updates[0]); i++) {
            const fx_update_fixture *f = &original_fx_updates[i];
            memset(&w, 0, sizeof w); fx_set_timing(f->dos);
            w.e[0].type = f->type; w.e[0].alive = f->active; w.e[0].remaining = f->remaining;
            fx_update(&w, f->delta);
            assert(w.e[0].remaining == f->remaining_after && w.e[0].alive == f->active_after);
        }
        free(d);
    }
    puts("PASS: Mercs effect pool bounds, proximity flags, expiry, sound probability and type35 radius");
    puts("PASS: 40 creation and 28 countdown cases match original DOS/Windows lifecycle captures");
    return 0;
}
