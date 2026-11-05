/* Compare the real native consumers with executed original Mercs fixtures,
 * then exercise their integration through the native simulation/effect paths. */
#include "../src/msim.c"
#include <assert.h>
#include "../tests/fixtures/combat-completion/original-special.h"
#include "../tests/fixtures/combat-completion/original-debris.h"
#include "../tests/fixtures/combat-completion/original-fx-hold.h"
#include "../tests/fixtures/combat-completion/original-wreck.h"

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
typedef struct { unsigned roll[2]; int at, count, released, result; } rolls;
static unsigned draw(void *opaque, unsigned bound)
{
    rolls *r = opaque;
    return bound && r->at < r->count ? r->roll[r->at++] % bound : 0;
}
static void released(void *opaque, int slot, int result)
{
    rolls *r = opaque; assert(slot == 0); r->released++; r->result = result;
}
static void unit(combat_unit *u)
{
    int i;
    memset(u, 0, sizeof *u); u->human = 1; u->armor_mult = u->speed_gain = 1;
    for (i = 0; i < 8; i++) u->armor[i] = u->internal[i] = 100;
    u->lock_target = -2;
}
static msim *scene(void)
{
    msim *s = calloc(1, sizeof *s);
    assert(s); unit(&s->player_unit);
    s->actor_count = 1;
    s->actors = calloc(1, sizeof *s->actors); s->units = calloc(1, sizeof *s->units);
    s->armed = calloc(1, sizeof *s->armed); s->height = calloc(1, sizeof *s->height);
    s->centre_h = calloc(1, sizeof *s->centre_h); s->radius = calloc(1, sizeof *s->radius);
    assert(s->actors && s->units && s->armed && s->height && s->centre_h && s->radius);
    unit(s->units); s->units[0].human = 0; s->armed[0] = 1; s->height[0] = 950; s->radius[0] = 60;
    s->actors[0].friendly = 1; s->actors[0].alliance = 0; s->actors[0].table = 1;
    s->player_target = s->player_lock_target = s->player_sel_w = -1;
    s->player_aim_free = 1; s->player_radius = 60; s->rng = 1;
    s->no_player_muzzle = 1;
    return s;
}
static void free_scene(msim *s)
{
    /* Only fields explicitly allocated by this synthetic fixture. */
    free(s->actors[0].mech.parts); free(s->actors); free(s->units);
    free(s->armed); free(s->height); free(s->centre_h); free(s->radius); free(s);
}

