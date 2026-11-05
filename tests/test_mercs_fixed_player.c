/* Original fixed-player scenes use their archived STD vehicle while the
 * shell's assigned owned roster remains staged but absent from the scene.
 * These disposable integration fixtures do not represent campaign victories. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "mercsshell_data.h"
#include "mercsbridge.h"
#include "datapath.h"
#include "mek.h"

typedef struct {
    const char *campaign, *scene, *record, *skeleton, *loadout, *orders_word, *movie;
    int mek_id, skeleton_id, tons;
} fixed_case;

/* Independently read CPGN/GPS records in all three supplied archives; see
 * tests/fixtures/campaign-playthrough/original-fixed-player.json. */
static const fixed_case cases[] = {
    {"MCC4CAM1","MCC4SCN1","MCC4STAR","stalker","stk00std","STK-3F STALKER","MCC4BRF1",140,78,85},
    {"SIG2CAM1","SIG2SCN1","SIG2STAR","pegasus","peg00std","hovertank","NODATA",116,66,35},
    {"OUT3CAM1","OUT3SCN1","OUT3USER","kodiak","kdk00std","Kodiak","OUT3BRF1",82,56,100}
};

static void require(int code,const char *error)
{
    if(code){fprintf(stderr,"fixed-player: %s (%d)\n",error,code);abort();}
}

static void put16(uint8_t *p,unsigned v)
{ p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p,uint32_t v)
{ p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24); }

static const fixed_case *select_case(const char *name)
{
    size_t i;
    for(i=0;i<sizeof cases/sizeof cases[0];i++)
        if(!strcasecmp(cases[i].campaign,name))return &cases[i];
    fprintf(stderr,"Unknown fixed-player fixture %s\n",name);abort();
}

static void check_player(prj_archive *a,const fixed_case *f)
{
    bwd_gps player;
    mercs_shell_mission *mission=calloc(1,sizeof *mission);
    char error[512];
    assert(mission);
    memset(&player,0xa5,sizeof player);
    assert(mercs_shell_scene_player(a,f->scene,&player,error,sizeof error)==1);
    assert(player.is_player&&player.control==0&&player.table==0&&player.leader==1);
    assert(player.mek_id==f->mek_id&&player.skeleton_id==f->skeleton_id);
    assert(!strcasecmp(player.skeleton,f->skeleton)&&!strcasecmp(player.loadout,f->loadout));
    require(mercs_shell_mission_load(a,f->campaign,"FIXED PILOT",mission,error,sizeof error),error);
    if(!mission->launchable)fprintf(stderr,"%s unavailable: %s\n",f->campaign,mission->unavailable);
    assert(mission->launchable&&!strcmp(mission->scene,f->scene));
    assert(!strcasecmp(mission->briefing_smk,f->movie));
    assert(strstr(mission->briefing,f->orders_word));
    printf("CLASSIFY %s: %s/%s, orders contain %s, movie %s\n",
           f->campaign,player.skeleton,player.loadout,f->orders_word,mission->briefing_smk);
    free(mission);
}

static void generated(prj_archive *a,const uint8_t *chunks,size_t size,int expected)
{
    prj_record r={0};
    bwd_gps out,old;
    uint8_t raw[512]={0};
    char error[512];
    assert(size+12<=sizeof raw);
    memcpy(raw,"BWD\0",4);put32(raw+4,(uint32_t)size+12);put32(raw+8,100);
    memcpy(raw+12,chunks,size);
    strcpy(r.tag,"BWD");strcpy(r.name,"FXTSCN1");r.data=raw;r.size=size+12;
    require(prj_set_generated(a,&r,1),"install isolated malformed graph");
    memset(&out,0xa5,sizeof out);old=out;
    assert(mercs_shell_scene_player(a,"FXTSCN1",&out,error,sizeof error)==expected);
    if(expected<=0)assert(!memcmp(&out,&old,sizeof out));
    if(expected<0)assert(error[0]);
    require(prj_set_generated(a,NULL,0),"clear isolated malformed graph");
}

