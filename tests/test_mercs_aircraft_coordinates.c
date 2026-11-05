/* Original-backed class-2 coordinate regression.
 * The selectors are extracted verbatim from the reviewed glview.c copy.
 * Native scene steps and declared placements below are isolated fixtures,
 * never ordinary campaign wins or original whole-flight executions. */
#define _POSIX_C_SOURCE 200809L
#ifndef AIRCRAFT_MSIM_SOURCE
#define AIRCRAFT_MSIM_SOURCE "../src/msim.c"
#endif
#ifndef AIRCRAFT_SELECTORS
#define AIRCRAFT_SELECTORS "../build/mercs_aircraft_selectors.h"
#endif
#include AIRCRAFT_MSIM_SOURCE
#include "../tests/fixtures/aircraft-coordinates/original/original-aircraft-fixtures.h"

static msim g_sim;
static mech3d g_pl;
static world_actor *g_actors;
static int g_sim_ok, g_actor_count, g_tgt_any;
#include AIRCRAFT_SELECTORS

static unsigned checks, failures, model_count, profile_count, selector_cases;
static unsigned scene_loads, scene_steps, altitude_cases, thing_cases, gate_cases;
static unsigned wreck_cases;

static void check(int ok, const char *scope, const char *name, double actual, double expected)
{
    checks++;
    if (ok) return;
    failures++;
    if (failures <= 48)
        fprintf(stderr, "FAIL %s %s: %.9g expected %.9g\n", scope, name, actual, expected);
}

static void eq(double actual, double expected, double tolerance, const char *scope, const char *name)
{
    check(isfinite(actual) && fabs(actual - expected) <= tolerance, scope, name, actual, expected);
}

static int class_of(prj_archive *a, const char *name)
{
    prj_record r; bwd_chunk *ch; int n, k, c = -1;
    if (prj_read_named(a, "BWD", name, &r) != PRJ_OK) return -1;
    n = bwd_chunks(r.data, r.size, NULL, 0);
    ch = calloc((size_t)(n > 0 ? n : 1), sizeof *ch);
    if (!ch) exit(2);
    if (n > 0) bwd_chunks(r.data, r.size, ch, n);
    for (k = 0; k < n; k++) if (!strcmp(ch[k].tag, "GP") && ch[k].size >= 4)
        c = (int16_t)(ch[k].data[2] | ch[k].data[3] << 8);
    free(ch); prj_record_free(&r); return c;
}

static void models(prj_archive *a, int archive_index)
{
    int type = prj_find_type(a, "BWD"), row, first = 1;
    printf("{\"archive\":%d,\"models\":[", archive_index);
    for (row = 0; row < prj_symbol_count(a, type); row++) {
        const char *name = prj_symbol_name(a, type, row);
        mech3d model[2] = {{0},{0}}; int hips, p, k;
        if (class_of(a, name) != 2) continue;
        model_count++;
        for (hips = 0; hips < 2; hips++) {
            mech3d_dos_hips = hips;
            if (mech3d_load(a, name, 0, &model[hips])) {
                fprintf(stderr, "Unable to load class2 model %s\n", name); exit(2);
            }
            model[hips].origin[0] = 1234; model[hips].origin[1] = 12345;
            model[hips].origin[2] = -4321; model[hips].heading = 27;
            mech3d_pose(&model[hips], NULL, 0);
            eq(model[hips].controller_centre, 1, 0, "controller-centre class2", name);
            profile_count++;
        }
        check(model[0].part_count == model[1].part_count, "profile parts", name,
              model[0].part_count, model[1].part_count);
        for (p = 0; p < model[0].part_count && p < model[1].part_count; p++)
            for (k = 0; k < 3; k++)
                eq(model[0].parts[p].pos[k], model[1].parts[p].pos[k], 0,
                   "profile part position", name);
        for (k = 0; k < 3; k++) if (model[0].eye_ok && model[1].eye_ok)
            eq(model[0].eye_pos[k], model[1].eye_pos[k], 0, "profile eye", name);
        printf("%s{\"name\":\"%s\",\"centre\":%d,\"bind_count\":%d,\"parts\":%d}",
               first?"":",", name, model[0].centre_h, model[0].bind_count, model[0].part_count);
        first = 0;
        mech3d_free(&model[0]); mech3d_free(&model[1]);
    }
    puts("]}");
}

