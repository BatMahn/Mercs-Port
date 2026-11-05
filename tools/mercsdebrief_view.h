#ifndef MERCS_DEBRIEF_VIEW_H
#define MERCS_DEBRIEF_VIEW_H

/* Original DOS debrief presentation, 0x3b5d0/0x3c384. This header deliberately
 * owns no campaign state. The caller first performs the verified return/entry
 * and salvage roll, then prepares this view from the still-pending document.
 * Page0 is authored DBFS/DBFF narrative with objective report substituted at
 * \q, followed by finances/salvage/losses. Page1 is the authored \s analysis.
 * There is one toggle and one conditional leave control, not six tabs.
 * See tests/fixtures/MERCS_DEBRIEF_SETTLEMENT_AUDIT.md for addresses and limits.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mercsart.h"
#include "mercstext.h"
#include "mercsdebrief.h"
#include "mercssave.h"
#include "mercsshell_data.h"
#include "mercsshell_chassis.h"
#include "mercsdebrief_view_data.h"

enum {
    MERCS_DEBRIEF_VIEW_NONE,
    MERCS_DEBRIEF_VIEW_TOGGLE,
    MERCS_DEBRIEF_VIEW_NEXT,
    MERCS_DEBRIEF_VIEW_CONTRACTS,
    MERCS_DEBRIEF_VIEW_REPLAY
};
enum { MERCS_DEBRIEF_VIEW_FONT=10, MERCS_DEBRIEF_VIEW_WIDTH=558,
       MERCS_DEBRIEF_VIEW_HEIGHT=355, MERCS_DEBRIEF_VIEW_LIMIT=1024*1024 };
typedef struct {
    int content_mode, scroll[2], leave_action, ready;
    mercs_text_layout text[2];
    char resource[9], portrait[16];
} mercs_debrief_view;

typedef struct { char *data; size_t size, capacity; int failed; } mdv_buffer;

static inline uint32_t mdv_u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static inline int32_t mdv_i32(const uint8_t *p)
{
    uint32_t value=mdv_u32(p);
    return value<=INT32_MAX?(int32_t)value:-1-(int32_t)(UINT32_MAX-value);
}
static inline int mdv_upper(int c)
{ return c>='a'&&c<='z'?c-'a'+'A':c; }
static inline int mdv_error(char *error,size_t capacity,const char *message)
{ if(error&&capacity)snprintf(error,capacity,"%s",message);return -1; }

static inline int mdv_append_n(mdv_buffer *out,const char *text,size_t length)
{
    size_t capacity;
    char *next;
    if(out->failed)return -1;
    if(length>MERCS_DEBRIEF_VIEW_LIMIT||out->size>MERCS_DEBRIEF_VIEW_LIMIT-length) {
        out->failed=1;return -1;
    }
    if(out->size+length+1>out->capacity) {
        capacity=out->capacity?out->capacity:512;
        while(capacity<out->size+length+1)capacity*=2;
        next=(char *)realloc(out->data,capacity);
        if(!next){out->failed=1;return -1;}
        out->data=next;out->capacity=capacity;
    }
    memcpy(out->data+out->size,text,length);out->size+=length;
    out->data[out->size]=0;return 0;
}
static inline int mdv_append(mdv_buffer *out,const char *text)
{ return mdv_append_n(out,text,strlen(text)); }
static inline int mdv_printf(mdv_buffer *out,const char *format,...)
{
    char local[2048];
    int count;
    va_list args;
    va_start(args,format);count=vsnprintf(local,sizeof local,format,args);va_end(args);
    if(count<0||(size_t)count>=sizeof local){out->failed=1;return -1;}
    return mdv_append_n(out,local,(size_t)count);
}

/* DOS0x5abcc: decimal signed integer with comma groups and no currency suffix. */
static inline void mdv_money(int32_t value,char out[24])
{
    char raw[16];size_t i,n,first,at=0;
    snprintf(raw,sizeof raw,"%d",(int)value);n=strlen(raw);first=raw[0]=='-'?1u:0u;
    for(i=0;i<n;i++) {
        if(i>first&&(n-i)%3==0)out[at++]=',';
        out[at++]=raw[i];
    }
    out[at]=0;
}
static inline int mdv_right(mdv_buffer *out,mercs_art *art,const char *text)
{
    int width=mercs_art_width(art,MERCS_DEBRIEF_VIEW_FONT,text);
    if(width<0||width>999){out->failed=1;return -1;}
    return mdv_printf(out,"\\g514\\b%03d%s",width,text);
}
static inline int mdv_amount(mdv_buffer *out,mercs_art *art,
                              const char *label,int32_t value)
{
    char money[24];mdv_money(value,money);
    if(mdv_append(out,label)||mdv_right(out,art,money))return -1;
    return mdv_append(out,"\\n");
}

