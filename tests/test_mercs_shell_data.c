#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mercsshell_data.h"
#include "mek.h"
#include "bwd.h"
#include "datapath.h"
#include "lance.h"
#include "mercs_career_original_fixtures.h"

static void original_deployment(const unsigned char *bytes, size_t size,
                                const char *edition, const char *loadout)
{
    char name[64];
    size_t i, j, at = 0;
    const career_loader_fixture *f = NULL;
    snprintf(name, sizeof name, "%s/%s.MEK", edition, loadout);
    for (i = 0; i < sizeof career_loader_fixtures / sizeof career_loader_fixtures[0]; i++)
        if (!strcmp(career_loader_fixtures[i].name, name)) { f = &career_loader_fixtures[i]; break; }
    assert(f);
    for (i = 0; i < 2; i++) {
        const char *hex = i ? f->deployment.b : f->deployment.a;
        for (j = 0; j < strlen(hex); j += 2) {
            unsigned v;
            assert(sscanf(hex + j, "%2x", &v) == 1 && at < size && bytes[at] == v);
            at++;
        }
    }
    assert(at == size);
}

static void malformed_counts(void)
{
    /* UINT32_MAX items + one link used to wrap the old sum check to zero,
     * making this otherwise minimal, zero-item payload appear valid. */
    unsigned char malformed[24 + MEK_LOC_COUNT * 40 + 50] = {0};
    unsigned char out[MEK_MAX_SERIALIZED_BYTES];
    static const int counts[][2] = {
        {-1, 1}, {1, -1},
        {MEK_MAX_ITEMS + 1, 0}, {0, MEK_MAX_ITEMS + 1},
        {MEK_MAX_ITEMS, 1}, {INT_MAX, 1}
    };
    mek_def m;
    size_t i, k;
    memset(malformed + 0x10, 0xff, 4);
    malformed[0x14] = 1;
    assert(mek_parse(malformed, sizeof malformed, &m) != 0);

    /* Invalid signed/count-sum inputs must fail before arithmetic, indexing,
     * or writing any output, even when the caller supplies a full buffer. */
    for (i = 0; i < sizeof counts / sizeof counts[0]; i++) {
        memset(&m, 0, sizeof m);
        m.item_count = counts[i][0];
        m.link_count = counts[i][1];
        memset(out, 0xa5, sizeof out);
        assert(mek_write(&m, out, sizeof out) == 0);
        for (k = 0; k < sizeof out; k++) assert(out[k] == 0xa5);
    }
}

static void roundtrip(prj_archive *a, const char *name, int extension)
{
    prj_record r;
    mek_def m;
    unsigned char bytes[MEK_MAX_SERIALIZED_BYTES];
    size_t n;
    assert(prj_read_named(a, "MEK", name, &r) == 0);
    assert(mek_parse(r.data, r.size, &m) == 0);
    assert(m.has_config_extension == extension);
    assert(!strncmp(m.config_name, (const char *)r.data + 344 + (m.item_count + m.link_count) * 8, 50));
    n = mek_write(&m, bytes, sizeof bytes);
    assert(n == r.size && !memcmp(bytes, r.data, n));
    if (extension) {
        m.config_extension[7] = 0x53;
        assert(mek_write(&m, bytes, sizeof bytes) == n && bytes[n - 1] == 0x53);
        assert(mek_parse(bytes, n, &m) == 0 && m.config_extension[7] == 0x53);
    }
    prj_record_free(&r);
}

