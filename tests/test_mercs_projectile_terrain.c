/* Full native combat_step projectile consumer versus executed original
 * DOS/retail-Windows rows, plus archive-loaded native world-path controls.
 * No mission, campaign input, or original program executes in this fixture. */
#ifndef MERCS_SIM_SOURCE
#define MERCS_SIM_SOURCE "../src/msim.c"
#endif
#include MERCS_SIM_SOURCE
#include <assert.h>
#include "original-projectile-terrain-fixtures.h"
#include "original-titanium-fallback-fixtures.h"
#include "retained-sol3-path-fixtures.h"

#define COUNT(a) ((int)(sizeof(a)/sizeof((a)[0])))
typedef struct { int n, type; float p[3]; } impact_capture;
static int checks, failures, original_rows, titanium_rows, consumer_steps, archive_sweeps;
static const char *context;

static void equal(const char *field, double got, double want, double tolerance)
{
    checks++;
    if(!isfinite(got) || !isfinite(want) || fabs(got-want)>tolerance){
        failures++;
        fprintf(stderr,"FAIL %s %s: got %.9g expected %.9g\n",context,field,got,want);
    }
}

static void impact(void *opaque, int type, const float p[3])
{
    impact_capture *c=opaque; c->n++; c->type=type; memcpy(c->p,p,sizeof c->p);
}

static msim *new_sim(mech3d *world, impact_capture *capture)
{
    msim *s=calloc(1,sizeof *s);int i;assert(s);
    s->player_unit.human=1; s->player_unit.armor_mult=s->player_unit.speed_gain=1;
    for(i=0;i<8;i++)s->player_unit.armor[i]=s->player_unit.internal[i]=100;
    s->player_unit.lock_target=-2; s->player_target=s->player_lock_target=s->player_sel_w=-1;
    s->player_aim_free=1; s->player_radius=60; s->player_centre_h=550; s->rng=1;
    s->no_player_muzzle=1; s->shot_user=capture; s->on_effect=impact;
    if(world)msim_set_world(s,world);
    return s;
}

static void set_shot(msim *s,const float from[3],const float velocity[3],float life)
{
    msim_shot *shot=&s->shots[0];memset(shot,0,sizeof *shot);
    s->shot_count=1; shot->weapon=56; shot->owner=-1;shot->homing=-2;shot->ams_distance=-1;
    shot->x=from[0];shot->y=from[1];shot->z=from[2];
    shot->vx=velocity[0];shot->vy=velocity[1];shot->vz=velocity[2];shot->life=life;
}

static void plane(mech3d_part *p,int left,int right,int extent)
{
    wtb_object *o;int i;memset(p,0,sizeof *p);p->coll=5;p->parent_obj=-1;
    p->model.objects=calloc(1,sizeof *p->model.objects);assert(p->model.objects);
    p->model.object_count=1;o=p->model.objects;
    o->verts=calloc(4,sizeof *o->verts);o->polys=calloc(1,sizeof *o->polys);assert(o->verts&&o->polys);
    o->vert_count=4;o->poly_count=1;
    o->verts[0].x=-extent;o->verts[0].y=left;o->verts[0].z=-extent;
    o->verts[1].x=extent;o->verts[1].y=right;o->verts[1].z=-extent;
    o->verts[2].x=extent;o->verts[2].y=right;o->verts[2].z=extent;
    o->verts[3].x=-extent;o->verts[3].y=left;o->verts[3].z=extent;
    o->polys[0].n=4;for(i=0;i<4;i++)o->polys[0].idx[i]=(uint16_t)i;
}

static void original_consumers(void)
{
    int edition,i;
    combat_set_weapon_profile(COMBAT_PROFILE_RETAIL);
    for(edition=0;edition<2;edition++){
        combat_set_weapon_timing(edition==0);
        for(i=0;i<COUNT(original_projectile_cases);i++){
            const original_projectile_case *f=&original_projectile_cases[i];
            mech3d world={0};impact_capture cap={0};msim *s;int j,frame;
            float start[3],velocity[3];context=f->name;original_rows++;
            world.part_count=f->object_n;
            world.parts=calloc((size_t)(world.part_count?world.part_count:1),sizeof *world.parts);assert(world.parts);
            for(j=0;j<f->object_n;j++)plane(&world.parts[j],f->heights[j],f->heights[j],10000);
            s=new_sim(&world,&cap);
            for(j=0;j<3;j++){start[j]=(float)f->start[j];velocity[j]=(float)f->velocity_q16[j]/65536.0f;}
            set_shot(s,start,velocity,(float)f->life);
            for(frame=0;frame<f->frames;frame++){
                s->fx_delta_ticks=f->ticks;
                combat_step(s,(float)f->ticks/COMBAT_TICKS_PER_S);consumer_steps++;
                equal("active",s->shot_count,f->active[frame],0);
                if(f->active[frame] && s->shot_count){
                    const msim_shot *q=&s->shots[0];
                    equal("x",q->x,f->position[frame][0],.01);equal("y",q->y,f->position[frame][1],.01);equal("z",q->z,f->position[frame][2],.01);
                    equal("life",q->life,f->life_after[frame],.001);equal("age",q->age,f->age_after[frame],.001);
                }
            }
            equal("ground effect count",cap.n,f->impact,0);
            if(f->impact && cap.n)for(j=0;j<3;j++)equal("impact coordinate",cap.p[j],f->impact_point[j],.02);
            msim_free(s);free(s);mech3d_free(&world);
        }
    }
}

