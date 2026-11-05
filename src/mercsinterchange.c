#include "mercsinterchange.h"
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
static int fail(char *e,size_t n,const char *fmt,...)
{va_list args;if(e&&n){va_start(args,fmt);vsnprintf(e,n,fmt,args);va_end(args);}return -1;}
static int resource_name(const char *s,size_t n)
{size_t i;for(i=0;i<n;i++){unsigned c=(unsigned char)s[i];if(!c)return i!=0;
 if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return 0;}return 0;}
static int fixed_equal(const uint8_t *a,const uint8_t *b,size_t n)
{return memchr(a,0,n)&&memchr(b,0,n)&&!strcasecmp((const char*)a,(const char*)b);}

int mercs_interchange_screen_valid(uint32_t event)
{
    switch(event){
    case MERCS_ORIGINAL_DEBRIEF:case MERCS_ORIGINAL_REGISTER:case MERCS_ORIGINAL_DROPSHIP:
    case MERCS_ORIGINAL_MECH_ROOM:case MERCS_ORIGINAL_OFFICE:case MERCS_ORIGINAL_MECHBAY:
    case MERCS_ORIGINAL_HALL:case MERCS_ORIGINAL_FACTORY:case MERCS_ORIGINAL_TRADING:
    case MERCS_ORIGINAL_INVENTORY:case MERCS_ORIGINAL_FINANCE:case MERCS_ORIGINAL_COMPUTER:
    case MERCS_ORIGINAL_PERSONNEL:case MERCS_ORIGINAL_CONTRACTS:case MERCS_ORIGINAL_ROSTER:
    case MERCS_ORIGINAL_NEWS:case MERCS_ORIGINAL_REPAIR:case MERCS_ORIGINAL_MENU:
    case MERCS_ORIGINAL_SAVE:case MERCS_ORIGINAL_GROUPS:case MERCS_ORIGINAL_MECHLAB:
    case MERCS_ORIGINAL_MECHOPS:case MERCS_ORIGINAL_BRIEFING:case MERCS_ORIGINAL_COMMAND:
    case MERCS_ORIGINAL_CREW_BIO:return 1;
    default:return 0;
    }
}

int mercs_interchange_prepare(mercs_campaign *c,const mercs_shell_state *v,
                               uint32_t event,char *e,size_t n)
{
    mercs_campaign next;uint8_t *h;uint32_t selected;size_t i;
    if(!c||!v||!mercs_interchange_screen_valid(event)||event==MERCS_ORIGINAL_SAVE||
       v->mission_count<0||v->mission_count>MERCS_SHELL_MAX_MISSIONS||
       c->phase==MERCS_CAMPAIGN_DEPLOYED||c->phase==MERCS_CAMPAIGN_RETURN||c->phase==MERCS_CAMPAIGN_ENDED||
       ((c->phase==MERCS_CAMPAIGN_DEBRIEF)!=(event==MERCS_ORIGINAL_DEBRIEF)))
        return fail(e,n,"This career cannot be exported at the selected original screen");
    if(mercs_campaign_clone(c,&next))return fail(e,n,"Out of memory preparing original saved game");
    if(mercs_campaign_sync(&next,v,e,n)){mercs_campaign_free(&next);return -1;}
    h=mercs_save_header_edit(next.document);p32(h+0x23c3,event);
    /* The original writer copies current to previous. Keeping both staged
     * values equal also makes the standalone document valid before encoding. */
    p32(h+0x23c7,event);p32(h+0x23cb,event);
    if(event==MERCS_ORIGINAL_DEBRIEF){h[0x23b7]=1;h[0x23b8]=0;}
    /* While offers are displayed, their indices match the original active
     * SLST/FLST pool. After acceptance the CPGN has replaced that pool, so
     * the retained former offers must not overwrite its history slots. */
    if(!c->active)for(i=0;i<(size_t)v->mission_count;i++)
        if(!strncasecmp(v->missions[i].campaign,"RAN",3))
            p32(h+0x2342+4*i,v->missions[i].procedural_seed);
    if(c->active&&!strncasecmp(c->campaign,"RAN",3)){
        selected=u32(h+0x239e);
        if(selected>=10){mercs_campaign_free(&next);return fail(e,n,"Invalid original procedural contract selection");}
        p32(h+0x2342+4*selected,c->procedural_seed_override?c->procedural_seed:u32(h+0x162));
        p32(h+0x23cf,(uint32_t)(isdigit((unsigned char)c->campaign[3])?c->campaign[3]-'0':0));
    }
    mercs_campaign_free(c);*c=next;return 0;
}

/* CPGN0x68150 copies these exact fields. The saved date can advance during
 * play, so it only breaks an otherwise ambiguous match. Shop and salvage
 * rows are deliberately never regenerated to identify their campaign. */