static void original_consumers(void)
{
    int i, j;
    combat_set_weapon_timing(0);
    for (i = 0; i < COUNT(original_ams_toggle); i++) {
        const original_ams_toggle_case *f = &original_ams_toggle[i];
        combat_unit u; unit(&u); u.weapon_count = 1;
        u.weapons[0].weapon = f->weapon; u.weapons[0].state = f->state;
        assert(combat_toggle_ams(&u) == f->want_toggle);
        assert(u.weapons[0].state == f->want_state);
    }
    for (i = 0; i < COUNT(original_ams_pre); i++) {
        const original_ams_pre_case *f = &original_ams_pre[i];
        combat_unit u; rolls r = {{0}, 0, 0, 0, 0}; unit(&u); u.weapon_count = 1;
        u.weapons[0].weapon = 29; u.weapons[0].state = f->state; u.weapons[0].ammo = 20;
        combat_ams_step(&u, 0, 0, combat_ams_threat(f->kind, f->enemy, f->active, f->delta, f->velocity), draw, &r, released);
        assert(u.weapons[0].state == f->want_state && u.weapons[0].shots == f->want_shots);
    }
    for (i = 0; i < COUNT(original_ams_consume); i++) {
        const original_ams_consume_case *f = &original_ams_consume[i];
        combat_unit u; rolls r = {{(unsigned)f->roll,0}, 0, 1, 0, 0}; unit(&u);
        u.weapon_count = 1; u.bin_count = 2; u.unlimited_ammo = f->unlimited;
        u.ammo_pool_count = 1; u.ammo_pools[0].weapon = 29;
        u.ammo_pools[0].rounds = f->ammo; u.ammo_pools[0].bin_n = 2;
        u.weapons[0].weapon = 29; u.weapons[0].state = CW_FIRING; u.weapons[0].shots = f->shots;
        u.weapons[0].ammo = f->ammo; u.weapons[0].bin_n = 2; u.bins[0].rounds = f->bin_rounds;
        combat_ams_step(&u, 0, (float)combat_weapon_interval(29), 0, draw, &r, released);
        assert(r.result == f->result && r.released == (f->ammo != 0));
        assert(u.weapons[0].ammo == f->want_ammo && u.bins[0].rounds == f->want_bin);
        assert(u.weapons[0].bin_cur == f->want_cur && u.weapons[0].state == f->want_state);
        assert(u.ammo_pools[0].rounds == f->want_ammo && u.ammo_pools[0].bin_cur == f->want_cur);
        assert(u.weapons[0].pulses_due == 0); /* AMS never enters the ordinary projectile queue. */
    }
    for (i = 0; i < COUNT(original_ams_intercept); i++) {
        const original_ams_intercept_case *f = &original_ams_intercept[i];
        rolls r = {{(unsigned)f->rolls[0],(unsigned)f->rolls[1]}, 0, f->roll_n, 0, 0};
        int distance = f->threshold;
        assert(combat_ams_intercept(f->kind, f->enemy, f->active, f->delta, f->velocity, &distance, draw, &r) == f->want_prox);
        assert(distance == f->want_threshold);
    }
    for (i = 0; i < COUNT(original_guidance); i++) {
        const original_guidance_case *f = &original_guidance[i];
        int32_t a[3]; memcpy(a, f->accel, sizeof a);
        combat_guidance(f->delta, f->velocity, a, f->ticks, f->torpedo, f->dos);
        for (j = 0; j < 3; j++) assert(a[j] == f->want[j]);
    }
    for (i = 0; i < COUNT(original_attach); i++) {
        const original_attach_case *f = &original_attach[i];
        fx_defs *defs = calloc(1, sizeof *defs); fx_world *w = calloc(1, sizeof *w);
        float p[3] = {123,456,789}; int at;
        assert(defs && w); defs->slots[10] = 1; fx_set_timing(f->dos);
        at = fx_spawn(w, defs, 10, p, 0, 6); assert(at >= 0);
        if (f->attached) w->e[at].remaining = fx_initial_life(10, 1, 6);
        assert(w->e[at].remaining == f->life);
        free(w); free(defs);
    }
    for (i = 0; i < COUNT(original_chunks); i++) {
        assert(fx_impact_type(11, original_chunks[i].flags) == -1);
        assert(fx_impact_chunks(11, original_chunks[i].flags) == original_chunks[i].count);
    }
    for (i = 0; i < COUNT(original_thing); i++) {
        const original_thing_case *f = &original_thing[i];
        if (!f->gone && f->position[0] < f->radius + f->object_radius && (f->flags & 0xf0) != 0xb0)
            assert(100 - fx_thing_damage(f->coefficient, f->ticks) == f->hp);
    }
    for (i = 0; i < COUNT(original_fx_animation); i++) {
        const original_fx_animation_case *f = &original_fx_animation[i];
        fx_defs *defs = calloc(1, sizeof *defs); fx_world *world = calloc(1, sizeof *world);
        fx_live *e;
        assert(defs && world); e = &world->e[0]; e->alive = 1;
        defs->tex[0].frames = f->frames; defs->tex[0].rate = f->rate;
        world->clocks_ready = 1; world->definitions = defs;
        world->clocks[0].frame = f->frame; world->clocks[0].mode = f->mode; world->clocks[0].stamp = f->stamp;
        world->clocks[0].state = 1; world->clocks[0].rate = f->rate;
        for (j = 0; j < f->frames; j++) defs->tex[0].cel[j] = j + 1;
        fx_animate(world, defs, f->now);
        assert(world->clocks[0].frame == f->want_frame && world->clocks[0].mode == f->want_mode && world->clocks[0].stamp == f->want_stamp);
        assert(fx_frame(world, defs, e) == (f->want_mode ? f->want_frame + 1 : -1));
        free(world); free(defs);
    }
    for (i = 0; i < COUNT(original_fx_local_light); i++) assert(fx_local_light(i) == original_fx_local_light[i]);
    puts("PASS: original AMS states/release/interception, DOS/Windows guidance, burn lifetime, slug counts and area rounding");
    puts("PASS: original DOS/Windows texture animation decisions and all36 local-light producer branches");
}

