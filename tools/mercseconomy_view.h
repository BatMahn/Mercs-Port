#ifndef MERCS_ECONOMY_VIEW_H
#define MERCS_ECONOMY_VIEW_H

/* Original DOS finance0x422e0 and personnel0x453ec presentation. Assets,
 * positions, original font10/6, remaps, biography area and input rectangles
 * come from those constructors and their original list builders. This view
 * never changes a save; the shell stages, persists, then rehydrates actions. */
#include "mercsart.h"
#include "mercstext.h"
#include "mercseconomy.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline uint32_t mev_u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static inline int32_t mev_i32(const uint8_t *p)
{ uint32_t v=mev_u32(p);return v<=INT32_MAX?(int32_t)v:(int32_t)(-1-(int64_t)(UINT32_MAX-v)); }
static inline void mev_money(int32_t value,char out[24])
{
    char raw[16];size_t i,n,first,at=0;
    snprintf(raw,sizeof raw,"%ld",(long)value);n=strlen(raw);first=raw[0]=='-'?1u:0u;
    for(i=0;i<n;i++){if(i>first&&(n-i)%3==0)out[at++]=',';out[at++]=raw[i];}out[at]=0;
}
static inline int mev_inside(int x,int y,int x0,int y0,int x1,int y1)
{ return x>=x0&&x<=x1&&y>=y0&&y<=y1; }
static inline void mev_funds(mercs_art *a,const mercs_save_state *s)
{
    char text[24];const uint8_t *h=mercs_save_header(s);
    if(!h||mev_u32(h+0x23bb)!=1)return;
    mev_money(mev_i32(h+0x12a),text);mercs_art_text(a,6,490,15,"Funds:",17);
    mercs_art_text(a,6,620-mercs_art_width(a,6,text),15,text,17);
}

typedef struct mercs_finance_view { mercs_text_layout text;int dropship,scroll; } mercs_finance_view;
typedef struct mev_text { char bytes[8192];size_t at;int failed; } mev_text;
static inline void mev_append(mev_text *b,const char *format,...)
{
    va_list ap;int n;if(b->failed)return;
    va_start(ap,format);n=vsnprintf(b->bytes+b->at,sizeof b->bytes-b->at,format,ap);va_end(ap);
    if(n<0||(size_t)n>=sizeof b->bytes-b->at){b->failed=1;return;}b->at+=(size_t)n;
}
static inline void mev_finance_line(mev_text *b,mercs_art *a,const char *label,int32_t value,int font)
{
    char amount[24];mev_money(value,amount);
    mev_append(b,"%s\\g280\\b%03d%s\\n",label,mercs_art_width(a,font,amount),amount);
}
static inline void mercs_finance_view_free(mercs_finance_view *v)
{ if(v){mercs_text_free(&v->text);memset(v,0,sizeof *v);} }
static inline int mercs_finance_view_prepare(mercs_finance_view *v,mercs_art *a,
                                             const mercs_save_state *s,int dropship)
{
    mercs_finance f;mev_text b={{0},0,0};size_t i;char name[33],label[80];
    mercs_finance_view next;
    if(!v||!a||mercs_economy_finance(s,&f))return -1;
    memset(&next,0,sizeof next);next.dropship=!!dropship;
    mev_append(&b,"\\lExpenses: \\g280\\b%03dC-Bills\\n\\nUnit Upkeep:\\n",mercs_art_width(a,6,"C-Bills"));
    mev_finance_line(&b,a,"\\tBattleMech Maintenance:",f.maintenance,10);
    mev_finance_line(&b,a,"\\tTechnician Salaries:",f.technicians_display,10);
    mev_finance_line(&b,a,"\\tTravel Costs:",f.travel,10);
    mev_finance_line(&b,a,"\\tBroker Fees:",f.broker,10);
    mev_finance_line(&b,a,"\\tGuild Dues:",f.guild,10);
    if(mercs_save_list_count(s,MERCS_SAVE_OWNED_PILOTS)>1)mev_append(&b,"\\nPilot Salaries:\\n");
    for(i=0;i<mercs_save_list_count(s,MERCS_SAVE_OWNED_PILOTS);i++){
        const uint8_t *p=mercs_save_list_record(s,MERCS_SAVE_OWNED_PILOTS,i);
        if(p[0x2064])continue;
        memcpy(name,p,32);name[32]=0;snprintf(label,sizeof label,"\\t%s",name);
        mev_finance_line(&b,a,label,mev_i32(p+0x3c),10);
    }
    for(i=0;i<mercs_save_list_count(s,MERCS_SAVE_OWNED_TECHS);i++){
        const uint8_t *p=mercs_save_list_record(s,MERCS_SAVE_OWNED_TECHS,i);
        memcpy(name,p,32);name[32]=0;snprintf(label,sizeof label,"\\tAeroTech: %s",name);
        mev_finance_line(&b,a,label,mev_i32(p+0x24),10);
    }
    mev_finance_line(&b,a,"\\nTotal Monthly Costs: ",f.monthly_total,10);mev_append(&b,"\\n");
    mev_finance_line(&b,a,"\\lTotal Funds: ",f.funds,6);
    if(b.failed||mercs_text_prepare(&next.text,a,b.bytes,"",10,407)){mercs_finance_view_free(&next);return -1;}
    mercs_finance_view_free(v);*v=next;return 0;
}
static inline void mercs_finance_view_draw(mercs_finance_view *v,mercs_art *a)
{
    if(!v||!a)return;
    mercs_art_background(a,v->dropship?45:29);
    mercs_art_button(a,v->dropship?"occfe":"ocbfe",510,440,0);
    mercs_text_draw(&v->text,a,180,109,291,v->scroll,17);
}
static inline int mercs_finance_view_exit(int x,int y)
{ return mev_inside(x,y,500,432,602,472); }
static inline void mercs_finance_view_scroll(mercs_finance_view *v,int pixels)
{
    int max;if(!v)return;max=v->text.height-291;if(max<0)max=0;
    v->scroll+=pixels;if(v->scroll<0)v->scroll=0;if(v->scroll>max)v->scroll=max;
}

