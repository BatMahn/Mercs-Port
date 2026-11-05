#include "mercsinterchange.h"
#include "mercsstore.h"
#include "mercseconomy.h"
#include "mercsrepair.h"
#include "mercsmechlab.h"
#include "datapath.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* This standalone target compiles its sources with
 * -Dtime=mercs_interchange_test_time. Shipping and UI objects retain time(). */
static time_t test_clock=1700000000;
time_t mercs_interchange_test_time(time_t *out)
{if(out)*out=test_clock;return test_clock;}
enum { PREVIEWS_OFFSET=532, PREVIEWS_BYTES=84 };

static void checked(int rc,const char *error,int line)
{if(rc){fprintf(stderr,"interchange line%d rc%d: %s\n",line,rc,error);abort();}}
#define check(c,e) checked((c),(e),__LINE__)
static uint32_t u32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void p32(uint8_t *p,uint32_t v)
{p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static void write_file(const char *dir,const char *name,const uint8_t *bytes,size_t size)
{char path[4096];FILE *f;snprintf(path,sizeof path,"%s/%s",dir,name);f=fopen(path,"wb");assert(f);
 assert(fwrite(bytes,1,size,f)==size&&!fclose(f));}
static int find_mission(const mercs_shell_state *v,const char *name)
{int i;for(i=0;i<v->mission_count;i++)if(!strcasecmp(v->missions[i].campaign,name))return i;return -1;}
static void resume_crc(uint8_t resume[1024])
{p32(resume+1020,mercs_campaign_crc(resume,1020));}
static void reject_resume(prj_archive *a,const uint8_t *bytes,size_t size,const uint8_t resume[1024])
{
    mercs_campaign out={0},old;mercs_shell_state *v=malloc(sizeof *v),*before=malloc(sizeof *before);
    prj_record *graph=NULL,*after=NULL;size_t count=0,after_count=0,i;char error[384];
    assert(v&&before);out.phase=17;memcpy(&old,&out,sizeof old);
    memset(v,0x5b,sizeof *v);memcpy(before,v,sizeof *v);assert(!prj_get_generated(a,&graph,&count));
    assert(mercs_campaign_decode_resume(a,bytes,size,resume,1024,&out,v,error,sizeof error));
    assert(!memcmp(&old,&out,sizeof out)&&!memcmp(v,before,sizeof *v));
    assert(!prj_get_generated(a,&after,&after_count)&&count==after_count);
    for(i=0;i<count;i++)assert(!strcmp(graph[i].name,after[i].name)&&!strcmp(graph[i].tag,after[i].tag)&&
        graph[i].size==after[i].size&&!memcmp(graph[i].data,after[i].data,graph[i].size));
    prj_free_generated(graph,count);prj_free_generated(after,after_count);free(v);free(before);
}
static void result(mercs_campaign *c,mercs_shell_state *v,prj_archive *a)
{
    char error[384];mercs_car_init_dos(&v->last_car);v->last_car.bytes[0x1e]=2;
    v->last_car.bytes[100]=0x9a;memset(&v->last_msn,0,sizeof v->last_msn);
    v->last_msn.outcome=2;v->last_msn.row_count=1;v->last_msn.rows[0].succeeded=1;
    v->last_msn.rows[0].priority=1;strcpy((char*)v->last_msn.rows[0].text,"Original saved objective");
    v->last_return_valid=1;v->last_return_mission=c->accepted;
    strcpy(v->last_return_token,"0123456789abcdef0123456789abcdef");
    check(mercs_campaign_return(c,v,error,sizeof error),error);
    check(mercs_campaign_enter_debrief(a,c,v,error,sizeof error),error);
}
static void roundtrip(prj_archive *a,mercs_campaign *c,mercs_shell_state *v,
                       uint32_t event,const char *dir,const char *name)
{
    mercs_campaign imported={0},reload={0};mercs_shell_state *r=calloc(1,sizeof *r),*again=calloc(1,sizeof *again);
    mercs_save_state *expected;uint8_t *bytes,*wanted,*actual,resume[1024];size_t size,ws,as;
    char error[384];uint32_t got=0;size_t i;
    assert(r&&again);mercs_save_header_edit(c->document)[0x23bf]=0x9b;
    check(mercs_interchange_prepare(c,v,event,error,sizeof error),error);
    if(c->phase==MERCS_CAMPAIGN_DEBRIEF)check(mercs_campaign_report(c,v,error,sizeof error),error);
    assert(!mercs_save_encode(c->document,1,&bytes,&size));write_file(dir,name,bytes,size);
    expected=mercs_save_clone(c->document);assert(expected);mercs_save_restore_markers(expected);
    mercs_save_header_edit(expected)[0x239a]=mercs_save_header_edit(expected)[0x239b]=0;
    assert(!mercs_save_encode(expected,1,&wanted,&ws));mercs_save_free(expected);
    test_clock+=17; /* Original import must use cached offers, not this new clock. */
    check(mercs_interchange_import(a,bytes,size,&imported,r,&got,error,sizeof error),error);
    assert(got==event&&imported.phase==c->phase&&imported.active==c->active);
    assert(!strcasecmp(imported.campaign,c->campaign));assert(r->funds==v->funds&&r->owned_count==v->owned_count);
    if(!c->active)assert(!memcmp(r->missions,v->missions,sizeof r->missions));
    assert(!mercs_save_encode(imported.document,0,&actual,&as));assert(as==ws&&!memcmp(actual,wanted,as));free(actual);
    if(c->phase==MERCS_CAMPAIGN_DEBRIEF){
        assert(r->last_return_valid&&r->last_return_mission==imported.accepted);
        assert(imported.report.balance==c->report.balance&&imported.report.income==c->report.income);
        assert(!memcmp(r->last_car.bytes,v->last_car.bytes,80));
        for(i=80;i<MERCS_CAR_BYTES;i++)assert(!r->last_car.bytes[i]);
        assert(!imported.report.select_salvage);
    }else assert(!r->last_return_valid&&r->last_return_mission==-1);
    assert(!mercs_campaign_encode_resume(&imported,r,wanted,ws,resume));
    test_clock+=23; /* Native resume must preserve every preview across clock changes. */
    check(mercs_campaign_decode_resume(a,wanted,ws,resume,sizeof resume,&reload,again,error,sizeof error),error);
    assert(reload.phase==imported.phase&&reload.procedural_seed_override==imported.procedural_seed_override);
    assert(!memcmp(r,again,sizeof *r));
    if(imported.active&&!strncasecmp(imported.campaign,"RAN",3)){
        uint8_t invalid[1024];size_t row=PREVIEWS_OFFSET+4+(size_t)imported.accepted*8;
        mercs_campaign old_loaded={0};mercs_shell_state *old_view=calloc(1,sizeof *old_view);
        prj_record *graph=NULL,*after=NULL;size_t count=0,after_count=0;
        assert(old_view&&!prj_get_generated(a,&graph,&count));
        memcpy(invalid,resume,sizeof invalid);memset(invalid+PREVIEWS_OFFSET,0,PREVIEWS_BYTES);resume_crc(invalid);
        test_clock+=3600;
        check(mercs_campaign_decode_resume(a,wanted,ws,invalid,sizeof invalid,&old_loaded,old_view,error,sizeof error),error);
        assert(!memcmp(r,old_view,sizeof *r)&&old_loaded.procedural_year==imported.procedural_year&&
               old_loaded.procedural_seed_override==imported.procedural_seed_override);
        assert(!prj_get_generated(a,&after,&after_count)&&count==after_count);
        for(i=0;i<count;i++)assert(!strcmp(graph[i].name,after[i].name)&&!strcmp(graph[i].tag,after[i].tag)&&
            graph[i].size==after[i].size&&!memcmp(graph[i].data,after[i].data,graph[i].size));
        prj_free_generated(graph,count);prj_free_generated(after,after_count);mercs_campaign_free(&old_loaded);free(old_view);
        memcpy(invalid,resume,sizeof invalid);p32(invalid+row,u32(invalid+row)^1u);resume_crc(invalid);
        reject_resume(a,wanted,ws,invalid);
        memcpy(invalid,resume,sizeof invalid);p32(invalid+row+4,u32(invalid+row+4)+1);resume_crc(invalid);
        reject_resume(a,wanted,ws,invalid);
    }
    printf("PASS standalone %s: campaign=%s event=%x phase=%d exact restored document\n",name,imported.campaign,got,imported.phase);
    free(bytes);free(wanted);mercs_campaign_free(&imported);mercs_campaign_free(&reload);free(r);free(again);
}

static void preview_continuations(prj_archive *a,const mercs_campaign *source,const mercs_shell_state *view)
{
    mercs_campaign c={0},loaded={0},again={0};
    mercs_shell_state *v=malloc(sizeof *v),*r=calloc(1,sizeof *r),*next=calloc(1,sizeof *next);
    mercs_shell_mission *expected=malloc(sizeof *expected);uint8_t *bytes,resume[1024],legacy[1024],bad[1024];
    uint8_t history[80];size_t size,row;int ran,authored;uint32_t event;char error[384];
    assert(v&&r&&next&&expected&&!mercs_campaign_clone(source,&c));*v=*view;
    ran=find_mission(v,"RAN1CAM1");authored=find_mission(v,"BET1CAM1");assert(ran>=0&&authored>=0);
    row=PREVIEWS_OFFSET+4+(size_t)ran*8;
    check(mercs_shell_mission_load_seeded(a,"RAN1CAM1",v->pilot,3044,0,&v->missions[ran],error,sizeof error),error);
    assert(v->missions[ran].launchable);v->year=3045;
    check(mercs_campaign_sync(&c,v,error,sizeof error),error);
    assert(u32(mercs_save_header(c.document)+0x2342+4*(size_t)ran)==UINT32_MAX);
    assert(!mercs_save_encode(c.document,1,&bytes,&size));
    assert(!mercs_campaign_encode_resume(&c,v,bytes,size,resume));
    assert(!memcmp(resume+PREVIEWS_OFFSET,"PRV1",4)&&u32(resume+row)==0&&u32(resume+row+4)==3044);
    test_clock+=3600;
    check(mercs_campaign_decode_resume(a,bytes,size,resume,sizeof resume,&loaded,r,error,sizeof error),error);
    assert(r->year==3045&&r->missions[ran].procedural_year==3044&&r->missions[ran].procedural_seed==0);
    assert(!memcmp(v->missions,r->missions,sizeof v->missions));mercs_campaign_free(&loaded);
    memcpy(bad,resume,sizeof bad);bad[PREVIEWS_OFFSET]='X';resume_crc(bad);reject_resume(a,bytes,size,bad);
    memcpy(bad,resume,sizeof bad);p32(bad+row+4,3043);resume_crc(bad);reject_resume(a,bytes,size,bad);
    memcpy(bad,resume,sizeof bad);p32(bad+PREVIEWS_OFFSET+4+(size_t)authored*8,1);resume_crc(bad);reject_resume(a,bytes,size,bad);
    memcpy(bad,resume,sizeof bad);bad[PREVIEWS_OFFSET+4+9*8]=1;resume_crc(bad);reject_resume(a,bytes,size,bad);
    memcpy(bad,resume,sizeof bad);bad[PREVIEWS_OFFSET+PREVIEWS_BYTES]=1;resume_crc(bad);reject_resume(a,bytes,size,bad);
    memcpy(bad,resume,sizeof bad);memset(bad+PREVIEWS_OFFSET,0,4);resume_crc(bad);reject_resume(a,bytes,size,bad);

    /* Legacy files without a history seed cannot reconstruct an unstored
     * offer. One fresh preview is generated, then its next save persists it. */
    memcpy(legacy,resume,sizeof legacy);memset(legacy+PREVIEWS_OFFSET,0,PREVIEWS_BYTES);resume_crc(legacy);
    test_clock+=3600;
    check(mercs_campaign_decode_resume(a,bytes,size,legacy,sizeof legacy,&loaded,r,error,sizeof error),error);
    assert(r->missions[ran].procedural_seed!=0&&r->missions[ran].procedural_year==3045);
    assert(!mercs_campaign_encode_resume(&loaded,r,bytes,size,resume));test_clock+=3600;
    check(mercs_campaign_decode_resume(a,bytes,size,resume,sizeof resume,&again,next,error,sizeof error),error);
    assert(!memcmp(r,next,sizeof *r));mercs_campaign_free(&loaded);mercs_campaign_free(&again);free(bytes);

    /* A legacy original history seed of zero is present. Its generation year
     * is the current original saved year, not an invented header field. */
    p32(mercs_save_header_edit(c.document)+0x2342+4*(size_t)ran,0);
    assert(!mercs_save_encode(c.document,1,&bytes,&size));
    assert(!mercs_campaign_encode_resume(&c,v,bytes,size,resume));
    memcpy(legacy,resume,sizeof legacy);memset(legacy+PREVIEWS_OFFSET,0,PREVIEWS_BYTES);resume_crc(legacy);test_clock+=3600;
    check(mercs_campaign_decode_resume(a,bytes,size,legacy,sizeof legacy,&loaded,r,error,sizeof error),error);
    check(mercs_shell_mission_load_seeded(a,"RAN1CAM1",v->pilot,3045,0,expected,error,sizeof error),error);
    assert(!memcmp(expected,&r->missions[ran],sizeof *expected));mercs_campaign_free(&loaded);free(bytes);

    /* Original inactive-history checks are signed: 0x80000000 is absent,
     * while the explicit native block can preserve all32 seed bits. */
    check(mercs_interchange_prepare(&c,v,MERCS_ORIGINAL_HALL,error,sizeof error),error);
    assert(!mercs_save_encode(c.document,1,&bytes,&size));test_clock+=3600;
    check(mercs_interchange_import(a,bytes,size,&loaded,r,&event,error,sizeof error),error);
    assert(event==MERCS_ORIGINAL_HALL&&!memcmp(expected,&r->missions[ran],sizeof *expected));
    mercs_campaign_free(&loaded);free(bytes);
    p32(mercs_save_header_edit(c.document)+0x2342+4*(size_t)ran,0x80000000u);
    assert(!mercs_save_encode(c.document,1,&bytes,&size));test_clock+=3600;
    assert(!mercs_campaign_encode_resume(&c,v,bytes,size,resume));
    memcpy(legacy,resume,sizeof legacy);memset(legacy+PREVIEWS_OFFSET,0,PREVIEWS_BYTES);resume_crc(legacy);
    check(mercs_campaign_decode_resume(a,bytes,size,legacy,sizeof legacy,&loaded,r,error,sizeof error),error);
    assert(r->missions[ran].procedural_seed<=INT32_MAX&&r->missions[ran].procedural_year==3045);
    mercs_campaign_free(&loaded);test_clock+=3600;
    check(mercs_interchange_import(a,bytes,size,&loaded,r,&event,error,sizeof error),error);
    assert(event==MERCS_ORIGINAL_HALL&&r->missions[ran].procedural_seed<=INT32_MAX&&
           u32(mercs_save_header(loaded.document)+0x2342+4*(size_t)ran)==0x80000000u);
    mercs_campaign_free(&loaded);free(bytes);
    check(mercs_shell_mission_load_seeded(a,"RAN1CAM1",v->pilot,3044,UINT32_MAX,&v->missions[ran],error,sizeof error),error);
    assert(!mercs_save_encode(c.document,1,&bytes,&size));
    assert(!mercs_campaign_encode_resume(&c,v,bytes,size,resume));test_clock+=3600;
    check(mercs_campaign_decode_resume(a,bytes,size,resume,sizeof resume,&loaded,r,error,sizeof error),error);
    assert(r->missions[ran].procedural_seed==UINT32_MAX&&!memcmp(v->missions,r->missions,sizeof v->missions));
    mercs_campaign_free(&loaded);free(bytes);

    /* An active authored mission retains other offers in its native view.
     * Export must not remap these former offers into the replacement CPGN. */
    check(mercs_campaign_accept(a,&c,v,authored,error,sizeof error),error);
    memcpy(history,mercs_save_header(c.document)+0x2342,sizeof history);
    check(mercs_interchange_prepare(&c,v,MERCS_ORIGINAL_COMMAND,error,sizeof error),error);
    assert(!memcmp(history,mercs_save_header(c.document)+0x2342,sizeof history));
    assert(!mercs_save_encode(c.document,1,&bytes,&size));
    assert(!mercs_campaign_encode_resume(&c,v,bytes,size,resume));test_clock+=3600;
    check(mercs_campaign_decode_resume(a,bytes,size,resume,sizeof resume,&loaded,r,error,sizeof error),error);
    assert(loaded.active&&!strcmp(loaded.campaign,"BET1CAM1")&&!memcmp(v->missions,r->missions,sizeof v->missions));
    mercs_campaign_free(&loaded);mercs_campaign_free(&c);free(bytes);free(v);free(r);free(next);free(expected);
    puts("PASS deterministic preview continuation: changed clock/year, seed0/full32, inactive/active offers, legacy cache/fallback and transactional metadata rejection");
}
static void rejection(prj_archive *a,mercs_campaign *c,mercs_shell_state *v)
{
    mercs_campaign out={0},old={0};mercs_shell_state *r=calloc(1,sizeof *r),*before=calloc(1,sizeof *before);
    uint8_t *bytes,*edited;size_t size;uint32_t event=0xabcdef;char error[384];
    assert(r&&before);check(mercs_interchange_prepare(c,v,MERCS_ORIGINAL_COMMAND,error,sizeof error),error);
    assert(!mercs_save_encode(c->document,1,&bytes,&size));edited=malloc(size);assert(edited);
    out.phase=17;old=out;memset(r,0x6b,sizeof *r);*before=*r;
    assert(mercs_interchange_import(a,bytes,size-1,&out,r,&event,error,sizeof error));
    assert(!memcmp(&out,&old,sizeof out)&&!memcmp(r,before,sizeof *r)&&event==0xabcdef);
    memcpy(edited,bytes,size);p32(edited+0x23c7,0xdead);
    assert(mercs_interchange_import(a,edited,size,&out,r,&event,error,sizeof error));
    memcpy(edited,bytes,size);p32(edited+0x1e6,123456789);
    assert(mercs_interchange_import(a,edited,size,&out,r,&event,error,sizeof error));
    assert(!memcmp(&out,&old,sizeof out)&&!memcmp(r,before,sizeof *r)&&event==0xabcdef);
    free(bytes);free(edited);free(r);free(before);puts("PASS malformed/truncated/unknown context reject without mutation");
}
static mercs_save_state *windows_document(const mercs_save_state *source)
{
    mercs_save_state *out=NULL;uint8_t *bytes,*windows;size_t size,extra=0,i,j,at=MERCS_SAVE_HEADER_BYTES,to=MERCS_SAVE_HEADER_BYTES;
    assert(!mercs_save_windows(source)&&!mercs_save_encode(source,1,&bytes,&size));
    for(i=0;i<14;i++)if(mercs_save_list_stride((enum mercs_save_list)i)==0x850)
        extra+=3*mercs_save_list_count(source,(enum mercs_save_list)i);
    windows=malloc(size+extra);assert(windows);memcpy(windows,bytes,MERCS_SAVE_HEADER_BYTES);
    for(i=0;i<14;i++){size_t stride=mercs_save_list_stride((enum mercs_save_list)i);
        for(j=0;j<mercs_save_list_count(source,(enum mercs_save_list)i);j++){
            memcpy(windows+to,bytes+at,stride);at+=stride;to+=stride;
            if(stride==0x850){windows[to++]=(uint8_t)(17+i);windows[to++]=(uint8_t)(31+j);windows[to++]=0xa5;}
        }}
    assert(to==size+extra&&at==size);assert(!mercs_save_decode(windows,to,&out)&&mercs_save_windows(out));
    free(bytes);assert(!mercs_save_encode(out,0,&bytes,&size));assert(size==to&&!memcmp(bytes,windows,size));
    free(bytes);free(windows);return out;
}
static void windows_cycle(prj_archive *a,const mercs_campaign *source,const mercs_shell_state *view,const char *directory)
{
    mercs_campaign c=*source;mercs_shell_state *v=malloc(sizeof *v);mercs_save_state *training,*finished=NULL;
    mercs_mechlab_session *lab=NULL;uint8_t current[3],reference[3],player[3];size_t bought,shop,last,i,n;uint32_t id;
    char error[384];assert(v);*v=*view;c.document=windows_document(source->document);
    roundtrip(a,&c,v,MERCS_ORIGINAL_COMMAND,directory,"windows-bet1.sav");
    memcpy(player,mercs_save_mech_extension(c.document,MERCS_SAVE_OWNED_MECHS,0),3);
    memcpy(current,mercs_save_mech_extension(c.document,MERCS_SAVE_SHOP_MECHS,0),3);
    memcpy(reference,mercs_save_mech_extension(c.document,MERCS_SAVE_SHOP_REFERENCES,0),3);
    p32(mercs_save_header_edit(c.document)+0x12a,100000000);bought=mercs_save_list_count(c.document,MERCS_SAVE_OWNED_MECHS);
    assert(!mercs_economy_trade_mech(c.document,1,0));assert(bought==1);
    assert(!memcmp(current,mercs_save_mech_extension(c.document,MERCS_SAVE_OWNED_MECHS,bought),3));
    assert(!memcmp(reference,mercs_save_mech_extension(c.document,MERCS_SAVE_OWNED_REFERENCES,bought),3));
    assert(!mercs_economy_assign_pilot(c.document,0,(int)bought));
    assert(!mercs_economy_weapon_group(c.document,bought,-1));
    assert(!mercs_mechlab_begin(c.document,0,&lab));assert(!mercs_mechlab_rename(lab,"WINDOWS DESIGN"));
    assert(!mercs_mechlab_finish(lab,1,&finished));mercs_mechlab_free(lab);mercs_save_free(c.document);c.document=finished;
    id=u32(mercs_save_list_record(c.document,MERCS_SAVE_OWNED_MECHS,0)+0x810);
    n=mercs_save_list_count(c.document,MERCS_SAVE_OWNED_REFERENCES);last=n;
    for(i=0;i<n;i++)if(u32(mercs_save_list_record(c.document,MERCS_SAVE_OWNED_REFERENCES,i)+0x810)==id)last=i;
    assert(last<n&&!memcmp(player,mercs_save_mech_extension(c.document,MERCS_SAVE_OWNED_REFERENCES,last),3));
    assert(!mercs_economy_trade_mech(c.document,0,bought));shop=mercs_save_list_count(c.document,MERCS_SAVE_SHOP_MECHS);
    assert(!memcmp(current,mercs_save_mech_extension(c.document,MERCS_SAVE_SHOP_MECHS,shop-1),3));
    assert(!memcmp(reference,mercs_save_mech_extension(c.document,MERCS_SAVE_SHOP_REFERENCES,shop-1),3));
    check(mercs_campaign_hydrate(a,&c,v,error,sizeof error),error);
    roundtrip(a,&c,v,MERCS_ORIGINAL_COMMAND,directory,"windows-traded-lab.sav");
    /* Training references are new whole-record copies of the shop current
     * record, not the possibly different former shop reference extension. */
    training=mercs_save_clone(c.document);assert(training);p32(mercs_save_header_edit(training)+0x23bb,0);
    memcpy(current,mercs_save_mech_extension(training,MERCS_SAVE_SHOP_MECHS,0),3);
    id=u32(mercs_save_list_record(training,MERCS_SAVE_SHOP_MECHS,0)+0x810);
    assert(!mercs_save_transfer_training(training));n=mercs_save_list_count(training,MERCS_SAVE_OWNED_MECHS);last=n;
    for(i=0;i<n;i++)if(u32(mercs_save_list_record(training,MERCS_SAVE_OWNED_MECHS,i)+0x810)==id)last=i;
    assert(last<n&&!memcmp(current,mercs_save_mech_extension(training,MERCS_SAVE_OWNED_MECHS,last),3));
    assert(!memcmp(current,mercs_save_mech_extension(training,MERCS_SAVE_OWNED_REFERENCES,last),3));
    mercs_save_free(training);
    /* A selected original salvage record moves whole; its newly loaded stock
     * reference has a freshly zero-allocated Windows extension. */
    result(&c,v,a);{
        uint8_t candidate[MERCS_CAREER_MECH_BYTES],tail[3]={0xe1,0x92,0x47};
        memcpy(candidate,mercs_save_list_record(c.document,MERCS_SAVE_OWNED_MECHS,0),sizeof candidate);
        id=u32(mercs_save_header(c.document)+0x15e);p32(candidate+0x810,id);p32(candidate+0x80c,UINT32_MAX);
        p32(mercs_save_header_edit(c.document)+0x15e,id+1);
        candidate[0x84e]=1;assert(!mercs_save_set_list(c.document,MERCS_SAVE_SALVAGE_MECHS,candidate,1));
        assert(!mercs_save_set_mech_extension(c.document,MERCS_SAVE_SALVAGE_MECHS,0,tail));
        check(mercs_campaign_continue(a,&c,v,error,sizeof error),error);assert(v->owned_count==2);
        assert(!memcmp(tail,mercs_save_mech_extension(c.document,MERCS_SAVE_OWNED_MECHS,1),3));
        assert(!memcmp("\0\0\0",mercs_save_mech_extension(c.document,MERCS_SAVE_OWNED_REFERENCES,1),3));
        roundtrip(a,&c,v,MERCS_ORIGINAL_COMMAND,directory,"windows-salvage.sav");
    }
    mercs_campaign_free(&c);memset(v,0,sizeof *v);
    check(mercs_campaign_new(a,0,"WIN CADET","TEST UNIT",&c,v,error,sizeof error),error);
    training=windows_document(c.document);mercs_save_free(c.document);c.document=training;
    assert(!mercs_economy_assign_pilot(c.document,0,2));check(mercs_campaign_hydrate(a,&c,v,error,sizeof error),error);
    memcpy(reference,mercs_save_mech_extension(c.document,MERCS_SAVE_OWNED_REFERENCES,2),3);
    p32(v->owned[2].current.bytes+0x768,1);result(&c,v,a);
    assert(!memcmp(reference,mercs_save_mech_extension(c.document,MERCS_SAVE_OWNED_MECHS,2),3));
    assert(u32(v->owned[2].current.bytes+0x768)==u32(v->owned[2].reference.bytes+0x768));
    roundtrip(a,&c,v,MERCS_ORIGINAL_DEBRIEF,directory,"windows-novice-debrief.sav");
    check(mercs_campaign_continue(a,&c,v,error,sizeof error),error);assert(!strcasecmp(c.campaign,"NOVICE2"));
    assert(!memcmp(reference,mercs_save_mech_extension(c.document,MERCS_SAVE_OWNED_MECHS,2),3));
    roundtrip(a,&c,v,MERCS_ORIGINAL_BRIEFING,directory,"windows-novice2.sav");
    mercs_campaign_free(&c);free(v);
    puts("PASS Windows layout and opaque extensions through import/export, buy, assignment, groups, Mech Lab, sale, training transfer/restoration and salvage");
}

static void large_fleet(prj_archive *a,const char *directory)
{
    mercs_campaign c={0};mercs_shell_state *v=calloc(1,sizeof *v);
    mercs_career_mech_pair *pairs=calloc(96,sizeof *pairs);
    mercs_repair_session *repair=NULL;const uint8_t *items,*ammo;
    char error[384],name[20];size_t i;int index,leave=0;int32_t amount=0,funds;
    assert(v&&pairs&&MERCS_SHELL_MAX_OWNED==(int)MERCS_SAVE_MAX_LIST_RECORDS);
    check(mercs_campaign_new(a,1,"FLEET","TEST UNIT",&c,v,error,sizeof error),error);
    index=find_mission(v,"BET1CAM1");assert(index>=0);
    check(mercs_campaign_accept(a,&c,v,index,error,sizeof error),error);
    for(i=0;i<96;i++){
        pairs[i]=v->owned[0];snprintf(name,sizeof name,"PLY%02uMER.MEK",(unsigned)i);
        strcpy((char*)pairs[i].current.bytes+0x823,name);strcpy((char*)pairs[i].reference.bytes+0x823,name);
        p32(pairs[i].current.bytes+0x810,(uint32_t)(100+i));p32(pairs[i].reference.bytes+0x810,(uint32_t)(100+i));
        mercs_career_set_association(&pairs[i].current,i?-1:(int32_t)v->pilot_instance_id);
        mercs_career_set_association(&pairs[i].reference,-1);
    }
    assert(!mercs_save_set_owned(c.document,pairs,96));assert(!mercs_save_player_assignment(c.document,100));
    p32(mercs_save_header_edit(c.document)+0x15e,196);
    check(mercs_campaign_hydrate(a,&c,v,error,sizeof error),error);
    assert(v->owned_count==96&&v->owned_index==0);
    assert(!mercs_economy_assign_pilot(c.document,0,95));
    assert(!mercs_economy_weapon_group(c.document,95,-1));
    check(mercs_campaign_hydrate(a,&c,v,error,sizeof error),error);assert(v->owned_index==95);
    /* The final owned design still participates in repair and actual MEK/
     * USERSTAR serialization; this is not merely a larger parser limit. */
    assert(u32(v->owned[95].current.bytes+0x768)>0);
    p32(v->owned[95].current.bytes+0x768,u32(v->owned[95].current.bytes+0x768)-1);
    items=mercs_save_list_record(c.document,MERCS_SAVE_OWNED_WEAPONS,0);
    ammo=mercs_save_list_record(c.document,MERCS_SAVE_OWNED_AMMO,0);funds=v->funds;
    assert(!mercs_repair_begin(&v->owned[95],items,mercs_save_list_count(c.document,MERCS_SAVE_OWNED_WEAPONS),
        ammo,mercs_save_list_count(c.document,MERCS_SAVE_OWNED_AMMO),funds,0,&repair));
    assert(!mercs_repair_all(repair,0,&amount));assert(!mercs_repair_finish(repair,1,0,&leave)&&leave);
    v->owned[95].current=*mercs_repair_current(repair);v->funds=mercs_repair_funds(repair);
    assert(u32(v->owned[95].current.bytes+0x768)==u32(v->owned[95].reference.bytes+0x768));
    assert(v->funds<=funds);mercs_repair_free(repair);
    check(mercs_shell_prepare_launch(a,v,index,directory,error,sizeof error),error);
    assert(!strcmp(v->starter_loadout,"PLY95MER")&&mercs_career_deployed(&v->owned[95].current));
    roundtrip(a,&c,v,MERCS_ORIGINAL_COMMAND,directory,"fleet-96.sav");
    /* The public bound remains explicit: the next record cannot replace a
     * valid owned list or evade the file's exact-length validation. */
    assert(mercs_save_set_owned(c.document,pairs,MERCS_SAVE_MAX_LIST_RECORDS+1)==MERCS_SAVE_INVALID);
    assert(mercs_save_list_count(c.document,MERCS_SAVE_OWNED_MECHS)==96);
    puts("PASS 96-owned fleet: assignment, weapon groups, repair, deployment, original save/import and native resume");
    mercs_campaign_free(&c);free(pairs);free(v);
}
int main(int argc,char **argv)
{
    prj_archive *a;mercs_campaign c={0};mercs_shell_state *v=calloc(1,sizeof *v);
    char error[384],root[4096],*slash;int index;
    assert((argc==3||argc==4)&&v);snprintf(root,sizeof root,"%s",argv[1]);slash=strrchr(root,'/');assert(slash);*slash=0;
    dp_clear_roots();assert(!dp_add_root(root));slash=strrchr(root,'/');if(slash){*slash=0;assert(!dp_add_root(root));}
    a=prj_open(argv[1],error,sizeof error);assert(a);
    if(argc==4){FILE *f=fopen(argv[2],"rb");long length;uint8_t *bytes,*out;size_t size;uint32_t event;
        assert(f);fseek(f,0,SEEK_END);length=ftell(f);rewind(f);assert(length>=0);bytes=malloc((size_t)length);assert(bytes);
        assert(fread(bytes,1,(size_t)length,f)==(size_t)length&&!fclose(f));
        check(mercs_interchange_import(a,bytes,(size_t)length,&c,v,&event,error,sizeof error),error);
        assert(!mercs_save_encode(c.document,0,&out,&size));f=fopen(argv[3],"wb");assert(f);
        assert(fwrite(out,1,size,f)==size&&!fclose(f));free(bytes);free(out);
        printf("Imported original campaign=%s event=%x phase=%d\n",c.campaign,event,c.phase);goto done;
    }
    check(mercs_campaign_new(a,1,"IMPORTER","TEST UNIT",&c,v,error,sizeof error),error);
    preview_continuations(a,&c,v);
    roundtrip(a,&c,v,MERCS_ORIGINAL_HALL,argv[2],"regular-hall.sav");
    index=find_mission(v,"BET1CAM1");assert(index>=0);check(mercs_campaign_accept(a,&c,v,index,error,sizeof error),error);
    roundtrip(a,&c,v,MERCS_ORIGINAL_COMMAND,argv[2],"accepted-bet1.sav");
    rejection(a,&c,v);windows_cycle(a,&c,v,argv[2]);result(&c,v,a);
    roundtrip(a,&c,v,MERCS_ORIGINAL_DEBRIEF,argv[2],"bet1-debrief.sav");
    check(mercs_campaign_continue(a,&c,v,error,sizeof error),error);
    roundtrip(a,&c,v,MERCS_ORIGINAL_COMMAND,argv[2],"bet2-old-result.sav");
    mercs_campaign_free(&c);memset(v,0,sizeof *v);
    check(mercs_campaign_new(a,0,"CADET","TEST UNIT",&c,v,error,sizeof error),error);
    roundtrip(a,&c,v,MERCS_ORIGINAL_BRIEFING,argv[2],"novice1.sav");
    mercs_campaign_free(&c);memset(v,0,sizeof *v);
    check(mercs_campaign_new(a,1,"RANDOM","TEST UNIT",&c,v,error,sizeof error),error);
    index=find_mission(v,"RAN1CAM1");assert(index>=0);check(mercs_campaign_accept(a,&c,v,index,error,sizeof error),error);
    roundtrip(a,&c,v,MERCS_ORIGINAL_COMMAND,argv[2],"ran1.sav");
    /* The original restore's history seed is authoritative even if header162
     * contains a different accepted-generator seed. Preserve both. */
    c.procedural_seed=u32(mercs_save_header(c.document)+0x162);c.procedural_seed_override=1;
    p32(mercs_save_header_edit(c.document)+0x162,c.procedural_seed^0x1234);
    roundtrip(a,&c,v,MERCS_ORIGINAL_COMMAND,argv[2],"ran1-history-seed.sav");
    large_fleet(a,argv[2]);
    puts("PASS original DOS standalone import/export, exact records, debrief without replay, procedural history and atomic rejection");
done:mercs_campaign_free(&c);free(v);prj_close(a);dp_clear_roots();return 0;
}