/* 0x38894 collapses every run of bytes <= space to one literal space. */
static inline void mdv_objective(const uint8_t raw[MERCS_MSN_TEXT_BYTES],char out[33])
{
    size_t i,n=0;int previous_space=0;
    for(i=0;i<MERCS_MSN_TEXT_BYTES&&raw[i];i++) {
        if(raw[i]<=32) { if(!previous_space)out[n++]=' ';previous_space=1; }
        else { out[n++]=(char)raw[i];previous_space=0; }
    }
    out[n]=0;
}
static inline int mdv_objectives(mdv_buffer *out,mercs_art *art,
                                  const mercs_debrief_terms *terms,
                                  const mercs_debrief_report *report,
                                  const mercs_msn_record *msn)
{
    uint32_t order[MERCS_MSN_ROWS];size_t i;
    if(mercs_debrief_order_rows(msn,order))return -1;
    mdv_append(out,"Time\\g040Type\\g110Objective\\g400Status");
    mdv_right(out,art,terms->mode==1?"Reward":"");mdv_append(out,"\\n");
    for(i=0;i<msn->row_count;i++) {
        const mercs_msn_row *row=&msn->rows[order[i]];
        const char *kind="Unknown",*status;
        int32_t reward=0;char objective[33],time[24],money[24];
        switch(row->priority) {
        case 0:kind="Default";break;
        case 1:kind="Target";reward=row->succeeded?terms->target_pay:0;break;
        case 2:kind="Bonus";reward=row->succeeded?terms->bonus_pay:0;break;
        case 4:kind="Bounty";reward=row->succeeded?terms->bounty_pay:0;break;
        case 8:kind="Return";break;
        default:break;
        }
        status=row->succeeded==1?"Successful":row->succeeded==0?"Failed":"Unknown";
        mdv_objective(row->text,objective);
        if(row->resolved_seconds<0)strcpy(time,"DNF");
        else snprintf(time,sizeof time,"%02d:%02d",
                      (int)(row->resolved_seconds/60%60),(int)(row->resolved_seconds%60));
        mdv_printf(out,"%s\\g040%s\\g110%s",time,kind,objective);
        if(row->priority!=0) {
            if(terms->mode==1)mdv_money(reward,money);else money[0]=0;
            mdv_right(out,art,money);
        }
        mdv_printf(out,"\\g400%s",status);
        if(i+1<msn->row_count||report->confirmed_kills)mdv_append(out,"\\n");
    }
    if(terms->mode==1) {
        if(report->confirmed_kills) {
            mdv_printf(out,"\\n\\g110Cash bounty for %u confirmed kill%s",
                       (unsigned)report->confirmed_kills,report->confirmed_kills==1?"":"s");
            { char money[24];mdv_money(report->kill_income,money);mdv_right(out,art,money); }
        }
    } else mdv_printf(out,"\\n\\n\\g110Total confirmed kills: %u",(unsigned)report->confirmed_kills);
    return out->failed?-1:0;
}

