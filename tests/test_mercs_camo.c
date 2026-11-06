/* test_mercs_camo - the missions' camouflage sets (bwd_mission_camo_sets, Mercs DOS 0x62bf6 -> 0xbc92c) and the plate
 * remap they feed (glr: MW2.EXE 0x62e74). DOSBox BET1SCN1: the player's star (STAR 6) wears set 3 ('d' plates, tiger
 * stripes), its lancemates (STAR 1) set 4 ('e'), with the Draconis insignia 0x3f + 1.
 *   test_mercs_camo MW2.PRJ */
#include <stdio.h>
#include "bwd.h"
#include "prj.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(int argc, char **argv)
{
    static const int BASE[8] = {0, 1, 0x56, 0x66, 0x76, 0x86, 0xf3, 0xf9};
    prj_archive *a;
    int s[8], n, k;
    char err[256];
    if (argc < 2) { fprintf(stderr, "usage: %s MW2.PRJ\n", argv[0]); return 2; }
    if (!(a = prj_open(argv[1], err, sizeof err))) { fprintf(stderr, "%s\n", err); return 2; }
    n = bwd_mission_camo_sets(a, "BET1SCN1", s);
    CHECK(n == 3, "BET1SCN1: %d CAMO chunks, want 3", n);
    CHECK(s[1] == 4 && s[6] == 3 && s[7] == 5, "BET1SCN1 sets 1/6/7 = %d %d %d, want 4 3 5", s[1], s[6], s[7]);
    for (k = 0; k < 8; k++) if (k != 1 && k != 6 && k != 7) CHECK(s[k] == k, "BET1SCN1 set %d = %d (identity)", k, s[k]);
    /* the remap of plate slot 4 (vplate3c) for the player: set 3 -> 0x66 + 2 = vplate3d */
    k = s[6] < 2 ? 4 + s[6] : BASE[s[6]] + 4 / 2;
    CHECK(k == 0x68, "player plate slot 4 -> %02x, want 68", k);
    n = bwd_mission_camo_sets(a, "NOSUCHSC", s);
    CHECK(n == -1 && s[0] == 0 && s[7] == 7, "a missing scene: %d, identity", n);
    prj_close(a);
    printf("test_mercs_camo: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
