/* Original normal outputs, actual archive collision support, and a bounded
 * native vertical consumer. No msim_init/msim_step, AI, mission-table steps,
 * frontend inputs or career writes. The six-second vertical chain is exactly
 * ground-query -> combat_jets, as used by msim's headless vertical branch;
 * production glview also has movement/contact integration and is a separate
 * validation. Source macros permit a private pre-fix negative control. */
#define _POSIX_C_SOURCE 200809L
#ifndef MERCS_NORMAL_SOURCE
#define MERCS_NORMAL_SOURCE "../src/msim.c"
#endif
#ifndef MERCS_NORMAL_FIXTURES
#define MERCS_NORMAL_FIXTURES "../tests/fixtures/polygon-normals/original-normal-fixtures.h"
#endif
#include MERCS_NORMAL_SOURCE
#include "mercscockpit.h"
#include MERCS_NORMAL_FIXTURES

static unsigned checks, failures;

static void check(int ok, const char *name)
{
    checks++;
    if (!ok) { fprintf(stderr, "FAIL: %s\n", name); failures++; }
}

static void normals(void)
{
    size_t i;
    printf("{\"normal_cases\":[");
    for (i = 0; i < sizeof mercs_normal_fixtures / sizeof mercs_normal_fixtures[0]; i++) {
        const mercs_normal_fixture *f = &mercs_normal_fixtures[i];
        wtb_vertex vertices[7] = {{0}};
        wtb_poly poly = {0};
        wtb_object obj = {0};
        mech3d_part part = {0};
        int32_t shared[3] = {0}, cockpit[3] = {0};
        float collision[3] = {0}, expected[3];
        unsigned before = failures;
        int j;
        obj.verts = vertices; obj.vert_count = f->count;
        obj.polys = &poly; obj.poly_count = 1;
        part.model.objects = &obj; part.model.object_count = 1;
        poly.n = (uint16_t)f->count;
        for (j = 0; j < f->count; j++) {
            vertices[j].x = f->vertices[j][0]; vertices[j].y = f->vertices[j][1];
            vertices[j].z = f->vertices[j][2]; poly.idx[j] = (uint16_t)j;
        }
        for (j = 0; j < 3; j++) expected[j] = (float)((double)f->normal[j] / 536870912.0);
        check(wtb_mercs_normal_q29(&obj, &poly, shared) == 0, "shared normal accepted");
        check(memcmp(shared, f->normal, sizeof shared) == 0, f->name);
        check(mercs_cockpit_dos_normal(&obj, &poly, cockpit) == 0, "cockpit wrapper accepted");
        check(memcmp(cockpit, f->normal, sizeof cockpit) == 0, "cockpit matches original Q29");
        check(eng_poly_normal(&part, &poly, collision) == 1, "collision normal accepted");
        check(collision[0] == expected[0] && collision[1] == expected[1] && collision[2] == expected[2],
              "collision keeps original Q29 magnitude");
        printf("%s{\"name\":\"%s\",\"normal_q29\":[%d,%d,%d],"
               "\"collision\":[%.9g,%.9g,%.9g],\"pass\":%s}",
               i ? "," : "", f->name, shared[0], shared[1], shared[2],
               collision[0], collision[1], collision[2], before == failures ? "true" : "false");
        /* Native integration: preserve the existing matrix route, including
         * non-unit magnitude. This is not an original world-transform test. */
        part.has_rot = 1; part.rot[2] = 1; part.rot[4] = 1; part.rot[6] = -1;
        check(eng_poly_normal(&part, &poly, collision) == 1, "rotated collision normal accepted");
        check(collision[0] == expected[2] && collision[1] == expected[1] && collision[2] == -expected[0],
              "existing world rotation preserves the Q29 normal");
    }
    printf("],\"archives\":[");
}

