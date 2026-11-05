/* Test-only observation/input bridge for real mission playthroughs.
 * Included only by a MERCS_PLAYTHROUGH_TEST build of glview.c. The driver can
 * read state and submit ordinary SDL key/mouse inputs; it cannot write a
 * position, damage record, objective, RNG state or mission outcome. */
#ifndef MERCS_PLAYTHROUGH_INPUT_H
#define MERCS_PLAYTHROUGH_INPUT_H
#include "wtable_titanium.h"
#include "mercs_playthrough_geometry.h"

static bool playthrough_held[SDL_SCANCODE_COUNT];
static int playthrough_enabled(void)
{ return g_auto_on && getenv("MERCS_PLAYTHROUGH") != NULL; }

static int playthrough_draw_world(void)
{
    const char *value = getenv("MERCS_PLAYTHROUGH_DRAW_EVERY");
    int interval = value ? atoi(value) : 1;
    if (!playthrough_enabled() || interval < 2) return 1;
    return !(g_frame_no % interval) || g_auto_elapsed + g_auto_dt >= g_auto_secs || g_sim.ending;
}

static void playthrough_string(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    putchar('"');
    for (; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p >= 32 && *p < 127) putchar(*p);
        else printf("\\u%04x", *p);
    }
    putchar('"');
}

static void playthrough_node(int table, int index)
{
    const mtbl_node *n = &g_sim.logic.tables[table].nodes[index];
    char objective[2] = {n->objective,0}; int item;
    printf("{\"id\":%d,\"kind\":%u,\"state\":%u,\"shown\":%d,\"main\":%d,\"objective\":",
           index,n->kind,g_sim.logic.state[table][index],g_sim.logic.shown[table][index],n->objective=='M');
    playthrough_string(objective);printf(",\"target\":");playthrough_string(n->target);
    printf(",\"text\":");playthrough_string(n->text);
    printf(",\"require_all\":%d,\"delay\":%u,\"other_table\":%d,\"other_node\":%d,\"started\":%d,\"items\":[",
           n->require_all,n->delay,n->other_table,n->other_node,g_sim.logic.started[table][index]);
    for(item=0;item<n->item_count;item++) {
        char type[2]={n->items[item].type,0};
        printf("%s{\"type\":",item?",":"");playthrough_string(type);
        printf(",\"table\":%d,\"node\":%d}",n->items[item].table,n->items[item].node);
    }
    printf("]}");
}