static void player_reference(const int32_t feet[3], float center)
{
    int k;
    for (k = 0; k < 3; k++) { g_sim.player[k] = (float)feet[k]; g_pl.origin[k] = feet[k]; }
    g_sim.player_unit.y = (float)feet[1]; g_sim.player_centre_h = center;
}

static void selectors(prj_archive *a)
{
    size_t row;
    for (row = 0; row < sizeof original_aircraft_targets / sizeof original_aircraft_targets[0]; row++) {
        const mercs_original_aircraft_target *f = &original_aircraft_targets[row];
        world_actor actor = {0}; combat_unit u = {0}; int armed = 1;
        float center, d = -1, delta[3]; int k, ok;
        memset(&g_sim, 0, sizeof g_sim); memset(&g_pl, 0, sizeof g_pl);
        mech3d_dos_hips = 1;
        if (mech3d_load(a, f->model, 0, &actor.mech)) exit(2);
        center = (float)actor.mech.centre_h;
        g_sim.actor_count = g_actor_count = 1; g_sim.actors = g_actors = &actor;
        g_sim.units = &u; g_sim.armed = &armed; g_sim.centre_h = &center;
        g_sim_ok = 1; g_tgt_any = 0; actor.alliance = 1; actor.unit_class = 2;
        actor.unit_flags = (unsigned)f->target_flags;
        player_reference(f->source_feet, (float)f->source_center);
        for (k = 0; k < 3; k++) actor.mech.origin[k] = f->target_feet[k];
        u.y = (float)f->target_feet[1];
        if (f->override_y_enabled) center = (float)f->override_y - u.y;
        ok = msim_lock_delta(&g_sim, -1, 0, delta);
        eq(ok, 1, 0, "target delta available", f->name);
        for (k = 0; k < 3; k++)
            eq(delta[k], (double)f->target_body[k] - f->source_body[k], 0,
               "original target delta", f->name);
        ok = unit_targetable(0, -1, &d);
        eq(ok, f->status == 1, 0, "original actor selector", f->name);
        if (f->status == 1)
            eq(d, f->distance, 1.1, "integer original / float native range", f->name);
        selector_cases++;
        /* Existing non-distance gates remain ordered and effective. */
        if (row == 3) {
            eq(unit_targetable(0, 0, NULL), 0, 0, "selector gate", "opposite alliance"); gate_cases++;
            u.shutdown = 1; eq(unit_targetable(0, -1, NULL), 0, 0, "selector gate", "shutdown"); gate_cases++;
            g_tgt_any = 1; eq(unit_targetable(0, -1, NULL), 1, 0, "selector gate", "Q permits shutdown"); gate_cases++;
            u.shutdown = 0; actor.unit_flags = 0x800; eq(unit_targetable(0, -1, NULL), 0, 0, "selector gate", "hidden flag"); gate_cases++;
            actor.unit_flags = 0x400; u.destroyed = 1; eq(unit_targetable(0, -1, NULL), 0, 0, "selector gate", "destroyed"); gate_cases++;
            u.destroyed = 0; actor.unit_flags = 0x1000; actor.mech.origin[2] = 300000;
            eq(unit_targetable(0, -1, NULL), 1, 0, "selector gate", "long range flag"); gate_cases++;
        }
        mech3d_free(&actor.mech);
    }
    /* Declared thing points isolate the already original-proven source body
     * operand. These are native consumer checks, not extra original inputs. */
    {
        mech3d world = {0}; mech3d_part part = {0}; struct msim_building b = {0};
        const int32_t feet[3] = {0,199776,0}; float d = -1;
        memset(&g_sim, 0, sizeof g_sim); g_actor_count = 0; g_actors = NULL;
        g_sim_ok = 1; g_tgt_any = 0; player_reference(feet, 771);
        world.parts = &part; world.part_count = 1; b.hp = 100; b.flags = 0x400; b.intact = 0;
        g_sim.world = &world; g_sim.bld = &b; g_sim.bld_count = 1;
        part.pos[1] = 200547; part.pos[2] = 50000;
        eq(thing_targetable(0, &d), 1, 0, "thing body range", "near");
        eq(d, 50000, 0, "thing body range", "near distance"); thing_cases++;
        part.pos[2] = 0; part.pos[1] = 375547;
        eq(thing_targetable(0, &d), 1, 0, "thing body range", "maximum equality");
        eq(d, 175000, 0, "thing body range", "maximum distance"); thing_cases++;
        part.pos[1] = 375548;
        eq(thing_targetable(0, NULL), 0, 0, "thing body range", "past maximum"); thing_cases++;
        b.flags = 0x1000; eq(thing_targetable(0, NULL), 1, 0, "thing gate", "long range flag"); gate_cases++;
        b.flags = 0; g_tgt_any = 0; part.pos[1] = 200547;
        eq(thing_targetable(0, NULL), 0, 0, "thing gate", "unflagged before Q"); gate_cases++;
        g_tgt_any = 1; eq(thing_targetable(0, NULL), 1, 0, "thing gate", "unflagged after Q"); gate_cases++;
        b.flags = 0x800; eq(thing_targetable(0, NULL), 0, 0, "thing gate", "hidden flag"); gate_cases++;
        b.flags = 0x400; part.hidden = 1; eq(thing_targetable(0, NULL), 0, 0, "thing gate", "hidden object"); gate_cases++;
        part.hidden = 0; b.hp = 0; eq(thing_targetable(0, NULL), 0, 0, "thing gate", "destroyed"); gate_cases++;
    }
    memset(&g_sim, 0, sizeof g_sim); g_sim_ok = 0; g_actors = NULL; g_actor_count = 0;
}