static int planet_read(prj_archive *a, combat_planet *planet)
{
    bwd_mission mission;
    int r, c, found = 0;
    if (bwd_mission_load(a, "SPA1SCN1", &mission) != 0) return -1;
    for (r = 0; r < mission.record_count; r++)
        for (c = 0; c < mission.records[r].chunk_count; c++) {
            const bwd_chunk *chunk = &mission.records[r].chunks[c];
            if (!memcmp(chunk->tag, "PLNT", 4) && chunk->size >= 4) {
                uint32_t q = (uint32_t)chunk->data[0] | (uint32_t)chunk->data[1] << 8 |
                             (uint32_t)chunk->data[2] << 16 | (uint32_t)chunk->data[3] << 24;
                planet->gravity_g = (float)(int32_t)q / 65536.0f; found++;
            }
        }
    bwd_mission_free(&mission);
    return found == 1 && planet->gravity_g > 0 ? 0 : -1;
}

static int scene(const char *path, int index)
{
    const mercs_normal_fixture *authored = &mercs_normal_fixtures[0];
    const float query_y[] = {201771,201000,200447.4375f,199417.96875f,200547,199000,100000000};
    const float x = -45000, z = 35000, top = 199776, centre = 771;
    char error[256];
    prj_archive *a = prj_open(path, error, sizeof error);
    mech3d world = {0}; msim *s;
    terrain_t *t; cobj_t *object = NULL; cpoly_t *face = NULL;
    combat_planet planet = {1,0,100000,0};
    combat_unit body;
    float ground[7], at3 = 0, landed = -1, first_ground, min_y = 201000;
    int32_t q29[3] = {0};
    unsigned before = failures, rng = 1;
    int i, j, part = -1, co = -1, supported = 0, left_surface = 0;
    if (!a) { fprintf(stderr, "%s\n", error); return -1; }
    if (world3d_mission(a, "SPA1SCN1", 1, &world) < 0) return -1;
    s = calloc(1, sizeof *s); if (!s) return -1;
    s->arch = a; strcpy(s->scene, "SPA1SCN1"); s->world = &world;
    clist_order(s, &world); t = terrain_build(&world, s->clist_seq, s->clist_seq_n);
    s->terrain = t; if (!t) return -1;
    for (i = 0; i < world.part_count; i++)
        if (!strcmp(world.parts[i].rec, "SPA1ARE1") && world.parts[i].obj_index == 14 &&
            !strcmp(world.parts[i].model_name, "T_1IBTRE")) { part = i; break; }
    check(part >= 0, "actual SPA1 OBJ14 ice model present");
    if (part < 0) return -1;
    {
        mech3d_part *p = &world.parts[part];
        wtb_object *o = &p->model.objects[0];
        wtb_poly *poly = &o->polys[0];
        check(p->coll == 5 && !p->hidden && !p->moving && p->parent_obj == -2,
              "authored ice is visible fixed type-5 terrain");
        check(p->pos[0] == 27563 && p->pos[1] == 179489 && p->pos[2] == 20000 && !p->has_rot && p->yaw == 0,
              "authored ice placement unchanged");
        check(o->vert_count == 20 && o->poly_count == 28 && poly->n == authored->count,
              "actual ice model shape agrees with original probe");
        for (i = 0; i < authored->count; i++) {
            const wtb_vertex *v = &o->verts[poly->idx[i]];
            check(v->x == authored->vertices[i][0] && v->y == authored->vertices[i][1] &&
                  v->z == authored->vertices[i][2], "actual archive top vertices agree with original probe");
        }
        check(wtb_mercs_normal_q29(o, poly, q29) == 0 && !memcmp(q29, authored->normal, sizeof q29),
              "actual archive face normal matches executed original output");
    }
    for (i = 0; i < t->nco; i++) if (t->co[i].part == part) { co = i; object = &t->co[i]; break; }
    check(object && object->type == 5 && object->np == 28 && object->root >= 0,
          "ice terrain is present in collision list and quadtree");
    if (!object) return -1;
    face = &t->cp[object->p0];
    check(cpoly_in(face, 0, 2, x, z), "spawn is inside authored top face");
    check(face->nrm[0] == 0 && face->nrm[1] == (float)((double)authored->normal[1]/536870912.0) && face->nrm[2] == 0,
          "real collision face preserves original upward Q29 magnitude");
    {
        int candidates[64], n = clist_near(s,x,z,x,z,candidates,64), found = 0;
        float height = -1; const float *normal = NULL;
        for (i = 0; i < n; i++) if (candidates[i] == co) found = 1;
        check(found, "broadphase returns actual ice entry at spawn");
        check(qt_ground(t,object->root,x,z,&height,&normal) && height == top && normal && normal[1] > 1,
              "quadtree finds original upward ice face without renormalization");
    }
    for (i = 0; i < 7; i++) {
        float normal[3];
        ground[i] = msim_ground_n(s,x,z,query_y[i],normal);
        check(ground[i] == top && normal[1] == face->nrm[1], "captured-height support query returns authored ice");
    }
    check(msim_ground(s,x,z,top-1000) == 0 && msim_ground(s,x,z,top-1001) == 0,
          "unchanged strict query window excludes the deck at or below its minus-1000 boundary");
    check(msim_ground(s,x,z,top-999) == top,
          "unchanged query window accepts the deck one centimeter inside the boundary");
    check(planet_read(a,&planet) == 0, "authored SPA1 gravity available");
    combat_set_planet(&planet);
    if (combat_init(&body,a,"STK00STD") != 0) { fprintf(stderr,"missing stock Stalker fixture\n"); return -1; }
    body.y = 201000; body.vy = 0;
    first_ground = msim_ground(s,x,z,body.y+centre);
    for (i = 0; i < 180; i++) {
        float dt = (float)(i%3 == 2 ? 34 : 33)/1000.0f;
        body.ground = msim_ground(s,x,z,body.y+centre);
        combat_jets(&body,0,dt,&rng);
        if (body.y < min_y) min_y = body.y;
        if (body.y == top && body.vy == 0) {
            supported++;
            if (landed < 0) landed = (float)((i/3)*100 + (i%3 == 0 ? 33 : i%3 == 1 ? 66 : 100))/1000.0f;
        } else if (landed >= 0) left_surface++;
        if (i == 89) at3 = body.y;
    }
    check(first_ground == top, "initial native vertical query sees ice before falling");
    check(landed > 0 && landed < 4 && supported > 60 && !left_surface,
          "native query/combat_jets chain lands then stays supported");
    check(min_y >= top && body.y == top && body.vy == 0 && body.ground == top,
          "six-second stationary vertical chain never passes through deck");
    printf("%s{\"archive\":%d,\"world_parts\":%d,\"part\":%d,\"collision_entry\":%d,"
           "\"normal_q29\":[%d,%d,%d],\"collision_normal\":[%.9g,%.9g,%.9g],\"queries\":[",
           index ? "," : "", index, world.part_count, part, co,
           q29[0],q29[1],q29[2],face->nrm[0],face->nrm[1],face->nrm[2]);
    for (j=0;j<7;j++) printf("%s{\"y\":%.9g,\"ground\":%.9g}",j?",":"",query_y[j],ground[j]);
    printf("],\"gravity_g\":%.9g,\"vertical_seconds\":6,\"steps\":180,"
           "\"landed_at_seconds\":%.9g,\"height_at_3_seconds\":%.9g,\"final_y\":%.9g,"
           "\"final_vy\":%.9g,\"minimum_y\":%.9g,\"supported_steps\":%d,\"pass\":%s}",
           planet.gravity_g,landed,at3,body.y,body.vy,min_y,supported,before==failures?"true":"false");
    terrain_free(t); free(s->clist_seq); free(s);
    mech3d_free(&world); prj_close(a); return 0;
}

int main(int argc, char **argv)
{
    int i;
    if (argc < 2) { fprintf(stderr,"usage: %s ARCHIVE.PRJ [ARCHIVE.PRJ ...]\n",argv[0]); return 2; }
#ifdef _WIN32
    _putenv_s("MERCS_INSTALL_DIR",""); _putenv_s("MERCS_CD_DIR",""); _putenv_s("MERCS_CD_IMAGE","");
#else
    unsetenv("MERCS_INSTALL_DIR"); unsetenv("MERCS_CD_DIR"); unsetenv("MERCS_CD_IMAGE");
#endif
    dp_clear_roots(); combat_set_weapon_profile(COMBAT_PROFILE_RETAIL);
    normals();
    for (i=1;i<argc;i++) if (scene(argv[i],i-1)) return 2;
    printf("],\"checks\":%u,\"failures\":%u,\"pass_all\":%s}\n",checks,failures,failures?"false":"true");
    return failures ? 1 : 0;
}
