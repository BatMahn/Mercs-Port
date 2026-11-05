/* Read-only query fixtures. Reuse the existing whole-world/scratch snapshot
 * helpers; the renamed legacy main is compiled but is not run by this gate. */
#include <stdlib.h>
static int fail_view_allocation;
static void *world_ray_fixture_malloc(size_t bytes)
{ return fail_view_allocation ? NULL : malloc(bytes); }
#define MERCS_PLAYTHROUGH_GEOMETRY_MALLOC world_ray_fixture_malloc
#define main mercs_legacy_geometry_fixture_main
#include "test_mercs_playthrough_geometry.c"
#undef main
#include "retained-rays.h"

static int ray_checks, ray_failures;
static void ray_check(int ok, const char *what)
{
    ray_checks++;
    if (!ok) { ray_failures++; fprintf(stderr,"FAIL world ray: %s\n",what); }
}

static void save_query_inputs(snapshot *snap, const msim *s)
{
    save_sim(snap,s);
    SAVE_ARRAY(snap,s->minds,s->actor_count);
    SAVE_ARRAY(snap,s->armed,s->actor_count);
    SAVE_ARRAY(snap,s->navs,s->nav_count);
    SAVE_ARRAY(snap,s->clist_seq,s->clist_seq_n);
}

static mercs_playthrough_world_ray check_ray(msim *s, const float from[3],
                                            const float to[3], const char *label)
{
    mercs_playthrough_world_ray a,b;
    snapshot snap={0}; scratch_snapshot before,after;
    seed_scratch(s); save_query_inputs(&snap,s); before=save_scratch();
    ray_check(mercs_playthrough_world_obstruction(s,from,to,&a)==0,label);
    ray_check(mercs_playthrough_world_obstruction(s,from,to,&b)==0,label);
    ray_check(a.available && !memcmp(&a,&b,sizeof a),"repeat output identical");
    after=save_scratch(); ray_check(!memcmp(&before,&after,sizeof before),"collision scratch unchanged");
    compare_sim(&snap); restore_scratch(&before);
    printf("SYNTHETIC %s available %d hit %d part %d type %d length %.4f distance %.4f saved %zu\n",
           label,a.available,a.blocked,a.hit_part,a.hit_collision_type,
           a.segment_length_cm,a.hit_distance_cm,snap.bytes);
    return a;
}

static void synthetic_rays(void)
{
    mech3d world={0}; msim *s; mercs_playthrough_world_ray r;
    const int nearlo[3]={-500,0,1000}, nearhi[3]={500,2000,1100};
    const int farlo[3]={-500,0,2000}, farhi[3]={500,2000,2100};
    const int sphlo[3]={1000,0,-400}, sphhi[3]={1800,1200,400};
    float from[3]={0,1000,-1000},to[3]={0,1000,3000},bad[3]={NAN,0,0};
    world.part_count=2;world.parts=calloc(2,sizeof *world.parts);assert(world.parts);
    fixture_box(&world.parts[0],1,farlo,farhi);
    fixture_box(&world.parts[1],1,nearlo,nearhi);
    s=fixture_sim(&world);
    r=check_ray(s,from,to,"nearest-world-box");
    ray_check(r.blocked && r.hit_part==1 && r.hit_collision_type==1,"nearest box identity");
    ray_check(fabsf(r.hit_distance_cm-2000)<.01f,"nearest box distance");
    ray_check(fabsf(r.hit_point[2]-1000)<.01f,"hit point on nearest box");
    to[2]=500;r=check_ray(s,from,to,"unextended-clear-before-box");
    ray_check(!r.blocked && r.hit_part==-1 && r.hit_distance_cm==-1,"clear before endpoint");
    fail_stamp_allocation=1;
    ray_check(mercs_playthrough_world_obstruction(s,from,to,&r)==-1 && !r.available,"stamp allocation failure unavailable");
    fail_stamp_allocation=0;fail_view_allocation=1;
    ray_check(mercs_playthrough_world_obstruction(s,from,to,&r)==-1 && !r.available,"view allocation failure unavailable");
    fail_view_allocation=0;
    ray_check(mercs_playthrough_world_obstruction(s,bad,to,&r)==-1 && !r.available,"nonfinite unavailable");
    ray_check(mercs_playthrough_world_obstruction(s,from,from,&r)==-1 && !r.available,"zero segment unavailable");
    ray_check(mercs_playthrough_world_obstruction(NULL,from,to,&r)==-1 && !r.available,"null sim unavailable");
    fixture_free(s,&world);

    memset(&world,0,sizeof world);world.part_count=1;world.parts=calloc(1,sizeof *world.parts);assert(world.parts);
    fixture_plane(&world.parts[0],2000,2000,5000);s=fixture_sim(&world);
    from[0]=0;from[1]=4000;from[2]=-2000;to[0]=0;to[1]=0;to[2]=2000;
    r=check_ray(s,from,to,"type5-raised-floor");
    ray_check(r.blocked && r.hit_part==0 && r.hit_collision_type==5,"type5 present in world query");
    ray_check(fabsf(r.hit_point[1]-2000)<.1f,"type5 hit height");
    fixture_free(s,&world);

    memset(&world,0,sizeof world);world.part_count=1;world.parts=calloc(1,sizeof *world.parts);assert(world.parts);
    fixture_box(&world.parts[0],6,sphlo,sphhi);s=fixture_sim(&world);
    from[0]=-3000;from[1]=600;from[2]=0;to[0]=3000;to[1]=600;to[2]=0;
    r=check_ray(s,from,to,"type6-world-sphere");
    ray_check(r.blocked && r.hit_collision_type==6,"probe includes type6 excluded from walking");
    fixture_free(s,&world);
}