static void playthrough_observe(void)
{
    const combat_unit *u = &g_sim.player_unit;
    int i, table = g_sim.player_table;
    printf("PLAY {\"time\":%.5f,\"now\":%d,\"position\":[%d,%.3f,%d],"
           "\"heading\":%.5f,\"twist\":%.5f,\"pitch\":%.5f,\"speed\":%.5f,"
           "\"throttle\":%.5f,\"reverse\":%d,\"heat\":%.5f,\"health\":%d,\"destroyed\":%d,"
           "\"shutdown\":%d,\"online_at\":%.3f,\"invulnerable\":%d,\"no_heat\":%d,"
           "\"outcome\":%d,\"ending\":%d,\"over\":%d,\"nav\":%d,\"autopilot\":%d,"
           "\"target\":%d,\"thing\":%d,\"reticle_bld\":%d,\"menu\":%d,\"selected_weapon\":%d,"
           "\"group_fire\":%d,\"hits\":%d,\"fired\":%d,\"mouse\":[%.5f,%.5f],"
           "\"camera\":[%.5f,%.5f,%.5f],\"weapon_eye\":[%.5f,%.5f,%.5f],"
           "\"aim_yaw\":%.5f,\"aim_pitch\":%.5f,\"unlimited_ammo\":%d,"
           "\"collision_damage\":%d,\"splash_damage\":%d,\"difficulty\":%d,"
           "\"missile_lock\":%d,\"lock_target\":%d,\"blocked\":%d,\"immobile\":%d,\"speed_gain\":%.5f,\"weapons\":[",
           (double)g_auto_elapsed, g_sim.now, g_pl.origin[0], (double)u->y, g_pl.origin[2],
           (double)g_pl.heading, (double)g_pl.twist, (double)g_pitch, (double)g_pl_speed,
           (double)g_throttle, g_reverse, (double)u->heat, combat_health(u), u->destroyed,
           u->shutdown, (double)g_sim.player_online_at, u->invulnerable, u->no_heat,
           g_sim.outcome, g_sim.ending, g_sim.over, g_nav_sel, g_autopilot,
           g_tgt_sel, g_tgt_thing, g_sim.reticle_bld, g_mm_depth, g_sel_weapon, g_group_fire,
           g_sim.hits, test_fired, (double)g_im_dev.mouse_x, (double)g_im_dev.mouse_y,
           (double)g_cam_eye[0], (double)g_cam_eye[1], (double)g_cam_eye[2],
           (double)g_sim.player_eye[0], (double)g_sim.player_eye[1], (double)g_sim.player_eye[2],
           (double)g_sim.player_aim_yaw, (double)g_sim.player_aim_pitch,
           u->unlimited_ammo, g_sim.collision_damage, g_sim.splash, g_sim.difficulty,
           g_sim.player_lock, g_sim.player_lock_target,u->blocked,u->immobile,(double)u->speed_gain);
    for (i = 0; i < u->weapon_count; i++) {
        const combat_weapon *w = &u->weapons[i];
        const sim_weapon *definition = g_edition == PORTCFG_ED_D3D ? &sim_weapons_titanium[w->weapon] : &sim_weapons[w->weapon];
        printf("%s{\"id\":%d,\"state\":%d,\"ammo\":%d,\"location\":%d,\"speed\":%.3f,\"range\":%.3f,\"lock_min_range_cm\":%.3f,\"heat\":%.3f,\"damage\":%.3f,\"guided\":%d,\"per_volley\":%d,\"group\":%d}",
               i ? "," : "", w->weapon, w->state, w->ammo, w->location,
               (double)(combat_weapon_speed(w->weapon)*182.0f), (double)combat_weapon_range(w->weapon),
               (double)sim_weapons[w->weapon].field3c,
               (double)combat_weapon_heat(w->weapon), (double)combat_weapon_damage(w->weapon), msim_weapon_guided(w->weapon), definition->per_volley, w->return_group);
    }
    /* Raw combat floats, in enum mek_location order:
     * HEAD, RT, CT, LT, RA, LA, RL, LL. Observe without normalization. */
    printf("],\"player_armor\":[");
    for (i = 0; i < MEK_LOC_COUNT; i++) printf("%s%.5f", i ? "," : "", (double)u->armor[i]);
    printf("],\"player_rear_armor\":[");
    for (i = 0; i < MEK_LOC_COUNT; i++) printf("%s%.5f", i ? "," : "", (double)u->rear[i]);
    printf("],\"player_internal\":[");
    for (i = 0; i < MEK_LOC_COUNT; i++) printf("%s%.5f", i ? "," : "", (double)u->internal[i]);
    printf("],\"actors\":[");
    for (i = 0; i < g_actor_count; i++) {
        const world_actor *a = &g_actors[i];
        float range = 0, aim[3], origin[3], lock_delta[3], lock_distance;
        mercs_playthrough_world_ray world_ray;
        int world_ray_result = -1;
        /* Observe the actual posed mesh transform, including its authored
         * root offset. Controller ground/body Y is a different quantity. */
        origin[0] = (float)a->mech.origin[0]; origin[1] = (float)a->mech.origin[1]; origin[2] = (float)a->mech.origin[2];
        if (mech3d_group_point(&a->mech, 3, origin, a->mech.heading, aim)) { memcpy(aim, origin, sizeof aim); aim[1] += (float)a->mech.centre_h; }
        /* Use the same read-only center/foot delta as native acquisition.
         * Observe each potential actor target, not only the selected one;
         * the posed mesh aim point remains a separate rendering quantity. */
        lock_distance = msim_lock_delta(&g_sim, -1, i, lock_delta)
                      ? sqrtf(lock_delta[0] * lock_delta[0] + lock_delta[1] * lock_delta[1]
                              + lock_delta[2] * lock_delta[2]) : -1.0f;
        /* The diagnostic uses a private query view and the exact eye/aim
         * published below. Before the first valid weapon-eye update it is
         * unknown, not a camera fallback or a zero-origin clear ray. */
        memset(&world_ray, 0, sizeof world_ray);
        world_ray.hit_part = world_ray.hit_collision_type = world_ray.hit_thing = -1;
        world_ray.hit_distance_cm = -1.0f;
        if (g_sim.player_eye_ok)
            world_ray_result = mercs_playthrough_world_obstruction(&g_sim, g_sim.player_eye,
                                                                  aim, &world_ray);
        if (world_ray_result != 0) world_ray.available = 0;
        int targetable = unit_targetable(i, -1, &range);
        printf("%s{\"id\":%d,\"name\":", i ? "," : "", i);
        playthrough_string(a->name); printf(",\"group\":"); playthrough_string(a->group);
        printf(",\"position\":[%d,%.5f,%d],\"height\":%.5f,\"side\":%d,"
               "\"armed\":%d,\"destroyed\":%d,\"health\":%d,\"targetable\":%d,\"class\":%u,\"lock_distance_cm\":%.5f,\"aim\":[%.5f,%.5f,%.5f],"
               "\"table\":%d,\"unit_class\":%d,\"inspected\":%d,\"narc_beacon\":%d",
               a->mech.origin[0], (double)(g_sim.armed[i] ? g_sim.units[i].y : (float)a->mech.origin[1]),
               a->mech.origin[2], (double)g_sim.height[i], unit_side(i),
               g_sim.armed[i], g_sim.armed[i] ? g_sim.units[i].destroyed : 0,
               g_sim.armed[i] ? combat_health(&g_sim.units[i]) : -1, targetable, a->obj_class, (double)lock_distance, (double)aim[0], (double)aim[1], (double)aim[2],
               a->table,a->unit_class,g_sim.inspected ? g_sim.inspected[i] : 0,
               g_sim.armed[i] ? g_sim.units[i].narc_beacon : 0);
        printf(",\"world_obstruction\":{\"available\":%d,\"blocked\":%d,"
               "\"hit_part\":%d,\"hit_collision_type\":%d,\"hit_thing\":%d,"
               "\"from\":[%.5f,%.5f,%.5f],\"to\":[%.5f,%.5f,%.5f],"
               "\"segment_length_cm\":%.5f,\"hit_distance_cm\":%.5f,"
               "\"hit_point\":[%.5f,%.5f,%.5f],\"normal\":[%.8f,%.8f,%.8f]}}",
               world_ray.available,world_ray.blocked,world_ray.hit_part,
               world_ray.hit_collision_type,world_ray.hit_thing,
               (double)world_ray.from[0],(double)world_ray.from[1],(double)world_ray.from[2],
               (double)world_ray.to[0],(double)world_ray.to[1],(double)world_ray.to[2],
               (double)world_ray.segment_length_cm,(double)world_ray.hit_distance_cm,
               (double)world_ray.hit_point[0],(double)world_ray.hit_point[1],(double)world_ray.hit_point[2],
               (double)world_ray.normal[0],(double)world_ray.normal[1],(double)world_ray.normal[2]);
    }
    printf("],\"navs\":[");
    for (i = 0; i < g_sim.nav_count; i++) {
        const msim_nav *n = &g_sim.navs[i];
        printf("%s{\"id\":%d,\"name\":", i ? "," : "", i); playthrough_string(n->name);
        printf(",\"position\":[%.5f,%.5f,%.5f],\"radius\":%.5f,\"shown\":%d,\"selectable\":%d,\"visited\":%d,\"class\":%u}",
               (double)n->x, (double)n->y, (double)n->z, (double)n->radius, n->shown, n->selectable, n->visited,n->cls);
    }
    printf("],\"buildings\":[");
    for (i = 0; i < g_sim.bld_count; i++) {
        const struct msim_building *b = &g_sim.bld[i];
        float point[3], lock_delta[3], lock_distance;
        /* The native target/guidance point transforms the local vertex-box
         * midpoint. It can differ from the world AABB midpoint below.
         * Read both through the existing pure production helpers, without
         * selecting the thing or altering the reticle's retained identity. */
        int point_valid = msim_thing_point(&g_sim, i, point);
        lock_distance = msim_lock_delta(&g_sim, -1, MSIM_TGT_THING(i), lock_delta)
                      ? sqrtf(lock_delta[0] * lock_delta[0] + lock_delta[1] * lock_delta[1]
                              + lock_delta[2] * lock_delta[2]) : -1.0f;
        printf("%s{\"id\":%d,\"name\":", i ? "," : "", i); playthrough_string(b->name);
        printf(",\"record\":"); playthrough_string(g_sim.world->parts[b->intact].rec);
        printf(",\"hp\":%d,\"side\":%d,\"type\":%d,\"position\":[%.5f,%.5f,%.5f],\"bounds\":[%.5f,%.5f,%.5f,%.5f,%.5f,%.5f],\"inspected\":%d,\"lock_distance_cm\":%.5f,\"thing_point\":",
               b->hp, b->side, b->type, (double)((b->mn[0]+b->mx[0])*0.5f),
               (double)((b->mn[1]+b->mx[1])*0.5f), (double)((b->mn[2]+b->mx[2])*0.5f),
               (double)b->mn[0], (double)b->mn[1], (double)b->mn[2],
               (double)b->mx[0], (double)b->mx[1], (double)b->mx[2], b->inspected, (double)lock_distance);
        if (point_valid) printf("[%.5f,%.5f,%.5f]", (double)point[0], (double)point[1], (double)point[2]);
        else printf("null");
        putchar('}');
    }
    printf("],\"obstacles\":[");
    {
        int emitted = 0;
        for (i = 0; i < g_mech.part_count; i++) {
            const mech3d_part *p = &g_mech.parts[i];
            float mn[3] = {1e30f,1e30f,1e30f}, mx[3] = {-1e30f,-1e30f,-1e30f};
            int j, k;
            if (p->hidden || p->model.object_count < 1 ||
                (p->coll != 0 && p->coll != 1 && p->coll != 2 && p->coll != 7)) continue;
            for (j = 0; j < p->model.objects[0].vert_count; j++) {
                const wtb_vertex *v = &p->model.objects[0].verts[j];
                float q[3];
                if (p->has_rot) {
                    for (k = 0; k < 3; k++) q[k] = p->rot[k*3] * (float)v->x + p->rot[k*3+1] * (float)v->y + p->rot[k*3+2] * (float)v->z + (float)p->pos[k];
                } else {
                    float c = cosf(p->yaw * 3.14159265f / 180.0f), sn = sinf(p->yaw * 3.14159265f / 180.0f);
                    q[0] = c*(float)v->x+sn*(float)v->z+(float)p->pos[0];
                    q[1] = (float)v->y+(float)p->pos[1];
                    q[2] = -sn*(float)v->x+c*(float)v->z+(float)p->pos[2];
                }
                for (k = 0; k < 3; k++) { if (q[k] < mn[k]) mn[k] = q[k]; if (q[k] > mx[k]) mx[k] = q[k]; }
            }
            if (mn[0] > mx[0]) continue;
            printf("%s{\"id\":%d,\"type\":%d,\"bounds\":[%.3f,%.3f,%.3f,%.3f,%.3f,%.3f]}", emitted++ ? "," : "", i, p->coll,
                   (double)mn[0],(double)mn[1],(double)mn[2],(double)mx[0],(double)mx[1],(double)mx[2]);
        }
    }
    {
        mercs_playthrough_geometry geometry;
        float feet[3]={(float)g_pl.origin[0],u->y,(float)g_pl.origin[2]};
        int sample;
        (void)mercs_playthrough_geometry_sample(&g_sim,feet,g_pl.heading,&geometry);
        printf("],\"geometry\":{\"available\":%d,\"feet_y\":%.5f,\"ground_y\":%.5f,"
               "\"query_y\":%.5f,\"centre_h\":%.5f,\"radius\":%.5f,\"vertical_speed\":%.5f,"
               "\"airborne\":%d,\"normal\":[%.5f,%.5f,%.5f],\"directions\":[",
               geometry.available,(double)geometry.feet_y,(double)geometry.ground_y,
               (double)geometry.query_y,(double)geometry.centre_h,(double)geometry.radius,
               (double)geometry.vertical_speed,geometry.airborne,
               (double)geometry.normal[0],(double)geometry.normal[1],(double)geometry.normal[2]);
        for(sample=0;sample<geometry.sample_count;sample++){
            const mercs_playthrough_direction *d=&geometry.directions[sample];
            printf("%s{\"angle_relative_deg\":%.5f,\"distance_cm\":%.5f,\"position\":[%.5f,%.5f],"
                   "\"ground_y\":%.5f,\"normal\":[%.5f,%.5f,%.5f],\"min_ground_y\":%.5f,\"max_ground_y\":%.5f,"
                   "\"max_step_down_cm\":%.5f,\"max_step_up_cm\":%.5f,\"blocked\":%d,\"hit_kind\":%d,"
                   "\"hit_part\":%d,\"hit_distance_cm\":%.5f,\"hit_normal\":[%.5f,%.5f,%.5f]}",
                   sample?",":"",(double)d->angle_relative_deg,(double)d->distance_cm,
                   (double)d->position[0],(double)d->position[1],(double)d->ground_y,
                   (double)d->normal[0],(double)d->normal[1],(double)d->normal[2],
                   (double)d->min_ground_y,(double)d->max_ground_y,(double)d->max_step_down_cm,
                   (double)d->max_step_up_cm,d->blocked,d->hit_kind,d->hit_part,
                   (double)d->hit_distance_cm,(double)d->hit_normal[0],(double)d->hit_normal[1],(double)d->hit_normal[2]);
        }
        printf("]},\"objectives\":[");
    }
    if (table >= 0 && table < g_sim.logic.table_count) {
        for (i = 0; i < g_sim.logic.tables[table].node_count; i++) {
            if(i)putchar(',');
            playthrough_node(table,i);
        }
    }
    printf("],\"scene\":");playthrough_string(g_sim.scene);
    printf(",\"player_table\":%d,\"tables\":[",table);
    for(i=0;i<g_sim.logic.table_count;i++) {
        int node;const mtbl_table *t=&g_sim.logic.tables[i];
        printf("%s{\"id\":%d,\"authored_index\":%d,\"result\":%d,\"nodes\":[",
               i?",":"",i,t->index,g_sim.logic.result[i]);
        for(node=0;node<t->node_count;node++){if(node)putchar(',');playthrough_node(i,node);}
        printf("]}");
    }
    puts("]}"); fflush(stdout);
}