static int campaign_match(const uint8_t *h,const prj_record *r,int *date_match)
{
    static const unsigned fields[][2]={{0,0x1fe},{92,0x1e6},{96,0x1ea},{104,0x1fa},
                                     {124,0x1ee},{128,0x1f2},{132,0x1f6}};
    static const unsigned strings[][2]={{28,0x176},{44,0x186},{60,0x196},{108,0x1d6},{136,0x1c6}};
    size_t at,limit,i;const uint8_t *p;int found=0,procedural=!strncasecmp(r->name,"RAN",3);
    if(r->size<172||memcmp(r->data,"BWD\0",4)||(limit=u32(r->data+4))!=r->size)return 0;
    for(at=12;at<limit;){
        const uint8_t *q=r->data+at;size_t z;
        if(limit-at<8||(z=u32(q+4))<8||z>limit-at)return 0;
        if(!memcmp(q,"CPGN",4)){
            if(found||z<160)return 0;
            found=1;p=q+8;
            for(i=0;i<sizeof fields/sizeof fields[0];i++)
                if(u32(p+fields[i][0])!=u32(h+fields[i][1]))return 0;
            for(i=0;i<sizeof strings/sizeof strings[0];i++)
                if(!fixed_equal(p+strings[i][0],h+strings[i][1],16))return 0;
            if(procedural){
                char scene[16],saved[16];size_t length;
                snprintf(scene,sizeof scene,"RAN%dSCN1",isdigit((unsigned char)r->name[3])?r->name[3]-'0':0);
                if(!memchr(h+0x166,0,16))return 0;
                memcpy(saved,h+0x166,sizeof saved);length=strlen(saved);
                if(length>4&&!strcasecmp(saved+length-4,".BWD"))saved[length-4]=0;
                if(strcasecmp(saved,scene))return 0;
            }else if(!fixed_equal(p+12,h+0x166,16))return 0;
            *date_match=u32(p+4)==u32(h+0x122)&&u32(p+8)==u32(h+0x126);
            /* CPGN length includes its nested tagged children. */
            for(i=160;i<z;){
                const uint8_t *child=q+i;size_t child_size,base,count,j;
                if(z-i<8||(child_size=u32(child+4))<8||child_size>z-i){
                    if(prj_known_mercs_campaign_tail(r)&&at+i==1812)break;
                    return 0;
                }
                if(!memcmp(child,"SLST",4)||!memcmp(child,"FLST",4)){
                    base=!memcmp(child,"SLST",4)?0x202:0x2a2;count=(child_size-8)/16;
                    if((child_size-8)%16||count>10||count!=u32(h+(base==0x202?0x2392:0x2396)))return 0;
                    for(j=0;j<count;j++)if(!fixed_equal(child+8+j*16,h+base+j*16,16))return 0;
                }
                i+=child_size;
            }
        }
        at+=z;
    }
    return found;
}

static int resolve_campaign(prj_archive *a,const uint8_t *h,char out[16],char *e,size_t n)
{
    int type=prj_find_type(a,"BWD"),i,total,matches=0,dated=0;char match[16]={0},date[16]={0};
    if(type<0)return fail(e,n,"The archive has no campaign resources");
    total=prj_symbol_count(a,type);
    for(i=0;i<total;i++){
        const char *name=prj_symbol_name(a,type,i);prj_record r;int calendar=0;
        if(!resource_name(name,16)||prj_read(a,type,prj_symbol_id(a,type,i),&r))continue;
        if(!r.generated&&campaign_match(h,&r,&calendar)){
            snprintf(match,sizeof match,"%s",name);matches++;
            if(calendar){snprintf(date,sizeof date,"%s",name);dated++;}
        }
        prj_record_free(&r);
    }
    if(matches==1)strcpy(out,match);
    else if(dated==1)strcpy(out,date);
    else return fail(e,n,matches?"The original save matches more than one campaign resource":
                                "The original save's campaign fields do not match this game archive");
    return 0;
}

