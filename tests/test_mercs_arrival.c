/* Declared native fixtures consuming captured original outputs.
 * No physics trajectory, save, pilot input or campaign win is synthesized. */
#ifndef MERCS_MSIM_UNDER_TEST
#define MERCS_MSIM_UNDER_TEST "../src/msim.c"
#endif
#include MERCS_MSIM_UNDER_TEST
#include "original-arrival-fixtures.h"

static int checks, failures, consumers, visit_ungraded;
static const char *current_case;
static void expect(int actual, int expected, const char *what)
{
    checks++;
    if (actual != expected) {
        if (failures < 60) fprintf(stderr,"FAIL %s: %s actual %d expected %d\n",current_case,what,actual,expected);
        failures++;
    }
}

static void actor(msim *s, int i, const int feet[3], int centre, int flags, const char *group)
{
    world_actor *a=&s->actors[i];
    a->table=1; a->leader=1; a->obj_class=0x100;
    a->unit_flags=(unsigned)flags; snprintf(a->group,sizeof a->group,"%s",group);
    memcpy(a->mech.origin,feet,sizeof a->mech.origin);
    s->units[i].y=(float)feet[1];s->centre_h[i]=(float)centre;s->armed[i]=1;
}

static void player(msim *s, const int feet[3], int centre, int flags)
{
    int k;for(k=0;k<3;k++)s->player[k]=(float)feet[k];
    s->player_unit.y=(float)feet[1];s->player_centre_h=(float)centre;
    s->player_unit_flags=(unsigned)flags;s->player_class=0x100;
    s->player_unit.human=1;
}

static msim *fixture(const arrival_case *c)
{
    msim *s=calloc(1,sizeof *s);int k;mtbl_node *n;
    if(!s)exit(2);
    s->actor_count=1;s->actors=calloc(1,sizeof *s->actors);
    s->units=calloc(1,sizeof *s->units);s->armed=calloc(1,sizeof *s->armed);
    s->centre_h=calloc(1,sizeof *s->centre_h);
    if(!s->actors||!s->units||!s->armed||!s->centre_h)exit(2);
    if(c->table)actor(s,0,c->source,c->source_center,c->source_flags,"SOURCE");
    else player(s,c->source,c->source_center,c->source_flags);
    if(c->type==1) {
        s->nav_count=1;
        snprintf(s->navs[0].name,sizeof s->navs[0].name,"%s",c->target_name);
        s->navs[0].x=(float)c->target[0];s->navs[0].y=(float)c->target[1];s->navs[0].z=(float)c->target[2];
        s->navs[0].radius=(float)c->radius;s->navs[0].shown=c->shown;s->navs[0].cls=0x100;
    } else if(c->type==2) {
        if(c->table)player(s,c->target,c->target_center,c->target_flags);
        else actor(s,0,c->target,c->target_center,c->target_flags,c->target_name);
    } else {
        mech3d *world=calloc(1,sizeof *world);mech3d_part *p;
        if(!world)exit(2);
        s->world=world;world->part_count=1;
        world->parts=calloc(1,sizeof *world->parts);s->bld=calloc(1,sizeof *s->bld);s->bld_count=1;
        if(!world->parts||!s->bld)exit(2);
        p=&world->parts[0];snprintf(p->rec,sizeof p->rec,"%s",c->target_name);
        memcpy(p->pos,c->origin,sizeof p->pos);s->bld[0].type=0x100;s->bld[0].flags=c->target_flags;
        s->bld[0].hp=c->target_flags&4?0:100;
        if(c->present) {
            wtb_object *o;p->model.object_count=1;p->model.objects=calloc(1,sizeof *p->model.objects);
            if(!p->model.objects)exit(2);
            o=p->model.objects;o->vert_count=2;o->verts=calloc(2,sizeof *o->verts);
            if(!o->verts)exit(2);
            for(k=0;k<2;k++) {
                o->verts[k].x=c->target[0]-c->origin[0]+(k?5:-5);
                o->verts[k].y=c->target[1]-c->origin[1]+(k?5:-5);
                o->verts[k].z=c->target[2]-c->origin[2]+(k?5:-5);
            }
        }
    }
    s->logic.table_count=c->table+1;s->logic.current[c->table]=0;
    s->logic.tables[c->table].node_count=1;s->logic.state[c->table][0]=MTBL_ACTIVE;
    n=&s->logic.tables[c->table].nodes[0];n->kind=MTBL_K_REACH;n->require_all=1;
    n->delay=UINT32_MAX;n->objective='N';snprintf(n->target,sizeof n->target,"%s",c->target_name);
    return s;
}

