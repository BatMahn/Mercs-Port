/* Actual Mercs BET3ENS5 actor plus the retail DOS level-normalization edges.
 * Usage: test_mercs_gps_ai /path/to/retail/MW2.PRJ
 */
#include "bwd.h"
#include "prj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    if (!ok) { fprintf(stderr, "FAIL: %s\n", what); failures++; }
}

int main(int argc, char **argv)
{
    prj_archive *a;
    prj_record r;
    bwd_chunk *chunks, c;
    bwd_gps g;
    unsigned char bytes[92];
    char err[256];
    int i, n, found = 0;
    static const unsigned char raw[] = {0, 1, 4, 5, 6, 7, 127, 128, 255};
    static const int expected[] = {1, 1, 4, 5, 6, 6, 6, 6, 6};
    if (argc != 2) { fprintf(stderr, "usage: %s RETAIL_MW2.PRJ\n", argv[0]); return 2; }
    a = prj_open(argv[1], err, sizeof err);
    if (!a) { fprintf(stderr, "%s\n", err); return 2; }
    if (prj_read_named(a, "BWD", "BET3ENS5", &r) != PRJ_OK) {
        fprintf(stderr, "BET3ENS5 fixture unavailable\n"); prj_close(a); return 2;
    }
    n = bwd_chunks(r.data, r.size, NULL, 0);
    chunks = n > 0 ? calloc((size_t)n, sizeof *chunks) : NULL;
    if (!chunks) { prj_record_free(&r); prj_close(a); return 2; }
    bwd_chunks(r.data, r.size, chunks, n);
    memset(&c, 0, sizeof c);
    for (i = 0; i < n; i++) if (strcmp(chunks[i].tag, "GPS") == 0) { c = chunks[i]; found++; }
    check(found == 1 && c.size == sizeof bytes, "original BET3ENS5 has one GPS100");
    if (found != 1 || c.size != sizeof bytes) { free(chunks); prj_record_free(&r); prj_close(a); return 1; }
    check(bwd_gps_decode(&c, &g) == 0 && g.pilot_level == 5 && g.attributes[4] == 5 &&
          g.skill == 5 && g.ranges[0] == 500 && g.ranges[1] == 400 && g.ranges[2] == 400 &&
          g.attributes[5] == 1 && strcmp(g.name, "Rebel Jenner") == 0 && strcmp(g.pilot, "Scout") == 0,
          "original Rebel Jenner keeps level five and 500/400/400-metre range words");

    memcpy(bytes, c.data, sizeof bytes);
    c.data = bytes;
    for (i = 0; i < (int)(sizeof raw / sizeof raw[0]); i++) {
        bytes[16] = raw[i];
        bytes[17] = 0x12; /* the consumer uses the low byte, not the entire word */
        check(bwd_gps_decode(&c, &g) == 0 && g.pilot_level == expected[i] &&
              g.attributes[4] == (int16_t)(0x1200u + raw[i]),
              "DOS unsigned low-byte level saturates without losing the raw word");
    }
    bytes[10] = 0xff; bytes[11] = 0xff;
    check(bwd_gps_decode(&c, &g) == 0 && g.ranges[0] == 65535 && g.attributes[1] == -1,
          "range interpretation remains unsigned while the raw attribute remains signed");
    free(chunks);
    prj_record_free(&r);
    prj_close(a);
    printf("test_mercs_gps_ai: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
