#ifndef MERCS_TRADING_VIEW_H
#define MERCS_TRADING_VIEW_H

/* Retail equipment shop: DOS constructor0x285f4, category0x5aed8,
 * list builder0x27ab8, callbacks0x275e4/0x276d4/0x277b4. Original
 * DATABASE18, category movies, authored buttons, font10 and hitboxes.
 * Transactions are deliberately outside this read-only presentation. */
#include "mercseconomy_view.h"
#include "mek.h"

enum { MERCS_TRADING_NONE,MERCS_TRADING_EXIT,MERCS_TRADING_CATEGORY,
       MERCS_TRADING_SELECT,MERCS_TRADING_BUY,MERCS_TRADING_SELL };
typedef struct mercs_trading_view {
    int category,side,restart,scroll[2];
    size_t selected,count[2];
    mercs_item_group *groups[2]; /* 0 owned/right; 1 shop/left */
} mercs_trading_view;

static inline const char *mtv_item_name(const mercs_item_group *row,char buffer[64])
{
    static const char *const equipment[12]={"Engine:","Single heat sink:","Double heat sink:",
        "Heatsink upgrade:","Heatsink dngrade:","CASE:","Jump Jets:",
        "Ferro-fibrous armor upgrade:","Endo-steel internals upgrade:",
        "Armor Allocation:","MASC:","Max equipment:"};
    if(row->type==2){
        if(row->id>=0&&row->id<12)return equipment[row->id];
        snprintf(buffer,64,"BAD EQUIP!! %ld",(long)row->id);return buffer;
    }
    if(row->id>=0&&row->id<MEK_WEAPON_COUNT)return mek_weapons[row->id].name;
    snprintf(buffer,64,"BAD WEAPON!! %ld",(long)row->id);return buffer;
}
static inline void mercs_trading_view_init(mercs_trading_view *v)
{ memset(v,0,sizeof *v);v->side=-1;v->restart=1; }
static inline void mercs_trading_view_free(mercs_trading_view *v)
{ if(v){free(v->groups[0]);free(v->groups[1]);mercs_trading_view_init(v);} }
static inline int mercs_trading_view_prepare(mercs_trading_view *v,const mercs_save_state *s,int category)
{
    mercs_item_group *p[2]={NULL,NULL};size_t n[2]={0,0},i;
    int side,same,oldside=-1;int32_t oldid=0;
    if(!v||!s||category<0||category>MERCS_ITEM_AMMO)return -1;
    same=v->category==category;
    if(same&&v->side>=0&&v->side<=1&&v->selected<v->count[v->side]){
        oldside=v->side;oldid=v->groups[oldside][v->selected].id;
    }
    for(side=0;side<2;side++)if(mercs_economy_item_groups(s,side,(enum mercs_item_category)category,&p[side],&n[side])){
        free(p[0]);free(p[1]);return -1;
    }
    free(v->groups[0]);free(v->groups[1]);
    v->category=category;v->side=-1;v->selected=0;
    if(!same){v->scroll[0]=v->scroll[1]=0;v->restart=1;}
    for(side=0;side<2;side++){
        v->groups[side]=p[side];v->count[side]=n[side];
        if(v->scroll[side]>(int)n[side]-1)v->scroll[side]=n[side]?(int)n[side]-1:0;
    }
    if(oldside>=0)for(i=0;i<n[oldside];i++)if(p[oldside][i].id==oldid){v->side=oldside;v->selected=i;break;}
    return 0;
}
static inline int mercs_trading_view_select(mercs_trading_view *v,int side,size_t group)
{
    if(!v||side<0||side>1||group>=v->count[side])return -1;
    v->side=side;v->selected=group;return 0;
}
/* Obtain the original save-list row that is actually moved by the selected
 * buy/sell action. Rebuild this view after every committed transaction. */