static inline const char *mdv_chassis(const uint8_t *name)
{
    size_t i;
    for(i=0;i<sizeof mercs_shell_chassis/sizeof mercs_shell_chassis[0];i++)
        if(mdv_upper(name[0])==mercs_shell_chassis[i].prefix[0]&&
           mdv_upper(name[1])==mercs_shell_chassis[i].prefix[1]&&
           mdv_upper(name[2])==mercs_shell_chassis[i].prefix[2])return mercs_shell_chassis[i].name;
    return ""; /* original0x57400 has no substitute invented chassis name */
}

/* 0x3a324 groups selected items by numeric ID only, including its trailing
 * comma behavior and empty text for type2. It temporarily clears selection
 * during formatting and restores it; our private flags leave the save intact. */
static inline int mdv_salvage(mdv_buffer *out,const mercs_save_state *document)
{
    size_t ni=mercs_save_list_count(document,MERCS_SAVE_SALVAGE_ITEMS),i,j,count=0,groups=0;
    size_t nm=mercs_save_list_count(document,MERCS_SAVE_SALVAGE_MECHS),selected=0,printed=0;
    uint8_t *flags=ni?(uint8_t *)malloc(ni):NULL;
    const uint8_t *last=NULL;
    int tab=0;
    if(ni&&!flags)return -1;
    for(i=0;i<ni;i++) {
        const uint8_t *p=mercs_save_list_record(document,MERCS_SAVE_SALVAGE_ITEMS,i);
        flags[i]=p[9];if(flags[i]==1)count++;
    }
    if(!count)mdv_append(out,"\\nThere were no weapons salvaged.\\n");
    else {
        mdv_append(out,"\\nSalvaged weapons:\\n");
        for(i=0;i<ni;i++)if(flags[i]) {
            const uint8_t *p=mercs_save_list_record(document,MERCS_SAVE_SALVAGE_ITEMS,i);
            int32_t id=mdv_i32(p);size_t amount=1;
            flags[i]=0;groups++;
            for(j=i+1;j<ni;j++)if(flags[j]==1) {
                const uint8_t *other=mercs_save_list_record(document,MERCS_SAVE_SALVAGE_ITEMS,j);
                if(mdv_i32(other)==id){amount++;flags[j]=0;}
            }
            if(id<0)continue;
            if(!tab){mdv_append(out,"\\t");tab=1;}
            if(p[8]==0||p[8]==1) {
                if(id>=66){free(flags);return -1;}
                mdv_printf(out,p[8]==0?"%u %s":"%u tons (%s) ammo",
                           (unsigned)amount,mercs_debrief_weapon_names[id]);
            }
            if(groups<count)mdv_append(out,", ");
        }
    }
    free(flags);
    for(i=0;i<nm;i++) {
        last=mercs_save_list_record(document,MERCS_SAVE_SALVAGE_MECHS,i);
        if(last[0x84e]==1)selected++;
    }
    if(!selected)mdv_append(out,"\\nThere were no mechs salvaged.\\n");
    else {
        mdv_append(out,"\\nSalvaged Mechs:\\n");tab=0;
        /* Original0x3a6a1 tests the PREVIOUS row, before fetching index i.
         * This display-only quirk never changes which mechs transfer. */
        for(i=0;i<nm;i++)if(last[0x84e]) {
            size_t length;
            last=mercs_save_list_record(document,MERCS_SAVE_SALVAGE_MECHS,i);printed++;
            if(!tab){mdv_append(out,"\\t");tab=1;}
            for(length=0;length<256&&last[0x100+length];length++){}
            if(length==256)return -1;
            if(length)mdv_append_n(out,(const char *)last+0x100,length);
            else mdv_append(out,mdv_chassis(last+0x823));
            if(printed<selected)mdv_append(out,", ");
        }
        mdv_append(out,"\\n");
    }
    return out->failed?-1:0;
}