/* New terrain must compete with the existing object result by distance.
 * These controls are native integration, not extra original input coverage. */
static void mixed_objects(void)
{
    int reverse;context="native terrain/box nearest competition";
    for(reverse=0;reverse<2;reverse++){
        mech3d world={0};impact_capture cap={0};msim *s;float from[3]={0,1000,0},v[3]={100,-1000,200};
        world.part_count=2;world.parts=calloc(2,sizeof *world.parts);assert(world.parts);
        plane(&world.parts[0],reverse?800:300,reverse?800:300,10000);
        plane(&world.parts[1],reverse?300:800,reverse?300:800,10000);world.parts[1].coll=0;
        s=new_sim(&world,&cap);set_shot(s,from,v,100);s->fx_delta_ticks=1;
        combat_step(s,1.0f/COMBAT_TICKS_PER_S);consumer_steps++;
        equal("retired",s->shot_count,0,0);equal("one effect",cap.n,1,0);
        if(cap.n){equal("nearest x",cap.p[0],20,.02);equal("nearest y",cap.p[1],800,.02);equal("nearest z",cap.p[2],40,.02);}
        msim_free(s);free(s);mech3d_free(&world);
    }
}

static void native_identity_controls(void)
{
    int mode;
    /* Preserve current native non-terrain tie priority and direct thing_of
     * dispatch. These are integration invariants, not an original tie proof. */
    for(mode=0;mode<4;mode++){
        mech3d world={0};impact_capture cap={0};msim *s;terrain_t *t;int bound=mode?0:1;
        const float from[3]={0,1000,0},velocity[3]={100,-1000,200};
        const char *names[]={"native exact terrain/box tie","native terrain GT identity","native destroyed terrain GT","native hidden terrain"};
        context=names[mode];
        world.part_count=mode?1:2;world.parts=calloc((size_t)world.part_count,sizeof *world.parts);assert(world.parts);
        plane(world.parts,500,500,10000);
        if(!mode){plane(&world.parts[1],500,500,10000);world.parts[1].coll=0;}
        s=new_sim(&world,&cap);t=s->terrain;
        free(s->bld);s->bld=calloc(1,sizeof *s->bld);assert(s->bld);s->bld_count=1;
        s->bld[0].intact=bound;s->bld[0].destroyed=-1;s->bld[0].hp=mode==2?0:100;
        t->thing_of[bound]=0;
        if(mode==3)world.parts[0].hidden=1;
        set_shot(s,from,velocity,100);s->fx_delta_ticks=1;
        combat_step(s,1.0f/COMBAT_TICKS_PER_S);consumer_steps++;
        equal("active",s->shot_count,mode>=2,0);
        equal("effect count",cap.n,mode<2,0);
        equal("direct GT health",s->bld[0].hp,mode==2?0:mode==3?100:100-(int)combat_weapon_damage(56),0);
        if(mode<2 && cap.n)equal("contact height",cap.p[1],500,.02);
        msim_free(s);free(s);mech3d_free(&world);
    }
}

