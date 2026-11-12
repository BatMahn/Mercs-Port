#ifndef MERCS_SALVAGE_VIEW_H
#define MERCS_SALVAGE_VIEW_H

/* Titanium's Salvage Yard (MW2SHELL.DLL Salvage.cpp: setup 0x10069bc0, events
 * 0x10065e2f, tabs 0x100647cf, location list 0x100630ed, discards 0x10064049).
 * Background: Titanium DATABASE.MW2 resource 47 (zero-based entry 46). Art not
 * on the retail disc (MSBACK, ARROW, TAKE, LEAVE, CHSS, RCHASS SHPs and the
 * S* category clips) comes from the Titanium folder's SMK directory. Fonts:
 * shell font entries 10 (lists) and 8 (the wreck box), as Titanium loads them.
 * Ink: 22 normal, 17 selected / headers, 143 damaged, 34 a recoverable wreck. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mercsart.h"
#include "mercssalvage.h"
#include "mercsdebrief.h"
#include "mercssave.h"

enum { MSV_ROWS = 10, MSV_GROUPS = 128 };

typedef struct { unsigned kind; int32_t id; int intact, damaged; } msv_group;

typedef struct {
    int ready, wreck, tab, discards, location, list_open, selected_slot, right, middle;
    int32_t selected_code;        /* the item highlighted in the location list (its stock code) */
    msv_group right_rows[MSV_GROUPS], middle_rows[MSV_GROUPS];
    int right_count, middle_count;
    const shell_image *background;
} mercs_salvage_view;

enum { MSV_NONE, MSV_EXIT, MSV_NOTICE };

static const char *const msv_tabs[6] = {"bcmiss","bcenrg","bcproj","bcequp","bcammo","chss"};
static const int msv_tab_x[6] = {15,105,194,284,373,462};
static const char *const msv_pieces[8] = {"head","rtor","ctor","ltor","rarm","larm","rleg","lleg"};
static const int msv_piece_x[8] = {105,75,88,120,26,154,11,129}, msv_piece_y[8] = {122,108,154,107,126,126,173,173};
/* the diagram's hotspots (table 0x100b9ba8 entries 10..17) and the location each selects */
static const int msv_loc_box[8][4] = {{104,122,135,157},{135,101,163,156},{99,156,140,229},{74,101,104,156},
                                      {18,121,67,228},{164,121,218,228},{7,228,107,354},{143,228,231,355}};
static const int msv_loc_of[8] = {0,3,2,1,4,5,6,7};
static const char *const msv_clips[6] = {"smissile","slaser","sproj","sequip","sammo","smech"};
static const char *const msv_loc_name[8] = {"Head","Right Torso","Center Torso","Left Torso",
                                            "Right Arm","Left Arm","Right Leg","Left Leg"};

static inline int msv_in(int x,int y,int x0,int y0,int x1,int y1){return x>=x0&&x<=x1&&y>=y0&&y<=y1;}
static inline int msv_slots(int loc){return loc==0||loc==6||loc==7?6:12;}

static inline void msv_money(int32_t v,char *out,size_t n)
{
    char d[24];int len,i,j=0;
    snprintf(d,sizeof d,"%ld",(long)(v<0?-(long)v:(long)v));len=(int)strlen(d);
    if(v<0&&j<(int)n-1)out[j++]='-';
    for(i=0;i<len&&j<(int)n-1;i++){out[j++]=d[i];if((len-i-1)%3==0&&i<len-1&&j<(int)n-1)out[j++]=',';}
    out[j]=0;
}