static inline int mdv_losses(mdv_buffer *out,const mercs_save_state *document)
{
    size_t i,j,np=mercs_save_list_count(document,MERCS_SAVE_OWNED_PILOTS);
    size_t nm=mercs_save_list_count(document,MERCS_SAVE_OWNED_MECHS);
    for(i=0;i<np;i++) {
        const uint8_t *pilot=mercs_save_list_record(document,MERCS_SAVE_OWNED_PILOTS,i);
        if(pilot[0x2064]!=0)continue;
        if(!memchr(pilot,0,32))return -1;
        for(j=0;j<nm;j++) {
            const uint8_t *mech=mercs_save_list_record(document,MERCS_SAVE_OWNED_MECHS,j);
            if(mdv_u32(mech+0x80c)==mdv_u32(pilot+0x24)) {
                mercs_career_mech current;memcpy(current.bytes,mech,sizeof current.bytes);
                if(mercs_career_mech_lost(&current)==1)
                    mdv_printf(out,"%s died in battle losing a %s\\n",
                               (const char *)pilot,mdv_chassis(mech+0x81c));
                break;
            }
        }
    }
    /* Aerospace return-MEK loading and its newly written pilot+0x5d loss
     * marker belong to the return adapter, not a read-only drawing helper. */
    return out->failed?-1:0;
}

static inline int mdv_finances(mdv_buffer *out,mercs_art *art,
                                const mercs_debrief_terms *terms,
                                const mercs_debrief_report *report)
{
    uint32_t negated=0u-(uint32_t)report->expense;
    int32_t negative=negated<=INT32_MAX?(int32_t)negated:-1-(int32_t)(UINT32_MAX-negated);
    mdv_amount(out,art,"Funds: ",terms->funds);
    mdv_amount(out,art,"Contract compensation: ",report->contract_income);
    mdv_amount(out,art,"Total Income: ",report->income);
    mdv_amount(out,art,"Total expenses:  ",negative);
    mdv_right(out,art,"--------------");mdv_append(out,"\\n");
    mdv_amount(out,art,"Balance: ",report->balance);
    return out->failed?-1:0;
}

static inline void mercs_debrief_view_free(mercs_debrief_view *view)
{
    if(!view)return;
    mercs_text_free(&view->text[0]);mercs_text_free(&view->text[1]);memset(view,0,sizeof *view);
}

/* Original leave-button selection0x3b0c0/0x3b27c. Replay overlays the regular
 * next rectangle through hotspot4; hotspot5/exit is constructed but hidden. */
static inline int mdv_leave_action(const mercs_debrief_terms *terms,
                                    const mercs_debrief_report *report)
{
    if(terms->mode==0) {
        if(!report->success)return MERCS_DEBRIEF_VIEW_REPLAY;
        if(terms->campaign_type==1)return MERCS_DEBRIEF_VIEW_NEXT;
        if(terms->campaign_type>=2&&terms->campaign_type<=4)return MERCS_DEBRIEF_VIEW_CONTRACTS;
    } else {
        if(terms->campaign_type==1)return terms->owned_mech_count?
            MERCS_DEBRIEF_VIEW_NEXT:MERCS_DEBRIEF_VIEW_CONTRACTS;
        if(terms->campaign_type==3&&report->success)return MERCS_DEBRIEF_VIEW_NEXT;
        if(terms->campaign_type>=2&&terms->campaign_type<=4)return MERCS_DEBRIEF_VIEW_CONTRACTS;
    }
    return MERCS_DEBRIEF_VIEW_NONE;
}

/* DOS0x3b27c calls layout0x6fb0c again on the existing narrative object.
 * It does not concatenate across the authored section stop: an unfinished
 * line before \\s is discarded. LUT4DBFS exercises that original behavior. */
