/* Original Mercs archives keep MW2_SHT1 / MW2_XPL1 resource names.
 * Verify the real effect assets load after separating the port's env namespace.
 * This checks asset availability; original table/dispatch and area arithmetic
 * have separate comparisons, while complete special callbacks remain open. */
#include "fx.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    static const int pools[FX_TYPES] = {
        10, 10, 10, 8, 8, 10, 6, 8, 6, 10, 8, 32, 8, 2, 2, 2,
        2, 2, 4, 8, 8, 6, 4, 32, 0, 8, 0, 0, 0, 0, 0, 0,
        0, 0, 8, 4
    };
    char err[256];
    prj_archive *a;
    fx_defs *d;
    int i, animations = 0, slots = 0, failed = 0;
    if (argc != 2) { fprintf(stderr, "usage: %s retail-MW2.PRJ\n", argv[0]); return 2; }
    a = prj_open(argv[1], err, sizeof err);
    if (!a) { fprintf(stderr, "%s\n", err); return 1; }
    d = calloc(1, sizeof *d);
    if (!d) { prj_close(a); return 1; }
    if (fx_load(a, d) != 0) {
        fprintf(stderr, "Original Mercs effect definitions did not load\n");
        free(d); prj_close(a); return 1;
    }
    /* Independently counted from the original BWD: 201 BMID animation bindings
     * and 226 XPLO slots, including eight type34 and four type35 objects. */
    for (i = 0; i < FX_TYPES; i++) {
        if (d->slots[i] != pools[i]) {
            fprintf(stderr, "effect type %d: %d slots, expected %d\n", i, d->slots[i], pools[i]);
            failed = 1;
        }
        slots += d->slots[i];
    }
    for (i = 0; i < 512; i++) if (d->tex[i].frames > 0) animations++;
    if (animations != 201) {
        fprintf(stderr, "%d animation bindings, expected 201\n", animations);
        failed = 1;
    }
    free(d); prj_close(a);
    if (!failed) printf("PASS: original MW2_SHT1/MW2_XPL1 load %d pool slots and %d animation bindings across all 36 effect types.\n", slots, animations);
    return failed;
}
