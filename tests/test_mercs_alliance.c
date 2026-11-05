/* Original-backed STAR/startup and bounded AMS IFF regressions.
 * No simulation steps, input injection, career writes or mission outcomes.
 * Include msim privately so the fixture needs no new production API. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/msim.c"
#include "../tests/fixtures/campaign-playthrough/original-alliance-fixtures.h"

static unsigned checks, startups, scene_loads, iff_pairs;

static void equal(const char *name, const char *field, int actual, int expected)
{
    checks++;
    if (actual != expected) {
        fprintf(stderr, "FAIL %s %s: native %d, original %d\n", name, field, actual, expected);
        exit(1);
    }
}

static void require(int condition, const char *message)
{
    if (!condition) { fprintf(stderr, "Fixture error: %s\n", message); exit(2); }
}

static unsigned get16(const unsigned char *p)
{
    return (unsigned)p[0] | (unsigned)p[1] << 8;
}

static void put32(unsigned char *p, int value)
{
    uint32_t u = (uint32_t)value;
    int i;
    for (i = 0; i < 4; i++) p[i] = (unsigned char)(u >> (i * 8));
}

static void compare_counts(const msim *s, const mercs_alliance_fixture *f)
{
    int i;
    for (i = 0; i < 6; i++) {
        char label[48];
        snprintf(label, sizeof label, "startup CAR+0x%x", 0x38 + 2 * i);
        equal(f->name, label, (int)get16(s->career + 0x38 + 2 * i), (int)f->initial_counts[i]);
    }
    startups++;
}

static void iff(const char *name, int native_player_alliance, int original_player_alliance)
{
    msim *s = calloc(1, sizeof *s);
    world_actor actors[3];
    combat_unit units[3];
    msim_shot shot;
    int32_t delta[3], velocity[3];
    int i;
    require(s != NULL, "allocating IFF fixture");
    memset(actors, 0, sizeof actors); memset(units, 0, sizeof units);
    memset(&shot, 0, sizeof shot);
    s->player_alliance = native_player_alliance;
    s->actors = actors; s->units = units; s->actor_count = 3;
    for (i = 0; i < 3; i++) {
        actors[i].alliance = i;
        shot.owner = i;
        equal(name, "player AMS incoming side", ams_delta(s, -1, &shot, delta, velocity),
              original_player_alliance != i);
        shot.owner = -1;
        equal(name, "actor AMS player-shot side", ams_delta(s, i, &shot, delta, velocity),
              i != original_player_alliance);
        iff_pairs += 2;
    }
    free(s);
}

/* Feed the original's exact STAR and GT affiliation inputs to the private
 * production startup consumer. NPC alliances are supplied by executed original
 * getters, leaving player selection and GT affiliation mapping under test. */
static void fixture_startup(prj_archive *archive, const mercs_alliance_fixture *f)
{
    msim *s = calloc(1, sizeof *s);
    world_actor *actors = calloc((size_t)f->unit_count, sizeof *actors);
    bwd_chunk *chunks = calloc((size_t)(1 + 2 * f->thing_count), sizeof *chunks);
    unsigned char *gt = calloc((size_t)f->thing_count, 68);
    unsigned char *affl = calloc((size_t)f->thing_count, 4);
    bwd_record record;
    bwd_mission mission;
    int i;
    require(s && actors && chunks && gt && affl, "allocating original startup fixture");
    memset(&record, 0, sizeof record); memset(&mission, 0, sizeof mission);
    strcpy(record.name, "FIXTURE"); record.chunks = chunks;
    record.chunk_count = 1 + 2 * f->thing_count;
    mission.records = &record; mission.record_count = 1;
    require(f->star_bytes >= 8, "STAR header");
    strcpy(chunks[0].tag, "STAR"); chunks[0].data = f->star + 8;
    chunks[0].size = f->star_bytes - 8;
    for (i = 0; i < f->thing_count; i++) {
        put32(affl + 4 * i, f->thing_affiliations[i]);
        strcpy(chunks[1 + 2 * i].tag, "AFFL");
        chunks[1 + 2 * i].data = affl + 4 * i; chunks[1 + 2 * i].size = 4;
        strcpy(chunks[2 + 2 * i].tag, "GT");
        chunks[2 + 2 * i].data = gt + 68 * i; chunks[2 + 2 * i].size = 68;
    }
    s->player_table = f->player_table;
    strcpy(s->player_skel, "TIMBRWLF");
    s->actors = actors; s->actor_count = f->unit_count - 1;
    for (i = 0; i < s->actor_count; i++) {
        actors[i].table = f->unit_tables[i + 1];
        actors[i].alliance = f->unit_alliances[i + 1];
    }
    require(stats_initial_load(s, archive, &mission) == 0, "original-input startup load");
    equal(f->name, "player alliance", s->player_alliance, f->player_alliance);
    compare_counts(s, f);
    iff(f->name, s->player_alliance, f->player_alliance);
    free(affl); free(gt); free(chunks); free(actors); free(s);
}

