/* Uses the same bridge as the real simulator and the same import as the real
 * shell. Controlled end states are explicit fixtures, not full playthroughs. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mercsshell_data.h"
#include "mercsbridge.h"
#include "datapath.h"
#include "mek.h"

static void require(int code,const char *error)
{ if(code){fprintf(stderr,"roundtrip: %s (%d)\n",error,code);abort();} }
static void begin(prj_archive *archive,mercs_shell_state *state,int mission,const char *overlay,
                   const char *edition,mercs_exchange_launch *launch)
{
    char error[512];
    memset(launch,0,sizeof *launch);
    require(mercs_shell_prepare_launch(archive,state,mission,overlay,error,sizeof error),error);
    strcpy(launch->scene,state->missions[mission].scene); strcpy(launch->edition,edition);
    launch->unit_count=1; launch->units[0].actor_index=-1; launch->units[0].controller_flags=2;
    strcpy(launch->units[0].name,state->starter_loadout);
    require(mercs_exchange_begin(overlay,launch),"create exchange");
}
static void dispose_sim(msim *sim,world_actor *actors,int count)
{
    int i;msim_free(sim);for(i=0;i<count;i++)mech3d_free(&actors[i].mech);free(actors);
}
static void remove_deployed(const char *overlay,const mercs_career_import_result *imported)
{
    char logical[80],actual[4096];
    if(!imported->request_delete)return;
    snprintf(logical,sizeof logical,"MEK/%s%s",imported->filename,strchr(imported->filename,'.')?"":".MEK");
    assert(!dp_resolve_in(overlay,logical,actual,sizeof actual));assert(!remove(actual));
}
static void cycle(prj_archive *archive,const char *overlay,const char *edition,
                   int training_choice,int outcome,int loss,int missing)
{
    mercs_shell_state *state=calloc(1,sizeof *state),*snapshot=malloc(sizeof *snapshot);
    msim *sim=calloc(1,sizeof *sim);
    mercs_exchange_launch launch;
    mercs_exchange_result packet,bad;
    mercs_career_import_result imported;
    mercs_career_mech reference,current;
    world_actor *actors=NULL;
    unsigned char *original,*after;
    size_t original_size,after_size;
    char error[512],path[64],actual[4096];
    int count,mission=training_choice<0?1:0;
    mek_def initial,redeployed;
    uint32_t instance;
    assert(state&&snapshot&&sim);
    require(mercs_shell_register(archive,state,training_choice<0,"Return Pilot","Return Unit",error,sizeof error),error);
    if(training_choice>=0)require(mercs_shell_assign(archive,state,0,training_choice,error,sizeof error),error);
    begin(archive,state,mission,overlay,edition,&launch);
    reference=state->owned[state->owned_index].reference;
    current=state->owned[state->owned_index].current;
    instance=mercs_career_instance_id(&current);
    assert(instance==(uint32_t)(training_choice<0?11:20+training_choice));
    assert(mercs_career_association_id(&current)==(training_choice<0?12:19));
    assert(mercs_career_association_id(&reference)==-1);
    snprintf(path,sizeof path,"MEK/%s.MEK",state->starter_loadout);
    assert(!dp_read_file(path,&original,&original_size));assert(!mek_parse(original,original_size,&initial));
    assert(!world3d_actors(archive,state->missions[mission].scene,&actors,&count));
    assert(!msim_init(sim,archive,state->missions[mission].scene,actors,count));
    assert(sim->player_unit.return_valid&&sim->player_table==0);
    sim->player_unit.armor[MEK_HEAD]=8.0f; /* original player divisor4 ->2 */
    sim->player_unit.internal[MEK_LA]=0;
    sim->player_unit.weapons[0].destroyed=1;
    sim->player_unit.destroyed=loss!=0;
    sim->player_unit.ejected=loss==2?1:loss==4?2:0;
    sim->logic.result[sim->player_table]=outcome;
    sim->logic.result_time[sim->player_table]=outcome?21000:-1;
    sim->outcome=outcome;sim->now=47000;sim->ended_at=47000;
    if(missing){assert(!dp_resolve_in(overlay,path,actual,sizeof actual));assert(!remove(actual));}
    if(count && training_choice<0 && outcome==0 && !missing) {
        char saved[sizeof actors[0].loadout];memcpy(saved,actors[0].loadout,sizeof saved);
        strcpy(actors[0].loadout,"ABC00usr");
        assert(mercs_bridge_publish(sim,overlay,launch.token,edition)!=0);
        assert(mercs_exchange_read(overlay,&launch,&packet)==MERCS_EXCHANGE_PENDING);
        memcpy(actors[0].loadout,saved,sizeof saved);
    }
    assert(!mercs_bridge_publish(sim,overlay,launch.token,edition));
    if(!missing){assert(!dp_read_file(path,&after,&after_size));assert(original_size==after_size&&!memcmp(original,after,after_size));free(after);}
    require(mercs_exchange_read(overlay,&launch,&packet),"read complete return");
    assert(packet.mech_count==(size_t)!missing);
    *snapshot=*state;
    bad=packet;bad.mission[0]='X';
    assert(mercs_shell_apply_return(state,mission,&launch,&bad,&imported,error,sizeof error)!=0);
    assert(!memcmp(snapshot,state,sizeof *state));
    require(mercs_shell_apply_return(state,mission,&launch,&packet,&imported,error,sizeof error),error);
    assert(imported.action==(missing?MERCS_CAREER_IMPORT_MISSING:MERCS_CAREER_IMPORT_APPLIED));
    assert(imported.request_delete==!missing);
    assert(!memcmp(&reference,&state->owned[state->owned_index].reference,sizeof reference));
    assert(mercs_career_instance_id(&state->owned[state->owned_index].current)==instance);
    assert(mercs_career_association_id(&state->owned[state->owned_index].current)==(training_choice<0?12:19));
    assert(!mercs_career_deployed(&state->owned[state->owned_index].current));
    assert(state->last_return_valid&&state->last_msn.outcome==(unsigned)outcome);
    assert(state->last_msn.resolved_seconds==(outcome?21:-1));
    assert(mercs_car_player_status(&state->last_car)==(loss==2?2:loss?4:0));
    *snapshot=*state;
    assert(mercs_shell_apply_return(state,mission,&launch,&packet,&imported,error,sizeof error)!=0);
    assert(!memcmp(snapshot,state,sizeof *state));
    imported.request_delete=!missing;strcpy(imported.filename,state->starter_loadout);
    remove_deployed(overlay,&imported);
    assert(!mercs_exchange_acknowledge(overlay,&launch));
    assert(mercs_exchange_read(overlay,&launch,&packet)==MERCS_EXCHANGE_APPLIED);
    require(mercs_shell_prepare_launch(archive,state,mission,overlay,error,sizeof error),error);
    assert(!dp_read_file(path,&after,&after_size));assert(!mek_parse(after,after_size,&redeployed));
    assert(redeployed.has_config_extension);
    assert(redeployed.walk_mp==initial.walk_mp); /* preserve shell engine, not rounded return MP */
    assert(redeployed.loc[MEK_HEAD].armor==(missing?initial.loc[MEK_HEAD].armor:2));
    assert(redeployed.loc[MEK_LA].internal==(missing?initial.loc[MEK_LA].internal:0));
    assert(redeployed.item_count==initial.item_count-(missing?0:1));
    if(training_choice>=0){
        mercs_career_mech keep=state->owned[state->owned_index].current;
        mercs_shell_unassign(state,0);
        require(mercs_shell_assign(archive,state,0,training_choice,error,sizeof error),error);
        /* Assignment changes only association, and the same assignment restores it. */
        assert(!memcmp(&keep,&state->owned[state->owned_index].current,sizeof keep));
    }
    free(original);free(after);dispose_sim(sim,actors,count);free(sim);free(snapshot);free(state);
}

