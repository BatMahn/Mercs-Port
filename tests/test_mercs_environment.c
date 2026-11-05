/* The original OUT3PLT1 begins at dusk: phase2, day1,000,000.
 * DOS 0x6169d -> 0x152e4; Windows 0x1006c828 -> 0x1000221e. */
#include "bwd.h"
#include "prj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int32_t i32(const unsigned char *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 |
                     (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

int main(int argc, char **argv)
{
    prj_archive *a;
    prj_record r;
    bwd_chunk *chunks;
    char err[256];
    int n, i, phase = -1, day = -1;
    if (argc != 2) return 2;
    a = prj_open(argv[1], err, sizeof err);
    if (!a) { fprintf(stderr, "%s\n", err); return 2; }
    if (prj_read_named(a, "BWD", "OUT3PLT1", &r) != PRJ_OK) { prj_close(a); return 2; }
    n = bwd_chunks(r.data, r.size, NULL, 0);
    chunks = n > 0 ? calloc((size_t)n, sizeof *chunks) : NULL;
    if (!chunks) { prj_record_free(&r); prj_close(a); return 2; }
    bwd_chunks(r.data, r.size, chunks, n);
    for (i = 0; i < n; i++) {
        if (!strcmp(chunks[i].tag, "INIT") && chunks[i].size >= 12) phase = i32(chunks[i].data + 8);
        if (!strcmp(chunks[i].tag, "PLNT") && chunks[i].size >= 16) day = i32(chunks[i].data + 12);
    }
    free(chunks); prj_record_free(&r); prj_close(a);
    if (phase != 2 || day != 1000000 || bwd_mercs_day_start(phase, day) != 708322 ||
        bwd_mercs_day_start(1, 86400) != 25200 || bwd_mercs_day_start(-1, day) != -1 ||
        bwd_mercs_day_start(4, day) != -1 || bwd_mercs_day_start(0, 0) != -1) {
        fputs("FAIL: Mercenaries environment phase\n", stderr); return 1;
    }
    puts("test_mercs_environment: original OUT3 phase2 -> dusk at708322, not night at2seconds; ok");
    return 0;
}