static void scene_free(mech3d *world)
{
    int i;
    msim_free(&g_sim); mech3d_free(world);
    for (i = 0; i < g_actor_count; i++) mech3d_free(&g_actors[i].mech);
    free(g_actors); g_actors = NULL; g_actor_count = 0; g_sim_ok = 0;
    memset(&g_sim, 0, sizeof g_sim);
}

/* Raw-state controls of the statically proven state4 dispatch/early returns.
 * They do not claim an ordinary aircraft kill or subsequent-frame equivalence
 * to original OBJ+0x78: native still infers grounded state from y<=ground. */
static void wreck_controls(void)
{
    int test;
    for (test = 0; test < 5; test++) {
        combat_unit *u = &g_sim.units[0]; world_actor *a = &g_actors[0];
        u->destroyed = 1; u->intact_out = u->body_detached = 0;
        u->unit_class = a->unit_class = 2; u->y = 2000; u->ground = 0; u->vy = -1;
        a->mech.origin[0] = 400000; a->mech.origin[1] = 2000; a->mech.origin[2] = 400000;
        a->mech.heading = 0; g_sim.minds[0].speed = 100; g_sim.across[0] = 0;
        g_sim.wreck_until[0] = 1; /* Declared expired FX timer: no unrelated death explosion. */
        if (test == 1) { u->y = u->ground = 123; u->vy = 0; a->mech.origin[1] = 123; }
        if (test == 2) { u->body_detached = 1; u->y = 4321; a->mech.origin[1] = 4321; }
        if (test == 3) {
            /* The attempted body segment closes toward a real live unit. */
            g_actors[1].mech.origin[0] = 400000; g_actors[1].mech.origin[2] = 400100;
            g_actors[1].mech.origin[1] = 2000; g_sim.units[1].y = 2000;
        }
        if (test == 4) {
            u->y = 199778; u->ground = 199776; u->vy = -10;
            a->mech.origin[0] = -38022; a->mech.origin[1] = 199778; a->mech.origin[2] = 32508;
        }
        mech3d_pose(&a->mech, NULL, 0);
        msim_step(&g_sim, 33); scene_steps++; wreck_cases++;
        if (test == 0) {
            eq(u->body_detached, 0, 0, "state4 free publication", "body retained");
            eq(u->y, 0, 0, "state4 free publication", "class2 baseline");
            eq(a->mech.origin[1], 0, 0, "state4 free publication", "render baseline");
            eq(g_sim.minds[0].speed, 100, 0, "state4 free publication", "preceding velocity retained");
        } else if (test == 1) {
            eq(u->y, 123, 0, "state4 grounded exclusion", "baseline retained");
            eq(a->mech.origin[1], 123, 0, "state4 grounded exclusion", "render baseline retained");
        } else if (test == 2) {
            eq(u->y, 4321, 0, "state4 detached exclusion", "baseline retained");
            eq(a->mech.origin[1], 4321, 0, "state4 detached exclusion", "render baseline retained");
        } else if (test == 3) {
            eq(u->body_detached, 1, 0, "state4 collision early return", "detached");
            eq(u->y, 2000, 0, "state4 collision early return", "not clamped");
            g_actors[1].mech.origin[0] = -8500; g_actors[1].mech.origin[2] = 91500;
            g_actors[1].mech.origin[1] = 0; g_sim.units[1].y = 0;
        } else {
            eq(u->body_detached, 1, 0, "state4 hard landing early return", "detached");
            eq(u->y, 199776, .1, "state4 hard landing early return", "not clamped");
        }
    }
}