static inline int mdv_append_layout(mercs_text_layout *out,mercs_art *art,const char *raw)
{
    mercs_text_layout tail={0};mercs_text_run *runs;size_t i,count;int y;
    if(mercs_text_prepare(&tail,art,raw,"",MERCS_DEBRIEF_VIEW_FONT,MERCS_DEBRIEF_VIEW_WIDTH))return -1;
    if(out->rows>INT32_MAX/out->line_height||tail.rows>INT32_MAX-out->rows||
       out->count>SIZE_MAX-tail.count)goto bad;
    y=out->rows*out->line_height;count=out->count+tail.count;
    if(count>SIZE_MAX/sizeof *runs||tail.height>INT32_MAX-y)goto bad;
    if(count>out->capacity) {
        runs=(mercs_text_run *)realloc(out->runs,count*sizeof *runs);
        if(!runs)goto bad;
        out->runs=runs;out->capacity=count;
    }
    for(i=0;i<tail.count;i++) {
        tail.runs[i].y+=y;out->runs[out->count+i]=tail.runs[i];
    }
    out->count=count;out->rows+=tail.rows;
    if(out->height<y+tail.height)out->height=y+tail.height;
    out->unsupported_controls+=tail.unsupported_controls;
    /* Run strings now belong to out; the extra layout's expansion is private. */
    free(tail.runs);tail.runs=NULL;tail.count=tail.capacity=0;mercs_text_free(&tail);return 0;
bad:
    mercs_text_free(&tail);return -1;
}

/* Returns0 on success. A zero-initialized view is required on first use;
 * subsequent successful preparations replace it atomically. Original story
 * text remains in the user's PRJ. No replacement mission prose is generated. */
static inline int mercs_debrief_view_prepare(mercs_debrief_view *view,mercs_art *art,
    prj_archive *archive,const mercs_save_state *document,const mercs_shell_state *shell_view,
    const mercs_debrief_terms *terms,const mercs_debrief_report *report,const char *scene,
    char *error,size_t error_size)
{
    mercs_debrief_view next;
    mdv_buffer raw={0},objectives={0},tail={0},page={0};
    prj_record record;size_t at,i,section;int found=0,rc=-1;
    const uint8_t *header;
    memset(&next,0,sizeof next);memset(&record,0,sizeof record);
    if(!view||!art||!archive||!document||!shell_view||!terms||!report||!scene)
        return mdv_error(error,error_size,"Invalid original debrief view");
    for(i=0;i<4&&scene[i]&&scene[i]!='.';i++)next.resource[i]=(char)mdv_upper((unsigned char)scene[i]);
    while(i<4)next.resource[i++]='_';
    memcpy(next.resource+4,report->success?"DBFS":"DBFF",5);
    if(prj_read_named(archive,"BWD",next.resource,&record)!=PRJ_OK) {
        if(error&&error_size)snprintf(error,error_size,"Missing original debrief %.8s",next.resource);
        return -1;
    }
    if(record.size<12||memcmp(record.data,"BWD\0",4)||mdv_u32(record.data+4)!=record.size)
        goto malformed;
    for(at=12;at<record.size;) {
        uint32_t size;
        if(record.size-at<8)goto malformed;
        size=mdv_u32(record.data+at+4);
        if(size<8||size>record.size-at)goto malformed;
        if(!memcmp(record.data+at,"ORDR",4)) {
            const uint8_t *text=record.data+at+8;
            size_t length=0;
            while(length<size-8&&text[length])length++;
            if(found)mdv_append(&raw,"\\n\\n");
            mdv_append_n(&raw,(const char *)text,length);found++;
        }
        at+=size;
    }
    if(!found||raw.failed)goto malformed;
    if(mdv_objectives(&objectives,art,terms,report,&shell_view->last_msn))goto malformed;
    if(terms->mode==1) {
        if(mdv_finances(&tail,art,terms,report)||
           (report->success&&mdv_salvage(&tail,document))||mdv_losses(&tail,document))goto malformed;
    }
    /* Locate authored section before substitution, so a literal player name
     * cannot be interpreted as an extra original page boundary. */
    section=raw.size;
    for(i=0;i+1<raw.size;i++)if(raw.data[i]=='\\'&&mdv_upper(raw.data[i+1])=='S'){section=i;break;}
    mdv_append_n(&page,raw.data,section);
    if(section<raw.size)mdv_append(&page,"\\s");
    if(page.failed||mercs_text_prepare_names(&next.text[0],art,page.data?page.data:"",
         shell_view->pilot,shell_view->unit,objectives.data?objectives.data:"",
         MERCS_DEBRIEF_VIEW_FONT,MERCS_DEBRIEF_VIEW_WIDTH))goto malformed;
    if(tail.data&&mdv_append_layout(&next.text[0],art,tail.data))goto malformed;
    if(mercs_text_prepare_names(&next.text[1],art,section<raw.size?raw.data+section+2:"",
         shell_view->pilot,shell_view->unit,objectives.data?objectives.data:"",
         MERCS_DEBRIEF_VIEW_FONT,MERCS_DEBRIEF_VIEW_WIDTH))goto malformed;
    next.leave_action=mdv_leave_action(terms,report);
    header=mercs_save_header(document);
    if(terms->mode==1&&header&&memchr(header+0x1b6,0,16))
        memcpy(next.portrait,header+0x1b6,16);
    next.ready=1;mercs_debrief_view_free(view);*view=next;memset(&next,0,sizeof next);rc=0;
    goto done;
malformed:
    mdv_error(error,error_size,"Original debrief text or report could not be prepared");
done:
    mercs_debrief_view_free(&next);prj_record_free(&record);
    free(raw.data);free(objectives.data);free(tail.data);free(page.data);return rc;
}

