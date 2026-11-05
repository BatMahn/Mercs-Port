/* Pure-observation fixtures, never mission attempts. This translation unit
 * includes the test implementation so it can verify its private collision
 * scratch and terrain-cache isolation without adding a production API. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int fail_stamp_allocation;
static void *geometry_fixture_calloc(size_t count, size_t size)
{
    if (fail_stamp_allocation) return NULL;
    return calloc(count, size);
}
#define MERCS_PLAYTHROUGH_GEOMETRY_CALLOC geometry_fixture_calloc
#include "mercs_playthrough_geometry.c"

typedef struct { const void *live; void *before; size_t size; } saved_region;
typedef struct { saved_region *regions; size_t count, capacity, bytes; } snapshot;
typedef struct {
    float block_n[3]; const char *block_what;
    int block_id, hit_kind, impact_part;
    float hit_t, push[3], hit_at[4], hit_ext, impact_at[4];
} scratch_snapshot;

static void save_region(snapshot *snap, const void *ptr, size_t bytes)
{
    saved_region *r;
    if (!bytes || !ptr) return;
    if (snap->count == snap->capacity) {
        snap->capacity = snap->capacity ? snap->capacity * 2 : 64;
        snap->regions = realloc(snap->regions, snap->capacity * sizeof *snap->regions);
        assert(snap->regions);
    }
    r = &snap->regions[snap->count++];
    r->live = ptr; r->size = bytes; r->before = malloc(bytes); assert(r->before);
    memcpy(r->before, ptr, bytes); snap->bytes += bytes;
}
#define SAVE_ARRAY(snap, ptr, count) save_region((snap), (ptr), (size_t)(count) * sizeof *(ptr))

static void save_model(snapshot *snap, const mech3d *m)
{
    int i, j;
    SAVE_ARRAY(snap, m, 1); SAVE_ARRAY(snap, m->parts, m->part_count);
    for (i = 0; i < m->part_count; i++) {
        const wtb_model *model = &m->parts[i].model;
        SAVE_ARRAY(snap, model->objects, model->object_count);
        for (j = 0; j < model->object_count; j++) {
            const wtb_object *o = &model->objects[j];
            SAVE_ARRAY(snap, o->verts, o->vert_count);
            SAVE_ARRAY(snap, o->polys, o->poly_count);
        }
    }
}

static void save_sim(snapshot *snap, const msim *s)
{
    const terrain_t *t = (const terrain_t *)s->terrain;
    int i, cells = t->gw * t->gh;
    SAVE_ARRAY(snap, s, 1); SAVE_ARRAY(snap, t, 1);
    SAVE_ARRAY(snap, t->wall, t->nwall);
    SAVE_ARRAY(snap, t->obj, t->nobj); SAVE_ARRAY(snap, t->obj_box, t->nobj);
    SAVE_ARRAY(snap, t->obj_sph, t->nobj);
    SAVE_ARRAY(snap, t->thing_of, t->nparts); SAVE_ARRAY(snap, t->stamp, t->nparts);
    SAVE_ARRAY(snap, t->mov, t->nmov); SAVE_ARRAY(snap, t->mov_box, t->nmov);
    SAVE_ARRAY(snap, t->co, t->nco); SAVE_ARRAY(snap, t->cp, t->ncp);
    SAVE_ARRAY(snap, t->qn, t->nqn); SAVE_ARRAY(snap, t->qidx, t->nqidx);
    SAVE_ARRAY(snap, t->ccell, cells); SAVE_ARRAY(snap, t->ocell, cells);
    for (i = 0; i < cells; i++) {
        if (t->ccell) SAVE_ARRAY(snap, t->ccell[i].idx, t->ccell[i].n);
        if (t->ocell) SAVE_ARRAY(snap, t->ocell[i].idx, t->ocell[i].n);
    }
    SAVE_ARRAY(snap, s->actors, s->actor_count); SAVE_ARRAY(snap, s->units, s->actor_count);
    SAVE_ARRAY(snap, s->bld, s->bld_count);
    SAVE_ARRAY(snap, s->obstacles, s->obstacle_count);
    SAVE_ARRAY(snap, s->obstacle_part, s->obstacle_count);
    save_model(snap, s->world);
    for (i = 0; i < s->actor_count; i++) save_model(snap, &s->actors[i].mech);
}

static void compare_sim(snapshot *snap)
{
    size_t i;
    for (i = 0; i < snap->count; i++) {
        assert(memcmp(snap->regions[i].live, snap->regions[i].before, snap->regions[i].size) == 0);
        free(snap->regions[i].before);
    }
    free(snap->regions); snap->regions = NULL;
}

static scratch_snapshot save_scratch(void)
{
    scratch_snapshot r;
    memset(&r, 0, sizeof r);
    memcpy(r.block_n, g_block_n, sizeof r.block_n); r.block_what = g_block_what;
    r.block_id = g_block_id; r.hit_kind = g_hit_kind; r.impact_part = g_impact_part;
    r.hit_t = g_hit_t; r.hit_ext = g_hit_ext;
    memcpy(r.push, g_push, sizeof r.push); memcpy(r.hit_at, g_hit_at, sizeof r.hit_at);
    memcpy(r.impact_at, g_impact_at, sizeof r.impact_at);
    return r;
}

static void restore_scratch(const scratch_snapshot *r)
{
    memcpy(g_block_n, r->block_n, sizeof g_block_n); g_block_what = r->block_what;
    g_block_id = r->block_id; g_hit_kind = r->hit_kind; g_impact_part = r->impact_part;
    g_hit_t = r->hit_t; g_hit_ext = r->hit_ext;
    memcpy(g_push, r->push, sizeof g_push); memcpy(g_hit_at, r->hit_at, sizeof g_hit_at);
    memcpy(g_impact_at, r->impact_at, sizeof g_impact_at);
}

static void seed_scratch(msim *s)
{
    terrain_t *t = (terrain_t *)s->terrain;
    int i;
    /* A near-overflow live cache must not be incremented by observation. */
    t->stamp_now = INT_MAX - 1;
    for (i = 0; i < t->nparts; i++) t->stamp[i] = i + 17;
    g_block_what = "geometry-purity-sentinel";
    g_block_id = 41; g_hit_kind = 2; g_impact_part = 19;
    g_hit_t = 123.25f; g_hit_ext = 47.5f;
    for (i = 0; i < 3; i++) { g_block_n[i] = (float)i + .125f; g_push[i] = (float)i - .75f; }
    for (i = 0; i < 4; i++) { g_hit_at[i] = (float)i + 91.0f; g_impact_at[i] = (float)i - 33.0f; }
}