static void scenes(prj_archive *a, int archive_index)
{
    int hips;
    for (hips = 0; hips < 2; hips++) {
        mech3d world = {0}; int i, frame; const int32_t player[3] = {-45132,199776,44291};
        unsigned before = failures; float hp_before = 0;
        memset(&g_sim, 0, sizeof g_sim); memset(&g_pl, 0, sizeof g_pl);
        mech3d_dos_hips = hips; msim_dos_int = hips;
        if (world3d_mission(a, "SPA1SCN1", 1, &world) < 0 ||
            world3d_actors(a, "SPA1SCN1", &g_actors, &g_actor_count) ||
            msim_init(&g_sim, a, "SPA1SCN1", g_actors, g_actor_count)) exit(2);
        g_sim_ok = 1; g_tgt_any = 0; scene_loads++;
        eq(g_actor_count, 6, 0, "SPA1", "actor count");
        for (i = 0; i < g_actor_count; i++) {
            eq(g_actors[i].unit_class, 2, 0, "SPA1 class", g_actors[i].skel);
            eq(g_sim.units[i].y, 0, 0, "SPA1 initial baseline", g_actors[i].skel);
            eq(g_sim.centre_h[i], 213000, 0, "SPA1 loaded centre", g_actors[i].skel);
        }
        msim_set_world(&g_sim, &world); player_reference(player, 771);
        for (i = 0; i < g_actor_count; i++)
            eq(g_sim.units[i].y, 0, 0, "SPA1 authored initial placement", g_actors[i].skel);
        /* A declared placement of the real loaded actor at the exact captured
         * nearby-Shilone X/Z. Fresh scratch state, no saved career involved. */
        g_actors[0].mech.origin[0] = -38022; g_actors[0].mech.origin[2] = 32508;
        g_actors[0].mech.origin[1] = 0; g_sim.units[0].y = 0;
        mech3d_pose(&g_actors[0].mech, NULL, 0);
        eq(msim_ground(&g_sim, -38022, 32508, 213000), 199776, 0.1, "SPA1 actual deck", "support");
        settle_spawn_ground(&g_sim);
        eq(g_sim.units[0].y, 0, 0, "SPA1 class2 spawn exclusion", "baseline under deck");
        /* The real initial startup timer keeps this short fixture stationary
         * while msim_step executes collision/vertical/publication. No control
         * state, mission result or gameplay input is supplied to the engine. */
        for (i = 0; i < 8; i++) hp_before += g_sim.units[0].armor[i] + g_sim.units[0].internal[i];
        for (frame = 0; frame < 90; frame++) {
            float hp = 0, delta[3];
            msim_step(&g_sim, 33); scene_steps++;
            eq(g_sim.units[0].y, 0, 0, "SPA1 repeated altitude publication", "baseline");
            eq(g_actors[0].mech.origin[1], 0, 0, "SPA1 repeated altitude publication", "render baseline");
            eq(g_sim.units[0].y + g_sim.centre_h[0], 213000, 0,
               "SPA1 repeated altitude publication", "body centre");
            eq(g_sim.units[0].ground, 199776, 0.1, "SPA1 repeated altitude publication", "ground retained");
            eq(g_sim.units[0].vy, 0, 0, "SPA1 repeated altitude publication", "no accumulated vertical velocity");
            for (i = 0; i < 8; i++) hp += g_sim.units[0].armor[i] + g_sim.units[0].internal[i];
            eq(hp, hp_before, 0, "SPA1 repeated altitude publication", "no repeated landing damage");
            check(msim_lock_delta(&g_sim, -1, 0, delta), "SPA1 delta", "available", 0, 1);
            eq(delta[1], 12453, 0, "SPA1 repeated altitude publication", "live body delta");
            eq(unit_targetable(0, -1, NULL), 1, 0, "SPA1 repeated altitude publication", "ordinary targetability");
        }
        /* Original fixed-altitude slice: a nonzero initial baseline lasts
         * through loading, then class2 publishes absolute MGEO altitude. */
        g_sim.units[0].y = 12345; g_sim.units[0].ground = 0;
        g_actors[0].mech.origin[0] = 0; g_actors[0].mech.origin[2] = 100000;
        g_actors[0].mech.origin[1] = 12345;
        msim_step(&g_sim, 33); scene_steps++;
        eq(g_sim.units[0].y, original_aircraft_altitudes[2].feet_y, 0,
           "original nonzero baseline publication", "class2"); altitude_cases++;
        /* Static/slice-proven class condition, using the same real geometry.
         * The fake class1 control keeps ordinary ground placement. */
        g_actors[0].unit_class = 1; g_sim.units[0].unit_class = 1;
        g_sim.units[0].y = 0; g_sim.units[0].vy = 0;
        g_actors[0].mech.origin[0] = -38022; g_actors[0].mech.origin[2] = 32508;
        msim_step(&g_sim, 33); scene_steps++;
        eq(g_sim.units[0].y, original_aircraft_altitudes[3].feet_y, 0.1,
           "original nonclass2 publication control", "ground baseline"); altitude_cases++;
        wreck_controls();
        printf("{\"archive\":%d,\"hips\":%d,\"scene\":\"SPA1SCN1\",\"declared_nearby_actor\":0,"
               "\"stationary_frames\":90,\"dt_ms\":33,\"additional_class_controls\":2,\"state4_controls\":5,\"failures\":%u}\n",
               archive_index, hips, failures-before);
        scene_free(&world);
    }
}

