/* Verify Mercs serialization against the original retail OUT3USER resource,
 * plus the original shell's field/slot rules. This does not create a career.
 * Usage: test_mercs_lance RETAIL_MW2.PRJ SCRATCH_OUTPUT_DIRECTORY
 */
#include "lance.h"
#include "bwd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    if (!ok) { fprintf(stderr, "FAIL: %s\n", what); failures++; }
}

static unsigned char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    unsigned char *b;
    long n;
    *len = 0;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) { fclose(f); return NULL; }
    b = malloc((size_t)n + 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *len = (size_t)n;
    return b;
}

static int decode_file(const char *path, bwd_gps *gps, int capacity)
{
    unsigned char *b;
    bwd_chunk chunks[8];
    size_t n;
    int count, i, found = 0;
    b = read_file(path, &n);
    if (!b) return -1;
    count = bwd_chunks(b, n, chunks, 8);
    if (count < 0 || count > 8) { free(b); return -1; }
    for (i = 0; i < count; i++) if (strcmp(chunks[i].tag, "GPS") == 0) {
        if (found >= capacity || bwd_gps_decode(&chunks[i], &gps[found])) { free(b); return -1; }
        found++;
    }
    free(b);
    return found;
}

int main(int argc, char **argv)
{
    prj_archive *a;
    prj_record original;
    bwd_chunk c[3];
    bwd_gps g[4];
    lance_mercs_slot slots[4];
    lance_slot oldslot;
    char err[256], path[1024];
    unsigned char *bytes;
    size_t size;
    int i;
    static const int16_t defaults[8] = {1, 500, 300, 400, 1, 0, 0, 0};
    if (argc != 3) { fprintf(stderr, "usage: %s RETAIL_MW2.PRJ SCRATCH_OUTPUT_DIRECTORY\n", argv[0]); return 2; }
    a = prj_open(argv[1], err, sizeof err);
    if (!a) { fprintf(stderr, "%s\n", err); return 2; }
    memset(&original, 0, sizeof original);
    if (prj_read_named(a, "BWD", "OUT3USER", &original) != PRJ_OK) {
        fprintf(stderr, "OUT3USER fixture unavailable\n"); prj_close(a); return 2;
    }
    check(original.size == 152 && bwd_chunks(original.data, original.size, c, 3) == 3,
          "original OUT3USER is a 152-byte BWD with three chunks");
    if (original.size != 152 || bwd_chunks(original.data, original.size, c, 3) != 3) {
        prj_record_free(&original); prj_close(a); return 1;
    }
    check(bwd_gps_decode(&c[2], &g[0]) == 0 && c[2].size == 92,
          "original OUT3USER contains one 100-byte GPS");
    check(g[0].mek_id == 82 && g[0].skeleton_id == 56 && g[0].table == 0 &&
          g[0].leader == 1 && g[0].control == 0 && g[0].is_player &&
          g[0].object_class == 6 && g[0].flags == 0x400 &&
          strcmp(g[0].skeleton, "kodiak") == 0 && strcmp(g[0].loadout, "kdk00std") == 0 &&
          strcmp(g[0].name, "Raider1") == 0 && g[0].pilot[0] == 0,
          "original mission assigns the Kodiak / kdk00std / Raider1 player");
    for (i = 0; i < 8; i++) check(g[0].attributes[i] == 0, "original OUT3USER attribute is zero");

    memset(slots, 0, sizeof slots);
    slots[0].active = 1;
    strcpy(slots[0].skeleton, "kodiak"); strcpy(slots[0].loadout, "kdk00std"); strcpy(slots[0].name, "Raider1");
    snprintf(path, sizeof path, "%s/out3-roundtrip.bwd", argv[2]);
    check(lance_mercs_write_star(a, path, slots, 1, 0, 8) == 0, "write original mission's explicit slot");
    bytes = read_file(path, &size);
    check(bytes && size == original.size && memcmp(bytes, original.data, size) == 0,
          "emitted BWD matches every original OUT3USER byte");
    free(bytes);

    /* Fixed-width names must not collapse an empty unit-name field. The eight
     * negative-word replacements are read from retail MW2SHELL.DLL 0x1008f970. */
    slots[0].name[0] = 0;
    strcpy(slots[0].pilot, "Assigned Pilot");
    slots[0].has_attributes = 1;
    for (i = 0; i < 8; i++) slots[0].attributes[i] = -1;
    slots[1] = slots[0];
    slots[1].has_attributes = 0;
    strcpy(slots[1].loadout, "KDK00STD");
    snprintf(path, sizeof path, "%s/two-slots.bwd", argv[2]);
    check(lance_mercs_write_star(a, path, slots, 2, 0, 8) == 0 && decode_file(path, g, 3) == 2,
          "two explicit slots decode");
    check(g[0].name[0] == 0 && strcmp(g[0].pilot, "Assigned Pilot") == 0,
          "empty display name does not consume the pilot field");
    check(g[0].control == 0 && g[0].leader == 1 && g[1].control == 2 && g[1].leader == 0 &&
          g[1].attributes[7] == 1 && g[1].mek_id == 82,
          "slot identity and case-insensitive standard loadout lookup");
    for (i = 0; i < 8; i++) check(g[0].attributes[i] == defaults[i], "negative pilot attribute uses original default");

    /* The engine supports a fourth GPS for AeroTech after two mechmates.
     * This tests the serializer's original per-record field rules, not the
     * shell's eligibility/stock-MEK policy (covered by the roster test). */
    {
        unsigned char full[LANCE_MERCS_STAR_MAX_BYTES], before[sizeof full];
        bwd_chunk chunks[6];
        size_t length=0;
        slots[2]=slots[1]; slots[3]=slots[1];
        slots[3].has_attributes=1;
        for(i=0;i<8;i++) slots[3].attributes[i]=(int16_t)(i+10);
        strcpy(slots[3].pilot,"Support Pilot");
        check(lance_mercs_star_bytes(a,slots,4,0,8,full,sizeof full,&length)==0 && length==452,
              "four explicit GPS records fit the452-byte USERSTAR transport");
        check(bwd_chunks(full,length,chunks,6)==6 && bwd_gps_decode(&chunks[5],&g[3])==0,
              "fourth support record decodes through the normal GPS parser");
        check(g[3].control==2 && !g[3].leader && !g[3].is_player && !strcmp(g[3].pilot,"Support Pilot"),
              "fourth support remains AI and retains its pilot identity");
        for(i=0;i<8;i++) check(g[3].attributes[i]==i+10,"fourth support retains every pilot attribute");
        memcpy(before,full,sizeof full);
        check(lance_mercs_star_bytes(a,slots,4,0,8,full,451,&length)==-1 && !length && !memcmp(before,full,sizeof full),
              "undersized four-slot output leaves bytes untouched");
        check(lance_mercs_star_bytes(a,slots,5,0,8,full,sizeof full,&length)==-1,
              "serializer rejects more than player,two mates andsupport");
    }

    slots[0].active = 0;
    snprintf(path, sizeof path, "%s/empty-player-slot.bwd", argv[2]);
    check(lance_mercs_write_star(a, path, slots, 2, 0, 8) == 0 && decode_file(path, g, 3) == 1 &&
          !g[0].is_player && !g[0].leader && g[0].attributes[7] == 1,
          "a wingman is not promoted when the player slot is empty");
    snprintf(path, sizeof path, "%s/enemy-slot.bwd", argv[2]);
    check(lance_mercs_write_star(a, path, slots, 2, 5, 7) == 0 && decode_file(path, g, 3) == 1 &&
          g[0].table == 5 && g[0].control == 2 && g[0].attributes[0] == 6 &&
          g[0].attributes[1] == 300 && g[0].attributes[4] == 6,
          "Mercs skill row seven retains its distinct raw attributes");

    strcpy(slots[1].loadout, "kdk01usr");
    snprintf(path, sizeof path, "%s/custom-slot.bwd", argv[2]);
    check(lance_mercs_write_star(a, path, slots, 2, 0, 0) == 0 && decode_file(path, g, 3) == 1 &&
          g[0].mek_id == 0xfffe && strcmp(g[0].loadout, "kdk01usr") == 0,
          "custom loadout keeps the external-resource marker");

    /* Failure must preserve a previously written file, not partially replace it. */
    strcpy(slots[1].loadout, "xxx00std");
    check(lance_mercs_write_star(a, path, slots, 2, 0, 0) == -1 && decode_file(path, g, 3) == 1 &&
          strcmp(g[0].loadout, "kdk01usr") == 0,
          "missing standard loadout fails before opening the output");
    strcpy(slots[1].loadout, "kdk00std"); strcpy(slots[1].skeleton, "missing");
    check(lance_mercs_write_star(a, path, slots, 2, 0, 0) == -1, "missing skeleton fails explicitly");
    memset(slots[1].skeleton, 'x', sizeof slots[1].skeleton);
    check(lance_mercs_write_star(a, path, slots, 2, 0, 0) == -1, "unterminated resource name fails explicitly");

    memset(&oldslot, 0, sizeof oldslot);
    oldslot.mech = 5; strcpy(oldslot.loadout, "mdg00std"); strcpy(oldslot.pilot, "Legacy");
    snprintf(path, sizeof path, "%s/legacy-mw2.bwd", argv[2]);
    check(lance_write_star(a, path, &oldslot, 1, 0, 0) == 0, "mature MW2 writer remains callable");
    bytes = read_file(path, &size);
    check(bytes && size == 144 && bwd_chunks(bytes, size, c, 3) == 3 && c[2].size == 84 &&
          bwd_gps_decode(&c[2], &g[0]) == 0 && strcmp(g[0].name, "Legacy") == 0,
          "mature MW2 92-byte GPS path is unchanged");
    free(bytes);
    prj_record_free(&original);
    prj_close(a);
    printf("test_mercs_lance: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