static void ammo_and_narc(void)
{
    int i;
    combat_unit u; unit(&u); u.weapon_count = 2; u.bin_count = 2;
    u.ammo_pool_count = 1; u.ammo_pools[0].weapon = 29;
    u.ammo_pools[0].rounds = 12; u.ammo_pools[0].bin_n = 2;
    for (i = 0; i < 2; i++) {
        u.weapons[i].weapon = 29; u.weapons[i].ammo = 12; u.weapons[i].state = CW_READY;
        u.bins[i].weapon = 29; u.bins[i].rounds = i ? 5 : 7;
    }
    assert(combat_jettison(&u, 0) == 1);
    for (i = 0; i < 2; i++) assert(u.weapons[i].ammo == 0 && u.weapons[i].state == -1 && u.bins[i].rounds == 0);
    assert(u.ammo_pools[0].rounds == 0);
    combat_damage_live = 0; u.invulnerable = 1; u.narc_beacon = 0;
    combat_hit(&u, 28, 3, 0, &u.rng); assert(u.narc_beacon == 1);
    combat_damage_live = 1;
    for (i = 0; i < COUNT(original_narc_lock); i++) {
        const original_narc_lock_case *f = &original_narc_lock[i];
        msim *s = scene();
        s->player_unit.weapon_count = 1; s->player_unit.weapons[0].weapon = f->weapon;
        s->player_unit.weapons[0].state = CW_READY; s->player_unit.weapons[0].ammo = -1;
        s->actors[0].mech.origin[0] = 700000; s->units[0].narc_beacon = f->beacon;
        s->player_lock_target = 0; s->player_sel_w = 0; s->player_lock_ticks = 123;
        s->fx_delta_ticks = 6;
        combat_step(s, 6.0f / 182.0f);
        assert(s->player_lock == f->flags && (int)s->player_lock_ticks == f->timer);
        free_scene(s);
    }
    puts("PASS: jettison returns empty linked bins; NARC survives damage gating and immediately locks outside the normal aim/range window");
}

static void attachments_and_blasts(void)
{
    int type;
    msim *s = scene(); const float impact[3] = {0,0,0};
    mech3d_part *p = calloc(1, sizeof *p); assert(p);
    s->actors[0].mech.parts = p; s->actors[0].mech.part_count = 1;
    p->node = 17; p->pos[0] = 3000; s->actors[0].mech.origin[0] = 3000;
    s->player[0] = -10000; s->fx_radius[10] = 40; s->fx_delta_ticks = 6; s->splash = 1;
    fx_set_timing(0);
    effect_source(s, 10, impact, 0, -1, 0, 17);
    assert(s->area_count == 1 && s->area[0].attached && s->area[0].remaining == 911);
    assert(s->area[0].pos[0] == 3000 && s->area[0].owner == -1);
    p->pos[0] = 5000; s->actors[0].mech.origin[0] = 5000;
    area_damage(s, 6);
    assert(s->area[0].pos[0] == 5000 && s->area[0].remaining == 905);
    assert(s->units[0].heat == 6144.0f / 65536.0f && s->player_unit.heat == 0);
    s->debris_count = 1; s->debris[0].kind = 2; s->debris[0].actor = 0; s->debris[0].part = 0; s->debris[0].p[0] = 8000;
    area_damage(s, 6); assert(s->area[0].pos[0] == 8000);
    s->area_count = 0; effect_source(s, 10, impact, 0, -1, -2, -1);
    assert(s->area[0].remaining == 187 && !s->area[0].attached);
    area_damage(s, 187); assert(s->area_count == 0);
    free_scene(s);
    for (type = 0; type < 2; type++) {
        msim_shot shot = {0}; s = scene(); shot.weapon = type ? 34 : 30; shot.owner = -1;
        s->actors[0].mech.origin[0] = 10000; s->fx_delta_ticks = 6; s->splash = 0;
        s->fx_radius[type ? 35 : 22] = 40;
        projectile_impact(s, &shot, impact, FX_HIT_UNIT, -2, -1);
        assert(s->area_count == 1 && s->area[0].type == (type ? 35 : 22));
        if (type) {
            int health = combat_health(&s->player_unit);
            area_damage(s, 6); assert(combat_health(&s->player_unit) == health); /* signed initial radius */
            area_damage(s, 6); assert(combat_health(&s->player_unit) < health);
        } else {
            area_damage(s, 6); assert(s->player_unit.heat == 6144.0f / 65536.0f);
            assert(combat_health(&s->player_unit) < 1600);
        }
        free_scene(s);
    }
    puts("PASS: Inferno/flamer burns follow live and detached nodes; short fallback expires; Arrow IV/nuke retain their original special splash behavior");
}