int main(int argc, char **argv)
{
    int arg;
    if (argc < 2) { fprintf(stderr, "usage: %s ARCHIVE.PRJ [ARCHIVE.PRJ ...]\n", argv[0]); return 2; }
#ifdef _WIN32
    _putenv_s("MERCS_INSTALL_DIR", ""); _putenv_s("MERCS_CD_DIR", ""); _putenv_s("MERCS_CD_IMAGE", "");
    _putenv_s("MERCS_OF", ""); _putenv_s("MERCS_OE", "");
#else
    unsetenv("MERCS_INSTALL_DIR"); unsetenv("MERCS_CD_DIR"); unsetenv("MERCS_CD_IMAGE");
    unsetenv("MERCS_OF"); unsetenv("MERCS_OE");
#endif
    dp_clear_roots();
    combat_set_weapon_profile(COMBAT_PROFILE_RETAIL);
    for (arg = 1; arg < argc; arg++) {
        char error[256]; prj_archive *a = prj_open(argv[arg], error, sizeof error);
        if (!a) { fprintf(stderr, "%s\n", error); return 2; }
        models(a, arg-1); selectors(a); scenes(a, arg-1); prj_close(a);
    }
    printf("{\"summary\":true,\"checks\":%u,\"failures\":%u,\"models\":%u,\"model_profiles\":%u,"
           "\"original_actor_selector_cases\":%u,\"native_thing_cases\":%u,\"preserved_gate_cases\":%u,"
           "\"scene_loads\":%u,\"native_scene_steps\":%u,\"native_altitude_controls\":%u,\"native_state4_controls\":%u,"
           "\"scope\":\"Retail weapon profile with each archive geometry; two model hip modes; native isolated fixtures; original nonzero override-global case excluded from single-player native scope\"}\n",
           checks, failures, model_count, profile_count, selector_cases, thing_cases, gate_cases,
           scene_loads, scene_steps, altitude_cases, wreck_cases);
    return failures ? 1 : 0;
}