static mercs_playthrough_geometry checked_samples(msim *s, const float feet[3],
                                                  float heading, const char *label)
{
    mercs_playthrough_geometry first, repeat;
    snapshot snap = {0}; scratch_snapshot before, after;
    msim *reference; terrain_t terrain;
    clock_t start; int i;
    double sample_total = 0.0, sample_min = 1e30, sample_max = 0.0;
    seed_scratch(s); save_sim(&snap, s); before = save_scratch();
    start = clock();
    assert(mercs_playthrough_geometry_sample(s, feet, heading, &first) == 0);
    assert(mercs_playthrough_geometry_sample(s, feet, heading, &repeat) == 0);
    assert(first.available && first.sample_count == MERCS_PLAYTHROUGH_GEOMETRY_SAMPLES);
    assert(memcmp(&first, &repeat, sizeof first) == 0);
    for (i = 0; i < 16; i++) {
        clock_t sample_start = clock();
        double elapsed;
        assert(mercs_playthrough_geometry_sample(s, feet, heading, &repeat) == 0);
        elapsed = (double)(clock() - sample_start) * 1000.0 / CLOCKS_PER_SEC;
        assert(memcmp(&first, &repeat, sizeof first) == 0);
        sample_total += elapsed;
        if (elapsed < sample_min) sample_min = elapsed;
        if (elapsed > sample_max) sample_max = elapsed;
    }
    after = save_scratch(); assert(memcmp(&before, &after, sizeof before) == 0);

    /* Compare every blocked result with the existing production movement
     * query on another private cache. This reference writes its collision
     * globals; preserve/restore them inside this disposable fixture only. */
    reference = malloc(sizeof *reference); assert(reference);
    memcpy(reference, s, sizeof *reference); terrain = *(terrain_t *)s->terrain;
    terrain.stamp = calloc((size_t)(terrain.nparts ? terrain.nparts : 1), sizeof *terrain.stamp);
    assert(terrain.stamp); terrain.stamp_now = 0; reference->terrain = &terrain;
    for (i = 0; i < first.sample_count; i++) {
        const mercs_playthrough_direction *d = &first.directions[i];
        int clear = msim_can_move(reference, feet[0], feet[2], d->position[0], d->position[1],
                                  feet[1], 0.0f, s->player_centre_h, s->player_radius);
        assert(d->blocked == !clear);
        assert(isfinite(d->ground_y) && isfinite(d->min_ground_y) && isfinite(d->max_ground_y));
        assert(d->max_step_down_cm >= 0 && d->max_step_up_cm >= 0);
    }
    free(terrain.stamp); free(reference); restore_scratch(&before);
    after = save_scratch(); assert(memcmp(&before, &after, sizeof before) == 0);
    compare_sim(&snap);
    printf("PASS geometry %s: 64 native movement comparisons, byte-identical repeated output and %zu saved bytes; CPU %.6f s\n",
           label, snap.bytes, (double)(clock() - start) / CLOCKS_PER_SEC);
    printf("LATENCY %s: 16 samples, each 961 support queries + 64 movement queries; CPU mean %.3f ms min %.3f ms max %.3f ms\n",
           label, sample_total / 16.0, sample_min, sample_max);
    return first;
}

