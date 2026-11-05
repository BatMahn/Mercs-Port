#ifndef MERCS_INVENTORY_VIEW_H
#define MERCS_INVENTORY_VIEW_H

/* Original computer/DropShip inventory, DOS0x4938c. This is a read-only
 * screen; equipment buying/selling belongs to the original Mech Bay shop.
 * Original list builders0x4871c/0x48f7c and preview0x48638 supply its
 * coordinates, columns, names, original movies and all seven hitboxes. */
#include "mercstrading_view.h"
#include "mercsdebrief.h"
#include "mercsshell_chassis.h"
#include <ctype.h>

enum { MERCS_INVENTORY_NONE,MERCS_INVENTORY_EXIT,MERCS_INVENTORY_CATEGORY,
       MERCS_INVENTORY_MECH };
typedef struct mercs_inventory_view {
    int dropship,category,scroll,restart;
    size_t selected,count;
    mercs_item_group *groups;
    int32_t *values;
} mercs_inventory_view;

static inline const char *miv_mech_name(const uint8_t *record,char name[257])
{
    size_t i;
    if(record[0x100]){memcpy(name,record+0x100,256);name[256]=0;return name;}
    for(i=0;i<sizeof mercs_shell_chassis/sizeof mercs_shell_chassis[0];i++)
        if(toupper(record[0x823])==mercs_shell_chassis[i].prefix[0]&&
           toupper(record[0x824])==mercs_shell_chassis[i].prefix[1]&&
           toupper(record[0x825])==mercs_shell_chassis[i].prefix[2])return mercs_shell_chassis[i].name;
    memcpy(name,record+0x823,MERCS_CAREER_FILENAME_BYTES);name[MERCS_CAREER_FILENAME_BYTES]=0;return name;
}
static inline void mercs_inventory_view_init(mercs_inventory_view *v)
{ memset(v,0,sizeof *v);v->restart=1; }
static inline void mercs_inventory_view_free(mercs_inventory_view *v)
{ if(v){free(v->groups);free(v->values);mercs_inventory_view_init(v);} }
static inline int mercs_inventory_view_prepare(mercs_inventory_view *v,const mercs_save_state *s,
                                               int dropship,int category)
{
    mercs_item_group *groups=NULL;int32_t *values=NULL;size_t count=0,i;int same;
    if(!v||!s||category<0||category>5)return -1;
    same=category==v->category;
    if(category){
        if(mercs_economy_item_groups(s,0,(enum mercs_item_category)(category-1),&groups,&count))return -1;
    }else{
        count=mercs_save_list_count(s,MERCS_SAVE_OWNED_MECHS);
        if(count&&!(values=calloc(count,sizeof *values)))return -1;
        for(i=0;i<count;i++){
            mercs_career_mech m;memcpy(m.bytes,mercs_save_list_record(s,MERCS_SAVE_OWNED_MECHS,i),sizeof m.bytes);
            if(mercs_debrief_mech_value(&m,mev_i32(mercs_save_header(s)+0x122),&values[i])){free(values);return -1;}
        }
    }
    free(v->groups);free(v->values);v->groups=groups;v->values=values;v->count=count;
    v->dropship=!!dropship;v->category=category;
    if(!same){v->scroll=0;v->selected=0;v->restart=1;}
    if(v->selected>=count)v->selected=0;
    return 0;
}
static inline int mercs_inventory_view_select(mercs_inventory_view *v,size_t row)
{
    if(!v||v->category||row>=v->count)return -1;
    v->selected=row;v->restart=1;return 0;
}
static inline void mercs_inventory_view_draw(mercs_inventory_view *v,mercs_art *a,const mercs_save_state *s)
{
    static const char *const buttons[2][7]={
        {"ocbime","ocbim","ocbien","ocbip","ocbieq","ocbia","ocbie"},
        {"inmech","inmiss","inenrg","inproj","inequp","inammo","inexit"}};
    static const int x[6]={66,156,245,334,424,514};
    static const char *const movie[2][5]={{"amissle1","alaser1","aproj1","aequipt1","aammo1"},
        {"amissle2","alaser2","aproj2","aequipt2","aammo2"}};
    int i,h=mercs_art_height(a,10),cat=v->category;size_t r;
    mercs_art_background(a,v->dropship?44:30);mev_funds(a,s);
    for(i=0;i<6;i++)mercs_art_button(a,buttons[v->dropship][i],x[i],46,i==cat);
    mercs_art_button(a,buttons[v->dropship][6],510,440,0);
    if(h<=0)return;
    if(cat){
        const char *qty=cat==5?"Tons":"Qty",*price=cat==5?"Price/ton":"Price/unit";
        mercs_art_clip_flags(a,movie[v->dropship][cat-1],418,122,0x28,(unsigned)v->restart);v->restart=0;
        if(!v->count){mercs_art_text(a,10,82,90+2*h,"None",22);return;}
        mercs_art_text(a,10,82,90+2*h,cat==5?"Ammo":cat==4?"Equipment":"Weapon",17);
        mercs_art_text(a,10,182-mercs_art_width(a,10,qty),90+2*h,qty,17);
        mercs_art_text(a,10,242-mercs_art_width(a,10,price),90+2*h,price,17);
        for(r=(size_t)v->scroll;r<v->count;r++){
            char name[64],qtytext[24],amount[24];int y=90+5*h+(int)(r-(size_t)v->scroll)*h;
            if(y+h>419)break;
            snprintf(qtytext,sizeof qtytext,"%lu",(unsigned long)v->groups[r].count);mev_money(v->groups[r].price,amount);
            mercs_art_text(a,10,82,y,mtv_item_name(&v->groups[r],name),22);
            mercs_art_text(a,10,182-mercs_art_width(a,10,qtytext),y,qtytext,22);
            mercs_art_text(a,10,242-mercs_art_width(a,10,amount),y,amount,22);
        }
    }else{
        mercs_art_text(a,10,82,115,"Mech",17);
        mercs_art_text(a,10,282-mercs_art_width(a,10,"Value"),115,"Value",17);
        if(v->count){
            const uint8_t *m=mercs_save_list_record(s,MERCS_SAVE_OWNED_MECHS,v->selected);char prefix[8],clip[16];
            memcpy(prefix,m+0x81c,7);prefix[7]=0;snprintf(clip,sizeof clip,"a%s",prefix);
            mercs_art_clip_flags(a,clip,418,122,0x28,(unsigned)v->restart);v->restart=0;
        }
        for(r=(size_t)v->scroll;r<v->count;r++){
            char name[257],amount[24];int y=115+(2+(int)(r-(size_t)v->scroll))*h,ink=r==v->selected?17:22;
            const uint8_t *m=mercs_save_list_record(s,MERCS_SAVE_OWNED_MECHS,r);
            if(y+h>419)break;
            mev_money(v->values[r],amount);mercs_art_text(a,10,82,y,miv_mech_name(m,name),ink);
            mercs_art_text(a,10,282-mercs_art_width(a,10,amount),y,amount,ink);
        }
    }
}
static inline int mercs_inventory_view_hit(const mercs_inventory_view *v,mercs_art *a,
                                           int x,int y,int *category,size_t *row)
{
    static const int bounds[6][2]={{60,152},{153,241},{242,331},{332,421},{422,511},{512,602}};
    int i,h=mercs_art_height(a,10);size_t at;
    if(mev_inside(x,y,500,433,602,473))return MERCS_INVENTORY_EXIT;
    for(i=0;i<6;i++)if(mev_inside(x,y,bounds[i][0],41,bounds[i][1],75)){
        if(category)*category=i;
        return MERCS_INVENTORY_CATEGORY;
    }
    if(v->category||h<=0||!mev_inside(x,y,82,115+2*h,281,418))return 0;
    at=(size_t)((y-(115+2*h))/h+v->scroll);if(at>=v->count)return 0;
    if(row)*row=at;
    return MERCS_INVENTORY_MECH;
}
static inline void mercs_inventory_view_scroll(mercs_inventory_view *v,mercs_art *a,int rows)
{
    int h,max,visible;if(!v)return;h=mercs_art_height(a,10);if(h<=0)return;
    visible=(419-(v->category?90+5*h:115+2*h))/h;max=(int)v->count-visible;if(max<0)max=0;
    v->scroll+=rows;if(v->scroll<0)v->scroll=0;if(v->scroll>max)v->scroll=max;
}

#endif