/* grouped rows of an item list in a category (0x100639a2 / 0x10063eb0) */
static inline int msv_group_rows(const uint8_t (*rows)[MERCS_SAVE_ITEM_BYTES],size_t count,int tab,
                                 msv_group *out,int cap)
{
    size_t i;int n=0,j;
    for(i=0;i<count;i++){
        const uint8_t *p=rows[i];
        int32_t id=(int32_t)((uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24);
        if(mercs_salvage_row_category(p[8],id)!=tab)continue;
        for(j=0;j<n;j++)if(out[j].kind==p[8]&&out[j].id==id)break;
        if(j==n){if(n>=cap)continue;out[n].kind=p[8];out[n].id=id;out[n].intact=out[n].damaged=0;n++;}
        if(mercs_salvage_row_damaged(p))out[j].damaged++;else out[j].intact++;
    }
    return n;
}

static inline int msv_list_groups(const mercs_save_state *d,int tab,msv_group *out,int cap)
{
    enum mercs_save_list list=tab==4?MERCS_SAVE_OWNED_AMMO:MERCS_SAVE_OWNED_WEAPONS;
    size_t n=mercs_save_list_count(d,list),i;int count;
    uint8_t (*rows)[MERCS_SAVE_ITEM_BYTES]=n?malloc(n*MERCS_SAVE_ITEM_BYTES):NULL;
    if(n&&!rows)return 0;
    for(i=0;i<n;i++)memcpy(rows[i],mercs_save_list_record(d,list,i),MERCS_SAVE_ITEM_BYTES);
    count=msv_group_rows((const uint8_t (*)[MERCS_SAVE_ITEM_BYTES])rows,n,tab,out,cap);
    free(rows);return count;
}

static inline void mercs_salvage_view_refresh(mercs_salvage_view *v,const mercs_salvage_yard *y)
{
    if(v->tab<5){
        v->right_count=msv_list_groups(y->document,v->tab,v->right_rows,MSV_ROWS);
        v->middle_count=msv_group_rows((const uint8_t (*)[MERCS_SAVE_ITEM_BYTES])y->discards,y->discard_count,
                                       v->tab,v->middle_rows,MSV_ROWS);
    }else{
        size_t n=mercs_save_list_count(y->document,MERCS_SAVE_OWNED_MECHS);
        v->right_count=(int)(n>MSV_ROWS?MSV_ROWS:n);v->middle_count=0;
    }
    if(v->right>=v->right_count)v->right=-1;
    if(v->middle>=v->middle_count)v->middle=-1;
    if(v->wreck>=(int)y->count)v->wreck=y->count?(int)y->count-1:0;
}

static inline void mercs_salvage_view_select_wreck(mercs_salvage_view *v,mercs_salvage_yard *y,int index)
{
    v->wreck=index;v->location=0;v->selected_slot=-1;v->selected_code=0;
    if(index>=0&&(size_t)index<y->count)y->entries[index].visited=1;
}

static inline void mercs_salvage_view_open(mercs_salvage_view *v,mercs_salvage_yard *y,const shell_image *bg)
{
    memset(v,0,sizeof *v);v->ready=1;v->background=bg;v->right=v->middle=-1;
    mercs_salvage_view_select_wreck(v,y,0);mercs_salvage_view_refresh(v,y);
}

static inline void msv_number_right(mercs_art *a,int x,int y,const char *text,int ink)
{ mercs_art_text(a,10,x-mercs_art_width(a,10,text),y,text,ink); }

static inline void mercs_salvage_view_draw(const mercs_salvage_view *v,mercs_art *a,const mercs_salvage_yard *y)
{
    char text[160],name[96];int i,j;
    const mercs_salvage_entry *e=y->count&&v->wreck<(int)y->count?&y->entries[v->wreck]:NULL;
    if(!v->ready)return;
    if(v->background)mercs_art_background_image(a,v->background);
    for(i=0;i<6;i++)mercs_art_button(a,msv_tabs[i],msv_tab_x[i],12,v->tab==i);
    /* the wreck box and its arrow (frame 0 while the list is open) */
    if(v->discards)mercs_art_text(a,8,22,68,"DISCARDS",17);
    else if(e){snprintf(text,sizeof text,"%.60s",(const char *)e->current.bytes);mercs_art_text(a,8,22,68,text,17);}
    mercs_art_named_shape(a,"arrow",v->list_open?0:1,200,60);
    if(e&&!v->discards){
        for(i=0;i<8;i++){
            int c=mercs_salvage_condition(e->current.bytes,i);
            if(c==3)continue;
            mercs_art_named_shape(a,msv_pieces[i],c+(i==v->location?3:0),msv_piece_x[i],msv_piece_y[i]);
        }
    }
    if(v->discards||v->list_open){
        mercs_art_named_shape(a,"msback",0,7,98);
        if(v->discards&&!v->list_open)
            mercs_art_clip_flags(a,msv_clips[v->tab],40,v->tab==5?150:160,8u,0);
    }
    if(v->list_open){
        size_t k;
        for(k=0;k<y->count;k++){
            const mercs_salvage_entry *w=&y->entries[k];
            snprintf(text,sizeof text,"%s %.60s",w->visited?"[*]":"[ ]",(const char *)w->current.bytes);
            mercs_art_text(a,10,40,125+12*(int)k,text,!v->discards&&(int)k==v->wreck?17:w->whole?34:22);
        }
        mercs_art_text(a,10,40,140+12*(int)y->count,"DISCARDS",v->discards?17:22);
    }
    if(e&&!v->discards&&e->whole)mercs_art_button(a,"rchass",25,409,0);
    mercs_art_button(a,"take",297,383,0);
    mercs_art_button(a,"leave",503,383,0);
    mercs_art_button(a,"bcexit",509,440,0);
    /* middle: the location's components, or the discards */
    if(!v->discards&&e){
        int cond=mercs_salvage_condition(e->current.bytes,v->location);
        mercs_art_text(a,10,256,86,msv_loc_name[v->location],17);
        for(j=0;j<msv_slots(v->location);j++){
            int32_t id=(int32_t)((uint32_t)e->listing.bytes[0x368+48*v->location+4*j]|
                                 (uint32_t)e->listing.bytes[0x369+48*v->location+4*j]<<8|
                                 (uint32_t)e->listing.bytes[0x36a+48*v->location+4*j]<<16|
                                 (uint32_t)e->listing.bytes[0x36b+48*v->location+4*j]<<24);
            int st=e->status[v->location][j],c=mercs_salvage_category(id),sel=v->selected_code&&id==v->selected_code;
            int yy=113+12*j,ink=sel?17:22;
            if(cond==3)continue;
            if(id<1||c<0)snprintf(name,sizeof name,"-");
            else if(c==4||st==0||st==-1)mercs_salvage_item_name(id,name,sizeof name);
            else if(st==1){mercs_salvage_item_name(id,name,sizeof name);if(!sel)ink=143;}
            else snprintf(name,sizeof name,"-");
            mercs_art_text(a,10,256,yy,name,ink);
        }
        if(v->selected_code){snprintf(text,sizeof text,"Critical slots : %d",mercs_salvage_item_slots(v->selected_code));
            mercs_art_text(a,10,256,339,text,22);}
    }else if(v->discards&&v->tab<5){
        mercs_art_text(a,10,256,86,"Discards               Qty",17);
        for(j=0;j<v->middle_count;j++){
            const msv_group *g=&v->middle_rows[j];int yy=110+12*j;
            mercs_salvage_row_name(g->kind,g->id,name,sizeof name);
            mercs_art_text(a,10,256,yy,name,j==v->middle?17:22);
            snprintf(text,sizeof text,g->damaged?"%d /":"%d",g->intact);msv_number_right(a,395,yy,text,22);
            if(g->damaged){snprintf(text,sizeof text,"%d",g->damaged);mercs_art_text(a,10,396,yy,text,143);}
        }
    }
    /* right: the dropship */
    if(v->tab<5){
        int used=mercs_salvage_hold_used(y);
        mercs_art_text(a,10,459,86,"Dropship            Qty  Slots",17);
        for(j=0;j<v->right_count;j++){
            const msv_group *g=&v->right_rows[j];int yy=110+12*j,total=g->intact+g->damaged,slots;
            mercs_salvage_row_name(g->kind,g->id,name,sizeof name);
            mercs_art_text(a,10,459,yy,name,j==v->right?17:22);
            snprintf(text,sizeof text,g->damaged?"%d /":"%d",g->intact);msv_number_right(a,565,yy,text,22);
            if(g->damaged){snprintf(text,sizeof text,"%d",g->damaged);mercs_art_text(a,10,566,yy,text,143);}
            slots=g->kind==0?total*mercs_salvage_item_slots(g->id*100):total;
            snprintf(text,sizeof text,"%d",slots);mercs_art_text(a,10,586,yy,text,22);
        }
        snprintf(text,sizeof text,"Current Load      : %d",used);mercs_art_text(a,10,459,339,text,22);
        snprintf(text,sizeof text,"Remaining Space   : %d",MERCS_SALVAGE_HOLD_SLOTS-used);mercs_art_text(a,10,459,357,text,22);
    }else{
        size_t pi;uint32_t pid=0,assigned=UINT32_MAX;int n=(int)mercs_save_list_count(y->document,MERCS_SAVE_OWNED_MECHS);
        const uint8_t *h=mercs_save_header(y->document);
        if(!mercs_save_player(y->document,&pi,&pid,NULL,0)){
            const uint8_t *p=mercs_save_list_record(y->document,MERCS_SAVE_OWNED_PILOTS,pi);
            if(p)assigned=(uint32_t)p[0x20]|(uint32_t)p[0x21]<<8|(uint32_t)p[0x22]<<16|(uint32_t)p[0x23]<<24;
        }
        mercs_art_text(a,10,459,86,"Dropship [Chassis]         Price",17);
        for(j=0;j<v->right_count;j++){
            mercs_career_mech m;int32_t value=0;int yy=110+12*j;uint32_t mid;
            memcpy(m.bytes,mercs_save_list_record(y->document,MERCS_SAVE_OWNED_MECHS,(size_t)j),sizeof m.bytes);
            mid=(uint32_t)m.bytes[0x810]|(uint32_t)m.bytes[0x811]<<8|(uint32_t)m.bytes[0x812]<<16|(uint32_t)m.bytes[0x813]<<24;
            snprintf(text,sizeof text,mid==assigned?"%.40s [Plyr]":"%.40s",(const char *)m.bytes);
            mercs_art_text(a,10,459,yy,text,j==v->right?17:22);
            if(h&&!mercs_debrief_mech_value(&m,(int32_t)((uint32_t)h[0x122]|(uint32_t)h[0x123]<<8|(uint32_t)h[0x124]<<16|(uint32_t)h[0x125]<<24),&value)){
                msv_money(value,text,sizeof text);mercs_art_text(a,10,563,yy,text,22);
            }
        }
        snprintf(text,sizeof text,"Current Load      : %d",n);mercs_art_text(a,10,459,339,text,22);
        snprintf(text,sizeof text,"Remaining Space   : %d",MERCS_SALVAGE_MECH_LIMIT-n);mercs_art_text(a,10,459,357,text,22);
    }
}

/* One click. Returns MSV_EXIT (the Exit button), MSV_NOTICE with *notice set,
 * or MSV_NONE. Yard operations happen here. */
static inline int mercs_salvage_view_click(mercs_salvage_view *v,mercs_salvage_yard *y,prj_archive *archive,
                                           int x,int y_,const char **notice)
{
    int i,rc;
    mercs_salvage_entry *e=y->count&&v->wreck<(int)y->count?&y->entries[v->wreck]:NULL;
    *notice=NULL;
    if(v->list_open){
        size_t k;
        for(k=0;k<y->count;k++)if(msv_in(x,y_,40,125+12*(int)k,140,136+12*(int)k)){
            v->discards=0;mercs_salvage_view_select_wreck(v,y,(int)k);v->list_open=0;
            mercs_salvage_view_refresh(v,y);return MSV_NONE;
        }
        if(msv_in(x,y_,40,140+12*(int)y->count,140,151+12*(int)y->count)){
            v->discards=1;v->list_open=0;v->middle=-1;mercs_salvage_view_refresh(v,y);return MSV_NONE;
        }
        if(msv_in(x,y_,18,63,233,89)){v->list_open=0;return MSV_NONE;}
        return MSV_NONE;
    }
    if(msv_in(x,y_,18,63,233,89)){v->list_open=1;return MSV_NONE;}
    if(msv_in(x,y_,502,432,604,473))return MSV_EXIT;
    for(i=0;i<6;i++){
        static const int tx0[6]={8,101,191,281,373,462},tx1[6]={100,190,280,369,460,520};
        if(msv_in(x,y_,tx0[i],8,tx1[i],44)){v->tab=i;v->right=v->middle=-1;mercs_salvage_view_refresh(v,y);return MSV_NONE;}
    }
    if(!v->discards&&e)for(i=0;i<8;i++){
        const int *b=msv_loc_box[i];int loc=msv_loc_of[i];
        if(msv_in(x,y_,b[0],b[1],b[2],b[3])&&mercs_salvage_condition(e->current.bytes,loc)!=3){
            v->location=loc;v->selected_code=0;v->selected_slot=-1;return MSV_NONE;
        }
    }
    /* the location list: select an item, its tab follows (0x100626bc) */
    if(!v->discards&&e&&msv_in(x,y_,256,113,376,113+12*msv_slots(v->location)-1)){
        int slot=(y_-113)/12;
        int32_t id=(int32_t)((uint32_t)e->listing.bytes[0x368+48*v->location+4*slot]|
                             (uint32_t)e->listing.bytes[0x369+48*v->location+4*slot]<<8|
                             (uint32_t)e->listing.bytes[0x36a+48*v->location+4*slot]<<16|
                             (uint32_t)e->listing.bytes[0x36b+48*v->location+4*slot]<<24);
        int c=mercs_salvage_category(id);
        if(id>0&&c>=0&&mercs_salvage_condition(e->current.bytes,v->location)!=3){
            if(v->selected_code==id){v->selected_code=0;v->selected_slot=-1;}
            else{v->selected_code=id;v->selected_slot=slot;v->tab=c;v->right=-1;mercs_salvage_view_refresh(v,y);}
        }
        return MSV_NONE;
    }
    if(v->discards&&v->tab<5&&msv_in(x,y_,256,110,376,110+12*v->middle_count-1)){
        int row=(y_-110)/12;v->middle=v->middle==row?-1:row;return MSV_NONE;
    }
    if(msv_in(x,y_,459,110,579,110+12*v->right_count-1)){
        int row=(y_-110)/12;v->right=v->right==row?-1:row;return MSV_NONE;
    }
    /* Take (0x100652b8 from the wreck, 0x1006565a from the discards) */
    if(msv_in(x,y_,281,378,386,418)){
        if(!v->discards&&e&&v->selected_slot>=0){
            rc=mercs_salvage_take_component(y,(size_t)v->wreck,v->location,v->selected_slot);
            if(rc==MERCS_SALVAGE_FULL)*notice="Cannot store any more items on dropship!|#Ok";
            else if(rc==MERCS_SALVAGE_REFUSED)*notice="Selected critical cannot be moved|into dropship inventory.#Ok";
            if(!rc){v->selected_code=0;v->selected_slot=-1;}
        }else if(v->discards&&v->tab<5&&v->middle>=0){
            const msv_group *g=&v->middle_rows[v->middle];
            if(mercs_salvage_hold_used(y)+(g->kind==0?mercs_salvage_item_slots(g->id*100):1)>MERCS_SALVAGE_HOLD_SLOTS)
                *notice="Cannot store any more items on dropship!|#Ok";
            else mercs_salvage_take_discard(y,g->kind,g->id);
        }
        mercs_salvage_view_refresh(v,y);return *notice?MSV_NOTICE:MSV_NONE;
    }
    /* Leave (0x1006581f to the discards; the Chassis page 0x100659c6) */
    if(msv_in(x,y_,485,378,590,418)){
        if(v->tab==5){
            if(v->right>=0){
                rc=mercs_salvage_discard_owned(y,(size_t)v->right);
                if(rc==MERCS_SALVAGE_LAST_MECH)*notice="Can't discard last remaining mech!!|#Ok";
                else if(!rc){v->discards=0;mercs_salvage_view_select_wreck(v,y,(int)y->count-1);v->right=-1;}
            }
        }else if(v->right>=0){
            const msv_group *g=&v->right_rows[v->right];
            if(!mercs_salvage_leave_item(y,g->kind,g->id)){v->discards=1;v->middle=-1;}
        }
        mercs_salvage_view_refresh(v,y);return *notice?MSV_NOTICE:MSV_NONE;
    }
    /* Recover the chassis (0x10065c2e) */
    if(!v->discards&&e&&e->whole&&msv_in(x,y_,25,409,170,471)){
        rc=mercs_salvage_recover(y,archive,(size_t)v->wreck);
        if(rc==MERCS_SALVAGE_MECHS_FULL)*notice="Cannot store any more 'mechs!|#Ok";
        else if(!rc){
            int next=v->wreck>0?v->wreck-1:0;
            if(y->count)mercs_salvage_view_select_wreck(v,y,next);else v->discards=1;
            v->tab=5;v->right=-1;
        }
        mercs_salvage_view_refresh(v,y);return *notice?MSV_NOTICE:MSV_NONE;
    }
    return MSV_NONE;
}

#endif