static void destroy_fixture(msim *s)
{
    mech3d *world=(mech3d *)s->world;world_actor *actors=s->actors;
    msim_free(s);free(actors);if(world){mech3d_free(world);free(world);}free(s);
}

static void captured_cases(void)
{
    size_t i;
    for(i=0;i<sizeof original_arrival_cases/sizeof original_arrival_cases[0];i++) {
        const arrival_case *c=&original_arrival_cases[i];msim *s=fixture(c);
        int idx=c->type==2&&c->table?-1:0,nok=-1;mtbl_world w={0};
        current_case=c->name;
        expect(tgt_within(s,c->table,c->type,idx),c->within,"direct original predicate");
        if(c->type==1) {
            if(c->grade_visited)expect(s->navs[0].visited,c->visited,"ordinary player/nav visit side effect");
            else {visit_ungraded++;printf("UNMODELED %s/%s native visit %d original visit %d\n",c->edition,c->name,s->navs[0].visited,c->visited);}
            s->navs[0].visited=0;
        }
        expect(cb_targets(s,c->table,0,MTBL_K_REACH,c->target_name,&nok),1,"production target attachment count");
        expect(nok,c->within,"production attachment predicate count");
        if(c->type==1)s->navs[0].visited=0;
        w.user=s;w.targets=cb_targets;mtbl_logic_step(&s->logic,0,&w);consumers++;
        expect(s->logic.state[c->table][0],c->state,"real ready-node consumer state");
        if(c->type==1&&c->grade_visited)expect(s->navs[0].visited,c->visited,"consumer visit side effect");
        if(c->type==4) {
            float p[3];int eligible=s->bld[0].hp>0,k;
            expect(msim_thing_point(s,0,p),eligible,"existing targeting eligibility");
            if(eligible)for(k=0;k<3;k++)expect((int)lrintf(p[k]),c->present?c->target[k]:c->origin[k],"existing targeting centre");
        }
        destroy_fixture(s);
    }
}

static void native_lifecycle_controls(void)
{
    size_t i;
    for(i=0;i<sizeof original_arrival_cases/sizeof original_arrival_cases[0];i++) {
        const arrival_case *c=&original_arrival_cases[i];msim *s;
        if(strcmp(c->edition,"dos")||strcmp(c->name,"target-flags-0"))continue;
        s=fixture(c);current_case="native wreck versus intact ejected body";
        s->units[0].destroyed=1;
        expect(tgt_within(s,0,2,0),0,"existing native wreck contract excludes arrival target");
        s->units[0].intact_out=1;
        expect(tgt_within(s,0,2,0),1,"intact ejected body retains position");
        destroy_fixture(s);
    }
    for(i=0;i<sizeof original_arrival_cases/sizeof original_arrival_cases[0];i++) {
        const arrival_case *c=&original_arrival_cases[i];msim *s;float p[3];
        if(strcmp(c->edition,"dos")||strcmp(c->name,"bbox-inside-origin-outside"))continue;
        s=fixture(c);current_case="native hidden object fallback";
        ((mech3d *)s->world)->parts[0].hidden=1;
        expect(tgt_within(s,0,4,0),0,"hidden object uses distant scene origin");
        expect(msim_thing_point(s,0,p),0,"existing target eligibility rejects hidden object");
        destroy_fixture(s);
    }
}

int main(void)
{
    captured_cases();native_lifecycle_controls();
    printf("{\"checks\":%d,\"failures\":%d,\"captured_cases\":%zu,\"native_ready_node_consumers\":%d,\"ungraded_original_flag0_visit_cases\":%d}\n",
           checks,failures,sizeof original_arrival_cases/sizeof original_arrival_cases[0],consumers,visit_ungraded);
    return failures?1:0;
}