static inline int mercs_trading_view_transaction(const mercs_trading_view *v,int buy,int *ammo,size_t *index)
{
    if(!v||(buy!=0&&buy!=1)||v->side!=buy||v->selected>=v->count[buy])return -1;
    if(ammo)*ammo=v->category==MERCS_ITEM_AMMO;
    if(index)*index=v->groups[buy][v->selected].first_index;
    return 0;
}
static inline void mercs_trading_view_draw(mercs_trading_view *v,mercs_art *a,const mercs_save_state *s)
{
    static const char *const movies[5]={"amissle","alaser","aproj","aequipt","aammo"};
    static const char *const buttons[5]={"bamiss","baenrg","baproj","baequp","baammo"};
    static const int buttonx[5]={15,105,194,284,373};
    static const char *const titles[2][5]={
        {"Missiles in Inventory","Energy Weapons in Inventory","Projectile Weapons in Inventory","Equipment in Inventory","Ammo in Inventory"},
        {"Missiles for Sale","Energy Weapons for Sale","Projectiles Weapons for Sale","Equipment for Sale","Ammo for Sale"}};
    int side,cat=v->category,h=mercs_art_height(a,10),i;size_t r;
    mercs_art_background(a,18);
    mercs_art_clip_flags(a,movies[cat],40,179,0x68,(unsigned)v->restart);v->restart=0;
    for(i=0;i<5;i++)mercs_art_button(a,buttons[i],buttonx[i],12,i==cat);
    mercs_art_button(a,"baexit",497,440,0);mercs_art_button(a,"babuy",291,382,0);
    mercs_art_button(a,"basell",496,382,0);mev_funds(a,s);
    if(h<=0)return;
    for(side=0;side<2;side++){
        int x=side?252:459,center=side?327:531;const char *qty=cat==4?"Tons":"Qty";
        const char *price=cat==4?"Price/ton":"Price/unit";
        mercs_art_center(a,10,center,90,titles[side][cat],17);
        if(!v->count[side]){mercs_art_text(a,10,x,90+2*h,"None",22);continue;}
        mercs_art_text(a,10,x,90+2*h,cat==4?"Ammo":cat==3?"Equipment":"Weapon",17);
        mercs_art_text(a,10,x+100-mercs_art_width(a,10,qty),90+2*h,qty,17);
        mercs_art_text(a,10,x+160-mercs_art_width(a,10,price),90+2*h,price,17);
        for(r=(size_t)v->scroll[side];r<v->count[side];r++){
            const mercs_item_group *row=&v->groups[side][r];char name[64],count[24],amount[24];
            int y=90+5*h+(int)(r-(size_t)v->scroll[side])*h,ink=v->side==side&&v->selected==r?17:22;
            if(y+h>373)break;
            snprintf(count,sizeof count,"%lu",(unsigned long)row->count);mev_money(row->price,amount);
            mercs_art_text(a,10,x,y,mtv_item_name(row,name),ink);
            mercs_art_text(a,10,x+100-mercs_art_width(a,10,count),y,count,ink);
            mercs_art_text(a,10,x+160-mercs_art_width(a,10,amount),y,amount,ink);
        }
    }
}
static inline int mercs_trading_view_hit(const mercs_trading_view *v,mercs_art *a,int x,int y,
                                         int *value,size_t *group)
{
    static const int bounds[5][2]={{9,102},{103,192},{193,281},{282,370},{371,462}};
    int i,h=mercs_art_height(a,10),side;size_t row;
    if(mev_inside(x,y,502,432,603,473))return MERCS_TRADING_EXIT;
    if(mev_inside(x,y,285,380,382,413))return MERCS_TRADING_BUY;
    if(mev_inside(x,y,487,380,586,413))return MERCS_TRADING_SELL;
    for(i=0;i<5;i++)if(mev_inside(x,y,bounds[i][0],13,bounds[i][1],38)){
        if(value)*value=i;
        return MERCS_TRADING_CATEGORY;
    }
    if(h<=0||y<90+5*h||y>=373)return 0;
    side=x>=459&&x<640?0:x>=252&&x<449?1:-1;if(side<0)return 0;
    row=(size_t)((y-(90+5*h))/h+v->scroll[side]);
    if(row>=v->count[side])return 0;
    if(value)*value=side;
    if(group)*group=row;
    return MERCS_TRADING_SELECT;
}
static inline void mercs_trading_view_scroll(mercs_trading_view *v,mercs_art *a,int side,int rows)
{
    int h,max,visible;if(!v||side<0||side>1)return;h=mercs_art_height(a,10);if(h<=0)return;
    visible=(373-(90+5*h))/h;max=(int)v->count[side]-visible;if(max<0)max=0;
    v->scroll[side]+=rows;if(v->scroll[side]<0)v->scroll[side]=0;
    if(v->scroll[side]>max)v->scroll[side]=max;
}

#endif