static void classification(prj_archive *a)
{
    prj_record record;
    bwd_chunk chunks[8];
    bwd_gps old,out;
    uint8_t player[100],test[256],incl[22]={0};
    int i,count,found=0;
    char error[512];
    for(i=0;i<3;i++)check_player(a,&cases[i]);
    memset(&out,0xa5,sizeof out);old=out;
    assert(mercs_shell_scene_player(a,"BET1SCN1",&out,error,sizeof error)==0);
    assert(!memcmp(&out,&old,sizeof out));
    assert(mercs_shell_scene_player(a,"TRA1SCN1",NULL,error,sizeof error)==0);
    assert(mercs_shell_scene_player(a,"NO_SCENE",NULL,error,sizeof error)<0);
    require(prj_read_named(a,"BWD","MCC4STAR",&record),"read original GPS");
    count=bwd_chunks(record.data,record.size,chunks,8);assert(count>0&&count<=8);
    for(i=0;i<count;i++)if(!strcmp(chunks[i].tag,"GPS")){
        assert(chunks[i].size==92&&!found++);
        memcpy(player,"GPS\0",4);put32(player+4,100);memcpy(player+8,chunks[i].data,92);
    }
    prj_record_free(&record);assert(found==1);
    memcpy(incl,"INCL",4);put32(incl+4,sizeof incl);put16(incl+8,0xfffe);memcpy(incl+10,"USERSTAR",9);
    generated(a,player,sizeof player,1); /* a single known stock GPS */
    generated(a,incl,sizeof incl,0);     /* an ordinary owned roster */
    generated(a,player,0,-1);           /* no human player source */
    memcpy(test,player,100);memcpy(test+100,player,100);generated(a,test,200,-1);
    memcpy(test,player,100);memcpy(test+100,incl,sizeof incl);generated(a,test,100+sizeof incl,-1);
    memcpy(test,incl,sizeof incl);memcpy(test+sizeof incl,incl,sizeof incl);generated(a,test,2*sizeof incl,-1);
    memcpy(test,player,100);put16(test+8,82);generated(a,test,100,-1); /* Kodiak id, Stalker name */
    memcpy(test,player,100);put16(test+10,56);generated(a,test,100,-1);
    memcpy(test,player,100);put16(test+8,0xfffe);generated(a,test,100,-1);
    memcpy(test,player,100);put16(test+8,0xffff);generated(a,test,100,-1); /* fixed must be exact archive identity */
    memcpy(test,player,100);memcpy(test+8+37,"STK00MER",9);generated(a,test,100,-1);
    memcpy(test,player,100);test[8+6]=2;generated(a,test,100,-1);
    memcpy(test,player,100);put32(test+4,101);generated(a,test,100,-1);
    puts("PASS: fixed/ordinary player classification, SIG2 orders/movie separation, 12 malformed or missing graph cases");
}

static mercs_shell_state *fixture(prj_archive *a,const fixed_case *f,
                                   mercs_shell_deploy_pilot pilots[3])
{
    static const char *const stock[]={"JEN00STD","PNT00STD"};
    mercs_shell_state *state=calloc(1,sizeof *state);
    char names[1][16],error[512],filename[32];
    int i,j;
    assert(state);
    require(mercs_shell_register(a,state,1,"Fixed Pilot","Fixed Fixture",error,sizeof error),error);
    for(i=0;i<2;i++){
        prj_record r;
        require(prj_read_named(a,"MEK",stock[i],&r),"read declared owned fixture");
        snprintf(filename,sizeof filename,"%.3s%02dMER.MEK",stock[i],i+1);
        require(mercs_career_pair_init(&state->owned[i+1],r.data,r.size,filename,
            (uint32_t)(100+i),300+i),"create declared owned fixture");
        prj_record_free(&r);
    }
    state->owned_count=3;
    for(i=0;i<3;i++){
        /* Recognizable preexisting damage ensures a loaner cannot overwrite
         * these records with its larger armor or fresh stock configuration. */
        put32(state->owned[i].current.bytes+0x768,(uint32_t)(i+2));
        memset(&pilots[i],0,sizeof pilots[i]);
        pilots[i].owned_index=i;
        pilots[i].pilot_instance_id=i?(uint32_t)(299+i):state->pilot_instance_id;
        snprintf(pilots[i].name,sizeof pilots[i].name,"Fixed Pilot%d",i);
        for(j=0;j<8;j++)pilots[i].attributes[j]=-1;
    }
    strcpy(names[0],f->campaign);
    require(mercs_shell_set_missions(a,state,names,1,error,sizeof error),error);
    assert(state->mission_count==1&&state->missions[0].launchable);
    assert(!strcmp(state->missions[0].scene,f->scene));
    return state;
}

static void prepare(prj_archive *a,mercs_shell_state *state,
                    const mercs_shell_deploy_pilot pilots[3],const char *overlay,
                    const char *edition,mercs_exchange_launch *launch)
{
    char error[512];int i;
    memset(launch,0,sizeof *launch);
    require(mercs_shell_prepare_roster(a,state,0,overlay,pilots,3,NULL,launch,error,sizeof error),error);
    assert(launch->unit_count==3);
    for(i=0;i<3;i++)assert(launch->units[i].actor_index==-i-1&&launch->units[i].controller_flags==2);
    strcpy(launch->scene,state->missions[0].scene);strcpy(launch->edition,edition);
    require(mercs_exchange_begin(overlay,launch),"begin fixed-player exchange");
}