static void fixture_plane(mech3d_part *p, int left_y, int right_y, int extent)
{
    wtb_object *o; int i;
    memset(p, 0, sizeof *p); p->coll = 5;
    p->model.objects = calloc(1, sizeof *p->model.objects); assert(p->model.objects);
    p->model.object_count = 1; o = p->model.objects;
    o->verts = calloc(4, sizeof *o->verts); o->polys = calloc(1, sizeof *o->polys);
    assert(o->verts && o->polys); o->vert_count = 4; o->poly_count = 1;
    o->verts[0].x = -extent; o->verts[0].z = -extent; o->verts[0].y = left_y;
    o->verts[1].x = extent; o->verts[1].z = -extent; o->verts[1].y = right_y;
    o->verts[2].x = extent; o->verts[2].z = extent; o->verts[2].y = right_y;
    o->verts[3].x = -extent; o->verts[3].z = extent; o->verts[3].y = left_y;
    o->polys[0].n = 4;
    for (i = 0; i < 4; i++) o->polys[0].idx[i] = (uint16_t)i;
}

static void fixture_box(mech3d_part *p, int type, const int lo[3], const int hi[3])
{
    wtb_object *o; int i;
    memset(p, 0, sizeof *p); p->coll = type;
    p->model.objects = calloc(1, sizeof *p->model.objects); assert(p->model.objects);
    p->model.object_count = 1; o = p->model.objects;
    o->verts = calloc(8, sizeof *o->verts); assert(o->verts); o->vert_count = 8;
    for (i = 0; i < 8; i++) {
        o->verts[i].x = i & 1 ? hi[0] : lo[0];
        o->verts[i].y = i & 2 ? hi[1] : lo[1];
        o->verts[i].z = i & 4 ? hi[2] : lo[2];
    }
}

static msim *fixture_sim(mech3d *world)
{
    msim *s = calloc(1, sizeof *s); assert(s);
    s->player_centre_h = 632; s->player_radius = 545;
    s->rng = 0x12345678u; s->player_unit.vy = -2.5f;
    msim_set_world(s, world);
    return s;
}