static void things(void)
{
    int i;
    for (i = 0; i < COUNT(original_thing); i++) {
        const original_thing_case *f = &original_thing[i];
        msim *s; mech3d world = {0}; mech3d_part part = {0}; wtb_object object = {0}; wtb_vertex verts[2] = {{0},{0}};
        int j, type = f->coefficient == 128 ? 10 : f->coefficient == 8192 ? 22 : -1;
        if (type < 0) continue; /* synthetic coefficients above are checked at the arithmetic boundary */
        s = scene(); s->world = &world; world.parts = &part; world.part_count = 1;
        part.model.objects = &object; part.model.object_count = 1; part.parent_obj = -1;
        object.verts = verts; object.vert_count = 2; verts[0].x = -f->object_radius; verts[1].x = f->object_radius;
        s->bld = calloc(1, sizeof *s->bld); assert(s->bld); s->bld_count = 1;
        s->bld[0].hp = 100; s->bld[0].destroyed = -1; s->bld[0].type = f->flags; s->bld[0].flags = f->gone;
        for (j = 0; j < 3; j++) s->bld[0].mn[j] = s->bld[0].mx[j] = (float)f->position[j];
        s->area_count = 1; s->area[0].type = type; s->area[0].r = (float)f->radius;
        s->area[0].remaining = s->area[0].duration = fx_duration(type); s->area[0].owner = -1;
        s->splash = 0; s->player[0] = -10000; s->actors[0].mech.origin[0] = -10000;
        area_damage(s, f->ticks);
        assert(s->bld[0].hp == f->hp);
        if (!f->hp) assert(part.hidden && (s->bld[0].flags & 4) && s->buildings_destroyed == 1);
        free(s->bld); free_scene(s);
    }
    puts("PASS: non-unit effects run with splash off, preserve rounded integer damage and destroy class0xb0 on a zero-point dispatched hit");
}

static void ams_integration(void)
{
    msim *s = scene(); msim_shot *shot = &s->shots[0];
    s->pl_mech_ok = 1; s->actors[0].alliance = 1;
    s->player_unit.weapon_count = 1; s->player_unit.weapons[0].weapon = 29;
    s->player_unit.weapons[0].state = CW_READY; s->player_unit.weapons[0].ammo = 20;
    s->player_unit.weapons[0].bin_n = 1; s->player_unit.bin_count = 1; s->player_unit.bins[0].rounds = 20;
    s->player_unit.ammo_pool_count = 1; s->player_unit.ammo_pools[0].weapon = 29;
    s->player_unit.ammo_pools[0].rounds = 20; s->player_unit.ammo_pools[0].bin_n = 1;
    s->shot_count = 1; shot->weapon = 0; shot->owner = 0; shot->x = 10000; shot->vx = -100;
    shot->life = 100; shot->ams_distance = -1; shot->homing = -1;
    ams_update(s, 11);
    assert(s->shot_count == 1 && s->player_unit.weapons[0].pulses_due == 0);
    assert(s->player_unit.weapons[0].ammo < 20 && s->player_unit.bins[0].rounds == 19);
    assert(s->player_unit.ammo_pools[0].rounds == s->player_unit.weapons[0].ammo);
    assert(shot->ams_distance >= 0);
    s->player_unit.weapon_count = 0; shot->prox = 1; shot->y = 1000;
    {
        int health = combat_health(&s->player_unit);
        combat_step(s, 1.0f / 182.0f);
        assert(s->shot_count == 0 && combat_health(&s->player_unit) < health); /* executed original forced-target quirk */
    }
    s->shot_count = 1; memset(shot, 0, sizeof *shot); shot->weapon = 0; shot->owner = 0;
    shot->x = 10000; shot->y = 1000; shot->prox = 1; shot->life = 100; shot->homing = -2;
    {
        int health = combat_health(&s->player_unit);
        combat_step(s, 1.0f / 182.0f);
        assert(s->shot_count == 0 && combat_health(&s->player_unit) == health);
    }
    free_scene(s);
    puts("PASS: live AMS uses the existing missile pool and original forced-hit classification without spawning ordinary bullets");
}

static int debris_explosion_count;
static void debris_effect(void *opaque, int type, const float point[3])
{
    (void)opaque; (void)point;
    assert(type == 7); debris_explosion_count++;
}

