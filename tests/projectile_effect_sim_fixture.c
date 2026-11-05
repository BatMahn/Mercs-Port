/* Test-only access to actual private simulation code. No collision, damage,
 * FX admission or projectile integration result is intercepted. */
#include "src/msim.c"
#include "projectile_effect_fixture.h"
#include <assert.h>

static void fixture_unit_base(combat_unit *u)
{
    int k; memset(u, 0, sizeof *u); u->human=1;
    u->armor_mult=u->speed_gain=1; u->lock_target=-2;
    for(k=0;k<8;k++)u->armor[k]=u->internal[k]=1000;
}

void effect_fixture_base(msim *s)
{
    int k; memset(s,0,sizeof *s); fixture_unit_base(&s->player_unit);
    s->player_target=s->player_lock_target=s->player_sel_w=-1;
    s->player_aim_free=1; s->player_radius=60; s->player_centre_h=550;
    s->rng=1; s->no_player_muzzle=1; s->fx_delta_ticks=6; s->fx_light_ok=1;
    for(k=0;k<FX_TYPES;k++)s->fx_radius[k]=40;
}

void effect_fixture_impact(msim *s, int weapon, const float c[3], const float e[3], int flags)
{
    msim_shot shot={0};shot.weapon=weapon;shot.owner=7;
#if MERCS_EFFECT_COORDINATES
    projectile_impact_point(s,&shot,c,flags,-2,-1,e);
#else
    (void)e;projectile_impact(s,&shot,c,flags,-2,-1);
#endif
}

void effect_fixture_segment(msim *s, int weapon, const float c[3], const float from[3], const float to[3], int flags)
{
    msim_shot shot={0};shot.weapon=weapon;shot.owner=7;
#if MERCS_EFFECT_COORDINATES
    float d[3]={to[0]-from[0],to[1]-from[1],to[2]-from[2]};
    projectile_segment_impact(s,&shot,c,from,d,flags,-2,-1);
#else
    (void)from;(void)to;projectile_impact(s,&shot,c,flags,-2,-1);
#endif
}

void effect_fixture_ordinary(msim *s,int type,const float point[3])
{
    effect(s,type,point,0);
}

void effect_fixture_step(msim *s,int ticks)
{
    s->fx_delta_ticks=ticks;combat_step(s,(float)ticks/COMBAT_TICKS_PER_S);
}

void effect_fixture_area(msim *s,int ticks) { area_damage(s,ticks); }

void effect_fixture_shot(msim *s,int weapon,const float from[3],const float velocity[3])
{
    msim_shot *q=&s->shots[0];memset(q,0,sizeof *q);s->shot_count=1;
    q->weapon=weapon;q->owner=-1;q->homing=-2;q->ams_distance=-1;q->life=100;
    q->x=from[0];q->y=from[1];q->z=from[2];q->vx=velocity[0];q->vy=velocity[1];q->vz=velocity[2];
}

void effect_fixture_plane(mech3d *world,int y)
{
    mech3d_part *p;wtb_object *o;int k;
    memset(world,0,sizeof *world);world->part_count=1;world->parts=calloc(1,sizeof *world->parts);assert(world->parts);
    p=world->parts;p->coll=5;p->parent_obj=-1;p->node=-1;
    p->model.object_count=1;p->model.objects=calloc(1,sizeof *p->model.objects);assert(p->model.objects);
    o=p->model.objects;o->vert_count=4;o->verts=calloc(4,sizeof *o->verts);o->poly_count=1;o->polys=calloc(1,sizeof *o->polys);assert(o->verts&&o->polys);
    o->verts[0].x=-10000;o->verts[0].y=y;o->verts[0].z=-10000;
    o->verts[1].x=10000;o->verts[1].y=y;o->verts[1].z=-10000;
    o->verts[2].x=10000;o->verts[2].y=y;o->verts[2].z=10000;
    o->verts[3].x=-10000;o->verts[3].y=y;o->verts[3].z=10000;
    o->polys[0].n=4;for(k=0;k<4;k++)o->polys[0].idx[k]=(uint16_t)k;
}

void effect_fixture_gt(msim *s,mech3d *world)
{
    terrain_t *t;msim_set_world(s,world);t=s->terrain;
    s->bld_count=1;s->bld=calloc(1,sizeof *s->bld);assert(s->bld);
    s->bld[0].intact=0;s->bld[0].destroyed=-1;s->bld[0].hp=10000;
    if(t&&t->thing_of)t->thing_of[0]=0;
}

void effect_fixture_unit(msim *s)
{
    mech3d_part *p;
    s->actor_count=1;s->actors=calloc(1,sizeof *s->actors);s->units=calloc(1,sizeof *s->units);
    s->armed=calloc(1,sizeof *s->armed);s->height=calloc(1,sizeof *s->height);
    s->centre_h=calloc(1,sizeof *s->centre_h);s->radius=calloc(1,sizeof *s->radius);
    assert(s->actors&&s->units&&s->armed&&s->height&&s->centre_h&&s->radius);
    fixture_unit_base(s->units);s->units[0].human=0;s->actors[0].friendly=1;s->actors[0].alliance=0;
    s->armed[0]=1;s->height[0]=950;s->centre_h[0]=550;s->radius[0]=100;
    s->actors[0].mech.part_count=1;s->actors[0].mech.parts=calloc(1,sizeof *p);assert(s->actors[0].mech.parts);
    p=s->actors[0].mech.parts;p->node=-1;p->parent_obj=-1;p->group=5;p->objl=1;p->coll=6;
    p->pos[1]=500;p->sphere_ok=1;p->hull_radius=100;
    p->model.object_count=1;p->model.objects=calloc(1,sizeof *p->model.objects);assert(p->model.objects);
}

void effect_fixture_cleanup(msim *s)
{
    int k;for(k=0;k<s->actor_count;k++)mech3d_free(&s->actors[k].mech);
    free(s->actors);s->actors=NULL;msim_free(s);memset(s,0,sizeof *s);
}
