#include "mercsrmg.h"
#include "mercscampaign.h"
#include "mercsstore.h"
#include "mercsexchange.h"
#include "datapath.h"
#include "bwd.h"
#include "mek.h"
#include "mech3d.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "mercs_rmg_original.h"

static int checks;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"RMG check failed at line %d: %s\n",__LINE__,#x);abort();}}while(0)
static void checked(int rc,const char *e,int line)
{checks++;if(rc){fprintf(stderr,"RMG failure at line%d: %s (%d)\n",line,e,rc);abort();}}
#define OK(x) checked((x),e,__LINE__)
static uint32_t u32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t hash(const uint8_t *p,size_t n)
{uint64_t h=UINT64_C(14695981039346656037);size_t i;for(i=0;i<n;i++)h=(h^p[i])*UINT64_C(1099511628211);return h;}
static const prj_record *file(const mercs_rmg_bundle *b,const char *name)
{size_t i;char whole[32];for(i=0;i<b->count;i++){snprintf(whole,sizeof whole,"%s.%s",b->files[i].name,b->files[i].tag);if(!strcasecmp(name,whole))return &b->files[i];}return NULL;}
static uint64_t world_hash(prj_archive *a)
{prj_record r;uint64_t h;T(prj_read_named(a,"BWD","RANDWLD1",&r)==0);h=hash(r.data,r.size);prj_record_free(&r);return h;}
static void corpus(prj_archive *a)
{
    size_t i,j;char e[384]={0};
    for(i=0;i<sizeof rmg_original_cases/sizeof rmg_original_cases[0];i++){
        const rmg_fixture *f=&rmg_original_cases[i];mercs_rmg_config cfg={0};mercs_rmg_bundle b={0};
        cfg.seed=f->seed;cfg.number=1;cfg.year=f->year;cfg.kind=f->kind;cfg.skill=f->skill;cfg.success_pay=200000;
        snprintf(cfg.planet,sizeof cfg.planet,"%s",f->planet);OK(mercs_rmg_generate(a,&cfg,&b,e,sizeof e));
        T(b.count==f->count+1); /* original launch-copy alias RANDWLD1 */
        for(j=0;j<f->count;j++){
            const rmg_file_fixture *wanted=&rmg_original_files[f->first+j];const prj_record *r=file(&b,wanted->name);
            T(r&&r->size==wanted->size&&hash(r->data,r->size)==wanted->hash);
        }
        {const prj_record *w=file(&b,"RAN1WLD1.BWD"),*alias=file(&b,"RANDWLD1.BWD");T(w&&alias&&w->size==alias->size&&!memcmp(w->data,alias->data,w->size));}
        OK(mercs_rmg_install(a,&b));T(prj_is_generated(a,"BWD",b.scene));
        mercs_rmg_free(&b);
    }
}
static void overlays(prj_archive *a)
{
    prj_record *copy=NULL,r={0};size_t count=0;uint64_t before=world_hash(a);char e[384]={0};
    OK(prj_get_generated(a,&copy,&count));T(count>0&&copy[0].generated);
    snprintf(r.name,sizeof r.name,"../UNSAFE");strcpy(r.tag,"BWD");r.data=(uint8_t *)"BWD";r.size=3;
    T(prj_set_generated(a,&r,1)!=0);T(world_hash(a)==before);
    OK(prj_set_generated(a,NULL,0));T(!prj_is_generated(a,"BWD","RANDWLD1"));
    prj_restore_generated(a,copy,count);T(world_hash(a)==before);
    {mercs_rmg_config cfg={0};mercs_rmg_bundle b={0};cfg.year=3043;T(mercs_rmg_generate(a,&cfg,&b,e,sizeof e)!=0);T(!b.count&&world_hash(a)==before);}
    {mercs_rmg_config cfg={0};mercs_rmg_bundle b={0};cfg.year=3044;cfg.seed=24884;
     T(mercs_rmg_generate(a,&cfg,&b,e,sizeof e)==MERCS_RMG_BAD_SEED);T(!b.count&&world_hash(a)==before);}
}
static void career(prj_archive *a,const char *overlay,const char *campaign,int year,uint32_t seed)
{
    mercs_campaign c={0},restored={0};mercs_shell_state *v=calloc(1,sizeof *v),*next=calloc(1,sizeof *next);
    mercs_shell_mission *preview=calloc(1,sizeof *preview);mercs_rmg_bundle bundle={0};
    uint8_t *save,resume[1024];size_t size;uint64_t old,accepted;char e[384]={0};int index=2,i,j,gpscount=0;
    bwd_mission mission;T(v&&next&&preview);
    OK(mercs_campaign_new(a,1,"RMG TEST","TEST UNIT",&c,v,e,sizeof e));
    old=world_hash(a);OK(mercs_shell_mission_load_seeded(a,campaign,v->pilot,year,seed,preview,e,sizeof e));
    if(!preview->launchable)fprintf(stderr,"preview unavailable: %s\n",preview->unavailable);
    T(preview->launchable);T(world_hash(a)==old);T(preview->procedural_year==year&&preview->procedural_seed==seed);
    v->missions[index]=*preview;OK(mercs_campaign_accept(a,&c,v,index,e,sizeof e));
    T(c.active&&c.procedural_year==year&&u32(mercs_save_header(c.document)+0x162)==seed);
    T(!strcmp(v->missions[index].contract,preview->contract));T(prj_is_generated(a,"BWD",preview->scene));
    T(strstr((const char *)mercs_save_header(c.document)+0x166,".bwd")!=NULL);
    accepted=world_hash(a);OK(mercs_rmg_campaign(a,campaign,year,seed,&bundle,e,sizeof e));
    {const prj_record *w=file(&bundle,"RANDWLD1.BWD");T(w&&accepted==hash(w->data,w->size));}
    mercs_rmg_free(&bundle);
    OK(mercs_shell_prepare_launch(a,v,index,overlay,e,sizeof e));
    OK(bwd_mission_load(a,v->missions[index].scene,&mission));
    if(mission.missing_count)for(i=0;i<mission.missing_count&&i<16;i++)fprintf(stderr,"missing %s\n",mission.missing[i]);
    T(mission.missing_count==0&&mission.nav_count>0&&mission.record_count>10);
    for(i=0;i<mission.record_count;i++)for(j=0;j<mission.records[i].chunk_count;j++){
        bwd_chunk *ch=&mission.records[i].chunks[j];bwd_gps gps;mek_def m;
        if(strcmp(ch->tag,"GPS"))continue;
        OK(bwd_gps_decode(ch,&gps));OK(mek_load(a,gps.loadout,&m));gpscount++;
    }
    T(gpscount>1);bwd_mission_free(&mission);
    {mech3d world={0};world_actor *actors=NULL;int count=0,generated_parts=0;
     T(world3d_mission(a,v->missions[index].scene,1,&world)>0);
     for(i=0;i<world.part_count;i++)if(prj_is_generated(a,"BWD",world.parts[i].rec))generated_parts++;
     T(generated_parts>0);OK(world3d_actors(a,v->missions[index].scene,&actors,&count));
     T(count==gpscount-1);for(i=0;i<count;i++)T(actors[i].mech.part_count>0&&actors[i].skel[0]);
     world3d_free_actors(actors,count);mech3d_free(&world);}

    /* Launch preparation changes the deployed bit, which must survive the
     * original save as well as the accepted seed and original generation year. */
    OK(mercs_campaign_sync(&c,v,e,sizeof e));OK(mercs_save_encode(c.document,1,&save,&size));
    OK(mercs_campaign_encode_resume(&c,v,save,size,resume));
    OK(prj_set_generated(a,NULL,0));T(!prj_is_generated(a,"BWD",preview->scene));
    OK(mercs_campaign_decode_resume(a,save,size,resume,sizeof resume,&restored,next,e,sizeof e));
    T(restored.procedural_year==year&&next->missions[index].procedural_seed==seed);
    T(!strcmp(next->missions[index].briefing,preview->briefing)&&world_hash(a)==accepted);
    OK(prj_set_generated(a,NULL,0));OK(mercs_campaign_prepare_resources(a,&restored,e,sizeof e));T(world_hash(a)==accepted);
    free(save);mercs_campaign_free(&restored);mercs_campaign_free(&c);free(preview);free(next);free(v);
}
static void all_contracts(prj_archive *a)
{
    static const char slots[]="123456789ABCDEFGHIJKLMNOPQRSTUV";size_t i;char e[384]={0};
    mercs_shell_state *v=calloc(1,sizeof *v);mercs_shell_mission *m=calloc(1,sizeof *m);T(v&&m);
    for(i=0;i<sizeof slots-1;i++){
        mercs_campaign c={0};char name[]="RAN0CAM1";name[3]=slots[i];
        OK(mercs_campaign_new(a,1,"ALL CONTRACTS","TEST UNIT",&c,v,e,sizeof e));
        OK(mercs_shell_mission_load_seeded(a,name,v->pilot,0,17,m,e,sizeof e));
        if(!m->launchable)fprintf(stderr,"%s unavailable: %s\n",name,m->unavailable);
        T(m->launchable);v->missions[2]=*m;OK(mercs_campaign_accept(a,&c,v,2,e,sizeof e));
        T(c.active&&prj_is_generated(a,"BWD",m->scene));
        if(slots[i]=='V')T(mercs_save_list_count(c.document,MERCS_SAVE_SHOP_MECHS)==9);
        mercs_campaign_free(&c);
    }
    free(m);free(v);
}
static void prepare_child(prj_archive *a,const char *overlay,const char *edition,const char *store)
{
    mercs_campaign c={0};mercs_shell_state *v=calloc(1,sizeof *v);mercs_exchange_launch launch={0};char e[384]={0};
    T(v);T(!strcmp(edition,"dos")||!strcmp(edition,"3dfx")||!strcmp(edition,"direct3d"));
    OK(mercs_campaign_new(a,1,"RMG CHILD","TEST UNIT",&c,v,e,sizeof e));
    OK(mercs_shell_mission_load_seeded(a,"RAN1CAM1",v->pilot,3044,1,&v->missions[2],e,sizeof e));
    T(v->missions[2].launchable);OK(mercs_campaign_accept(a,&c,v,2,e,sizeof e));
    OK(mercs_shell_prepare_launch(a,v,2,overlay,e,sizeof e));
    strcpy(launch.scene,v->missions[2].scene);strcpy(launch.edition,edition);launch.unit_count=1;
    launch.units[0].actor_index=-1;launch.units[0].controller_flags=2;strcpy(launch.units[0].name,v->starter_loadout);
    OK(mercs_exchange_begin(overlay,&launch));c.phase=MERCS_CAMPAIGN_DEPLOYED;
    strcpy(c.launch_token,launch.token);strcpy(c.launch_edition,edition);OK(mercs_campaign_sync(&c,v,e,sizeof e));
    mkdir(store,0700);OK(mercs_store_write(store,-1,&c,v,e,sizeof e));
    printf("RMG_DEPLOYED scene=%s edition=%s token=%s overlay=%s store=%s\n",launch.scene,edition,launch.token,overlay,store);
    mercs_campaign_free(&c);free(v);
}
int main(int argc,char **argv)
{
    char e[384]={0},root[1024];prj_archive *a;size_t len;
    if(argc!=4&&(argc!=7||strcmp(argv[4],"--prepare-launch"))){fprintf(stderr,"usage: %s MW2.PRJ CD_ROOT WRITABLE_TEST_OVERLAY [--prepare-launch EDITION CAREER_STORE]\n",argv[0]);return 2;}
    a=prj_open(argv[1],e,sizeof e);T(a!=NULL);T(strlen(argv[1])<sizeof root);strcpy(root,argv[1]);
    len=strlen(root);while(len&&root[len-1]!='/')len--;T(len>0);root[len-1]=0;
    mkdir(argv[3],0700);OK(dp_add_root(argv[3]));OK(dp_set_write_root(argv[3]));OK(dp_add_root(root));OK(dp_add_root(argv[2]));
    if(argc==7){prepare_child(a,argv[3],argv[5],argv[6]);prj_close(a);return 0;}
    OK(mercs_rmg_available(e,sizeof e));corpus(a);overlays(a);
    career(a,argv[3],"RAN1CAM1",3044,1);career(a,argv[3],"RANACAM1",3046,17);career(a,argv[3],"RANVCAM1",3051,31);
    all_contracts(a);
    dp_clear_roots();T(mercs_rmg_available(e,sizeof e)!=0&&strstr(e,"MW2SHELL.EXE"));
    prj_close(a);printf("Mercs RMG: %d checks,36 original generator cases,460 byte-hash comparisons; seeded acceptance, graph loading and save/resume passed.\n",checks);return 0;
}