static void debris_consumers(void)
{
    int i, j;
    for (i = 0; i < COUNT(original_debris_areas); i++) {
        const original_debris_area_case *f = &original_debris_areas[i];
        msim *s = scene(); mech3d world = {0}; mech3d_part part = {0};
        wtb_object model = {0}; wtb_vertex verts[2] = {{0},{0}};
        const float center[3] = {0,0,0};
        world.parts = &part; world.part_count = f->has_object;
        part.model.objects = &model; part.model.object_count = 1;
        model.verts = verts; model.vert_count = 2;
        verts[0].x = -f->object_radius; verts[1].x = f->object_radius;
        s->world = &world; s->on_effect = debris_effect; debris_explosion_count = 0;
        s->debris_count = 1; s->debris[0].kind = 3;
        s->debris[0].destructible = f->active; s->debris[0].health_q16 = f->health;
        for (j = 0; j < 3; j++) s->debris[0].p[j] = (float)f->position[j];
        debris_area_damage(s, center, f->radius, f->coefficient, f->ticks);
        assert(debris_explosion_count == f->want_destroyed);
        assert(s->debris_count == !f->want_destroyed);
        if (s->debris_count) assert(s->debris[0].health_q16 == f->want_health);
        free_scene(s);
    }
    for (i = 0; i < COUNT(original_debris_direct); i++) {
        const original_debris_direct_case *f = &original_debris_direct[i];
        msim *s = scene();
        s->debris_count = 1; s->debris[0].kind = 2; s->debris[0].destructible = 1;
        s->debris[0].health_q16 = f->health;
        s->on_effect = debris_effect; debris_explosion_count = 0;
        debris_damage(s, 0, f->damage);
        assert(debris_explosion_count == f->want_destroyed && s->debris_count == !f->want_destroyed);
        if (s->debris_count) assert(s->debris[0].health_q16 == f->want_health);
        free_scene(s);
    }
    for (i = 0; i < COUNT(original_debris_registration); i++) {
        const original_debris_registration_case *f = &original_debris_registration[i];
        mech3d_part p = {0};
        p.objtype = f->type; p.group = f->group; p.objl = f->group != 0; p.model.object_count = f->model;
        /* Allocation-failure and registry-full are service boundaries here;
         * the model/type/location guard is the original branch under test. */
        if (f->physics >= 0 && !f->full)
            assert(debris_model_eligible(f->node ? &p : NULL, f->location) == f->registered);
    }
    for (i = 0; i < COUNT(original_debris_expiry); i++) {
        const original_debris_expiry_case *f = &original_debris_expiry[i];
        msim *s = scene(); int now;
        fx_set_timing(f->dos); s->now = ((500 + f->elapsed) * 1000 + 181) / 182;
        now = (int)((int64_t)s->now * 182 / 1000); assert(now == 500 + f->elapsed);
        s->debris_count = 1; s->debris[0].destructible = 1;
        s->debris[0].until = 500 + fx_debris_duration(); s->player_lost = f->retained;
        debris_step(s, 0); assert(s->debris_count == f->active);
        free_scene(s);
    }
    fx_set_timing(0);
    puts("PASS: original detached-object health, rounded area quirk, tangent/large-distance cases, eligibility and DOS/Windows expiry boundaries");
}