static inline void mercs_debrief_view_draw(const mercs_debrief_view *view,mercs_art *art)
{
    const char *leave=NULL;
    if(!view||!art||!view->ready)return;
    /* DOS 0x3b5d0 requests one-based resource 0x26. shelldb indexes the
     * DATABASE table from zero; this is entry37, not repair entry38. */
    mercs_art_background(art,37);
    if(view->portrait[0])mercs_art_clip_flags(art,view->portrait,355,22,0x68u,0);
    mercs_text_draw(&view->text[view->content_mode?1:0],art,46,38,
                   MERCS_DEBRIEF_VIEW_HEIGHT,view->scroll[view->content_mode?1:0],22);
    mercs_art_button(art,view->content_mode?"lbbdb":"lbba",114,422,0);
    switch(view->leave_action) {
    case MERCS_DEBRIEF_VIEW_NEXT:leave="lbbn";break;
    case MERCS_DEBRIEF_VIEW_CONTRACTS:leave="lbcsp";break;
    case MERCS_DEBRIEF_VIEW_REPLAY:leave="lbrp";break;
    default:break;
    }
    if(leave)mercs_art_button(art,leave,351,433,0);
}

static inline void mercs_debrief_view_draw_rows(const mercs_debrief_view *view,
                                                mercs_art *art,int visible_rows)
{
    mercs_debrief_view clipped;
    mercs_text_layout *text;
    size_t i;
    if(!view||!view->ready)return;
    if(visible_rows<0) { mercs_debrief_view_draw(view,art); return; }
    clipped=*view;
    text=&clipped.text[clipped.content_mode?1:0];
    for(i=0;i<text->count;i++)
        if(text->line_height<=0||text->runs[i].y/text->line_height>=visible_rows)break;
    text->count=i;
    mercs_debrief_view_draw(&clipped,art);
}

static inline int mercs_debrief_view_click(mercs_debrief_view *view,int x,int y)
{
    if(!view||!view->ready)return MERCS_DEBRIEF_VIEW_NONE;
    if(x>=104&&x<=275&&y>=416&&y<=470) {
        view->content_mode=!view->content_mode;return MERCS_DEBRIEF_VIEW_TOGGLE;
    }
    if(x>=338&&x<=503&&y>=423&&y<=471)return view->leave_action;
    return MERCS_DEBRIEF_VIEW_NONE;
}
static inline void mercs_debrief_view_scroll(mercs_debrief_view *view,int delta)
{
    int mode,maximum;int64_t position;
    if(!view||!view->ready)return;
    mode=view->content_mode?1:0;maximum=view->text[mode].height-MERCS_DEBRIEF_VIEW_HEIGHT;
    if(maximum<0)maximum=0;
    position=(int64_t)view->scroll[mode]+delta;
    view->scroll[mode]=position<0?0:position>maximum?maximum:(int)position;
}

#endif