int mercs_interchange_import(prj_archive *a,const uint8_t *bytes,size_t size,
                              mercs_campaign *out,mercs_shell_state *view,
                              uint32_t *event_out,char *e,size_t n)
{
    mercs_campaign c={0};mercs_shell_state *v;uint8_t *h,*encoded=NULL,resume[1024];
    uint32_t event,selected;char names[10][16]={{0}};size_t count,i,encoded_size;int active;
    if(e&&n)e[0]=0;
    if(!a||!bytes||!out||!view||!event_out)return fail(e,n,"Missing original saved-game input");
    if(mercs_save_decode(bytes,size,&c.document))return fail(e,n,
        "The original saved game is truncated, invalid or exceeds the supported limits");
    v=calloc(1,sizeof *v);if(!v){mercs_campaign_free(&c);return fail(e,n,"Out of memory importing saved game");}
    h=mercs_save_header_edit(c.document);event=u32(h+0x23c7);selected=u32(h+0x239e);
    if(!mercs_interchange_screen_valid(event)||event==MERCS_ORIGINAL_SAVE){
        fail(e,n,"The original saved game has an unsupported resume screen");goto bad;
    }
    if(h[0x239c]>1||h[0x239d]>1||u32(h+0x23bb)>1||selected>=10||
       !memchr(h+2,0,256)||!memchr(h+0x166,0,16)){
        fail(e,n,"The original saved game has invalid campaign context");goto bad;
    }
    if(event==MERCS_ORIGINAL_DEBRIEF&&h[0x23b7]!=1){
        fail(e,n,"This unfinished mission return is not an original manual debrief save");goto bad;
    }
    if(resolve_campaign(a,h,c.campaign,e,n))goto bad;
    mercs_save_restore_markers(c.document);h[0x239a]=h[0x239b]=0;
    v->last_return_mission=-1;c.rng_state=u32(h+0x162);c.accepted=-1;
    if(mercs_campaign_hydrate(a,&c,v,e,n))goto bad;
    active=h[0x239d]==0||event==MERCS_ORIGINAL_DEBRIEF;
    if(active){
        c.active=1;c.accepted=0;c.selected=0;v->mission_count=1;
        if(!strncasecmp(c.campaign,"RAN",3)){
            c.procedural_year=v->year;c.procedural_seed=u32(h+0x2342+4*selected);
            c.procedural_seed_override=c.procedural_seed!=u32(h+0x162);
            if(mercs_shell_mission_load_seeded(a,c.campaign,v->pilot,c.procedural_year,
                                                c.procedural_seed,&v->missions[0],e,n))goto bad;
        }else if(mercs_shell_mission_load(a,c.campaign,v->pilot,&v->missions[0],e,n))goto bad;
    }else{
        size_t base=h[0x239c]?0x202:0x2a2;count=u32(h+(h[0x239c]?0x2392:0x2396));
        if(count>10){fail(e,n,"The original saved game has too many available contracts");goto bad;}
        for(i=0;i<count;i++){
            memcpy(names[i],h+base+16*i,16);
            if(!resource_name(names[i],16)){fail(e,n,"An original saved contract name is invalid");goto bad;}
        }
        if(mercs_shell_set_missions(a,v,(const char (*)[16])names,count,e,n))goto bad;
        c.selected=selected<count?(int)selected:0;
        /* DOS0x2af40 tests the cached DWORD as signed: zero is a valid
         * preview seed; every negative value means no cached preview. */
        for(i=0;i<count;i++)if(!strncasecmp(names[i],"RAN",3)&&u32(h+0x2342+4*i)<=INT32_MAX){
            if(mercs_shell_mission_load_seeded(a,names[i],v->pilot,v->year,u32(h+0x2342+4*i),&v->missions[i],e,n))goto bad;
        }
    }
    c.phase=event==MERCS_ORIGINAL_DEBRIEF?MERCS_CAMPAIGN_DEBRIEF:MERCS_CAMPAIGN_READY;
    if(c.phase==MERCS_CAMPAIGN_DEBRIEF){
        v->last_return_valid=1;v->last_return_mission=0;
        /* The original manual save contains only the80-byte CAR prefix.
         * The native-only multiplayer tail is zero initialized, not claimed
         * to have been recovered from this standalone file. */
        memcpy(v->last_car.bytes,h+0x2da7,80);
        if(mercs_msn_parse(h+0x23d3,MERCS_MSN_BYTES,&v->last_msn)){
            fail(e,n,"The original saved debrief has invalid embedded results");goto bad;
        }
        snprintf(v->last_return_token,sizeof v->last_return_token,"%08x%08x%08x%08x",
                 mercs_campaign_crc(bytes,size),(uint32_t)size,
                 mercs_campaign_crc(h+0x23d3,MERCS_MSN_BYTES),mercs_campaign_crc(h+0x2da7,80));
        if(mercs_campaign_report(&c,v,e,n))goto bad;
    }
    /* Check every native continuation invariant before installing generated
     * resources or replacing caller state. No normal-return merge/settlement. */
    if(mercs_save_encode(c.document,1,&encoded,&encoded_size)||
       mercs_campaign_encode_resume(&c,v,encoded,encoded_size,resume)){
        fail(e,n,"The original saved game cannot form a valid native continuation");goto bad;
    }
    if(mercs_campaign_prepare_resources(a,&c,e,n))goto bad;
    free(encoded);*out=c;*view=*v;*event_out=event;free(v);return 0;
bad:free(encoded);mercs_campaign_free(&c);free(v);return -1;
}