static void debris_integration(void)
{
    msim *s = scene(); mech3d world = {0}; mech3d_part part = {0}, drawn;
    wtb_object model = {0}; wtb_vertex verts[2] = {{0},{0}};
    float ray[3] = {0,0,0}, step[3] = {2000,0,0}, fraction = 2;
    int index = -1;
    world.parts = &part; world.part_count = 1; part.model.objects = &model;
    part.model.object_count = 1; model.verts = verts; model.vert_count = 2;
    verts[0].x = -20; verts[1].x = 20; part.coll = 6; part.yaw = 90; part.hidden = 1;
    s->world = &world; s->debris_count = 1;
    s->debris[0].kind = 3; s->debris[0].destructible = 1; s->debris[0].p[0] = 1000;
    s->debris[0].health_q16 = 0x100000; debris_capture_rotation(&s->debris[0], &part);
    part.yaw = 180; /* later source pose cannot rotate a detached piece */
    drawn = part; msim_debris_transform(&s->debris[0], &drawn);
    assert(fabsf(drawn.rot[0]) < 0.00001f && fabsf(drawn.rot[2] - 1) < 0.00001f);
    assert(drawn.pos[0] == 1000 && drawn.objtype == 0x50 && !drawn.hidden);
    assert(shot_debris(s, ray, step, &fraction, &index) && index == 0);
    assert(fabsf(fraction - 0.49f) < 0.00001f);
    /* One missile-sized direct hit leaves16HP; the exact lethal boundary
     * survives until one more point is delivered, as the original does. */
    debris_damage(s, 0, 16 * 65536); assert(s->debris_count == 1 && s->debris[0].health_q16 == 0);
    s->on_effect = debris_effect; debris_explosion_count = 0;
    debris_damage(s, 0, 65536); assert(!s->debris_count && debris_explosion_count == 1);
    fraction = 2; index = -1; assert(!shot_debris(s, ray, step, &fraction, &index));
    /* Generic CHUNKER objects have no destructible registration. */
    s->debris_count = 1; memset(&s->debris[0], 0, sizeof s->debris[0]);
    debris_damage(s, 0, 100 * 65536); assert(s->debris_count == 1);
    free_scene(s);
    puts("PASS: detached world debris shares its captured render/collision transform; destroyed debris leaves the collision pool and generic chunks remain distinct");
}

