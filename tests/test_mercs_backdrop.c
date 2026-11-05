/* Compare native classification with executed original-name decisions, then
 * audit the supplied editions' world and selected replacement-model paths. */
#include "mech3d.h"
#include "mercs_backdrop_original_fixtures.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int failures;
#define CHECK(c, msg) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); failures++; } } while (0)

static void original_fixtures(void)
{
    wtb_object objects[2];
    wtb_model model = {objects, 1, 0};
    size_t i;
    memset(objects, 0, sizeof objects);
    for (i = 0; i < sizeof mercs_backdrop_original_fixtures / sizeof mercs_backdrop_original_fixtures[0]; i++) {
        snprintf(objects[0].name, sizeof objects[0].name, "%s", mercs_backdrop_original_fixtures[i].name);
        if (mech3d_model_is_backdrop(&model) != mercs_backdrop_original_fixtures[i].backdrop) {
            fprintf(stderr, "FAIL original name decision: %s\n", objects[0].name);
            failures++;
        }
    }
    snprintf(objects[0].name, sizeof objects[0].name, "ground");
    snprintf(objects[1].name, sizeof objects[1].name, "tl1skydm");
    model.object_count = 2;
    CHECK(!mech3d_model_is_backdrop(&model), "an undrawn later WTBO is not the selected first model");
    model.objects++;
    model.object_count = 1;
    CHECK(mech3d_model_is_backdrop(&model), "classify the replacement model actually selected for drawing");
    model.object_count = 0;
    CHECK(!mech3d_model_is_backdrop(&model), "empty replacement is not a backdrop");
    CHECK(!mech3d_model_is_backdrop(NULL), "absent model is not a backdrop");
    model.objects = NULL; model.object_count = 1;
    CHECK(!mech3d_model_is_backdrop(&model), "absent WTBO array is not a backdrop");
}

static void replacement_path(prj_archive *archive, mech3d *world)
{
    wtb_model *store = NULL;
    int i, found = 0;
    CHECK(world3d_ti_attach(archive, world, &store) > 0, "accelerated world replacements load");
    for (i = 0; i < world->part_count; i++) {
        const mech3d_part *p = &world->parts[i];
        if (strcmp(p->rec, "SKYDOME") && strcmp(p->rec, "SKYDOM1") && strcmp(p->rec, "SKYDOMF")) continue;
        CHECK(p->ti_model != NULL, "backdrop has a selected accelerated replacement");
        CHECK(mech3d_model_is_backdrop(p->ti_model), "selected replacement preserves original backdrop identity");
        CHECK(!p->hidden, "loader retains fallback geometry for viewers without a backdrop pass");
        found++;
    }
    CHECK(found == 1, "one original backdrop owner in the representative scene");
    world3d_ti_free(store, world->part_count);
}

static void resource_archive(const char *path, int accelerated)
{
    prj_archive *archive;
    char error[256];
    int type, entry, scenes = 0, backgrounds = 0, dome = 0, star = 0, alternate = 0, other = 0;
    archive = prj_open(path, error, sizeof error);
    CHECK(archive != NULL, "open supplied archive");
    if (!archive) { fprintf(stderr, "%s\n", error); return; }
    type = prj_find_type(archive, "BWD");
    for (entry = 0; entry < prj_symbol_count(archive, type); entry++) {
        const char *name = prj_symbol_name(archive, type, entry);
        mech3d world = {0};
        int i, count = 0;
        if (strlen(name) != 8 || strcasecmp(name + 4, "SCN1")) continue;
        CHECK(world3d_mission(archive, name, 1, &world) >= 0, "load supplied original scene");
        scenes++;
        for (i = 0; i < world.part_count; i++) {
            const mech3d_part *p = &world.parts[i];
            if (!mech3d_model_is_backdrop(&p->model)) continue;
            count++;
            CHECK(!p->hidden && p->model.object_count == 1, "authored backdrop remains present and usable");
            if (!strcmp(p->rec, "SKYDOME")) dome++;
            else if (!strcmp(p->rec, "SKYDOM1")) {
                CHECK(!strcmp(name, "SPA1SCN1") || !strcmp(name, "SPA2SCN1"), "star backdrop belongs to SPA1/SPA2");
                CHECK(!strcmp(p->model_name, "TL1STARD"), "star backdrop retains its own model");
                star++;
            } else if (!strcmp(p->rec, "SKYDOMF")) {
                CHECK(!strncmp(name, "WOL", 3), "alternate backdrop belongs to WOL missions");
                CHECK(!strcmp(p->model_name, "TL1SKYDF"), "alternate backdrop retains its own model");
                alternate++;
            } else other++;
        }
        CHECK(count <= 1, "each supplied scene has at most one original backdrop owner");
        backgrounds += count;
        if (accelerated && (!strcmp(name, "TRA1SCN1") || !strcmp(name, "SPA1SCN1") || !strcmp(name, "WOL1SCN1")))
            replacement_path(archive, &world);
        mech3d_free(&world);
    }
    CHECK(scenes == 78, "all 78 supplied scene records audited");
    if (accelerated) {
        CHECK(backgrounds == 67 && dome == 60 && star == 2 && alternate == 5 && other == 0,
              "all 67 original backdrops include the seven scenes missed by the old SKYDOME-only lookup");
    } else {
        CHECK(backgrounds == 0, "retail/DOS worlds have no accelerated backdrops");
    }
    printf("%s: %d scenes, %d backdrops (SKYDOME=%d SKYDOM1=%d SKYDOMF=%d)\n",
           path, scenes, backgrounds, dome, star, alternate);
    prj_close(archive);
}

int main(int argc, char **argv)
{
    int i;
    original_fixtures();
    for (i = 1; i < argc; i += 2) {
        if (i + 1 >= argc || (strcmp(argv[i], "--retail") && strcmp(argv[i], "--accelerated"))) {
            fprintf(stderr, "usage: %s [--retail MW2.PRJ] [--accelerated models.prj ...]\n", argv[0]);
            return 2;
        }
        resource_archive(argv[i + 1], !strcmp(argv[i], "--accelerated"));
    }
    if (failures) { fprintf(stderr, "%d backdrop failures\n", failures); return 1; }
    puts("PASS Mercs backdrop classification and ownership resources");
    return 0;
}
