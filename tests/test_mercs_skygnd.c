/* MW2.EXE 0x49cc4 SKYGND.PAR reading: a scene without a line keeps the last
 * line's ground values (its one sscanf writes the statics for every line). */
#include "skygnd.h"
#include "mercsground.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failed, passed;
static void check(int ok, const char *what)
{
    if (ok) passed++;
    else { fprintf(stderr, "FAIL: %s\n", what); failed++; }
}

int main(void)
{
    char path[] = "/tmp/mercs_skygnd_XXXXXX";
    int fd = mkstemp(path);
    FILE *f = fd >= 0 ? fdopen(fd, "w") : NULL;
    skygnd_dos s;
    int32_t tiles[4][2];
    if (!f) { fprintf(stderr, "FAIL: temp file\n"); return 1; }
    fputs("mission=mcc4 globalintensity=70 tile_ground=15000 grounddelta=70 ground_max=160000 shadowtable=7 gclip=5000,40000,50000\r\n"
          "mission=avo1 globalintensity=130 tile_ground=12000 grounddelta=10 ground_max=150000 shadowtable=15 gclip=1500,20000,50000\r\n"
          "\r\n", f);
    fclose(f);

    check(skygnd_dos_lookup(path, "MCC4", &s) == 0 && s.found && s.intensity == 70 && s.shadowtable == 7 &&
          s.tile_ground == 15000 && s.grounddelta == 70 && s.gclip[0] == 5000 && s.gclip[1] == 40000 &&
          s.tile_side == 320000 && s.tile_half == 160000, "matching line: its values, ground_max doubled");
    check(skygnd_dos_lookup(path, "RAN0", &s) == -1 && !s.found && s.intensity == 128 && s.shadowtable == 0 &&
          s.tile_ground == 12000 && s.grounddelta == 10 && s.gclip[0] == 1500 && s.gclip[1] == 20000 &&
          s.gclip[2] == 50000 && s.tile_side == 150000 && s.tile_half == 160000,
          "no line: the last line's ground values, ground_max not doubled, 0xb3f4c unchanged");
    remove(path);
    check(skygnd_dos_lookup("/nonexistent/SKYGND.PAR", "RAN0", &s) == -1 && s.gclip[0] == 0,
          "no file: no textured ground");

    /* 0x4a3d4 with 0xb3f4c not half of 0xb3f48: the quadrant is always 0 */
    check(mercs_ground_tiles_half(150000, 10, 160000, 160000, tiles) == 4 &&
          tiles[0][0] == 0 && tiles[0][1] == 0 && tiles[1][0] == -1 && tiles[2][1] == -1,
          "quadrant 0 tiles when the divisor is the whole tile");
    check(mercs_ground_tiles_half(-10, -150000, 160000, 160000, tiles) == 4 &&
          tiles[0][0] == -1 && tiles[0][1] == -1 && tiles[1][0] == 0 && tiles[3][1] == 0,
          "negative side: the neighbours step back up");
    printf("mercs skygnd: %d passed, %d failed\n", passed, failed);
    return failed != 0;
}