static int wreck_sounds;
static void wreck_sound(void *opaque, int sound, const float point[3])
{
    (void)opaque; assert(sound == 0xa9 && point); wreck_sounds++;
}
static void wreck_consumers(void)
{
    int i, j;
    for (i = 0; i < COUNT(original_wreck_contacts); i++) {
        const original_wreck_contact_case *f = &original_wreck_contacts[i];
        msim *s = scene();
        float p0[3] = {600,0,0}, p1[3] = {(float)f->x,0,0}, n[3], c[3], radius, velocity[3];
        int hit;
        s->player_radius = s->radius[0] = 100;
        s->units[0].destroyed = f->state == 4 || f->state == 5;
        s->units[0].intact_out = f->state == 5;
        s->units[0].body_detached = (f->flags & 0x100) != 0;
        s->actors[0].unit_flags = (unsigned)f->object_flags;
        s->on_sound = wreck_sound; wreck_sounds = 0;
        hit = f->reciprocal ? (msim_unit_inside(s, -1, 0, p1, n, c, &radius, velocity) ? 0 : -2) :
              msim_unit_contact(s, -1, p0, p1, n, c, &radius, velocity);
        assert((hit != -2) == f->contact && wreck_sounds == f->breakup);
        assert(s->units[0].body_detached == ((f->flags & 0x100) != 0 || f->breakup));
        free_scene(s);
    }
    {
        msim *s = scene(); mech3d *m = &s->actors[0].mech;
        wtb_object objects[4];
        float point[3], ray[3] = {-1000,0,0}, dv[3] = {2000,0,0}, nearest = 2;
        int actor = -2, group = 0;
        unsigned removed[2] = {0,0};
        memset(objects, 0, sizeof objects);
        m->part_count = 4; m->parts = calloc(4, sizeof *m->parts); assert(m->parts);
        for (j = 0; j < 4; j++) {
            mech3d_part *p = &m->parts[j];
            p->model.object_count = 1; p->model.objects = &objects[j];
            p->objtype = 0x110; p->objl = 1; p->group = j + 3; p->node = j + 10;
            p->pos[0] = j * 100; p->pos[1] = 50;
            p->sphere_ok = 1; p->hull_radius = 60;
        }
        m->parts[1].objtype = 0x170; /* original non-debris class */
        m->parts[2].objl = 0;       /* inherited native group is not its own OBJL */
        s->units[0].destroyed = 1; s->on_sound = wreck_sound; wreck_sounds = 0;
        s->fly_seen = removed; s->player[0] = 10000;
        assert(msim_effect_attach_point(s, 0, 10, point));
        blow_off(s, 0, 1u << 3, 0); removed[1] = 1u << 3;
        assert(s->debris_count == 1 && s->debris[0].part == 0);
        assert(msim_break_wreck(s, 0) == 1 && s->units[0].body_detached);
        assert(s->debris_count == 2 && s->debris[1].part == 3 && wreck_sounds == 1);
        assert(s->debris[1].health_q16 == 16 * 65536 && s->debris[1].destructible);
        assert(msim_break_wreck(s, 0) == 0 && s->debris_count == 2 && wreck_sounds == 1);
        blow_offs(s); assert(s->debris_count == 2); /* no second node registration */
        s->debris[1].p[0] = 1234;
        assert(msim_effect_attach_point(s, 0, 13, point) && point[0] == 1234);
        shot_units_part(s, -1, ray, dv, &nearest, &actor, &group, NULL);
        assert(actor == -2 && nearest == 2); /* empty controller no longer has a hull */
        s->debris_count = 0;
        assert(!msim_effect_attach_point(s, 0, 13, point)); /* no reattachment to source copy */
        s->units[0].body_detached = 0; s->debris_count = MSIM_MAX_DEBRIS;
        assert(msim_break_wreck(s, 0) == 1 && s->debris_count == MSIM_MAX_DEBRIS);
        assert(wreck_sounds == 2); /* original full pool removes body anyway */
        s->fly_seen = NULL; free_scene(s);
    }
    for (i = 0; i < 3; i++) {
        msim *s = scene();
        s->player_vertical = 1; s->player_unit.destroyed = 1;
        s->player_unit.intact_out = i == 2;
        s->player_unit.y = 1; s->player_unit.vy = i ? -10 : -1;
        msim_step(s, 40);
        assert(s->player_unit.body_detached == (i == 1));
        if (s->player_unit.body_detached) {
            float y = s->player_unit.y;
            s->player_unit.vy = 100;
            combat_jets(&s->player_unit, 0, 1, &s->rng);
            assert(s->player_unit.y == y && s->player_unit.vy == 0);
        }
        free_scene(s);
    }
    for (i = 0; i < 3; i++) {
        /* Drive the real airborne state4 movement branch: a living unit,
         * a world box, then an unobstructed hard landing. */
        msim *s = scene();
        mech3d world = {0}; mech3d_part part = {0}; wtb_object object = {0};
        wtb_vertex vertices[8];
        s->minds = calloc(1, sizeof *s->minds); assert(s->minds); s->ai_ok = 1;
        s->units[0].destroyed = 1; s->units[0].y = i == 2 ? 2 : 1000;
        s->units[0].vy = i == 2 ? -10 : 0;
        s->actors[0].mech.heading = 90; s->actors[0].mech.origin[1] = (int32_t)s->units[0].y;
        s->minds[0].speed = i == 2 ? 0 : 2000;
        s->player[0] = i == 0 ? 180 : 10000; s->player_unit.y = 1000;
        if (i == 1) {
            memset(vertices, 0, sizeof vertices);
            for (j = 0; j < 8; j++) {
                vertices[j].x = j & 1 ? 200 : 100;
                vertices[j].y = j & 2 ? 1200 : 800;
                vertices[j].z = j & 4 ? 100 : -100;
            }
            object.verts = vertices; object.vert_count = 8;
            part.model.objects = &object; part.model.object_count = 1; part.coll = 1;
            world.parts = &part; world.part_count = 1;
            msim_set_world(s, &world);
        }
        msim_step(s, 40);
        if (!s->units[0].body_detached || s->player_unit.body_detached) fprintf(stderr,"airborne wreck fixture%d body%d player%d origin%d,%d y%.3f ground%.3f speed%.3f\n",i,s->units[0].body_detached,s->player_unit.body_detached,s->actors[0].mech.origin[0],s->actors[0].mech.origin[1],s->units[0].y,s->units[0].ground,s->minds[0].speed);
        assert(s->units[0].body_detached && !s->player_unit.body_detached);
        assert(s->actors[0].mech.origin[0] == 0 && s->minds[0].speed == 0);
        msim_free(s); free(s->actors); free(s);
    }
    puts("PASS: original wreck contact/reciprocal/exclusion, one-time breakup, remaining-node registry, full-pool removal, attachment teardown and hard-landing integration");
}