static void retained_gra3(const char *archive_path)
{
    char error[256]; prj_archive *a=prj_open(archive_path,error,sizeof error);
    mech3d world={0};world_actor *actors=NULL;int count=0,i,j;
    msim *s=calloc(1,sizeof *s);snapshot snap={0};scratch_snapshot before,after;
    size_t n=sizeof retained_world_rays/sizeof retained_world_rays[0];
    mercs_playthrough_world_ray first[sizeof retained_world_rays/sizeof retained_world_rays[0]],repeat;
    clock_t begin,end;double minimum=1e30,maximum=0,total=0;int samples=0;
    assert(a && s && world3d_mission(a,"GRA3SCN1",1,&world)>=0);
    assert(world3d_actors(a,"GRA3SCN1",&actors,&count)==0);
    assert(msim_init(s,a,"GRA3SCN1",actors,count)==0);msim_set_world(s,&world);
    seed_scratch(s);save_query_inputs(&snap,s);before=save_scratch();
    for(i=0;i<(int)n;i++) {
        const retained_world_ray *r=&retained_world_rays[i];
        ray_check(mercs_playthrough_world_obstruction(s,r->from,r->to,&first[i])==0,"retained query available");
        for(j=0;j<8;j++) {
            double elapsed;
            begin=clock();
            ray_check(mercs_playthrough_world_obstruction(s,r->from,r->to,&repeat)==0,"retained repeated query");
            end=clock();elapsed=(double)(end-begin)*1000.0/CLOCKS_PER_SEC;
            total+=elapsed;samples++;if(elapsed<minimum)minimum=elapsed;if(elapsed>maximum)maximum=elapsed;
            ray_check(!memcmp(&first[i],&repeat,sizeof repeat),"retained repeated result identical");
        }
    }
    after=save_scratch();ray_check(!memcmp(&before,&after,sizeof before),"all retained rays leave collision scratch unchanged");
    compare_sim(&snap);
    for(i=0;i<(int)n;i++) {
        const retained_world_ray *r=&retained_world_rays[i];const mercs_playthrough_world_ray *o=&first[i];
        const mech3d_part *p=o->hit_part>=0?&world.parts[o->hit_part]:NULL;
        printf("RAY {\"time\":%.5f,\"actor\":%d,\"selected\":%d,\"destroyed\":%d,\"available\":%d,\"blocked\":%d,\"hit_part\":%d,\"hit_collision_type\":%d,\"hit_thing\":%d,\"segment_length_cm\":%.5f,\"hit_distance_cm\":%.5f,\"hit_point\":[%.5f,%.5f,%.5f],\"normal\":[%.8f,%.8f,%.8f],\"record\":\"%s\",\"object_index\":%d,\"moving\":%d}\n",
               r->time,r->actor,r->selected,r->destroyed,o->available,o->blocked,o->hit_part,o->hit_collision_type,o->hit_thing,
               o->segment_length_cm,o->hit_distance_cm,o->hit_point[0],o->hit_point[1],o->hit_point[2],o->normal[0],o->normal[1],o->normal[2],
               p?p->rec:"",p?p->obj_index:-1,p?p->moving:0);
    }
    printf("PURITY GRA3: %zu bytes, %zu distinct segments, %d timed repeats, all live world/model/actor/cache/scratch bytes identical\n",snap.bytes,n,samples);
    printf("LATENCY GRA3: mean %.6f ms min %.6f max %.6f per ray including private allocations/copy\n",total/samples,minimum,maximum);
    msim_free(s);free(s);for(i=0;i<count;i++)mech3d_free(&actors[i].mech);
    free(actors);mech3d_free(&world);prj_close(a);
}

int main(int argc,char **argv)
{
    if(argc!=2){fprintf(stderr,"usage: %s RETAIL_ARCHIVE.PRJ\n",argv[0]);return 2;}
    dp_clear_roots();combat_set_weapon_profile(COMBAT_PROFILE_RETAIL);
    synthetic_rays();retained_gra3(argv[1]);
    printf("RESULT checks %d failures %d\n",ray_checks,ray_failures);
    return ray_failures?1:0;
}