int main(int argc, char **argv)
{
    prj_archive *a;
    mercs_shell_state *s = calloc(1, sizeof *s);
    char error[512];
    unsigned char *bytes = NULL;
    size_t size;
    bwd_chunk chunks[4];
    bwd_gps gps;
    mek_def owned;
    prj_record stock;
    int i, gps_count, hardware;
    assert((argc == 3 || argc == 4) && s);
    hardware = argc == 4 && strcmp(argv[3], "dos") != 0;
    if (hardware) assert(!strcmp(argv[3], "3dfx") || !strcmp(argv[3], "direct3d"));
    malformed_counts();
    a = prj_open(argv[1], error, sizeof error);
    if (!a) { fprintf(stderr, "%s\n", error); return 1; }
    /* Independently recorded source-data differences; keep explicit edition
     * expectations rather than deriving the answer from mek_parse itself.
     * See MERCS_VISUAL_EDITIONS_EVIDENCE.md and the earlier hardware checks. */
    roundtrip(a, "KDK00STD", hardware ? 1 : 0);
    roundtrip(a, "CMD00STD", 0);
    roundtrip(a, "PLY00STD", 1);
    roundtrip(a, "THG00STD", hardware ? 0 : 1);
    roundtrip(a, "BMB00STD", 1);
    roundtrip(a, "CHP00STD", 1);
    roundtrip(a, "LBT00STD", 1);
    roundtrip(a, "CRB00STD", 1);
    roundtrip(a, "SEN00STD", hardware ? 0 : 1);
    if (mercs_shell_register(a, s, 1, "Port Pilot", "First Company", error, sizeof error)) {
        fprintf(stderr, "register: %s\n", error); return 1;
    }
    assert(s->registered && s->mode == 1 && !strcmp(s->pilot, "PORT PILOT") && !strcmp(s->unit, "First Company"));
    assert(s->year == 3044 && s->month == 4 && s->funds == 1000000);
    assert(!strcmp(s->initial_campaign, "CAMP") && s->mission_count == 3);
    assert(!strcmp(s->starter_stock, "PLY00STD") && !strcmp(s->starter_loadout, "PLY00MER"));
    assert(!strcmp(s->starter_skeleton, "COMMANDO"));
    assert(strstr(s->news, "Dear PORT PILOT") && !strstr(s->news, "\\p"));
    assert(!strcmp(s->missions[0].campaign, "TRA1CAM1") && !strcmp(s->missions[0].scene, "TRA1SCN1"));
    assert(!strcmp(s->missions[1].campaign, "BET1CAM1") && !strcmp(s->missions[1].scene, "BET1SCN1"));
    assert(!strcmp(s->missions[2].campaign, "RAN1CAM1") && !s->missions[2].launchable);
    assert(strstr(s->missions[0].briefing, "Sergeant Unther") && strstr(s->missions[0].briefing, "NAV DELTA"));
    assert(strstr(s->missions[1].briefing, "Scalpel") && strstr(s->missions[1].contract, "Galedon V"));
    for (i = 0; i < 3; i++) printf("%s -> %s: %s (%s)\n", s->missions[i].campaign, s->missions[i].scene,
        s->missions[i].launchable ? "launchable" : "unavailable", s->missions[i].unavailable);
    assert(s->missions[0].launchable && s->missions[1].launchable);
    dp_clear_roots(); assert(dp_add_root(argv[2]) == 0);
    if (mercs_shell_prepare_launch(a, s, 0, argv[2], error, sizeof error)) {
        fprintf(stderr, "prepare: %s\n", error); return 1;
    }
    assert(dp_read_file("MEK/PLY00MER.MEK", &bytes, &size) == 0);
    assert(prj_read_named(a, "MEK", "PLY00STD", &stock) == 0);
    original_deployment(bytes, size, argc == 4 ? argv[3] : "dos", "PLY00MER");
    assert(mek_parse(bytes, size, &owned) == 0 && owned.tonnage == 25 && owned.item_count == 4 && owned.link_count == 2);
    free(bytes); prj_record_free(&stock);
    assert(dp_read_file("USERSTAR.BWD", &bytes, &size) == 0);
    assert(size == 152 && bwd_chunks(bytes, size, chunks, 4) == 3);
    assert(bwd_gps_decode(&chunks[2], &gps) == 0);
    assert(gps.is_player && gps.table == 0 && gps.leader && gps.mek_id == 0xfffe);
    assert(!strcmp(gps.name, "PORT PILOT") && !gps.pilot[0]);
    assert(!strcmp(gps.skeleton, "COMMANDO") && !strcmp(gps.loadout, "PLY00MER"));
    { const int16_t expected[8] = {1,500,300,400,1,0,0,0}; assert(!memcmp(gps.attributes, expected, sizeof expected)); }
    {
        lance_mercs_slot slots[3] = {{0}};
        unsigned char encoded[LANCE_MERCS_STAR_MAX_BYTES], small[16];
        bwd_chunk full[5];
        char path[2048];
        size_t encoded_size = 99, reference_size;
        unsigned char *reference;
        int k;
        slots[0].active = slots[0].has_attributes = 1;
        strcpy(slots[0].skeleton, "COMMANDO"); strcpy(slots[0].loadout, "PLY00MER");
        strcpy(slots[0].name, "PORT PILOT");
        for (k = 0; k < 8; k++) slots[0].attributes[k] = -1;
        memset(small, 0xa5, sizeof small);
        assert(lance_mercs_star_bytes(a, slots, 1, 0, 0, small, sizeof small, &encoded_size) != 0);
        assert(encoded_size == 0 && small[0] == 0xa5 && small[15] == 0xa5);
        assert(lance_mercs_star_bytes(a, slots, 1, 0, 0, encoded, sizeof encoded, &encoded_size) == 0);
        assert(encoded_size == size && !memcmp(encoded, bytes, size));
        snprintf(path, sizeof path, "%s/STARREF.BWD", argv[2]);
        assert(lance_mercs_write_star(a, path, slots, 1, 0, 0) == 0);
        assert(dp_read_file("STARREF.BWD", &reference, &reference_size) == 0);
        assert(reference_size == encoded_size && !memcmp(reference, encoded, encoded_size));
        free(reference);
        slots[1] = slots[2] = slots[0];
        assert(lance_mercs_star_bytes(a, slots, 3, 0, 0, encoded, sizeof encoded, &encoded_size) == 0);
        /* Three BattleMechs use three GPS records. The buffer also has room
         * for the separately tested fourth AeroTech support record. */
        assert(encoded_size == 52 + 3 * 100 && bwd_chunks(encoded, encoded_size, full, 5) == 5);
        for (k = 0; k < 3; k++) {
            bwd_gps g;
            assert(bwd_gps_decode(&full[k + 2], &g) == 0);
            assert(g.is_player == (k == 0) && g.leader == (k == 0));
        }
    }
    free(bytes);
    for (i = 0; i < 2; i++) {
        bwd_mission mission;
        int r, c;
        gps_count = 0;
        assert(bwd_mission_load(a, s->missions[i].scene, &mission) == 0 && mission.missing_count == 0);
        for (r = 0; r < mission.record_count; r++) for (c = 0; c < mission.records[r].chunk_count; c++) {
            bwd_chunk *ch = &mission.records[r].chunks[c];
            if (strcmp(ch->tag, "GPS")) continue;
            assert(bwd_gps_decode(ch, &gps) == 0);
            assert(mek_load(a, gps.loadout, &owned) == 0);
            gps_count++;
        }
        assert(gps_count == (i == 0 ? 5 : 7));
        bwd_mission_free(&mission);
    }
    assert(mercs_shell_prepare_launch(a, s, 2, argv[2], error, sizeof error) != 0);
    /* This fixture deliberately exposes only its writable overlay, not the
     * CD. RAN contracts now work when their original shell tables/templates
     * are present; the negative case must describe that missing data. */
    assert(strstr(error, "original DOS MW2SHELL.EXE") && strstr(error, "CD/RMG"));
    assert(mercs_shell_register(a, s, 0, "Training Pilot", "Academy", error, sizeof error) == 0);
    assert(s->funds == 0 && s->year == 3039 && s->month == 3 && s->mission_count == 1);
    assert(!strcmp(s->missions[0].campaign, "NOVICE1") && !s->missions[0].launchable);
    assert(strstr(s->missions[0].unavailable, "Duty Roster"));
    assert(s->training_mech_count == 3 && s->assigned_index == -1);
    assert(!s->starter_loadout[0]);
    assert(mercs_shell_prepare_launch(a, s, 0, argv[2], error, sizeof error) != 0);
    assert(mercs_shell_assign(a, s, 0, -1, error, sizeof error) != 0);
    assert(mercs_shell_assign(a, s, 0, 3, error, sizeof error) != 0);
    assert(s->assigned_index == -1 && !s->missions[0].launchable);
    {
        static const char *stocks[] = {"PLY00STD", "JEN00STD", "PNT00STD"};
        static const char *loadouts[] = {"PLY00MER", "JEN00MER", "PNT00MER"};
        static const char *skeletons[] = {"COMMANDO", "ISJENNER", "PANTHER"};
        for (i = 0; i < 3; i++) {
            char path[32];
            assert(!strcmp(s->training_mechs[i].stock, stocks[i]));
            assert(!strcmp(s->training_mechs[i].loadout, loadouts[i]));
            assert(!strcmp(s->training_mechs[i].skeleton, skeletons[i]));
            assert(mercs_shell_assign(a, s, 0, i, error, sizeof error) == 0);
            assert(s->assigned_index == i && s->missions[0].launchable && !s->missions[0].unavailable[0]);
            assert(!strcmp(s->starter_loadout, loadouts[i]) && s->funds == 0);
            assert(mercs_shell_prepare_launch(a, s, 0, argv[2], error, sizeof error) == 0);
            assert(dp_read_file("USERSTAR.BWD", &bytes, &size) == 0);
            assert(size == 152 && bwd_chunks(bytes, size, chunks, 4) == 3);
            assert(bwd_gps_decode(&chunks[2], &gps) == 0 && gps.is_player && gps.mek_id == 0xfffe);
            assert(!strcmp(gps.skeleton, skeletons[i]) && !strcmp(gps.loadout, loadouts[i]));
            assert(!strcmp(gps.name, "TRAINING PILOT") && !gps.pilot[0]);
            free(bytes);
            snprintf(path, sizeof path, "MEK/%s.MEK", loadouts[i]);
            assert(dp_read_file(path, &bytes, &size) == 0);
            assert(prj_read_named(a, "MEK", stocks[i], &stock) == 0);
            original_deployment(bytes, size, argc == 4 ? argv[3] : "dos", loadouts[i]);
            free(bytes); prj_record_free(&stock);
        }
    }
    mercs_shell_unassign(s, 0);
    assert(s->assigned_index == -1 && !s->missions[0].launchable && !s->starter_loadout[0]);
    assert(mercs_shell_prepare_launch(a, s, 0, argv[2], error, sizeof error) != 0);
    assert(mercs_shell_assign(a, s, 0, 2, error, sizeof error) == 0);
    assert(s->assigned_index == 2 && s->missions[0].launchable);
    assert(mercs_shell_register(a, s, 0, "New Trainee", "Academy", error, sizeof error) == 0);
    assert(s->assigned_index == -1 && !s->missions[0].launchable && !s->starter_loadout[0]);
    puts("PASS: overflowing MEK item/link counts and negative/over-limit writer counts are rejected without output writes");
    puts("PASS: original registration, CAMP availability, owned Commando, exact Mercs GPS, both authored first missions and preserved 50/58-byte MEK tails");
    puts("PASS: original NOVICE1 MHLT choices, no implicit selection, explicit Commando/Jenner/Panther assignments, exact owned bytes/GPS and fresh-registration reset");
    prj_close(a); free(s);
    return 0;
}