static void validate_runtime(prj_archive *a,const fixed_case *f,msim *sim,
                             world_actor **actors,int *count)
{
    int i;
    require(world3d_actors(a,f->scene,actors,count),"load fixed-player actor graph");
    require(msim_init(sim,a,f->scene,*actors,*count),"initialize fixed-player simulator");
    assert(!strcasecmp(sim->player_loadout,f->loadout));
    assert(!strcasecmp(sim->player_skel,f->skeleton));
    assert(sim->player_unit.tons==f->tons);
    for(i=0;i<*count;i++)assert(strcasecmp((*actors)[i].group,"USERSTAR"));
    printf("RUNTIME %s: %s/%s %d tons; no staged USERSTAR actors\n",
           f->scene,sim->player_skel,sim->player_loadout,sim->player_unit.tons);
}

static void consume(mercs_shell_state *state,const char *overlay,
                    const mercs_exchange_launch *launch)
{
    mercs_exchange_result *packet=calloc(1,sizeof *packet),*bad=malloc(sizeof *bad);
    mercs_shell_state *before=malloc(sizeof *before);
    mercs_career_import_result imported;
    char error[512],logical[64];
    size_t i;
    assert(packet&&bad&&before);
    require(mercs_exchange_read(overlay,launch,packet),"read child return packet");
    assert(packet->mech_count==3);
    for(i=0;i<launch->unit_count;i++){
        uint8_t *bytes;size_t size,j;
        snprintf(logical,sizeof logical,"MEK/%s.MEK",launch->units[i].name);
        require(dp_read_file(logical,&bytes,&size),"read unchanged owned deployment");
        for(j=0;j<packet->mech_count&&packet->mechs[j].actor_index!=launch->units[i].actor_index;j++);
        assert(j<packet->mech_count&&!strcasecmp(packet->mechs[j].name,launch->units[i].name));
        assert(packet->mechs[j].size==size&&!memcmp(packet->mechs[j].bytes,bytes,size));
        free(bytes);
    }
    *before=*state;
    *bad=*packet;bad->mission[0]='X';
    assert(mercs_shell_apply_return(state,0,launch,bad,&imported,error,sizeof error));
    assert(!memcmp(state,before,sizeof *state));
    *bad=*packet;bad->mechs[1].actor_index=bad->mechs[0].actor_index;
    assert(mercs_shell_apply_return(state,0,launch,bad,&imported,error,sizeof error));
    assert(!memcmp(state,before,sizeof *state));
    require(mercs_shell_apply_return(state,0,launch,packet,&imported,error,sizeof error),error);
    assert(imported.action==MERCS_CAREER_IMPORT_APPLIED&&imported.request_delete);
    assert(state->owned_count==3&&state->owned_index==before->owned_index&&state->funds==before->funds);
    for(i=0;i<3;i++){
        const uint8_t *old=before->owned[i].current.bytes,*now=state->owned[i].current.bytes;
        static const size_t ranges[][2]={{0x28c,0x2f0},{0x2f0,0x360},{0x368,0x4e8},{0x760,0x7f0},{0x80c,0x84b}};
        size_t j;
        for(j=0;j<sizeof ranges/sizeof ranges[0];j++)
            assert(!memcmp(old+ranges[j][0],now+ranges[j][0],ranges[j][1]-ranges[j][0]));
        assert(!memcmp(&before->owned[i].reference,&state->owned[i].reference,sizeof state->owned[i].reference));
        assert(!mercs_career_deployed(&state->owned[i].current));
    }
    assert(state->last_return_valid&&state->last_msn.outcome!=2);
    require(mercs_exchange_acknowledge(overlay,launch),"acknowledge fixed-player return");
    assert(mercs_exchange_read(overlay,launch,packet)==MERCS_EXCHANGE_APPLIED);
    printf("PASS: %s stock-player abort -> complete CAR/MSN + three unchanged owned MEKs -> selective shell import; outcome %u; no campaign win\n",
           launch->scene,state->last_msn.outcome);
    free(before);free(packet);free(bad);
}

static void dispose(msim *sim,world_actor *actors,int count)
{ int i;msim_free(sim);for(i=0;i<count;i++)mech3d_free(&actors[i].mech);free(actors);free(sim); }

/* This is a transport-capacity fixture, not a campaign-legal Mech design.
 * Exercise all 14 weapon and 25 ammunition rows plus the original 58-byte
 * deployment trailer. None of this unused owned unit is spawned or driven. */