static void roster_cycle(prj_archive *archive,const char *overlay,const char *edition,int with_aero)
{
    static const char *const stock[]={"JEN00STD","PNT00STD"};
    mercs_shell_state *state=calloc(1,sizeof *state),*before=malloc(sizeof *before);
    mercs_shell_deploy_pilot pilots[3];mercs_shell_deploy_aircraft aero={0};
    mercs_exchange_launch launch={0},manifest_before;mercs_exchange_result packet,bad;
    mercs_career_import_result imported;mercs_career_mech refs[3];
    msim *sim=calloc(1,sizeof *sim);world_actor *actors=NULL;int count,i,j,actor[2];
    char error[512],name[32],logical[64];uint8_t *star_before,*star_after;size_t size_before,size_after;
    assert(state&&before&&sim);memset(pilots,0,sizeof pilots);
    require(mercs_shell_register(archive,state,1,"Roster Pilot","Roster Unit",error,sizeof error),error);
    for(i=0;i<2;i++){
        prj_record r;
        assert(!prj_read_named(archive,"MEK",stock[i],&r));snprintf(name,sizeof name,"%.3s%02dMER.MEK",stock[i],i+1);
        assert(!mercs_career_pair_init(&state->owned[i+1],r.data,r.size,name,(uint32_t)(100+i),300+i));
        prj_record_free(&r);
    }
    state->owned_count=3;
    for(i=0;i<3;i++){
        pilots[i].owned_index=i;pilots[i].pilot_instance_id=i?(uint32_t)(299+i):state->pilot_instance_id;
        snprintf(pilots[i].name,sizeof pilots[i].name,"Roster Pilot%d",i);
        for(j=0;j<8;j++)pilots[i].attributes[j]=-1;
        if(i)pilots[i].attributes[7]=(int16_t)(i==1?13:17);
        refs[i]=state->owned[i].reference;
    }
    strcpy(aero.stock,"ARO01STD");strcpy(aero.name,"Support Pilot");aero.temporary_instance_id=110;
    for(j=0;j<8;j++)aero.attributes[j]=-1;
    aero.attributes[7]=32;
    require(mercs_shell_prepare_roster(archive,state,1,overlay,pilots,3,with_aero?&aero:NULL,&launch,error,sizeof error),error);
    assert(launch.unit_count==3&&launch.units[0].actor_index==-1&&launch.units[1].actor_index==-2&&launch.units[2].actor_index==-3);
    assert(!dp_read_file("USERSTAR.BWD",&star_before,&size_before));assert(size_before==(with_aero?452u:352u));
    /* A bad later pilot/MEK cannot replace any existing launch or mark a unit. */
    *before=*state;manifest_before=launch;pilots[2].pilot_instance_id++;
    assert(mercs_shell_prepare_roster(archive,state,1,overlay,pilots,3,with_aero?&aero:NULL,&launch,error,sizeof error));
    assert(!memcmp(state,before,sizeof *state)&&!memcmp(&launch,&manifest_before,sizeof launch));pilots[2].pilot_instance_id--;
    assert(!dp_read_file("USERSTAR.BWD",&star_after,&size_after));assert(size_before==size_after&&!memcmp(star_before,star_after,size_before));free(star_before);free(star_after);
    strcpy(launch.scene,state->missions[1].scene);strcpy(launch.edition,edition);assert(!mercs_exchange_begin(overlay,&launch));
    assert(!world3d_actors(archive,launch.scene,&actors,&count));assert(!msim_init(sim,archive,launch.scene,actors,count));
    actor[0]=mercs_bridge_resolve_actor(sim,-2);actor[1]=mercs_bridge_resolve_actor(sim,-3);
    assert(actor[0]>=0&&actor[1]>=0&&actor[0]!=actor[1]);
    assert(sim->pilot_voice[1]==13&&sim->pilot_voice[2]==17);
    for(i=0;i<2;i++){
        assert(sim->armed[actor[i]]&&sim->units[actor[i]].return_valid);
        sim->units[actor[i]].armor[MEK_HEAD]=(float)(i+2);sim->units[actor[i]].internal[MEK_LA]=0;
    }
    if(with_aero){
        int found=0;
        for(i=0;i<count;i++)if(actors[i].table==sim->player_table&&actors[i].gps_slot==3&&!strcmp(actors[i].group,"USERSTAR")){
            assert(!strcmp(actors[i].loadout,"ARO01MER")&&sim->armed[i]);found++;
            assert(msim_point_actor(sim,4)==i&&sim->pilot_voice[4]==32);
        }
        assert(found==1);
    }
    sim->player_unit.armor[MEK_HEAD]=8;sim->units[actor[1]].destroyed=1;
    sim->outcome=2;sim->logic.result[sim->player_table]=2;sim->logic.result_time[sim->player_table]=21000;
    sim->now=47000;sim->ended_at=47000;
    assert(!mercs_bridge_publish(sim,overlay,launch.token,edition));assert(!mercs_exchange_read(overlay,&launch,&packet));
    assert(packet.mech_count==3);
    for(i=0;i<3;i++)assert(packet.mechs[i].actor_index==-i-1);
    *before=*state;bad=packet;bad.mechs[2].bytes[0]=255;
    assert(mercs_shell_apply_return(state,1,&launch,&bad,&imported,error,sizeof error));assert(!memcmp(state,before,sizeof *state));
    bad=packet;bad.mechs[2].actor_index=-2;
    assert(mercs_shell_apply_return(state,1,&launch,&bad,&imported,error,sizeof error));assert(!memcmp(state,before,sizeof *state));
    require(mercs_shell_apply_return(state,1,&launch,&packet,&imported,error,sizeof error),error);
    for(i=0;i<3;i++){
        assert(!memcmp(&refs[i],&state->owned[i].reference,sizeof refs[i]));
        assert(mercs_career_instance_id(&state->owned[i].current)==mercs_career_instance_id(&refs[i]));
        assert(mercs_career_association_id(&state->owned[i].current)==(int32_t)pilots[i].pilot_instance_id);
        assert(!mercs_career_deployed(&state->owned[i].current));
    }
    assert(!mercs_exchange_acknowledge(overlay,&launch));
    require(mercs_shell_prepare_roster(archive,state,1,overlay,pilots,3,with_aero?&aero:NULL,&launch,error,sizeof error),error);
    for(i=0;i<3;i++){
        mek_def m;unsigned char *raw;size_t len;snprintf(logical,sizeof logical,"MEK/%s.MEK",launch.units[i].name);
        assert(!dp_read_file(logical,&raw,&len));assert(!mek_parse(raw,len,&m));free(raw);
        if(i)assert(m.loc[MEK_LA].internal==0);
    }
    dispose_sim(sim,actors,count);free(sim);free(before);free(state);
}
int main(int argc,char **argv)
{
    prj_archive *archive;
    char error[512];
    int i;
    assert(argc==4||argc==5||argc==6||argc==7);
    archive=prj_open(argv[1],error,sizeof error);assert(archive);
    dp_clear_roots();assert(!dp_add_root(argv[2]));
    if(argc==7){
        prj_archive *other=prj_open(argv[5],error,sizeof error);
        mercs_shell_state *from=calloc(1,sizeof *from),*to=calloc(1,sizeof *to),*before=malloc(sizeof *before);
        assert(other&&from&&to&&before&&!strcmp(argv[4],"refresh"));
        for(i=0;i<2;i++){
            int j;
            require(mercs_shell_register(archive,from,i,"Return Pilot","Return Unit",error,sizeof error),error);
            require(mercs_shell_register(other,to,i,"Return Pilot","Return Unit",error,sizeof error),error);
            if(!i){
                require(mercs_shell_assign(archive,from,0,2,error,sizeof error),error);
                require(mercs_shell_assign(other,to,0,2,error,sizeof error),error);
            }
            /* A recognizable already-imported state, including Panther's
             * edition-dependent configuration, must survive a graphics change. */
            from->owned[from->owned_index].current.bytes[0x768]=2;
            from->funds=123456;from->year=3045;from->month=7;
            from->last_return_valid=1;from->last_return_mission=i?1:0;
            strcpy(from->last_return_token,"123456789abcdef0123456789abcdef0");
            from->last_msn.outcome=3;from->last_msn.resolved_seconds=21;
            require(mercs_shell_keep_owned_state(to,from,error,sizeof error),error);
            for(j=0;j<from->owned_count;j++)assert(!memcmp(&to->owned[j],&from->owned[j],sizeof from->owned[j]));
            assert(to->owned_index==from->owned_index&&to->funds==123456&&to->year==3045&&to->month==7);
            assert(to->last_return_valid&&to->last_msn.resolved_seconds==21&&!strcmp(to->last_return_token,from->last_return_token));
            *before=*to;from->pilot_instance_id++;
            assert(mercs_shell_keep_owned_state(to,from,error,sizeof error)!=0&&!memcmp(to,before,sizeof *to));
        }
        printf("PASS: regular and assigned Panther current/reference/results survive %s -> %s refresh; invalid identity is atomic\n",argv[3],argv[6]);
        prj_close(other);free(from);free(to);free(before);
    }else if(argc>=5){
        mercs_shell_state *state=calloc(1,sizeof *state);
        mercs_exchange_launch launch;
        mercs_exchange_result packet;
        mercs_career_import_result imported;
        assert(state);
        require(mercs_shell_register(archive,state,1,"Return Pilot","Return Unit",error,sizeof error),error);
        if(!strcmp(argv[4],"prepare")){
            assert(argc==5);begin(archive,state,1,argv[2],argv[3],&launch);puts(launch.token);
        }else{
            assert(argc==6&&!strcmp(argv[4],"consume"));
            require(mercs_exchange_load(argv[2],argv[5],&launch),"load real child context");
            mercs_career_set_deployed(&state->owned[0].current,1);
            require(mercs_exchange_read(argv[2],&launch,&packet),"real child result");
            require(mercs_shell_apply_return(state,1,&launch,&packet,&imported,error,sizeof error),error);
            assert(imported.action==MERCS_CAREER_IMPORT_APPLIED);
            remove_deployed(argv[2],&imported);assert(!mercs_exchange_acknowledge(argv[2],&launch));
            printf("PASS: actual application child -> CAR214/MSN2516/owned MEK -> shell import (outcome%u, status%d)\n",
                   state->last_msn.outcome,mercs_car_player_status(&state->last_car));
        }
        free(state);
    }else{
        static const int outcomes[]={2,3,4,0,3,3,0};
        static const int losses[]={0,0,0,0,2,4,0};
        for(i=0;i<7;i++)cycle(archive,argv[2],argv[3],-1,outcomes[i],losses[i],i==6);
        for(i=0;i<3;i++)cycle(archive,argv[2],argv[3],i,2,0,0);
        roster_cycle(archive,argv[2],argv[3],0);roster_cycle(archive,argv[2],argv[3],1);
        puts("PASS:12 complete real-data round trips, three owned pilots/voices/dead-mate returns and AeroTech support; exact outcomes, identity/reference and damage retained, atomic malformed-lance rejection, missing-file/replay/USR guards");
    }
    prj_close(archive);dp_set_write_root(NULL);dp_clear_roots();return 0;
}