static void fixture_free(msim *s, mech3d *world)
{ msim_free(s); free(s); mech3d_free(world); }

static void synthetic(void)
{
    mech3d w = {0}; msim *s; mercs_playthrough_geometry result;
    float feet[3] = {0,0,0};
    const int wall_lo[3] = {-3000,0,1500}, wall_hi[3] = {3000,2000,2000};
    const int sphere_lo[3] = {1000,0,-400}, sphere_hi[3] = {1800,1200,400};
    const int roof_lo[3] = {-10000,2000,-10000}, roof_hi[3] = {10000,2500,10000};
    w.part_count = 2; w.parts = calloc(2, sizeof *w.parts); assert(w.parts);
    fixture_plane(&w.parts[0],0,0,10000); fixture_box(&w.parts[1],1,wall_lo,wall_hi);
    s = fixture_sim(&w); result = checked_samples(s,feet,0,"flat-wall-radius");
    assert(!result.directions[0].blocked && result.directions[1].blocked);
    assert(result.directions[1].hit_part == 1 && result.directions[1].hit_kind == 1);
    assert(fabsf(result.directions[1].hit_distance_cm - 1500) < 1.0f);
    fail_stamp_allocation = 1;
    assert(mercs_playthrough_geometry_sample(s,feet,0,&result) == -1 && !result.available && !result.sample_count);
    fail_stamp_allocation = 0;
    feet[0] = NAN; assert(mercs_playthrough_geometry_sample(s,feet,0,&result) == -1 && !result.available);
    feet[0] = 0; assert(mercs_playthrough_geometry_sample(NULL,feet,0,&result) == -1 && !result.available);
    fixture_free(s,&w);

    memset(&w,0,sizeof w); w.part_count = 1; w.parts = calloc(1,sizeof *w.parts); assert(w.parts);
    fixture_plane(&w.parts[0],20000,20000,2000); s = fixture_sim(&w); feet[1] = 20000;
    result = checked_samples(s,feet,0,"raised-deck-edge");
    assert(result.ground_y == 20000 && !result.airborne);
    assert(!result.directions[2].blocked && result.directions[2].ground_y == 0);
    assert(result.directions[2].max_step_down_cm == 20000);
    assert(result.directions[2].min_ground_y == 0 && result.directions[2].max_ground_y == 20000);
    feet[1] = 25000; result = checked_samples(s,feet,0,"airborne-above-deck");
    assert(result.airborne && result.ground_y == 20000 && result.vertical_speed == -2.5f);
    fixture_free(s,&w);

    memset(&w,0,sizeof w); w.part_count = 1; w.parts = calloc(1,sizeof *w.parts); assert(w.parts);
    fixture_plane(&w.parts[0],1000,3000,10000); s = fixture_sim(&w); feet[1] = 2000;
    result = checked_samples(s,feet,270,"continuous-descending-ramp");
    assert(result.directions[3].position[1] == 0.0f);
    assert(fabsf(result.directions[3].min_ground_y - 1400) < 1.0f);
    assert(result.directions[3].max_step_down_cm < 10.1f && result.directions[3].max_step_down_cm > 9.9f);
    {
        /* Preserve the current native raw fractional query independently of
         * the observer's integer-position sampling policy. This is not a new
         * executed-original assertion about the quadtree implementation. */
        msim *query = malloc(sizeof *query); terrain_t terrain = *(terrain_t *)s->terrain;
        snapshot snap = {0}; scratch_snapshot before = save_scratch(), after;
        float raw, rounded;
        assert(query); save_sim(&snap,s); memcpy(query,s,sizeof *query);
        terrain.stamp = calloc((size_t)terrain.nparts,sizeof *terrain.stamp); assert(terrain.stamp);
        terrain.stamp_now = 0; query->terrain = &terrain;
        raw = msim_ground(query,-6000,.25f,2632);
        rounded = msim_ground(query,-6000,(float)lrintf(.25f),2632);
        assert(raw == 0 && fabsf(rounded - 1400) < 1.0f);
        free(terrain.stamp); free(query);
        after = save_scratch(); assert(memcmp(&before,&after,sizeof before) == 0);
        compare_sim(&snap);
        printf("PASS native raw seam: fractional z=.25 support %.3f, stored-position z=0 support %.3f; no production query changes\n",raw,rounded);
    }
    fixture_free(s,&w);

    memset(&w,0,sizeof w); w.part_count = 2; w.parts = calloc(2,sizeof *w.parts); assert(w.parts);
    fixture_plane(&w.parts[0],0,0,10000); fixture_box(&w.parts[1],0,roof_lo,roof_hi);
    w.parts[1].moving = 1; s = fixture_sim(&w); feet[1] = 0;
    result = checked_samples(s,feet,0,"road-under-moving-roof");
    assert(result.ground_y == 0 && result.query_y == 632);
    assert(result.directions[3].ground_y == 0);
    fixture_free(s,&w);

    memset(&w,0,sizeof w); w.part_count = 2; w.parts = calloc(2,sizeof *w.parts); assert(w.parts);
    fixture_plane(&w.parts[0],0,0,10000); fixture_box(&w.parts[1],6,sphere_lo,sphere_hi);
    s = fixture_sim(&w); result = checked_samples(s,feet,90,"endpoint-sphere");
    assert(result.directions[1].blocked && result.directions[1].hit_kind == 2);
    assert(result.directions[1].hit_distance_cm == -1);
    fixture_free(s,&w);
    puts("PASS geometry refusal: invalid/null input and forced stamp-allocation failure remain unavailable");
}

