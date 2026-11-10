/* Native integration of separately verified DOS shell transitions. */
#include "mercscampaign.h"
#include "bwd.h"
#include "mercsrmg.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static uint32_t u32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void p32(uint8_t *p,uint32_t v)
{p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static int32_t signed32(uint32_t v)
{int32_t result;memcpy(&result,&v,sizeof result);return result;}
static int fail(char *e,size_t n,const char *fmt,...)
{va_list a;if(e&&n){va_start(a,fmt);vsnprintf(e,n,fmt,a);va_end(a);}return -1;}
static int resource_name(const char *s,size_t capacity,int empty)
{size_t i;if(!s)return 0;for(i=0;i<capacity;i++){unsigned c=(unsigned char)s[i];
 if(!c)return empty||i>0;
 if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return 0;}return 0;}
static int token_ok(const char *s)
{size_t i;if(!memchr(s,0,33))return 0;if(!s[0])return 1;if(strlen(s)!=32)return 0;
 for(i=0;i<32;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0;
 return 1;}
/* Optional MRCSRES2 extension. Each cached RAN preview has its own seed
 * and generation year, including offers not yet accepted. A zero seed is
 * valid; the marker distinguishes it from an older continuation record. */
enum { RESUME_PREVIEWS_OFFSET=532, RESUME_PREVIEWS_BYTES=84 };
static int scene_equal(const uint8_t *saved,const char *name)
{
    char scene[17];size_t len;
    if(!memchr(saved,0,16))return 0;
    memcpy(scene,saved,16);scene[16]=0;len=strlen(scene);
    if(len>4&&!strcasecmp(scene+len-4,".BWD"))scene[len-4]=0;
    return resource_name(scene,sizeof scene,0)&&!strcasecmp(scene,name);
}

int mercs_campaign_prepare_resources(prj_archive *a,const mercs_campaign *c,char *e,size_t n)
{
    mercs_rmg_bundle bundle={0};int rc;
    if(!a||!c||!c->document)return fail(e,n,"Missing career resources");
    if(!c->active||strncasecmp(c->campaign,"RAN",3))return prj_set_generated(a,NULL,0);
    if(c->procedural_year<3044||c->procedural_year>32767)return fail(e,n,"Invalid saved procedural generation year");
    rc=mercs_rmg_campaign(a,c->campaign,c->procedural_year,
          c->procedural_seed_override?c->procedural_seed:u32(mercs_save_header(c->document)+0x162),&bundle,e,n);
    if(!rc)rc=mercs_rmg_install(a,&bundle);
    mercs_rmg_free(&bundle);return rc;
}

void mercs_campaign_free(mercs_campaign *c)
{if(c){mercs_save_free(c->document);memset(c,0,sizeof *c);}}
int mercs_campaign_clone(const mercs_campaign *s,mercs_campaign *out)
{mercs_campaign c;if(!s||!s->document||!out||s==out)return -1;c=*s;c.document=mercs_save_clone(s->document);
 if(!c.document)return -1;
 *out=c;return 0;}

static int sync_inplace(mercs_campaign *c,const mercs_shell_state *v,char *e,size_t n)
{
    uint8_t *h;
    int32_t assignment=-1;uint32_t player;size_t i,j;
    char pilot[32];
    if(!c||!c->document||!v||!v->registered||v->owned_count<0||v->owned_count>MERCS_SHELL_MAX_OWNED||
       v->owned_index < -1||v->owned_index>=v->owned_count)return fail(e,n,"Invalid career state");
    if(mercs_save_player(c->document,NULL,&player,pilot,sizeof pilot)||
       !memchr(v->pilot,0,sizeof v->pilot)||strcmp(v->pilot,pilot)||
       v->pilot_instance_id!=player||u32(mercs_save_header(c->document)+0x23bb)!=(uint32_t)v->mode)
        return fail(e,n,"Career view and saved player identity differ");
    for(i=0;i<(size_t)v->owned_count;i++){
        uint32_t id=mercs_career_instance_id(&v->owned[i].current);
        if(id>INT32_MAX||id!=mercs_career_instance_id(&v->owned[i].reference))
            return fail(e,n,"Owned current/reference identities differ");
        for(j=0;j<i;j++)if(id==mercs_career_instance_id(&v->owned[j].current))
            return fail(e,n,"Duplicate owned-mech identity");
        if((i==(size_t)v->owned_index)!=
           (mercs_career_association_id(&v->owned[i].current)==signed32(player)))
            return fail(e,n,"Player and owned-mech assignment differ");
    }
    if(v->owned_index>=0)assignment=signed32(mercs_career_instance_id(&v->owned[v->owned_index].current));
    if(mercs_save_set_owned(c->document,v->owned,(size_t)v->owned_count)||
       mercs_save_player_assignment(c->document,assignment))return fail(e,n,"Cannot synchronize owned records");
    h=mercs_save_header_edit(c->document);
    p32(h+0x12a,(uint32_t)v->funds);p32(h+0x122,(uint32_t)v->year);p32(h+0x126,(uint32_t)v->month);
    if(v->last_return_valid){
        size_t written;
        memcpy(h+0x2da7,v->last_car.bytes,80);
        if(mercs_msn_encode(&v->last_msn,h+0x23d3,MERCS_MSN_BYTES,&written))return fail(e,n,"Invalid mission results");
    }
    /* Taking a mech from another pilot clears that pilot's assignment in
     * original0x3ca79. The view tracks the player's assignment only. */
    if(assignment!=-1)for(i=0;i<mercs_save_list_count(c->document,MERCS_SAVE_OWNED_PILOTS);i++){
        uint8_t *p=mercs_save_list_record_edit(c->document,MERCS_SAVE_OWNED_PILOTS,i);
        if(p[0x2064]==0&&signed32(u32(p+0x20))==assignment)p32(p+0x20,UINT32_MAX);
    }
    return 0;
}
int mercs_campaign_sync(mercs_campaign *c,const mercs_shell_state *v,char *e,size_t n)
{
    mercs_campaign copy;int rc;
    if(!c||!v||mercs_campaign_clone(c,&copy))return fail(e,n,"Cannot stage career synchronization");
    rc=sync_inplace(&copy,v,e,n);
    if(!rc){mercs_save_free(c->document);c->document=copy.document;copy.document=NULL;}
    mercs_campaign_free(&copy);return rc;
}

static int hydrate_inplace(prj_archive *a,mercs_campaign *c,mercs_shell_state *v,char *e,size_t n)
{
    const uint8_t *h,*player;
    uint32_t id;
    size_t count,i,j,pi;
    int selected=-1;
    if(!a||!c||!c->document||!v)return fail(e,n,"Missing career document");
    h=mercs_save_header(c->document);count=mercs_save_list_count(c->document,MERCS_SAVE_OWNED_MECHS);
    if(count>MERCS_SHELL_MAX_OWNED||count!=mercs_save_list_count(c->document,MERCS_SAVE_OWNED_REFERENCES)||
       h[1]!=1||u32(h+0x23bb)>1||!memchr(h+0x102,0,32)||strlen((const char *)h+0x102)>=sizeof v->unit||
       mercs_save_player(c->document,&pi,&id,v->pilot,sizeof v->pilot))
        return fail(e,n,"This saved career has unsupported ownership or pilot data");
    if(id>INT32_MAX)return fail(e,n,"Invalid saved player identity");
    player=mercs_save_list_record(c->document,MERCS_SAVE_OWNED_PILOTS,pi);
    v->registered=1;v->mode=(int)u32(h+0x23bb);v->pilot_instance_id=id;
    v->year=(int32_t)u32(h+0x122);v->month=(int32_t)u32(h+0x126);v->funds=(int32_t)u32(h+0x12a);
    strcpy(v->unit,(const char *)h+0x102);
    strcpy(v->initial_campaign,v->mode?"CAMP":"NOVICE");
    v->owned_count=(int)count;memset(v->owned,0,sizeof v->owned);
    for(i=0;i<count;i++){
        mercs_shell_mech design;
        memcpy(v->owned[i].current.bytes,mercs_save_list_record(c->document,MERCS_SAVE_OWNED_MECHS,i),MERCS_CAREER_MECH_BYTES);
        const uint8_t *reference=NULL;uint32_t identity=mercs_career_instance_id(&v->owned[i].current);
        if(identity>INT32_MAX)return fail(e,n,"Invalid saved owned identity");
        for(j=0;j<i;j++)if(mercs_career_instance_id(&v->owned[j].current)==identity)
            return fail(e,n,"Duplicate saved owned identity");
        for(j=0;j<count;j++){
            const uint8_t *row=mercs_save_list_record(c->document,MERCS_SAVE_OWNED_REFERENCES,j);
            if(u32(row+0x810)==identity){if(reference)return fail(e,n,"Duplicate saved reference identity");reference=row;}
        }
        if(!reference)return fail(e,n,"Saved owned reference is missing");
        memcpy(v->owned[i].reference.bytes,reference,MERCS_CAREER_MECH_BYTES);
        if(mercs_career_association_id(&v->owned[i].current)==signed32(id)&&u32(player+0x20)!=identity)
            return fail(e,n,"Saved player has conflicting mech assignments");
        if(mercs_shell_describe_owned(a,&v->owned[i],&design,e,n))return -1;
        if((int32_t)u32(player+0x20)!=(int32_t)-1&&mercs_career_instance_id(&v->owned[i].current)==u32(player+0x20)){
            if(selected>=0||mercs_career_association_id(&v->owned[i].current)!=(int32_t)id)
                return fail(e,n,"Saved player assignment is inconsistent");
            selected=(int)i;
        }
    }
    if((int32_t)u32(player+0x20)!=-1&&selected<0)return fail(e,n,"Saved player mech is missing");
    v->owned_index=selected;v->assigned_index=selected;
    v->starter_stock[0]=v->starter_loadout[0]=v->starter_skeleton[0]=0;strcpy(v->starter_name,"Unassigned");
    if(selected>=0){mercs_shell_mech d;if(mercs_shell_describe_owned(a,&v->owned[selected],&d,e,n))return -1;
        strcpy(v->starter_stock,d.stock);strcpy(v->starter_loadout,d.loadout);
        strcpy(v->starter_skeleton,d.skeleton);strcpy(v->starter_name,d.name);}
    v->training_mech_count=0;
    if(!v->mode&&count<=MERCS_SHELL_TRAINING_MECHS){
        for(i=0;i<count;i++)if(mercs_shell_describe_owned(a,&v->owned[i],&v->training_mechs[i],e,n))return -1;
        v->training_mech_count=(int)count;
    }
    {
        const uint8_t *end=memchr(h+0x342,0,4096);
        if(!end||mercs_shell_news_text(v,h+0x342,(size_t)(end-(h+0x342)),e,n))
            return fail(e,n,"Invalid saved campaign news");
    }
    return 0;
}
int mercs_campaign_hydrate(prj_archive *a,mercs_campaign *c,mercs_shell_state *v,char *e,size_t n)
{
    mercs_shell_state *copy;int rc;
    if(!v)return fail(e,n,"Missing career view");
    copy=malloc(sizeof *copy);if(!copy)return fail(e,n,"Out of memory restoring career");
    *copy=*v;rc=hydrate_inplace(a,c,copy,e,n);if(!rc)*v=*copy;free(copy);return rc;
}

static int accept_inplace(prj_archive *a,mercs_campaign *c,mercs_shell_state *v,
                           int index,char *e,size_t n)
{
    char name[16];mercs_shell_mission *mission;int procedural,rc;
    if(!a||!c||!c->document||!v||index<0||index>=v->mission_count||
       c->phase!=MERCS_CAMPAIGN_READY||c->active)return fail(e,n,"No available new contract selected");
    if(!resource_name(v->missions[index].campaign,16,0))return fail(e,n,"Invalid contract resource");
    strcpy(name,v->missions[index].campaign);
    procedural=!strncasecmp(name,"RAN",3);
    mission=malloc(sizeof *mission);if(!mission)return fail(e,n,"Out of memory checking contract");
    if(procedural)rc=mercs_shell_mission_load_seeded(a,name,v->pilot,v->missions[index].procedural_year,
                                                   v->missions[index].procedural_seed,mission,e,n);
    else rc=mercs_shell_mission_load(a,name,v->pilot,mission,e,n);
    if(rc){free(mission);return -1;}
    if(!mission->launchable){fail(e,n,"%s",mission->unavailable);free(mission);return -1;}
    if(sync_inplace(c,v,e,n)){free(mission);return -1;}
    if(procedural){
        mercs_rmg_bundle bundle={0};
        rc=mercs_rmg_campaign(a,name,mission->procedural_year,mission->procedural_seed,&bundle,e,n);
        if(!rc)rc=mercs_rmg_install(a,&bundle);
        mercs_rmg_free(&bundle);if(rc){free(mission);return -1;}
        p32(mercs_save_header_edit(c->document)+0x162,mission->procedural_seed);
    }else if(prj_set_generated(a,NULL,0)){free(mission);return -1;}
    if(mercs_save_load_campaign(a,c->document,name,e,n)){free(mission);return -1;}
    if(!v->mode&&mercs_save_transfer_training(c->document)){free(mission);return fail(e,n,"Cannot transfer the original training roster");}
    strcpy(c->campaign,name);c->accepted=index;c->selected=index;c->active=1;
    c->procedural_year=procedural?mission->procedural_year:0;
    c->procedural_seed_override=0;c->procedural_seed=0;
    p32(mercs_save_header_edit(c->document)+0x239e,(uint32_t)index);
    if(hydrate_inplace(a,c,v,e,n)){free(mission);return -1;}
    /* Launch eligibility is resource-based; assignment is checked at launch. */
    v->missions[index]=*mission;free(mission);
    return 0;
}

static int available_missions(prj_archive *a,mercs_campaign *c,mercs_shell_state *v,char *e,size_t n)
{
    const uint8_t *h=mercs_save_header(c->document);
    char names[MERCS_SHELL_MAX_MISSIONS][16]={{0}};size_t count,i;
    int success=h[0x239c]==1;
    count=u32(h+(success?0x2392:0x2396));
    if(count>MERCS_SHELL_MAX_MISSIONS)return fail(e,n,"Too many available contracts");
    for(i=0;i<count;i++){
        memcpy(names[i],h+(success?0x202:0x2a2)+16*i,16);
        if(!resource_name(names[i],16,0))return fail(e,n,"Invalid available contract resource");
    }
    if(mercs_shell_set_missions(a,v,(const char (*)[16])names,count,e,n))return -1;
    c->active=0;c->accepted=-1;c->selected=0;c->procedural_year=0;
    c->procedural_seed_override=0;c->procedural_seed=0;return 0;
}

int mercs_campaign_new(prj_archive *a,int mode,const char *pilot,const char *unit,
                        mercs_campaign *out,mercs_shell_state *v,char *e,size_t n)
{
    mercs_campaign c;
    mercs_shell_state *s;
    if(!a||!out||!v)return fail(e,n,"Invalid new career");
    memset(&c,0,sizeof c);c.accepted=-1;c.rng_state=1;
    s=calloc(1,sizeof *s);if(!s)return fail(e,n,"Out of memory creating career");
    if(mercs_shell_register(a,s,mode,pilot,unit,e,n)||
       mercs_save_initialize(a,mode,s->pilot,s->unit,&c.document,e,n))goto bad;
    strcpy(c.campaign,mode?"CAMP":"NOVICE");
    s->last_return_mission=-1;
    if(hydrate_inplace(a,&c,s,e,n)||available_missions(a,&c,s,e,n))goto bad;
    if(!mode){
        /* The Mercenary (novice) career takes its first contract at once, but the original keeps the starting
         * record's news - Colonel Harris's last will ("Dear <pilot>, if you're reading this ...") - on the
         * StarPort terminal: DOSBox, after registering, the news page and the terminal show the letter, not
         * NOVICE1's GALEDON V bulletin, until the DropShip is boarded. Keep the header's NEWS text (0x342, 4096
         * bytes) across the accept. */
        uint8_t *news=malloc(4096);
        if(!news){fail(e,n,"Out of memory creating career");goto bad;}
        memcpy(news,mercs_save_header(c.document)+0x342,4096);
        if(accept_inplace(a,&c,s,0,e,n)){free(news);goto bad;}
        memcpy(mercs_save_header_edit(c.document)+0x342,news,4096);free(news);
        if(hydrate_inplace(a,&c,s,e,n))goto bad;
    }
    *v=*s;*out=c;free(s);return 0;
bad:mercs_campaign_free(&c);free(s);return -1;
}

static int calculate_report(const mercs_campaign *c,const mercs_shell_state *v,
                             mercs_debrief_terms *terms,mercs_debrief_report *report,
                             char *e,size_t n)
{
    const uint8_t *h;
    mercs_debrief_terms t;
    size_t i;
    uint32_t pay=0,aero=0;
    if(!c||!c->document||!v||!v->last_return_valid||v->owned_count<0||
       v->owned_count>MERCS_SHELL_MAX_OWNED||v->mode<0||v->mode>1)
        return fail(e,n,"No valid mission result to debrief");
    h=mercs_save_header(c->document);memset(&t,0,sizeof t);
    t.mode=v->mode;t.restore_flag=h[0x23b7];t.campaign_type=(int32_t)u32(h+0x1fe);t.funds=v->funds;
    t.success_pay=(int32_t)u32(h+0x1e6);t.failure_pay=(int32_t)u32(h+0x1ea);
    t.target_pay=(int32_t)u32(h+0x1ee);t.bonus_pay=(int32_t)u32(h+0x1f2);t.bounty_pay=(int32_t)u32(h+0x1f6);
    t.expense_factor=(int32_t)u32(h+0x1fa);t.per_mech_a=(int32_t)u32(h+0x23a3);t.per_mech_b=(int32_t)u32(h+0x23a7);
    t.fixed_a=(int32_t)u32(h+0x23ab);t.fixed_b=(int32_t)u32(h+0x23af);t.fixed_c=(int32_t)u32(h+0x23b3);
    t.owned_mech_count=(uint32_t)v->owned_count;
    memcpy(t.success_next,h+0x202,sizeof t.success_next);memcpy(t.failure_next,h+0x2a2,sizeof t.failure_next);
    for(i=0;i<mercs_save_list_count(c->document,MERCS_SAVE_OWNED_PILOTS);i++){
        const uint8_t *p=mercs_save_list_record(c->document,MERCS_SAVE_OWNED_PILOTS,i);if(p[0x2064]==0)pay+=u32(p+0x3c);
    }
    for(i=0;i<mercs_save_list_count(c->document,MERCS_SAVE_OWNED_TECHS);i++)
        aero+=u32(mercs_save_list_record(c->document,MERCS_SAVE_OWNED_TECHS,i)+0x24);
    t.pilot_pay=(int32_t)pay;t.aero_pay=(int32_t)aero;
    if(mercs_debrief_build(&t,&v->last_car,&v->last_msn,report))return fail(e,n,"Unsupported original debrief result");
    *terms=t;return 0;
}
int mercs_campaign_report(mercs_campaign *c,const mercs_shell_state *v,char *e,size_t n)
{
    mercs_debrief_terms terms;mercs_debrief_report report;
    if(calculate_report(c,v,&terms,&report,e,n))return -1;
    c->terms=terms;c->report=report;return 0;
}
int mercs_campaign_preflight(const mercs_campaign *c,const mercs_shell_state *v,
                              mercs_debrief_gate *gate,char *e,size_t n)
{
    mercs_debrief_terms terms;mercs_debrief_report report;uint32_t value=0;size_t i,j;
    if(!c||!v||!gate||c->phase!=MERCS_CAMPAIGN_DEBRIEF)
        return fail(e,n,"No debrief is awaiting continuation");
    if(calculate_report(c,v,&terms,&report,e,n))return -1;
    if(terms.mode==1&&terms.campaign_type==4&&report.balance>=0){
        for(i=0;i<(size_t)v->owned_count;i++){
            int32_t price;
            if(mercs_debrief_mech_value(&v->owned[i].current,v->year,&price))
                return fail(e,n,"Cannot value an owned mech for the original ending");
            value+=(uint32_t)price;
        }
        for(j=0;j<2;j++){
            enum mercs_save_list list=j?MERCS_SAVE_OWNED_AMMO:MERCS_SAVE_OWNED_WEAPONS;
            for(i=0;i<mercs_save_list_count(c->document,list);i++)
                value+=(uint32_t)mercs_debrief_item_value(mercs_save_list_record(c->document,list,i));
        }
    }
    if(mercs_debrief_preflight(&terms,&report,&v->last_car,signed32(value),
           (const char *)mercs_save_header(c->document)+0x1d6,gate))
        return fail(e,n,"Unsupported original debrief continuation gate");
    return 0;
}

static int return_inplace(mercs_campaign *c,mercs_shell_state *v,char *e,size_t n)
{
    if(!c||!v||!v->last_return_valid||!c->active||c->phase==MERCS_CAMPAIGN_RETURN||
       c->phase==MERCS_CAMPAIGN_DEBRIEF||c->phase==MERCS_CAMPAIGN_ENDED)
        return fail(e,n,"This mission result has already entered the debrief");
    if(c->accepted<0||c->accepted>=v->mission_count||v->last_return_mission!=c->accepted||
       !token_ok(v->last_return_token)||!v->last_return_token[0]||
       (c->phase==MERCS_CAMPAIGN_DEPLOYED&&strcmp(c->launch_token,v->last_return_token)))
        return fail(e,n,"Mission return does not match the accepted deployment");
    if(sync_inplace(c,v,e,n)||mercs_debrief_enter(v->mode,0,&v->last_car,&c->entry))return -1;
    mercs_save_header_edit(c->document)[0x23b7]=0;
    c->phase=MERCS_CAMPAIGN_RETURN;c->launch_token[0]=c->launch_edition[0]=0;return 0;
}

static int select_salvage(mercs_campaign *c,char *e,size_t n)
{
    size_t ni=mercs_save_list_count(c->document,MERCS_SAVE_SALVAGE_ITEMS),
           nm=mercs_save_list_count(c->document,MERCS_SAVE_SALVAGE_MECHS),i;
    mercs_salvage_choice *items=calloc(ni?ni:1,sizeof *items),*mechs=calloc(nm?nm:1,sizeof *mechs);
    int rc=-1;
    if(!items||!mechs){fail(e,n,"Out of memory selecting salvage");goto done;}
    for(i=0;i<ni;i++){const uint8_t *p=mercs_save_list_record(c->document,MERCS_SAVE_SALVAGE_ITEMS,i);
        items[i].threshold=(int32_t)u32(p+0x12);items[i].selected=p[9];}
    for(i=0;i<nm;i++){const uint8_t *p=mercs_save_list_record(c->document,MERCS_SAVE_SALVAGE_MECHS,i);
        mechs[i].threshold=(int32_t)u32(p+0x847);mechs[i].selected=p[0x84e];}
    if(mercs_debrief_roll_salvage(c->terms.restore_flag,items,ni,mechs,nm,&c->rng_state))goto done;
    for(i=0;i<ni;i++)mercs_save_list_record_edit(c->document,MERCS_SAVE_SALVAGE_ITEMS,i)[9]=items[i].selected;
    for(i=0;i<nm;i++)mercs_save_list_record_edit(c->document,MERCS_SAVE_SALVAGE_MECHS,i)[0x84e]=mechs[i].selected;
    rc=0;
done:free(items);free(mechs);return rc;
}

static int enter_debrief_inplace(prj_archive *a,mercs_campaign *c,mercs_shell_state *v,char *e,size_t n)
{
    int i;
    if(!c||!v||c->phase!=MERCS_CAMPAIGN_RETURN)return fail(e,n,"No pending debrief entry");
    if(mercs_debrief_enter(v->mode,0,&v->last_car,&c->entry))return fail(e,n,"Invalid debrief entry");
    if(c->entry.career_end){c->phase=MERCS_CAMPAIGN_ENDED;c->active=0;c->accepted=-1;
        c->procedural_year=0;c->procedural_seed_override=0;c->procedural_seed=0;return sync_inplace(c,v,e,n);}
    if(c->entry.remove_player_mech&&v->owned_index>=0){
        i=v->owned_index;memmove(&v->owned[i],&v->owned[i+1],(size_t)(v->owned_count-i-1)*sizeof v->owned[0]);
        memset(&v->owned[--v->owned_count],0,sizeof v->owned[0]);v->owned_index=-1;
    }
    if(c->entry.restore_training_mechs)for(i=0;i<v->owned_count;i++){
        if(mercs_debrief_restore_training_pair(&v->owned[i]))return fail(e,n,"Invalid training reference record");
        if(mercs_save_windows(c->document)){
            uint32_t id=mercs_career_instance_id(&v->owned[i].current);size_t j,ci=SIZE_MAX,ri=SIZE_MAX;
            for(j=0;j<mercs_save_list_count(c->document,MERCS_SAVE_OWNED_MECHS);j++)
                if(u32(mercs_save_list_record(c->document,MERCS_SAVE_OWNED_MECHS,j)+0x810)==id)ci=j;
            for(j=0;j<mercs_save_list_count(c->document,MERCS_SAVE_OWNED_REFERENCES);j++)
                if(u32(mercs_save_list_record(c->document,MERCS_SAVE_OWNED_REFERENCES,j)+0x810)==id)ri=j;
            if(ci==SIZE_MAX||ri==SIZE_MAX||mercs_save_set_mech_extension(c->document,MERCS_SAVE_OWNED_MECHS,ci,
                mercs_save_mech_extension(c->document,MERCS_SAVE_OWNED_REFERENCES,ri)))
                return fail(e,n,"Invalid Windows training reference extension");
        }
    }
    if(sync_inplace(c,v,e,n)||hydrate_inplace(a,c,v,e,n)||mercs_campaign_report(c,v,e,n))return -1;
    mercs_save_header_edit(c->document)[0x239c]=(uint8_t)c->report.success;
    if(c->terms.restore_flag==0){
        uint8_t *h=mercs_save_header_edit(c->document);int kind=c->terms.campaign_type;
        if(kind==1)h[0x23a2]=1;
        else if(kind==2||(kind==3&&(v->mode==0||!c->report.success))){h[0x239d]=1;h[0x23a2]=0;}
    }
    if(c->report.select_salvage&&select_salvage(c,e,n))return -1;
    c->phase=MERCS_CAMPAIGN_DEBRIEF;return 0;
}

/* Completion below uses the original selected candidates; it never derives
 * salvage from a generic list of destroyed simulator actors. */
static int append_record(mercs_save_state *d,enum mercs_save_list list,const uint8_t *record)
{
    size_t count=mercs_save_list_count(d,list),stride=mercs_save_list_stride(list),i;
    uint8_t *bytes;
    int rc;
    if(count>=MERCS_SAVE_MAX_LIST_RECORDS)return -1;
    bytes=malloc((count+1)*stride);if(!bytes)return -1;
    for(i=0;i<count;i++)memcpy(bytes+i*stride,mercs_save_list_record(d,list,i),stride);
    memcpy(bytes+count*stride,record,stride);rc=mercs_save_set_list(d,list,bytes,count+1);free(bytes);return rc;
}

static int transfer_salvage(prj_archive *a,mercs_campaign *c,mercs_shell_state *v,char *e,size_t n)
{
    size_t i,ni=mercs_save_list_count(c->document,MERCS_SAVE_SALVAGE_ITEMS),nm=mercs_save_list_count(c->document,MERCS_SAVE_SALVAGE_MECHS);
    for(i=0;i<ni;i++){
        const uint8_t *raw=mercs_save_list_record(c->document,MERCS_SAVE_SALVAGE_ITEMS,i);
        uint8_t normalized[26];int destination;
        if(raw[9]!=1)continue;
        if(mercs_debrief_salvage_item(raw,normalized,&destination)||
           append_record(c->document,destination==1?MERCS_SAVE_OWNED_AMMO:MERCS_SAVE_OWNED_WEAPONS,normalized))
            return fail(e,n,"Cannot transfer this original salvage item");
    }
    for(i=0;i<nm;i++){
        const uint8_t *raw=mercs_save_list_record(c->document,MERCS_SAVE_SALVAGE_MECHS,i);
        mercs_career_mech candidate;mercs_career_mech_pair reference,pair;
        prj_record stock;
        char name[32],resource[9],existing[32];int number,j,rc;
        uint32_t id;uint8_t extension[3];const uint8_t *extra;
        if(raw[0x84e]!=1)continue;
        if(v->owned_count>=MERCS_SHELL_MAX_OWNED)return fail(e,n,"The native owned-mech limit has been reached");
        memcpy(candidate.bytes,raw,sizeof candidate.bytes);
        extra=mercs_save_mech_extension(c->document,MERCS_SAVE_SALVAGE_MECHS,i);
        if(extra)memcpy(extension,extra,3);
        if(mercs_career_filename(&candidate,name,sizeof name))return fail(e,n,"Invalid salvage filename");
        for(number=0;number<100;number++){
            for(j=0;j<v->owned_count;j++){
                if(mercs_career_filename(&v->owned[j].current,existing,sizeof existing))return -1;
                if(isdigit((unsigned char)existing[3])&&isdigit((unsigned char)existing[4])&&
                   (existing[3]-'0')*10+existing[4]-'0'==number)break;
            }
            if(j==v->owned_count)break;
        }
        if(number==100)return fail(e,n,"No original owned-mech filename is available");
        snprintf(resource,sizeof resource,"%.3s00STD",name);
        snprintf(name+3,sizeof name-3,"%02dMER.MEK",number);
        if(prj_read_named(a,"MEK",resource,&stock)!=PRJ_OK)return fail(e,n,"Missing salvage reference %s",resource);
        id=u32(mercs_save_header(c->document)+0x15e);
        if(id>INT32_MAX){prj_record_free(&stock);return fail(e,n,"Original identity counter is exhausted");}
        rc=mercs_career_pair_init(&reference,stock.data,stock.size,resource,id,-1);prj_record_free(&stock);
        if(rc||mercs_debrief_salvage_pair(&candidate,&reference.current,name,&pair))return fail(e,n,"Cannot convert the original salvage mech");
        p32(mercs_save_header_edit(c->document)+0x15e,id+1);v->owned[v->owned_count++]=pair;
        if(extra){
            if(mercs_save_set_owned(c->document,v->owned,(size_t)v->owned_count)||
               mercs_save_set_mech_extension(c->document,MERCS_SAVE_OWNED_MECHS,(size_t)v->owned_count-1,extension))
                return fail(e,n,"Cannot retain the salvaged Windows mech record");
        }
    }
    if(mercs_save_set_list(c->document,MERCS_SAVE_SALVAGE_ITEMS,NULL,0)||
       mercs_save_set_list(c->document,MERCS_SAVE_SALVAGE_MECHS,NULL,0))return -1;
    return 0;
}

static int cleanup_lost(mercs_campaign *c,mercs_shell_state *v,char *e,size_t n)
{
    size_t i,count=mercs_save_list_count(c->document,MERCS_SAVE_OWNED_PILOTS),stride=MERCS_SAVE_PILOT_BYTES;
    uint8_t *pilots=malloc((count?count:1)*stride);
    int j;
    if(!pilots)return fail(e,n,"Out of memory processing pilot losses");
    for(i=0;i<count;i++)memcpy(pilots+i*stride,mercs_save_list_record(c->document,MERCS_SAVE_OWNED_PILOTS,i),stride);
    for(i=0;i<count;i++)if(pilots[i*stride+0x2064]==0){
        uint32_t pilot=u32(pilots+i*stride+0x24);
        for(j=0;j<v->owned_count;j++)if(mercs_career_association_id(&v->owned[j].current)==(int32_t)pilot)break;
        if(j<v->owned_count&&mercs_career_mech_lost(&v->owned[j].current)){
            memmove(&v->owned[j],&v->owned[j+1],(size_t)(v->owned_count-j-1)*sizeof v->owned[0]);v->owned_count--;
            memset(&v->owned[v->owned_count],0,sizeof v->owned[0]);
            if(v->owned_index>j)v->owned_index--;
            memmove(pilots+i*stride,pilots+(i+1)*stride,(count-i-1)*stride);count--;
            /* DOS list loop advances its index after removal. */
        }
    }
    j=mercs_save_set_list(c->document,MERCS_SAVE_OWNED_PILOTS,pilots,count);free(pilots);
    if(j)return fail(e,n,"Cannot commit pilot losses");
    return 0;
}

static int continue_inplace(prj_archive *a,mercs_campaign *c,mercs_shell_state *v,char *e,size_t n)
{
    mercs_debrief_commit plan;mercs_debrief_gate gate;
    if(!a||!c||!v||c->phase!=MERCS_CAMPAIGN_DEBRIEF)return fail(e,n,"This debrief has already been continued");
    if(mercs_campaign_report(c,v,e,n)||mercs_campaign_preflight(c,v,&gate,e,n))return -1;
    if(gate.block_settlement){
        /* The original leave-handler checks these before the funds, loss,
         * salvage and next-contract operations in0x3a038. */
        if(gate.career_end){c->phase=MERCS_CAMPAIGN_ENDED;c->active=0;c->accepted=-1;c->procedural_year=0;
            c->procedural_seed_override=0;c->procedural_seed=0;}
        else{
            uint8_t *h;
            if(!resource_name(gate.campaign,sizeof gate.campaign,0)||
               mercs_save_load_campaign(a,c->document,gate.campaign,e,n))return -1;
            strcpy(c->campaign,gate.campaign);h=mercs_save_header_edit(c->document);
            h[0x239d]=1;h[0x239c]=0;h[0x23a2]=0;
            if(hydrate_inplace(a,c,v,e,n)||available_missions(a,c,v,e,n))return -1;
            c->phase=MERCS_CAMPAIGN_READY;
        }
        v->last_return_valid=0;v->last_return_mission=-1;return 0;
    }
    if(mercs_debrief_continue(&c->terms,&c->report,&plan))return fail(e,n,"Unsupported original continuation");
    if(plan.commit_funds)v->funds=plan.funds_after;
    if(plan.cleanup_lost&&cleanup_lost(c,v,e,n))return -1;
    if(plan.transfer_salvage&&transfer_salvage(a,c,v,e,n))return -1;
    if(sync_inplace(c,v,e,n))return -1;
    if(plan.next==MERCS_DEBRIEF_NEXT_CAREER_END){c->phase=MERCS_CAMPAIGN_ENDED;c->active=0;c->accepted=-1;c->procedural_year=0;
        c->procedural_seed_override=0;c->procedural_seed=0;}
    else if(plan.next==MERCS_DEBRIEF_NEXT_CAMPAIGN){
        char names[1][16]={{0}};
        if(!resource_name(plan.campaign,16,0))return fail(e,n,"Unsupported successor campaign name");
        if(mercs_save_load_campaign(a,c->document,plan.campaign,e,n))return -1;
        strcpy(c->campaign,plan.campaign);
        /* DOS3a038 kind1 continuation loads the next CPGN without transferring
         * a new training roster. MCC scenes are real missions, so a name
         * prefix cannot stand in for the original continuation action. */
        strcpy(names[0],plan.campaign);c->active=1;c->accepted=0;c->procedural_year=0;
        c->procedural_seed_override=0;c->procedural_seed=0;
        if(hydrate_inplace(a,c,v,e,n)||mercs_shell_set_missions(a,v,(const char (*)[16])names,1,e,n))return -1;
        c->selected=0;c->phase=MERCS_CAMPAIGN_READY;
    }else if(plan.next==MERCS_DEBRIEF_NEXT_CONTRACT_END||plan.next==MERCS_DEBRIEF_NEXT_TRAINING_CLEANUP){
        if(plan.next==MERCS_DEBRIEF_NEXT_TRAINING_CLEANUP&&mercs_save_training_cleanup(c->document))
            return fail(e,n,"Cannot apply original terminal training cleanup");
        if(hydrate_inplace(a,c,v,e,n)||available_missions(a,c,v,e,n))return -1;
        c->phase=MERCS_CAMPAIGN_READY;
        mercs_save_header_edit(c->document)[0x239d]=1;
        mercs_save_header_edit(c->document)[0x23a2]=0;
    }else c->phase=MERCS_CAMPAIGN_READY;
    v->last_return_valid=0;v->last_return_mission=-1;
    return hydrate_inplace(a,c,v,e,n);
}

static int stage(const mercs_campaign *c,const mercs_shell_state *v,
                  mercs_campaign *next,mercs_shell_state **view,char *e,size_t n)
{
    if(!c||!v||mercs_campaign_clone(c,next))return fail(e,n,"Cannot stage career transition");
    *view=malloc(sizeof **view);
    if(!*view){mercs_campaign_free(next);return fail(e,n,"Out of memory staging career transition");}
    **view=*v;return 0;
}
static void commit(mercs_campaign *c,mercs_shell_state *v,
                    mercs_campaign *next,const mercs_shell_state *view)
{mercs_campaign_free(c);*c=*next;next->document=NULL;*v=*view;}
int mercs_campaign_accept(prj_archive *a,mercs_campaign *c,mercs_shell_state *v,
                           int index,char *e,size_t n)
{
    mercs_campaign next;mercs_shell_state *view;int rc;prj_record *previous=NULL;size_t previous_count=0;
    if(stage(c,v,&next,&view,e,n))return -1;
    if(prj_get_generated(a,&previous,&previous_count)){
        mercs_campaign_free(&next);free(view);return fail(e,n,"Cannot stage mission resources");
    }
    rc=accept_inplace(a,&next,view,index,e,n);if(!rc)commit(c,v,&next,view);
    if(rc)prj_restore_generated(a,previous,previous_count);
    else prj_free_generated(previous,previous_count);
    mercs_campaign_free(&next);free(view);return rc;
}
int mercs_campaign_return(mercs_campaign *c,mercs_shell_state *v,char *e,size_t n)
{
    mercs_campaign next;mercs_shell_state *view;int rc;
    if(stage(c,v,&next,&view,e,n))return -1;
    rc=return_inplace(&next,view,e,n);if(!rc)commit(c,v,&next,view);
    mercs_campaign_free(&next);free(view);return rc;
}
int mercs_campaign_enter_debrief(prj_archive *a,mercs_campaign *c,mercs_shell_state *v,char *e,size_t n)
{
    mercs_campaign next;mercs_shell_state *view;int rc;
    if(stage(c,v,&next,&view,e,n))return -1;
    rc=enter_debrief_inplace(a,&next,view,e,n);if(!rc)commit(c,v,&next,view);
    mercs_campaign_free(&next);free(view);return rc;
}
int mercs_campaign_continue(prj_archive *a,mercs_campaign *c,mercs_shell_state *v,char *e,size_t n)
{
    mercs_campaign next;mercs_shell_state *view;int rc;
    if(stage(c,v,&next,&view,e,n))return -1;
    rc=continue_inplace(a,&next,view,e,n);if(!rc)commit(c,v,&next,view);
    mercs_campaign_free(&next);free(view);return rc;
}

uint32_t mercs_campaign_crc(const uint8_t *b,size_t n)
{uint32_t crc=0xffffffffu;size_t i;int j;for(i=0;i<n;i++){crc^=b[i];
 for(j=0;j<8;j++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}return ~crc;}

static int context_valid(const mercs_campaign *c,const mercs_shell_state *v)
{
    size_t i;const uint8_t *h;
    if(!c||!v||!c->document||!v->registered||c->phase<0||c->phase>MERCS_CAMPAIGN_ENDED||
       c->active<0||c->active>1||v->mission_count<0||v->mission_count>MERCS_SHELL_MAX_MISSIONS||
       c->accepted < -1||c->accepted>=v->mission_count||c->selected<0||
       (v->mission_count?c->selected>=v->mission_count:c->selected!=0)||
       c->resume_page<0||c->resume_page>=32||
       !resource_name(c->campaign,sizeof c->campaign,0)||
       !token_ok(v->last_return_token)||!token_ok(c->launch_token)||
       !memchr(c->launch_edition,0,sizeof c->launch_edition)||
       v->owned_count<0||v->owned_count>MERCS_SHELL_MAX_OWNED||
       v->owned_index < -1||v->owned_index>=v->owned_count||
       (v->last_return_valid!=0&&v->last_return_valid!=1))return 0;
    h=mercs_save_header(c->document);
    if(v->mode<0||v->mode>1||u32(h+0x23bb)!=(uint32_t)v->mode)return 0;
    for(i=0;i<(size_t)v->mission_count;i++)if(!resource_name(v->missions[i].campaign,16,0))return 0;
    if(c->active){
        if(c->accepted<0||strcasecmp(c->campaign,v->missions[c->accepted].campaign)||
           !scene_equal(h+0x166,v->missions[c->accepted].scene))return 0;
    }else if(c->accepted!=-1)return 0;
    if(c->active&&!strncasecmp(c->campaign,"RAN",3)){
        if(c->procedural_year<3044||c->procedural_year>32767||
           c->procedural_year!=v->missions[c->accepted].procedural_year||
           (c->procedural_seed_override?c->procedural_seed:u32(h+0x162))!=v->missions[c->accepted].procedural_seed)return 0;
    }else if(c->procedural_year||c->procedural_seed_override)return 0;
    if(c->procedural_seed_override<0||c->procedural_seed_override>1)return 0;
    if(v->last_return_valid){
        if(!v->last_return_token[0]||v->last_return_mission<0||v->last_return_mission>=v->mission_count)return 0;
    }else if(v->last_return_mission!=-1)return 0;
    if(c->phase==MERCS_CAMPAIGN_RETURN||c->phase==MERCS_CAMPAIGN_DEBRIEF){
        if(!c->active||!v->last_return_valid||v->last_return_mission!=c->accepted)return 0;
    }else if(c->phase!=MERCS_CAMPAIGN_ENDED&&v->last_return_valid)return 0;
    if(c->phase==MERCS_CAMPAIGN_DEPLOYED){
        if(!c->active||v->owned_index<0||!c->launch_token[0]||
           (strcmp(c->launch_edition,"dos")&&strcmp(c->launch_edition,"direct3d")&&strcmp(c->launch_edition,"3dfx"))||
           !mercs_career_deployed(&v->owned[v->owned_index].current))return 0;
    }else if(c->launch_token[0]||c->launch_edition[0])return 0;
    return 1;
}

int mercs_campaign_encode_resume(const mercs_campaign *c,const mercs_shell_state *v,
                                  const uint8_t *original,size_t size,uint8_t destination[1024])
{
    size_t i,encoded_size;uint8_t out[1024],*encoded=NULL;
    if(!original||!destination||size<MERCS_SAVE_HEADER_BYTES||size>MERCS_SAVE_MAX_BYTES||!context_valid(c,v))return -1;
    if(mercs_save_encode(c->document,1,&encoded,&encoded_size))return -1;
    if(encoded_size!=size||memcmp(encoded,original,size)){free(encoded);return -1;}
    free(encoded);
    memset(out,0,1024);memcpy(out,"MRCSRES2",8);p32(out+8,(uint32_t)size);p32(out+12,mercs_campaign_crc(original,size));
    p32(out+16,(uint32_t)c->phase);p32(out+20,c->rng_state);p32(out+24,(uint32_t)c->accepted);
    p32(out+28,(uint32_t)c->selected);p32(out+32,(uint32_t)c->resume_page);p32(out+36,(uint32_t)c->active);
    p32(out+40,(uint32_t)v->mission_count);p32(out+44,(uint32_t)v->last_return_valid);p32(out+48,(uint32_t)v->last_return_mission);
    memcpy(out+52,c->campaign,strlen(c->campaign));
    for(i=0;i<(size_t)v->mission_count;i++){
        if(!resource_name(v->missions[i].campaign,16,0))return -1;
        memcpy(out+68+i*16,v->missions[i].campaign,strlen(v->missions[i].campaign));
    }
    memcpy(out+228,v->last_return_token,strlen(v->last_return_token));memcpy(out+261,v->last_car.bytes,MERCS_CAR_BYTES);
    memcpy(out+475,c->launch_token,strlen(c->launch_token));memcpy(out+508,c->launch_edition,strlen(c->launch_edition));
    p32(out+520,(uint32_t)c->procedural_year);
    if(c->procedural_seed_override){p32(out+524,c->procedural_seed);p32(out+528,1);}
    for(i=0;i<(size_t)v->mission_count;i++)if(!strncasecmp(v->missions[i].campaign,"RAN",3)){
        if(v->missions[i].procedural_year<3044||v->missions[i].procedural_year>32767)return -1;
        memcpy(out+RESUME_PREVIEWS_OFFSET,"PRV1",4);
        p32(out+RESUME_PREVIEWS_OFFSET+4+i*8,v->missions[i].procedural_seed);
        p32(out+RESUME_PREVIEWS_OFFSET+8+i*8,(uint32_t)v->missions[i].procedural_year);
    }
    p32(out+1020,mercs_campaign_crc(out,1020));memcpy(destination,out,sizeof out);return 0;
}

int mercs_campaign_decode_resume(prj_archive *a,const uint8_t *original,size_t size,
                                  const uint8_t *r,size_t rsize,mercs_campaign *out,
                                  mercs_shell_state *view,char *e,size_t n)
{
    mercs_campaign c;mercs_shell_state *v;uint8_t check[1024];
    char names[MERCS_SHELL_MAX_MISSIONS][16];size_t count,i;int previews;
    if(!a||!original||!r||!out||!view||size<MERCS_SAVE_HEADER_BYTES||size>MERCS_SAVE_MAX_BYTES||
       rsize!=1024||memcmp(r,"MRCSRES2",8)||u32(r+8)!=size||
       u32(r+12)!=mercs_campaign_crc(original,size)||u32(r+1020)!=mercs_campaign_crc(r,1020))
        return fail(e,n,"The saved career and its continuation record do not match");
    previews=!memcmp(r+RESUME_PREVIEWS_OFFSET,"PRV1",4);
    if(!previews&&u32(r+RESUME_PREVIEWS_OFFSET))return fail(e,n,"Unsupported saved preview format");
    memset(&c,0,sizeof c);v=calloc(1,sizeof *v);if(!v)return fail(e,n,"Out of memory loading career");
    if(mercs_save_decode(original,size,&c.document)){fail(e,n,"Invalid original DOS career file");goto bad;}
    c.phase=(int32_t)u32(r+16);c.rng_state=u32(r+20);c.accepted=(int32_t)u32(r+24);c.selected=(int32_t)u32(r+28);
    c.resume_page=(int32_t)u32(r+32);c.active=(int32_t)u32(r+36);count=u32(r+40);
    c.procedural_year=(int32_t)u32(r+520);
    c.procedural_seed=u32(r+524);c.procedural_seed_override=(int32_t)u32(r+528);
    if(count>MERCS_SHELL_MAX_MISSIONS||u32(r+44)>1){fail(e,n,"Invalid saved continuation state");goto bad;}
    memcpy(c.campaign,r+52,16);memcpy(c.launch_token,r+475,33);memcpy(c.launch_edition,r+508,12);
    v->last_return_valid=(int)u32(r+44);v->last_return_mission=(int32_t)u32(r+48);
    memcpy(v->last_return_token,r+228,33);memcpy(v->last_car.bytes,r+261,MERCS_CAR_BYTES);
    memcpy(names,r+68,sizeof names);
    if(mercs_campaign_hydrate(a,&c,v,e,n)||mercs_shell_set_missions(a,v,(const char (*)[16])names,count,e,n))goto bad;
    for(i=0;i<count;i++)if(!strncasecmp(names[i],"RAN",3)){
        const uint8_t *row=r+RESUME_PREVIEWS_OFFSET+4+i*8;
        uint32_t seed;int year;
        if(c.active&&(int)i==c.accepted&&!strncasecmp(c.campaign,"RAN",3)){
            /* The accepted mission's existing fields remain authoritative;
             * contradictory preview metadata is invalid, never an override. */
            seed=c.procedural_seed_override?c.procedural_seed:u32(mercs_save_header(c.document)+0x162);
            year=c.procedural_year;
            if(previews&&(u32(row)!=seed||signed32(u32(row+4))!=year)){
                fail(e,n,"Saved accepted mission and preview identity differ");goto bad;
            }
        }else if(previews){seed=u32(row);year=signed32(u32(row+4));}
        else if(!c.active&&(seed=u32(mercs_save_header(c.document)+0x2342+4*i))<=INT32_MAX)
            year=v->year; /* Original offered-history cache uses the current saved year. */
        else continue; /* Old native files never stored this preview: keep one fresh offer. */
        if(year<3044||year>32767){fail(e,n,"Invalid saved preview generation year");goto bad;}
        if(mercs_shell_mission_load_seeded(a,names[i],v->pilot,year,seed,&v->missions[i],e,n))goto bad;
    }
    if(v->last_return_valid){
        if(v->last_return_mission<0||v->last_return_mission>=v->mission_count||
           memcmp(v->last_car.bytes,mercs_save_header(c.document)+0x2da7,80)||
           mercs_msn_parse(mercs_save_header(c.document)+0x23d3,MERCS_MSN_BYTES,&v->last_msn)){
            fail(e,n,"Invalid saved mission results");goto bad;
        }
    }
    if(mercs_campaign_encode_resume(&c,v,original,size,check)){
        fail(e,n,"Unsupported saved continuation fields");goto bad;
    }
    if(!previews){
        /* Legacy validation still compares every byte. Only the new block
         * is omitted when reconstructing its old canonical representation;
         * nonzero old reserved bytes therefore continue to be rejected. */
        memset(check+RESUME_PREVIEWS_OFFSET,0,RESUME_PREVIEWS_BYTES);
        p32(check+1020,mercs_campaign_crc(check,1020));
    }
    if(memcmp(check,r,sizeof check)){
        fail(e,n,"Unsupported saved continuation fields");goto bad;
    }
    if(c.phase==MERCS_CAMPAIGN_RETURN&&mercs_debrief_enter(v->mode,0,&v->last_car,&c.entry))goto bad;
    if(c.phase==MERCS_CAMPAIGN_DEBRIEF&&mercs_campaign_report(&c,v,e,n))goto bad;
    if(mercs_campaign_prepare_resources(a,&c,e,n))goto bad;
    *out=c;*view=*v;free(v);return 0;
bad:mercs_campaign_free(&c);free(v);return -1;
}