enum { MERCS_PERSONNEL_VIEW_NONE,MERCS_PERSONNEL_VIEW_EXIT,MERCS_PERSONNEL_VIEW_SELECT,
       MERCS_PERSONNEL_VIEW_HIRE,MERCS_PERSONNEL_VIEW_DISMISS };
typedef struct mercs_personnel_view {
    int list,scroll;size_t index;mercs_text_layout biography;
} mercs_personnel_view;
static inline enum mercs_save_list mev_personnel_list(int list)
{
    static const enum mercs_save_list lists[4]={MERCS_SAVE_OWNED_PILOTS,MERCS_SAVE_SHOP_PILOTS,
        MERCS_SAVE_OWNED_TECHS,MERCS_SAVE_SHOP_TECHS};return lists[list];
}
static inline void mercs_personnel_view_free(mercs_personnel_view *v)
{ if(v){mercs_text_free(&v->biography);memset(v,0,sizeof *v);v->list=-1;} }
static inline void mercs_personnel_view_init(mercs_personnel_view *v)
{ memset(v,0,sizeof *v);v->list=-1; }
static inline int mercs_personnel_view_select(mercs_personnel_view *v,mercs_art *a,
                                              const mercs_save_state *s,int list,size_t index)
{
    const uint8_t *p;size_t offset,n;char *text;mercs_text_layout bio={0};
    if(!v||!a||list<0||list>3||!(p=mercs_save_list_record(s,mev_personnel_list(list),index)))return -1;
    if(list<2&&p[0x2064])return -1;
    offset=list<2?0x64:0x5e;
    for(n=0;n<0x2000&&p[offset+n];n++){}
    text=malloc(n+1);if(!text)return -1;memcpy(text,p+offset,n);text[n]=0;
    if(mercs_text_prepare(&bio,a,text,"",10,144)){free(text);return -1;}free(text);
    mercs_text_free(&v->biography);v->biography=bio;v->list=list;v->index=index;v->scroll=0;return 0;
}
static inline void mercs_personnel_view_draw(mercs_personnel_view *v,mercs_art *a,const mercs_save_state *s)
{
    static const char *const headings[4]={"Current Mechwarriors: ","Mechwarriors for hire: ",
        "Current AeroTech Pilots: ","AeroTech Pilots for hire: "};
    int list,h=mercs_art_height(a,10);char name[33],salary[24];size_t i,row;
    mercs_art_background(a,31);mev_funds(a,s);
    mercs_art_button(a,"ocbhh",488,383,0);mercs_art_button(a,"ocbhf",270,383,0);
    mercs_art_button(a,"ocbhe",510,440,0);
    for(list=0;list<4;list++){
        int x=list&1?445:230,y=list<2?95:195,center=list&1?518:302;
        mercs_art_center(a,10,center,y,headings[list],22);row=0;
        for(i=0;i<mercs_save_list_count(s,mev_personnel_list(list));i++){
            const uint8_t *p=mercs_save_list_record(s,mev_personnel_list(list),i);int ink,ry;
            if(list<2&&p[0x2064])continue;
            ry=y+(int)(row+2)*h;row++;
            memcpy(name,p,32);name[32]=0;mev_money(mev_i32(p+(list<2?0x3c:0x24)),salary);
            ink=v->list==list&&v->index==i?17:22;
            mercs_art_text(a,10,x,ry,name,ink);
            mercs_art_text(a,10,x+150-mercs_art_width(a,10,salary),ry,salary,ink);
        }
    }
    mercs_text_draw(&v->biography,a,35,96,259,v->scroll,22);
}
/* SELECT returns the list/index via output arguments; the shell then calls
 * select() and plays the original click. Double click performs the selected
 * row's Hire/Dismiss action in the original callback. */
static inline int mercs_personnel_view_hit(const mercs_personnel_view *v,mercs_art *a,
                                           const mercs_save_state *s,int x,int y,int *list_out,size_t *index_out)
{
    int list,h=mercs_art_height(a,10);size_t i,row;
    if(mev_inside(x,y,500,431,603,473))return MERCS_PERSONNEL_VIEW_EXIT;
    if(mev_inside(x,y,472,380,589,421))return v->list>=0&&(v->list&1)?MERCS_PERSONNEL_VIEW_HIRE:0;
    if(mev_inside(x,y,257,379,370,420))return v->list>=0&&!(v->list&1)?MERCS_PERSONNEL_VIEW_DISMISS:0;
    if(h<=0)return 0;
    for(list=0;list<4;list++){
        int xx=list&1?445:230,yy=list<2?95:195;row=0;
        for(i=0;i<mercs_save_list_count(s,mev_personnel_list(list));i++){
            const uint8_t *p=mercs_save_list_record(s,mev_personnel_list(list),i);int top;
            if(list<2&&p[0x2064])continue;
            top=yy+(int)(row+2)*h;row++;
            if(mev_inside(x,y,xx,top,xx+149,top+h-1)){
                if(list_out)*list_out=list;
                if(index_out)*index_out=i;
                return MERCS_PERSONNEL_VIEW_SELECT;
            }
        }
    }
    return 0;
}
static inline void mercs_personnel_view_scroll(mercs_personnel_view *v,int pixels)
{
    int max;if(!v)return;max=v->biography.height-259;if(max<0)max=0;
    v->scroll+=pixels;if(v->scroll<0)v->scroll=0;if(v->scroll>max)v->scroll=max;
}

#endif
