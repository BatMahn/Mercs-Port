#include "mercscampaign.h"
#include "mercsstore.h"
#include "mercseconomy.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static void checked(int code,const char *error,int line)
{if(code){fprintf(stderr,"campaign check failed at line %d (%d): %s\n",line,code,error);abort();}}
#define check(code,error) checked((code),(error),__LINE__)
static uint32_t u32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void p32(uint8_t *p,uint32_t v)
{p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static int mission(const mercs_shell_state *v,const char *name)
{int i;for(i=0;i<v->mission_count;i++)if(!strcasecmp(v->missions[i].campaign,name))return i;return -1;}
static void same_document(const mercs_save_state *document,const uint8_t *expected,size_t length)
{uint8_t *bytes;size_t size;assert(!mercs_save_encode(document,0,&bytes,&size));
 assert(size==length&&!memcmp(bytes,expected,size));free(bytes);}
static void controlled_return(mercs_shell_state *v,int selected,int outcome,int status)
{
    mercs_car_init_dos(&v->last_car);
    v->last_car.bytes[MERCS_CAR_PLAYER_STATUS_OFFSET]=(uint8_t)status;
    v->last_car.bytes[0x1e]=2;
    v->last_car.bytes[100]=0xa5; /* full simulator tail survives native resume */
    memset(&v->last_msn,0,sizeof v->last_msn);v->last_msn.outcome=(uint32_t)outcome;
    v->last_msn.row_count=2;v->last_msn.rows[0].succeeded=1;v->last_msn.rows[0].priority=1;
    strcpy((char *)v->last_msn.rows[0].text,"Original target objective");
    v->last_msn.rows[1].succeeded=0;v->last_msn.rows[1].priority=2;
    strcpy((char *)v->last_msn.rows[1].text,"Original bonus objective");
    v->last_return_valid=1;v->last_return_mission=selected;
    strcpy(v->last_return_token,"0123456789abcdef0123456789abcdef");
}
static void check_regular(prj_archive *a,const char *dir)
{
    mercs_campaign c={0},loaded={0};mercs_shell_state *v=calloc(1,sizeof *v),*r=calloc(1,sizeof *r);
    char e[384]={0};int selected;size_t expected_items;int32_t before,after;
    assert(v&&r);check(mercs_campaign_new(a,1,"CHECKER","TEST UNIT",&c,v,e,sizeof e),e);
    assert(v->owned_count==1&&v->funds==1000000&&v->pilot_instance_id==12);
    selected=mission(v,"BET1CAM1");assert(selected>=0);
    check(mercs_campaign_accept(a,&c,v,selected,e,sizeof e),e);
    assert(c.active&&c.accepted==selected&&!strcmp(v->starter_loadout,"PLY00MER"));
    assert(v->missions[selected].launchable);
    /* BET1's authored AMMO quantities are signed -1, hence no shop ammo. */
    assert(mercs_save_list_count(c.document,MERCS_SAVE_SHOP_AMMO)==0);
    {
        uint8_t *bytes;size_t size;mercs_campaign old=c;*r=*v;
        assert(!mercs_save_encode(c.document,0,&bytes,&size));
        assert(mercs_campaign_accept(a,&c,v,selected,e,sizeof e));
        assert(!memcmp(&c,&old,sizeof c)&&!memcmp(v,r,sizeof *v));
        same_document(c.document,bytes,size);free(bytes);
    }
    p32(v->owned[0].current.bytes+0x768,2);
    controlled_return(v,selected,2,0);before=v->funds;
    check(mercs_campaign_return(&c,v,e,sizeof e),e);
    check(mercs_store_write(dir,-1,&c,v,e,sizeof e),e);
    check(mercs_store_read(dir,-1,a,&loaded,r,e,sizeof e),e);
    assert(loaded.phase==MERCS_CAMPAIGN_RETURN&&r->last_car.bytes[100]==0xa5);
    assert(!memcmp(v->news_raw,r->news_raw,sizeof v->news_raw));
    assert(!memcmp(v->owned,r->owned,sizeof v->owned));
    mercs_campaign_free(&c);c=loaded;memset(&loaded,0,sizeof loaded);*v=*r;
    check(mercs_campaign_enter_debrief(a,&c,v,e,sizeof e),e);
    assert(c.phase==MERCS_CAMPAIGN_DEBRIEF&&c.report.contract_income==250000);
    assert(c.report.objective_income==100000&&c.report.kill_income==50000&&c.report.income==400000);
    assert(u32(v->owned[0].current.bytes+0x768)==2);assert(v->funds==before);
    assert(mercs_campaign_enter_debrief(a,&c,v,e,sizeof e));
    expected_items=mercs_save_list_count(c.document,MERCS_SAVE_OWNED_WEAPONS)+mercs_save_list_count(c.document,MERCS_SAVE_OWNED_AMMO);
    {size_t i;for(i=0;i<mercs_save_list_count(c.document,MERCS_SAVE_SALVAGE_ITEMS);i++)
        if(mercs_save_list_record(c.document,MERCS_SAVE_SALVAGE_ITEMS,i)[9]==1)expected_items++;}
    after=c.report.balance;
    check(mercs_store_write(dir,0,&c,v,e,sizeof e),e);
    check(mercs_store_read(dir,0,a,&loaded,r,e,sizeof e),e);
    assert(loaded.phase==MERCS_CAMPAIGN_DEBRIEF&&loaded.rng_state==c.rng_state);
    assert(!memcmp(&c.report,&loaded.report,sizeof c.report));
    mercs_campaign_free(&c);c=loaded;memset(&loaded,0,sizeof loaded);*v=*r;
    check(mercs_campaign_continue(a,&c,v,e,sizeof e),e);
    assert(c.phase==MERCS_CAMPAIGN_READY&&!strcasecmp(c.campaign,"BET2CAM1"));
    assert(v->funds==after&&u32(v->owned[0].current.bytes+0x768)==2);
    assert(mercs_save_list_count(c.document,MERCS_SAVE_OWNED_WEAPONS)+mercs_save_list_count(c.document,MERCS_SAVE_OWNED_AMMO)==expected_items);
    assert(mercs_campaign_continue(a,&c,v,e,sizeof e));assert(v->funds==after);
    check(mercs_store_write(dir,-1,&c,v,e,sizeof e),e);
    check(mercs_store_read(dir,-1,a,&loaded,r,e,sizeof e),e);
    assert(r->funds==after&&loaded.phase==MERCS_CAMPAIGN_READY&&!r->last_return_valid);
    assert(!memcmp(v->owned,r->owned,sizeof v->owned));
    mercs_campaign_free(&loaded);mercs_campaign_free(&c);free(v);free(r);
}
static void check_training(prj_archive *a)
{
    mercs_campaign c={0};mercs_shell_state *v=calloc(1,sizeof *v);char e[384]={0};mercs_career_mech ref;uint32_t id;int i;
    assert(v);check(mercs_campaign_new(a,0,"CADET","TEST UNIT",&c,v,e,sizeof e),e);
    assert(v->owned_count==3&&v->pilot_instance_id==19&&v->owned_index==-1);
    {   /* Mission Command entry (DOS 0x3ca00) gives the unassigned novice owned mech 0 once */
        mercs_campaign t={0};mercs_shell_state *w=calloc(1,sizeof *w);size_t player;
        assert(w);*w=*v;check(mercs_campaign_clone(&c,&t),"clone");
        assert(mercs_economy_command_assign(t.document)==1&&mercs_economy_command_assign(t.document)==0);
        assert(!mercs_save_player(t.document,&player,NULL,NULL,0));
        assert(u32(mercs_save_list_record(t.document,MERCS_SAVE_OWNED_PILOTS,player)+0x20)==
               u32(mercs_save_list_record(t.document,MERCS_SAVE_OWNED_MECHS,0)+0x810));
        assert(u32(mercs_save_list_record(t.document,MERCS_SAVE_OWNED_MECHS,0)+0x80c)==19);
        check(mercs_campaign_hydrate(a,&t,w,e,sizeof e),e);assert(w->owned_index==0);
        mercs_campaign_free(&t);free(w);
    }
    check(mercs_shell_assign_owned(a,v,2,e,sizeof e),e);check(mercs_campaign_sync(&c,v,e,sizeof e),e);
    assert(mercs_career_instance_id(&v->owned[2].current)==22);id=mercs_career_instance_id(&v->owned[2].current);
    ref=v->owned[2].reference;p32(v->owned[2].current.bytes+0x768,1);
    controlled_return(v,0,2,0);check(mercs_campaign_return(&c,v,e,sizeof e),e);
    check(mercs_campaign_enter_debrief(a,&c,v,e,sizeof e),e);
    assert(u32(v->owned[2].current.bytes+0x768)==u32(ref.bytes+0x768));
    assert(mercs_career_association_id(&v->owned[2].reference)==19);
    check(mercs_campaign_continue(a,&c,v,e,sizeof e),e);
    assert(!strcasecmp(c.campaign,"NOVICE2")&&v->owned_count==3&&v->owned_index==2);
    assert(mercs_career_instance_id(&v->owned[2].current)==id);
    assert(v->funds==0&&v->missions[0].launchable);
    for(i=2;i<=4;i++){
        char name[]="NOVICE0";name[6]=(char)('0'+i);assert(!strcasecmp(c.campaign,name));
        controlled_return(v,c.accepted,2,0);check(mercs_campaign_return(&c,v,e,sizeof e),e);
        check(mercs_campaign_enter_debrief(a,&c,v,e,sizeof e),e);
        check(mercs_campaign_continue(a,&c,v,e,sizeof e),e);
        if(i<4)assert(v->owned_count==3&&v->owned_index==2&&mercs_career_instance_id(&v->owned[2].current)==id);
    }
    assert(!c.active&&c.accepted==-1&&v->owned_count==0&&v->owned_index==-1);
    assert(mercs_save_list_count(c.document,MERCS_SAVE_OWNED_PILOTS)==1);
    assert(u32(mercs_save_list_record(c.document,MERCS_SAVE_OWNED_PILOTS,0)+0x20)==UINT32_MAX);
    /* Original novice boarding chooses the saved contract index; terminal
     * cleanup does not create an office contract chooser or reset that field. */
    check(mercs_campaign_accept(a,&c,v,(int)u32(mercs_save_header(c.document)+0x239e),e,sizeof e),e);
    assert(!strcasecmp(c.campaign,"NOVICE5")&&v->owned_count==8&&v->pilot_instance_id==19);
    assert(mercs_career_instance_id(&v->owned[0].current)>id);
    check(mercs_shell_assign_owned(a,v,0,e,sizeof e),e);
    for(i=5;i<=7;i++){
        controlled_return(v,c.accepted,2,0);check(mercs_campaign_return(&c,v,e,sizeof e),e);
        check(mercs_campaign_enter_debrief(a,&c,v,e,sizeof e),e);
        check(mercs_campaign_continue(a,&c,v,e,sizeof e),e);
    }
    assert(!c.active&&v->owned_count==0);
    check(mercs_campaign_accept(a,&c,v,(int)u32(mercs_save_header(c.document)+0x239e),e,sizeof e),e);
    assert(!strcasecmp(c.campaign,"NOVICE8")&&c.active&&v->owned_count==10&&v->missions[c.accepted].launchable);
    assert(!strncasecmp(v->missions[c.accepted].scene,"MCC",3));
    /* An MCC scene is a real mission, not a resource-prefix test for a hub. */
    check(mercs_shell_assign_owned(a,v,0,e,sizeof e),e);
    controlled_return(v,c.accepted,2,0);check(mercs_campaign_return(&c,v,e,sizeof e),e);
    check(mercs_campaign_enter_debrief(a,&c,v,e,sizeof e),e);
    check(mercs_campaign_continue(a,&c,v,e,sizeof e),e);
    assert(!strcasecmp(c.campaign,"NOVICE9")&&c.active&&v->missions[c.accepted].launchable);
    mercs_campaign_free(&c);free(v);
}

static void check_failed_bet1(prj_archive *a)
{
    mercs_campaign c={0};mercs_shell_state *v=calloc(1,sizeof *v);char e[384]={0};
    int selected;int32_t balance;
    assert(v);check(mercs_campaign_new(a,1,"FAILURE","TEST UNIT",&c,v,e,sizeof e),e);
    selected=mission(v,"BET1CAM1");assert(selected>=0);
    check(mercs_campaign_accept(a,&c,v,selected,e,sizeof e),e);
    controlled_return(v,selected,3,0);
    check(mercs_campaign_return(&c,v,e,sizeof e),e);
    check(mercs_campaign_enter_debrief(a,&c,v,e,sizeof e),e);
    assert(!c.report.success);balance=c.report.balance;assert(balance!=v->funds);
    check(mercs_campaign_continue(a,&c,v,e,sizeof e),e);
    assert(c.phase==MERCS_CAMPAIGN_READY&&!strcasecmp(c.campaign,"BET4CAM1"));
    assert(v->funds==balance&&!v->last_return_valid);
    assert(mercs_save_list_count(c.document,MERCS_SAVE_SHOP_WEAPONS)==33);
    assert(mercs_save_list_count(c.document,MERCS_SAVE_SHOP_MECHS)==18);
    assert(mercs_campaign_continue(a,&c,v,e,sizeof e));assert(v->funds==balance);
    mercs_campaign_free(&c);free(v);
}

static void check_reference_identity(prj_archive *a)
{
    mercs_campaign c={0};mercs_shell_state *v=calloc(1,sizeof *v),*old=calloc(1,sizeof *old);
    mercs_career_mech refs[3],temporary;char e[384]={0};int i;
    assert(v&&old);check(mercs_campaign_new(a,0,"IDENTITY","TEST UNIT",&c,v,e,sizeof e),e);
    for(i=0;i<3;i++){refs[i]=v->owned[i].reference;refs[i].bytes[0x84f]=(uint8_t)(71+i);}
    temporary=refs[0];refs[0]=refs[2];refs[2]=temporary;
    assert(!mercs_save_set_list(c.document,MERCS_SAVE_OWNED_REFERENCES,(const uint8_t *)refs,3));
    check(mercs_campaign_hydrate(a,&c,v,e,sizeof e),e);
    for(i=0;i<3;i++)assert(v->owned[i].reference.bytes[0x84f]==71+i);
    *old=*v;
    memcpy(mercs_save_list_record_edit(c.document,MERCS_SAVE_OWNED_REFERENCES,0),
           mercs_save_list_record(c.document,MERCS_SAVE_OWNED_REFERENCES,1),MERCS_CAREER_MECH_BYTES);
    assert(mercs_campaign_hydrate(a,&c,v,e,sizeof e));assert(!memcmp(v,old,sizeof *v));
    mercs_campaign_free(&c);free(v);free(old);
}

static void check_late_failure(prj_archive *a)
{
    mercs_campaign c={0},old;mercs_shell_state *v=calloc(1,sizeof *v),*before=calloc(1,sizeof *before);
    uint8_t *bytes;size_t size;char e[384]={0};int selected;
    assert(v&&before);check(mercs_campaign_new(a,1,"ATOMIC","TEST UNIT",&c,v,e,sizeof e),e);
    selected=mission(v,"BET1CAM1");check(mercs_campaign_accept(a,&c,v,selected,e,sizeof e),e);
    controlled_return(v,selected,2,0);check(mercs_campaign_return(&c,v,e,sizeof e),e);
    check(mercs_campaign_enter_debrief(a,&c,v,e,sizeof e),e);
    /* Fault injection reaches the successor load only after staged funds and
     * selected salvage processing. Missing successor data must roll all
     * of those earlier changes back, including the once-only phase. */
    memset(mercs_save_header_edit(c.document)+0x202,0,16);
    strcpy((char *)mercs_save_header_edit(c.document)+0x202,"MISSING");
    assert(!mercs_save_encode(c.document,0,&bytes,&size));old=c;*before=*v;
    assert(mercs_campaign_continue(a,&c,v,e,sizeof e));
    assert(!memcmp(&c,&old,sizeof c)&&!memcmp(v,before,sizeof *v));
    same_document(c.document,bytes,size);free(bytes);
    mercs_campaign_free(&c);free(v);free(before);
}

static void check_gates(prj_archive *a,const char *dir)
{
    mercs_campaign c={0},loaded={0};mercs_shell_state *v=calloc(1,sizeof *v),*r=calloc(1,sizeof *r);
    mercs_debrief_gate gate;char e[384]={0};int selected,i;int32_t funds;size_t candidates;
    assert(v&&r);check(mercs_campaign_new(a,1,"BANKRUPT","TEST UNIT",&c,v,e,sizeof e),e);
    selected=mission(v,"BET1CAM1");check(mercs_campaign_accept(a,&c,v,selected,e,sizeof e),e);
    v->funds=-10000000;funds=v->funds;controlled_return(v,selected,3,0);
    check(mercs_campaign_return(&c,v,e,sizeof e),e);check(mercs_campaign_enter_debrief(a,&c,v,e,sizeof e),e);
    candidates=mercs_save_list_count(c.document,MERCS_SAVE_SALVAGE_ITEMS);
    check(mercs_campaign_preflight(&c,v,&gate,e,sizeof e),e);
    assert(gate.reason==MERCS_DEBRIEF_GATE_BANKRUPT&&gate.career_end&&!strcmp(gate.movies[0],"mpoor"));
    check(mercs_campaign_continue(a,&c,v,e,sizeof e),e);
    assert(c.phase==MERCS_CAMPAIGN_ENDED&&!c.active&&c.accepted==-1&&v->funds==funds&&v->owned_count==1);
    assert(mercs_save_list_count(c.document,MERCS_SAVE_SALVAGE_ITEMS)==candidates);
    check(mercs_store_write(dir,6,&c,v,e,sizeof e),e);check(mercs_store_read(dir,6,a,&loaded,r,e,sizeof e),e);
    assert(loaded.phase==MERCS_CAMPAIGN_ENDED&&loaded.accepted==-1&&r->funds==funds);
    mercs_campaign_free(&c);mercs_campaign_free(&loaded);

    check(mercs_campaign_new(a,1,"EJECTED","TEST UNIT",&c,v,e,sizeof e),e);
    selected=mission(v,"BET1CAM1");check(mercs_campaign_accept(a,&c,v,selected,e,sizeof e),e);
    funds=v->funds;controlled_return(v,selected,3,2);
    check(mercs_campaign_return(&c,v,e,sizeof e),e);check(mercs_campaign_enter_debrief(a,&c,v,e,sizeof e),e);
    check(mercs_campaign_preflight(&c,v,&gate,e,sizeof e),e);
    assert(gate.reason==MERCS_DEBRIEF_GATE_NO_MECHS&&!gate.career_end&&!strcasecmp(gate.campaign,"BET5CAM1"));
    check(mercs_campaign_continue(a,&c,v,e,sizeof e),e);
    assert(c.phase==MERCS_CAMPAIGN_READY&&!c.active&&c.accepted==-1&&!strcasecmp(c.campaign,"BET5CAM1"));
    assert(v->funds==funds&&v->owned_count==0&&mercs_save_header(c.document)[0x239c]==0);
    assert(mercs_save_header(c.document)[0x239d]==1&&mercs_save_header(c.document)[0x23a2]==0);
    mercs_campaign_free(&c);

    for(i=0;i<2;i++){
        const char names[1][16]={"LUT4CAM1"};
        check(mercs_campaign_new(a,1,"ENDING","TEST UNIT",&c,v,e,sizeof e),e);
        check(mercs_save_load_campaign(a,c.document,"LUT4CAM1",e,sizeof e),e);
        check(mercs_campaign_hydrate(a,&c,v,e,sizeof e),e);
        check(mercs_shell_set_missions(a,v,names,1,e,sizeof e),e);
        strcpy(c.campaign,"LUT4CAM1");c.active=1;c.accepted=c.selected=0;
        v->funds=i?50000000:1000000;funds=v->funds;controlled_return(v,0,2,0);
        check(mercs_campaign_return(&c,v,e,sizeof e),e);check(mercs_campaign_enter_debrief(a,&c,v,e,sizeof e),e);
        check(mercs_campaign_preflight(&c,v,&gate,e,sizeof e),e);
        assert(gate.reason==MERCS_DEBRIEF_GATE_ENDING&&gate.movie_count==2);
        assert(!strcmp(gate.movies[0],"mend")&&!strcmp(gate.movies[1],i?"mrich":"mpoor"));
        check(mercs_campaign_continue(a,&c,v,e,sizeof e),e);
        assert(c.phase==MERCS_CAMPAIGN_ENDED&&v->funds==funds);
        mercs_campaign_free(&c);
    }
    free(v);free(r);
}
static void check_loss(prj_archive *a,int mode,int status)
{
    mercs_campaign c={0};mercs_shell_state *v=calloc(1,sizeof *v);char e[384]={0};int selected=0;int32_t funds;
    assert(v);check(mercs_campaign_new(a,mode,"LOSS","TEST UNIT",&c,v,e,sizeof e),e);
    if(mode){selected=mission(v,"BET1CAM1");check(mercs_campaign_accept(a,&c,v,selected,e,sizeof e),e);}
    else check(mercs_shell_assign_owned(a,v,0,e,sizeof e),e);
    funds=v->funds;controlled_return(v,selected,3,status);
    check(mercs_campaign_return(&c,v,e,sizeof e),e);
    assert(c.entry.movie==(status==2?MERCS_DEBRIEF_MOVIE_EJECT:MERCS_DEBRIEF_MOVIE_DEATH));
    check(mercs_campaign_enter_debrief(a,&c,v,e,sizeof e),e);
    if(mode&&status==4)assert(c.phase==MERCS_CAMPAIGN_ENDED&&v->funds==funds);
    else if(mode)assert(v->owned_count==0&&v->owned_index==-1&&v->funds==funds);
    else assert(v->owned_count==3&&c.phase==MERCS_CAMPAIGN_DEBRIEF&&v->funds==0);
    mercs_campaign_free(&c);free(v);
}
static void check_corruption(prj_archive *a,const char *dir)
{
    mercs_campaign c={0},out={0};mercs_shell_state *v=calloc(1,sizeof *v),*target=calloc(1,sizeof *target);
    uint8_t *original,resume[1024],changed[1024];size_t size,i;char e[384]={0};mercs_save_state *sentinel;
    static const struct {size_t offset;uint32_t value;} invalid[]={
        {16,MERCS_CAMPAIGN_DEBRIEF},{16,MERCS_CAMPAIGN_DEPLOYED},
        {24,0},{36,1},{48,0},{32,32},{520,1}
    };
    assert(v&&target);check(mercs_campaign_new(a,1,"SERIAL","TEST UNIT",&c,v,e,sizeof e),e);
    check(mercs_save_encode(c.document,1,&original,&size),e);
    check(mercs_campaign_encode_resume(&c,v,original,size,resume),e);
    out.document=mercs_save_create();sentinel=out.document;target->funds=7654321;
    memcpy(changed,resume,sizeof changed);changed[261]^=1;
    assert(mercs_campaign_decode_resume(a,original,size,changed,sizeof changed,&out,target,e,sizeof e));
    assert(out.document==sentinel&&target->funds==7654321);
    assert(mercs_campaign_decode_resume(a,original,size,resume,1023,&out,target,e,sizeof e));
    for(i=0;i<sizeof invalid/sizeof invalid[0];i++){
        memcpy(changed,resume,sizeof changed);p32(changed+invalid[i].offset,invalid[i].value);
        p32(changed+1020,mercs_campaign_crc(changed,1020));
        assert(mercs_campaign_decode_resume(a,original,size,changed,1024,&out,target,e,sizeof e));
        assert(out.document==sentinel&&target->funds==7654321);
    }
    original[size-1]^=1;
    assert(mercs_campaign_decode_resume(a,original,size,resume,1024,&out,target,e,sizeof e));
    assert(out.document==sentinel&&target->funds==7654321);
    assert(mercs_store_read(dir,9,a,&out,target,e,sizeof e)==1);
    free(original);mercs_campaign_free(&out);mercs_campaign_free(&c);free(v);free(target);
}

static void check_store_failure(prj_archive *a,const char *dir)
{
    mercs_campaign c={0},read={0};mercs_shell_state *v=calloc(1,sizeof *v),*r=calloc(1,sizeof *r);
    char e[384]={0},title[256],path[4224],token[34]={0};FILE *f;int ch;int32_t funds;
    assert(v&&r);check(mercs_campaign_new(a,1,"STORE","TEST UNIT",&c,v,e,sizeof e),e);
    strcpy((char *)mercs_save_header_edit(c.document)+2,"BEFORE FIRST CONTRACT");funds=v->funds;
    check(mercs_store_write(dir,8,&c,v,e,sizeof e),e);
    assert(!mercs_store_title(dir,8,title,sizeof title)&&!strcmp(title,"BEFORE FIRST CONTRACT"));
    v->funds=1234;p32(v->owned[0].reference.bytes+0x810,999);
    assert(mercs_store_write(dir,8,&c,v,e,sizeof e));
    check(mercs_store_read(dir,8,a,&read,r,e,sizeof e),e);assert(r->funds==funds);
    assert(!memcmp(r->news_raw,v->news_raw,sizeof r->news_raw));
    snprintf(path,sizeof path,"%s/SLOT8.REF",dir);f=fopen(path,"rb");assert(f);
    assert(fread(token,1,33,f)==33&&!fclose(f));token[32]=0;
    snprintf(path,sizeof path,"%s/%s/NATIVE.DAT",dir,token);f=fopen(path,"r+b");assert(f);
    assert(!fseek(f,700,SEEK_SET));ch=fgetc(f);assert(ch!=EOF&&!fseek(f,700,SEEK_SET));
    assert(fputc(ch^1,f)!=EOF&&!fclose(f));
    assert(mercs_store_title(dir,8,title,sizeof title)==-1&&title[0]==0);
    mercs_campaign_free(&read);mercs_campaign_free(&c);free(v);free(r);
}
int main(int argc,char **argv)
{
    prj_archive *a;char e[384],dir[4096];
    if(argc!=3){fprintf(stderr,"usage: %s MW2.PRJ PRIVATE_OVERLAY\n",argv[0]);return 2;}
    a=prj_open(argv[1],e,sizeof e);assert(a);check(mercs_store_directory(argv[2],dir,sizeof dir),"private save directory");
    check_regular(a,dir);check_failed_bet1(a);check_training(a);check_reference_identity(a);check_late_failure(a);check_gates(a,dir);
    check_loss(a,1,2);check_loss(a,1,4);check_loss(a,0,2);check_loss(a,0,4);check_corruption(a,dir);check_store_failure(a,dir);
    prj_close(a);puts("PASS: regular settlement/reload; training chain/terminal cleanup/boarding/MCC missions; identity pairing; four loss branches; bankruptcy/wealth/no-mech gates; rollback, canonical metadata, corrupt slots, and once-only checks");return 0;
}