static void playthrough_keys(char *names, int held)
{
    char *p = names;
    while (p && *p && strcmp(p, "-")) {
        char *comma = strchr(p, ',');
        SDL_Keycode code;
        SDL_Scancode scan;
        int mods;
        if (comma) *comma = 0;
        code = test_keycode(p, &mods); scan = SDL_GetScancodeFromKey(code, NULL);
        if (scan > SDL_SCANCODE_UNKNOWN && scan < SDL_SCANCODE_COUNT) {
            if (held) {
                playthrough_held[scan] = true;
                if (mods & SDL_KMOD_LCTRL) playthrough_held[SDL_SCANCODE_LCTRL] = true;
                if (mods & SDL_KMOD_LSHIFT) playthrough_held[SDL_SCANCODE_LSHIFT] = true;
                if (mods & SDL_KMOD_LALT) playthrough_held[SDL_SCANCODE_LALT] = true;
            } else {
                SDL_Event event;
                memset(&event, 0, sizeof event);
                event.type = SDL_EVENT_KEY_DOWN; event.key.down = true;
                event.key.key = code; event.key.scancode = scan; event.key.mod = (SDL_Keymod)mods;
                SDL_PushEvent(&event);
            }
        }
        p = comma ? comma + 1 : NULL;
    }
}

static void playthrough_tick(void)
{
    static int remaining;
    char input[2048], press[512], hold[512];
    int frames, dx, dy;
    SDL_Event event;
    if (!playthrough_enabled() || !g_sim_ok || !g_world) return;
    if (remaining-- > 0) return;
    playthrough_observe();
    if (!fgets(input, sizeof input, stdin) ||
        sscanf(input, "frames=%d press=%511s hold=%511s mouse=%d,%d", &frames, press, hold, &dx, &dy) != 5 ||
        frames < 1 || frames > 60 || abs(dx) > 2048 || abs(dy) > 2048) {
        fprintf(stderr, "playthrough: driver ended or supplied invalid input\n");
        memset(&event, 0, sizeof event); event.type = SDL_EVENT_QUIT; SDL_PushEvent(&event);
        return;
    }
    remaining = frames - 1;
    memset(playthrough_held, 0, sizeof playthrough_held);
    playthrough_keys(press, 0); playthrough_keys(hold, 1);
    if (dx || dy) {
        memset(&event, 0, sizeof event); event.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.xrel = (float)dx; event.motion.yrel = (float)dy; SDL_PushEvent(&event);
    }
}
#endif