static void actual_scene(prj_archive *archive, const char *scene)
{
    world_actor *actors = NULL;
    msim *s = calloc(1, sizeof *s);
    int count = 0;
    size_t i;
    require(s != NULL, "allocating scene startup");
    require(world3d_actors(archive, scene, &actors, &count) == 0, "loading authored actors");
    require(msim_init(s, archive, scene, actors, count) == 0, "initializing authored scene");
    equal(scene, "player table", s->player_table, 0);
    equal(scene, "player alliance", s->player_alliance, 0);
    for (i = 0; i < MERCS_ALLIANCE_FIXTURE_COUNT; i++) {
        const mercs_alliance_fixture *f = &mercs_alliance_fixtures[i];
        if (!strcmp(scene, f->scene)) {
            equal(scene, "loaded actor count", count + 1, f->unit_count);
            compare_counts(s, f);
        }
    }
    iff(scene, s->player_alliance, 0);
    scene_loads++;
    msim_free(s); free(s); world3d_free_actors(actors, count);
}

int main(int argc, char **argv)
{
    static const char *const scenes[] = {
        "DOU1SCN1", "IVA1SCN1", "IVA2SCN1", "IVA3SCN1", "IVA4SCN1",
        "ROG1SCN1", "SOL1SCN1", "SOL3SCN1", "OUT3SCN1", "LUT2SCN1"
    };
    int arg;
    if (argc < 2) {
        fprintf(stderr, "usage: %s ARCHIVE.PRJ [ARCHIVE.PRJ ...]\n", argv[0]);
        return 2;
    }
    /* The fixture reads archives explicitly. Do not adopt a caller's live
     * USERSTAR, difficulty configuration, or writable career overlay. */
#ifdef _WIN32
    _putenv_s("MERCS_INSTALL_DIR", ""); _putenv_s("MERCS_CD_DIR", ""); _putenv_s("MERCS_CD_IMAGE", "");
#else
    unsetenv("MERCS_INSTALL_DIR"); unsetenv("MERCS_CD_DIR"); unsetenv("MERCS_CD_IMAGE");
#endif
    dp_clear_roots();
    for (arg = 1; arg < argc; arg++) {
        char error[256];
        prj_archive *archive = prj_open(argv[arg], error, sizeof error);
        size_t i;
        if (!archive) { fprintf(stderr, "Cannot open %s: %s\n", argv[arg], error); return 2; }
        for (i = 0; i < MERCS_ALLIANCE_FIXTURE_COUNT; i++) fixture_startup(archive, &mercs_alliance_fixtures[i]);
        for (i = 0; i < sizeof scenes / sizeof scenes[0]; i++) actual_scene(archive, scenes[i]);
        prj_close(archive);
        printf("PASS archive %s: four original-backed startup inputs, ten authored scene startups; no simulation steps\n", argv[arg]);
    }
    printf("PASS alliance: %u checks, %u authored startups, %u complete initial-count comparisons, %u bounded AMS IFF pairs\n",
           checks, scene_loads, startups, iff_pairs);
    return 0;
}