static void blast_lifetime(void)
{
    int i;
    for (i = 0; i < COUNT(original_fx_holds); i++) {
        const original_fx_hold_case *f = &original_fx_holds[i];
        fx_world w = {0};
        w.e[0].alive = 1; w.e[0].remaining = f->remaining; w.e[0].light_marked = f->marked;
        w.light_slot = f->active ? f->owner + 1 : 0; w.palette_left = f->fade;
        fx_update(&w, f->ticks);
        assert(w.e[0].remaining == f->want_remaining && w.e[0].alive == f->alive);
        assert((w.light_slot != 0) == f->light_active);
    }
    for (i = 0; i < COUNT(original_fx_palettes); i++) {
        const original_fx_palette_case *f = &original_fx_palettes[i];
        fx_world w = {0};
        w.palette_left = f->left; w.palette_back = f->back;
        assert(fx_palette_begin(&w, f->duration, f->mode, f->ticks) == f->result);
        assert(w.palette_left == f->want_left && w.palette_back == f->want_back);
    }
    for (i = 0; i < COUNT(original_fx_palette_steps); i++) {
        const original_fx_palette_step_case *f = &original_fx_palette_steps[i];
        fx_world w = {0};
        w.palette_left = f->left; w.palette_back = f->back;
        fx_palette_step(&w, f->ticks);
        assert(w.palette_left == f->want_left && w.palette_back == f->want_back);
    }
    for (i = 0; i < COUNT(original_fx_light_offers); i++) {
        const original_fx_light_offer_case *f = &original_fx_light_offers[i];
        fx_world w = {0}; float camera[3]; int j;
        w.e[0].alive = 1; w.e[0].type = f->type;
        w.e[0].pos[0] = 123; w.e[0].pos[1] = 456; w.e[0].pos[2] = 789;
        w.light_slot = f->held ? 2 : 0;
        for (j = 0; j < 3; j++) { camera[j] = (float)f->camera[j]; w.light_pos[j] = (float)f->prior[j]; }
        assert(fx_light_offer(&w, 0, camera, f->allowed, 1, 6) == f->accepted);
    }
    puts("PASS: executed original blast hold/release, palette frame count and return-phase quirk, new-light distance arbitration");
}

static int hold_visual(void *opaque, int instance, unsigned generation)
{
    return fx_retains(opaque, instance, generation);
}
static void blast_lifetime_integration(void)
{
    msim *s = scene(); fx_world w = {0}; fx_defs d = {0};
    const float point[3] = {0,0,0}, camera[3] = {0,0,-10000};
    int slot, health;
    float after;
    d.slots[3] = 4; d.anim[3] = -1; fx_set_timing(0);
    slot = fx_spawn(&w, &d, 3, point, 1, 6); assert(slot >= 0);
    assert(fx_light_offer(&w, slot, camera, 1, 1, 6));
    w.e[slot].remaining = 1; w.palette_left = 2; w.palette_back = 0;
    s->actors[0].mech.origin[0] = 10000; s->splash = 1; s->fx_radius[3] = 100;
    effect_source(s, 3, point, 0, -1, -2, -1);
    assert(s->area_count == 1);
    s->area[0].instance = slot; s->area[0].generation = w.e[slot].serial; s->area[0].remaining = 1;
    s->on_effect_retains = hold_visual; s->shot_user = &w;
    health = combat_health(&s->player_unit);
    area_damage(s, 6); fx_update(&w, 6);
    assert(s->area_count == 1 && s->area[0].remaining == -5 && w.e[slot].remaining == -5);
    assert(combat_health(&s->player_unit) < health); after = s->player_unit.armor[0];
    fx_palette_step(&w, 6); area_damage(s, 6); fx_update(&w, 6);
    assert(s->area_count == 1 && w.e[slot].alive && s->player_unit.armor[0] < after);
    fx_palette_step(&w, 6); assert(!w.palette_left);
    area_damage(s, 6); fx_update(&w, 6);
    assert(!s->area_count && !w.e[slot].alive && !w.light_slot);
    /* Reusing the pool entry must never let a new fade retain old damage. */
    {
        unsigned old_generation = w.e[slot].serial;
        int reused = fx_spawn(&w, &d, 3, point, 10, 6);
        assert(reused == slot && w.e[slot].serial != old_generation);
        assert(fx_light_offer(&w, slot, camera, 1, 1, 6));
        assert(!fx_retains(&w, slot, old_generation) && fx_retains(&w, slot, w.e[slot].serial));
    }
    free_scene(s);
    puts("PASS: palette-held blasts keep applying area damage, release visual/damage together, and reject stale recycled-slot identities");
}

int main(void)
{
    original_consumers(); ammo_and_narc(); attachments_and_blasts(); things(); ams_integration();
    debris_consumers(); debris_integration(); wreck_consumers();
    blast_lifetime(); blast_lifetime_integration();
    puts("PASS: Mercs special-weapon and effect integration");
    return 0;
}