static void capacity_boundary(prj_archive *a,const char *overlay,const char *edition)
{
    const fixed_case *f=&cases[0];
    mercs_shell_deploy_pilot pilots[3];mercs_exchange_launch launch;
    mercs_shell_state *state=fixture(a,f,pilots);
    msim *sim=calloc(1,sizeof *sim);world_actor *actors=NULL;int count,i,j;
    prj_record stock;mek_def parsed;
    uint8_t maximum[MERCS_CAREER_DEPLOY_MAX_BYTES]={0},*deployed;
    size_t size;char logical[64];
    assert(sizeof maximum==714&&MERCS_EXCHANGE_MEK_BYTES>=sizeof maximum&&sim);
    require(prj_read_named(a,"MEK","JEN00STD",&stock),"read capacity-fixture chassis");
    assert(stock.size>=344);memcpy(maximum,stock.data,344);prj_record_free(&stock);
    put32(maximum+16,14);put32(maximum+20,25);
    for(i=0;i<8;i++)for(j=0;j<12;j++)put16(maximum+24+40*i+12+2*j,0);
    for(i=0;i<14;i++){
        put32(maximum+344+8*i,(uint32_t)(1001+i));
        put32(maximum+348+8*i,1);
    }
    for(i=0;i<25;i++){
        put32(maximum+344+8*(14+i),(uint32_t)(11001+i));
        put32(maximum+348+8*(14+i),(uint32_t)(1000+i));
    }
    strcpy((char *)maximum+344+8*39,"Transport capacity fixture");
    require(mek_parse(maximum,sizeof maximum,&parsed),"parse maximum-size fixture");
    assert(parsed.item_count==14&&parsed.link_count==25&&parsed.has_config_extension);
    require(mercs_career_pair_init(&state->owned[1],maximum,sizeof maximum,
                                  "JEN01MER.MEK",100,300),"create maximum-size owned fixture");
    prepare(a,state,pilots,overlay,edition,&launch);
    snprintf(logical,sizeof logical,"MEK/%s.MEK",launch.units[1].name);
    require(dp_read_file(logical,&deployed,&size),"read maximum deployment");
    assert(size==714);
    validate_runtime(a,f,sim,&actors,&count);
    require(mercs_bridge_publish(sim,overlay,launch.token,edition),"publish maximum retained deployment");
    {
        mercs_exchange_result *packet=calloc(1,sizeof *packet);size_t k;
        assert(packet);
        require(mercs_exchange_read(overlay,&launch,packet),"read maximum retained transport");
        for(k=0;k<packet->mech_count&&packet->mechs[k].actor_index!=-2;k++);
        assert(k<packet->mech_count&&packet->mechs[k].size==714);
        assert(!memcmp(packet->mechs[k].bytes,deployed,size));
        free(packet);
    }
    free(deployed);consume(state,overlay,&launch);dispose(sim,actors,count);free(state);
    puts("PASS: synthetic 714-byte retained deployment -> bridge -> exchange -> selective import; capacity boundary, not a campaign loadout");
}

int main(int argc,char **argv)
{
    prj_archive *a;char error[512];
    assert(argc==4||argc==5||argc==6||argc==7);
    a=prj_open(argv[1],error,sizeof error);assert(a);
    dp_clear_roots();require(dp_add_root(argv[2]),"private overlay root");
    if(argc==4){
        size_t i;
        classification(a);
        for(i=0;i<3;i++){
            mercs_shell_deploy_pilot pilots[3];mercs_exchange_launch launch;
            mercs_shell_state *state=fixture(a,&cases[i],pilots);
            msim *sim=calloc(1,sizeof *sim);world_actor *actors=NULL;int count;
            assert(sim);prepare(a,state,pilots,argv[2],argv[3],&launch);
            validate_runtime(a,&cases[i],sim,&actors,&count);
            /* Controlled damage verifies that the archived player's state is
             * not serialized over an unused owned manifest member. */
            sim->player_unit.armor[MEK_HEAD]=0;
            sim->player_unit.weapons[0].destroyed=1;
            require(mercs_bridge_publish(sim,argv[2],launch.token,argv[3]),"publish controlled non-success return");
            consume(state,argv[2],&launch);dispose(sim,actors,count);free(state);
        }
        puts("PASS: three controlled stock-player integration fixtures; no mission outcomes injected and no victories claimed");
    }else if(argc==5){
        assert(!strcmp(argv[4],"capacity"));
        capacity_boundary(a,argv[2],argv[3]);
    }else{
        const fixed_case *f=select_case(argv[5]);
        mercs_shell_deploy_pilot pilots[3];mercs_exchange_launch launch;
        mercs_shell_state *state=fixture(a,f,pilots);
        if(!strcmp(argv[4],"prepare")){
            assert(argc==6);
            prepare(a,state,pilots,argv[2],argv[3],&launch);
            puts(launch.token);
        }else{
            int i;
            assert(argc==7&&!strcmp(argv[4],"consume"));
            require(mercs_exchange_load(argv[2],argv[6],&launch),"load actual child context");
            for(i=0;i<3;i++)mercs_career_set_deployed(&state->owned[i].current,1);
            consume(state,argv[2],&launch);
        }
        free(state);
    }
    dp_set_write_root(NULL);dp_clear_roots();prj_close(a);return 0;
}