static void archive_worlds(const char *path)
{
    static const char *scenes[] = {"MCC2SCN1","SPA1SCN1","SPA2SCN1"};
    char error[256]; prj_archive *a = prj_open(path,error,sizeof error); size_t scene;
    if (!a) { fprintf(stderr,"%s\n",error); exit(1); }
    for (scene = 0; scene < sizeof scenes/sizeof scenes[0]; scene++) {
        mech3d world = {0}; world_actor *actors = NULL; int count = 0, i;
        msim *s = calloc(1,sizeof *s); float feet[3]; mercs_playthrough_geometry result;
        assert(s && world3d_mission(a,scenes[scene],1,&world) >= 0);
        assert(world3d_actors(a,scenes[scene],&actors,&count) == 0);
        assert(msim_init(s,a,scenes[scene],actors,count) == 0);
        msim_set_world(s,&world);
        feet[0] = s->player[0]; feet[1] = s->player_unit.y; feet[2] = s->player[2];
        result = checked_samples(s,feet,s->player_heading,scenes[scene]);
        printf("WORLD %s feet %.3f %.3f %.3f support %.3f H %.3f R %.3f; loaded fixture, no simulation step\n",
               scenes[scene],feet[0],feet[1],feet[2],result.ground_y,result.centre_h,result.radius);
        if (scene == 0) {
            /* Read-only hypothetical queries at the observed MCC2 collision
             * areas. These do not move any actor or prove traversal. */
            feet[0] = -83600; feet[1] = 0; feet[2] = -13000;
            result = checked_samples(s,feet,180,"MCC2 south-wall query");
            printf("MCC2 south 10m blocked %d normalY %.3f drop %.3f\n",
                   result.directions[1].blocked,result.directions[1].hit_normal[1],result.directions[1].max_step_down_cm);
            feet[0] = 60000; feet[1] = 0; feet[2] = 24000;
            result = checked_samples(s,feet,90,"MCC2 east-wall query");
            printf("MCC2 east 10m blocked %d normalY %.3f drop %.3f\n",
                   result.directions[1].blocked,result.directions[1].hit_normal[1],result.directions[1].max_step_down_cm);
        }
        msim_free(s); free(s);
        for (i = 0; i < count; i++) mech3d_free(&actors[i].mech);
        free(actors); mech3d_free(&world);
    }
    prj_close(a);
}

int main(int argc, char **argv)
{
    synthetic();
    if (argc > 1) archive_worlds(argv[1]);
    return 0;
}
