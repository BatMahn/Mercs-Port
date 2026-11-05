/* Original-backed targeting coordinates and full native lock consumer.
 * Declared units/nodes only: no scene step, player inputs or career changes.
 * MERCS_TARGETING_SOURCE/BASELINE support a private negative control without
 * replacing shared production files or the campaign executable. */
#define _POSIX_C_SOURCE 200809L
#ifndef MERCS_TARGETING_SOURCE
#define MERCS_TARGETING_SOURCE "../src/msim.c"
#endif
#include MERCS_TARGETING_SOURCE
#include "../tests/fixtures/targeting-coordinates/original-targeting-fixtures.h"

static void unit(combat_unit *u)
{
    int i;
    memset(u, 0, sizeof *u); u->human = 1; u->armor_mult = u->speed_gain = 1;
    for (i = 0; i < 8; i++) u->armor[i] = u->internal[i] = 100;
    u->lock_target = -2;
}

static float model_height(const mech3d *m)
{
    /* Same rest-model reduction as the initialized actor in msim_init. */
    int p, v;
    float top = 0;
    for (p = 0; p < m->part_count; p++) {
        const mech3d_part *pp = &m->parts[p];
        if (pp->model.object_count < 1) continue;
        for (v = 0; v < pp->model.objects[0].vert_count; v++) {
            float y = (float)(pp->model.objects[0].verts[v].y + pp->pos[1] - m->origin[1]);
            if (y > top) top = y;
        }
    }
    return top > 100 ? top : 600;
}

int main(int argc, char **argv)
{
    unsigned mismatches = 0, count = 0, vector_checks = 0;
    int arg;
    if (argc < 2) { fprintf(stderr, "usage: %s ARCHIVE.PRJ [ARCHIVE.PRJ ...]\n", argv[0]); return 2; }
#ifdef _WIN32
    _putenv_s("MERCS_INSTALL_DIR", ""); _putenv_s("MERCS_CD_DIR", ""); _putenv_s("MERCS_CD_IMAGE", "");
#else
    unsetenv("MERCS_INSTALL_DIR"); unsetenv("MERCS_CD_DIR"); unsetenv("MERCS_CD_IMAGE");
#endif
    dp_clear_roots(); combat_set_weapon_profile(COMBAT_PROFILE_RETAIL);
    printf("{\"rows\":[\n");
    for (arg = 1; arg < argc; arg++) {
        char error[256];
        prj_archive *a = prj_open(argv[arg], error, sizeof error);
        size_t k;
        if (!a) { fprintf(stderr, "%s\n", error); return 2; }
        for (k = 0; k < sizeof cases / sizeof cases[0]; k++) {
            const fixture_case *f = &cases[k];
            msim *s = calloc(1, sizeof *s);
            world_actor *ac = calloc(1, sizeof *ac);
            combat_unit actor_unit;
            ai_mind mind;
            mech3d world;
            mech3d_part part;
            struct msim_building building;
            int armed = 1, source = -1, target = 0, valid = 0, j;
            float h, center, radius = 440, delta[3] = {0,0,0};
            unsigned flags, before = mismatches;
            if (!s || !ac || mech3d_load(a, f->source_ai ? f->source : f->target, 0, &ac->mech) != 0) return 2;
            h = model_height(&ac->mech);
            unit(&s->player_unit); unit(&actor_unit); actor_unit.human = 0;
            memset(&mind, 0, sizeof mind); memset(&world, 0, sizeof world);
            memset(&part, 0, sizeof part); memset(&building, 0, sizeof building);
            s->actors = ac; s->units = &actor_unit; s->armed = &armed; s->actor_count = 1;
            s->height = &h; s->centre_h = &center; s->radius = &radius;
            s->player_radius = 440; s->rng = 1; s->no_player_muzzle = 1;
            s->difficulty = f->difficulty;
            s->player_target = s->player_lock_target = s->player_sel_w = -1;
            ac->table = 1;
            if (f->source_ai) {
                source = 0; target = -1;
                center = (float)f->source_center;
                ac->alliance = 1; ac->unit_class = f->source_class;
                ac->gps_control = 2; ac->ai_gps6 = (uint8_t)f->behavior;
                ac->mech.heading = (float)f->yaw_q16 / 65536.0f;
                s->player[2] = (float)f->z;
                s->player_centre_h = (float)f->target_center;
                s->player_unit_class = f->target_class;
                s->player_unit.narc_beacon = f->narc;
                actor_unit.weapon_count = 1; actor_unit.ai_sel = 0;
                actor_unit.weapons[0].weapon = 37;
                actor_unit.weapons[0].state = CW_READY; actor_unit.weapons[0].ammo = -1;
                s->minds = &mind; s->ai_ok = 1;
                mind.tgt = -1; mind.pitch = (float)f->pitch_q16 / 65536.0f;
            } else {
                center = (float)f->target_center;
                ac->friendly = 1; ac->alliance = 0; ac->unit_class = f->target_class;
                ac->mech.origin[2] = f->z;
                s->player_unit_class = f->source_class;
                s->player_centre_h = (float)f->source_center;
                s->player_unit.weapon_count = 1;
                s->player_unit.weapons[0].weapon = 37;
                s->player_unit.weapons[0].state = CW_READY;
                s->player_unit.weapons[0].ammo = -1;
                s->player_lock_target = 0; s->player_sel_w = 0;
                s->player_aim_yaw = (float)f->yaw_q16 / 65536.0f;
                s->player_aim_pitch = (float)f->pitch_q16 / 65536.0f;
                actor_unit.narc_beacon = f->narc;
            }
            if (f->target_kind) {
                target = MSIM_TGT_THING(0); s->player_lock_target = target;
                world.parts = &part; world.part_count = 1;
                part.pos[1] = f->target_y; part.pos[2] = f->z;
                building.hp = 100; building.intact = 0;
                s->bld = &building; s->bld_count = 1; s->world = &world;
            }
#ifndef MERCS_TARGETING_BASELINE
            valid = msim_lock_delta(s, source, target, delta);
            if (!valid) mismatches++;
            for (j = 0; j < 3; j++) {
                if (delta[j] != (float)f->delta[j]) mismatches++;
                vector_checks++;
            }
#else
            (void)source; (void)target; (void)j;
#endif
            combat_step(s, 0.0f);
            flags = (unsigned)(f->source_ai ? mind.lock : s->player_lock) & 0x80c0u;
            if (flags != f->flags) mismatches++;
            printf("%s{\"archive\":%d,\"name\":\"%s\",\"source_ai\":%d,"
                   "\"source_center\":%d,\"target_center\":%d,\"native_render_height\":%.6g,"
                   "\"native_flags\":%u,\"original_flags\":%u,\"delta_valid\":%d,"
                   "\"delta\":[%.6g,%.6g,%.6g],\"pass\":%s}",
                   count++ ? ",\n" : "", arg-1, f->name, f->source_ai,
                   f->source_center, f->target_center, h, flags, f->flags, valid,
                   delta[0],delta[1],delta[2], before == mismatches ? "true" : "false");
            mech3d_free(&ac->mech); free(ac); free(s);
        }
        prj_close(a);
    }
    printf("\n],\"lock_checks\":%u,\"vector_component_checks\":%u,\"mismatches\":%u,\"pass_all\":%s}\n",
           count, vector_checks, mismatches, mismatches ? "false" : "true");
    return mismatches ? 1 : 0;
}