static void titanium_branch_consumers(void)
{
    int i,j;combat_set_weapon_profile(COMBAT_PROFILE_TITANIUM);
    for(i=0;i<COUNT(titanium_fallback_cases);i++){
        const titanium_fallback_case *f=&titanium_fallback_cases[i];
        mech3d world={0};impact_capture cap={0};msim *s;wtb_object *object;float velocity[3];
        context=f->name;titanium_rows++;
        /* A declared native support-only point at the query X/Z. Its zero
         * sphere cannot intersect a segment, but its type0 top is returned
         * by the unchanged support getter. This reproduces the captured
         * branch's supplied support/no-hit boundary, not original geometry. */
        world.part_count=1;world.parts=calloc(1,sizeof *world.parts);assert(world.parts);
        world.parts[0].coll=0;world.parts[0].parent_obj=-1;
        world.parts[0].model.object_count=1;world.parts[0].model.objects=calloc(1,sizeof *object);assert(world.parts[0].model.objects);
        object=world.parts[0].model.objects;object->vert_count=1;object->verts=calloc(1,sizeof *object->verts);assert(object->verts);
        object->verts[0].x=(int32_t)f->next[0];object->verts[0].y=(int32_t)f->support;object->verts[0].z=(int32_t)f->next[2];
        s=new_sim(&world,&cap);
        for(j=0;j<3;j++)velocity[j]=f->next[j]-f->from[j];
        /* Original Titanium capture begins after its outer-loop life
         * decrement. One native tick starts with that branch life plus1. */
        set_shot(s,f->from,velocity,(float)f->branch_life+1.0f);s->fx_delta_ticks=1;
        combat_step(s,1.0f/COMBAT_TICKS_PER_S);consumer_steps++;
        equal("Titanium active",s->shot_count,f->active,0);equal("Titanium collision",cap.n,f->collision,0);
        if(f->collision&&cap.n)for(j=0;j<3;j++)equal("Titanium contact",cap.p[j],f->impact[j],.02);
        if(f->active&&s->shot_count){
            equal("Titanium x",s->shots[0].x,f->next[0],.01);equal("Titanium y",s->shots[0].y,f->next[1],.01);equal("Titanium z",s->shots[0].z,f->next[2],.01);
            equal("Titanium branch life",s->shots[0].life,f->branch_life,.001);
        }
        msim_free(s);free(s);mech3d_free(&world);
    }
}

static void retained_scene(const char *path,int edition)
{
    prj_archive *a;mech3d world={0};char error[256];int i,j,moving=0;
    a=prj_open(path,error,sizeof error);assert(a);
    assert(world3d_mission(a,"SOL3SCN1",1,&world)>=0);
    for(i=0;i<world.part_count;i++)moving+=world.parts[i].moving!=0;
    context="retained SOL3 scene";equal("no moving world parts",moving,0,0);
    combat_set_weapon_profile(edition==0?COMBAT_PROFILE_DOS:edition==1?COMBAT_PROFILE_3DFX:COMBAT_PROFILE_TITANIUM);
    for(i=0;i<COUNT(retained_world_paths);i++){
        const retained_world_path *r=&retained_world_paths[i];
        impact_capture cap={0};msim *s=new_sim(&world,&cap);float velocity[3];char name[96];
        snprintf(name,sizeof name,"archive%d retained path%d",edition,r->index);context=name;
        for(j=0;j<3;j++)velocity[j]=r->to[j]-r->from[j];
        set_shot(s,r->from,velocity,100);s->fx_delta_ticks=1;
        /* This is a declared one-tick sweep of the captured reconstructed
         * segment, not a replay of its actual speed/time or unit collisions. */
        combat_step(s,1.0f/COMBAT_TICKS_PER_S);consumer_steps++;archive_sweeps++;
        equal("retirement",s->shot_count,!r->hit,0);equal("world effect",cap.n,r->hit,0);
        if(r->hit && cap.n)for(j=0;j<3;j++)equal("segment point",cap.p[j],r->point[j],.05);
        if(!r->hit && s->shot_count){
            equal("surviving x",s->shots[0].x,r->to[0],.05);equal("surviving y",s->shots[0].y,r->to[1],.05);equal("surviving z",s->shots[0].z,r->to[2],.05);
        }
        msim_free(s);free(s);
    }
    mech3d_free(&world);prj_close(a);
}

int main(int argc,char **argv)
{
    int i;
    if(argc!=4){fprintf(stderr,"usage: %s DOS.PRJ DEDICATED.PRJ TITANIUM.PRJ\n",argv[0]);return 2;}
    dp_clear_roots();original_consumers();mixed_objects();native_identity_controls();titanium_branch_consumers();
    for(i=1;i<argc;i++)retained_scene(argv[i],i-1);
    printf("SUMMARY {\"checks\":%d,\"failures\":%d,\"original_case_consumers\":%d,\"titanium_branch_consumers\":%d,\"combat_step_calls\":%d,\"archive_world_sweeps\":%d,\"archive_count\":3,\"mission_steps\":0}\n",checks,failures,original_rows,titanium_rows,consumer_steps,archive_sweeps);
    return failures?1:0;
}
