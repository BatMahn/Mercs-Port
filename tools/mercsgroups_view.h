#ifndef MERCS_GROUPS_VIEW_H
#define MERCS_GROUPS_VIEW_H

/* Retail DOS Weapon Groups0x43f58/0x443b4. Background42, bw* button
 * art, original font callbacks and animated mech preview. The editor shows
 * every installed weapon separately and cycles only its group nibble. */
#include "mercsinventory_view.h"
enum { MERCS_GROUPS_NONE,MERCS_GROUPS_EXIT,MERCS_GROUPS_PREVIOUS,
       MERCS_GROUPS_NEXT,MERCS_GROUPS_DEFAULT,MERCS_GROUPS_WEAPON };
typedef struct mercs_groups_view { size_t selected; int restart; } mercs_groups_view;
static inline void mercs_groups_view_prepare(mercs_groups_view *v,const mercs_save_state *s,int fresh)
{
    if(fresh||v->selected>=mercs_save_list_count(s,MERCS_SAVE_OWNED_MECHS))v->selected=0;
    v->restart=1;
}
static inline int mgv_top(const mercs_save_state *s,int height)
{ return 89+(mercs_save_list_count(s,MERCS_SAVE_OWNED_WEAPONS)?3:1)*height; }
static inline void mercs_groups_view_step(mercs_groups_view *v,const mercs_save_state *s,int direction)
{
    size_t n=mercs_save_list_count(s,MERCS_SAVE_OWNED_MECHS);if(n<2)return;
    v->selected=direction>0?(v->selected+1)%n:v->selected?v->selected-1:n-1;v->restart=1;
}
static inline void mercs_groups_view_draw(mercs_groups_view *v,mercs_art *a,const mercs_save_state *s,int x,int y)
{
    const uint8_t *m=mercs_save_list_record(s,MERCS_SAVE_OWNED_MECHS,v->selected);
    int i,yy,h=mercs_art_height(a,10);char name[257];const char *title;
    mercs_art_background(a,41);
    mercs_art_button(a,"bwexit",510,440,mev_inside(x,y,500,430,602,474));
    mercs_art_button(a,"bwdefl",390,440,mev_inside(x,y,380,430,483,474));
    mercs_art_button(a,"bwprev",71,355,mev_inside(x,y,60,347,152,395));
    mercs_art_button(a,"bwnext",159,356,mev_inside(x,y,153,347,247,395));
    if(!m)return;
    title=miv_mech_name(m,name);
    if(mercs_art_width(a,5,title)>186){const char *space=strchr(title,' ');if(space&&space[1])title=space+1;}
    mercs_art_center(a,5,151,86,title,17);
    {char clip[16];snprintf(clip,sizeof clip,"a%.7s",(const char*)m+0x81c);
     mercs_art_clip_flags(a,clip,71,170,0x28,(unsigned)v->restart);v->restart=0;}
    if(mercs_save_list_count(s,MERCS_SAVE_OWNED_WEAPONS)){
        mercs_art_text(a,10,370,89,"Weapon:",17);
        mercs_art_text(a,10,557-mercs_art_width(a,10,"Group"),89,"Group",17);
    }else mercs_art_text(a,10,370,89,"No weapons",17);
    yy=mgv_top(s,h);
    for(i=0;i<14;i++){
        int id=mev_i32(m+0x2f0+8*i),group=(mev_u32(m+0x2f4+8*i)>>8)&7,ink;
        char label[80],number[8];if(id<0)continue;
        if(group>4)group=0;
        if(id/100>=mek_weapon_count)snprintf(label,sizeof label,"%d:",id/100);
        else snprintf(label,sizeof label,"%s:",mek_weapons[id/100].name);
        snprintf(number,sizeof number,"%d",group);ink=mev_inside(x,y,370,yy,570,yy+h)?17:22;
        mercs_art_text(a,10,370,yy,label,ink);
        mercs_art_text(a,10,557-mercs_art_width(a,10,number),yy,number,ink);yy+=h;
    }
}
static inline int mercs_groups_view_hit(const mercs_groups_view *v,mercs_art *a,const mercs_save_state *s,int x,int y,int *slot)
{
    const uint8_t *m=mercs_save_list_record(s,MERCS_SAVE_OWNED_MECHS,v->selected);
    int i,yy,h=mercs_art_height(a,10);
    if(mev_inside(x,y,500,430,602,474))return MERCS_GROUPS_EXIT;
    if(mev_inside(x,y,60,347,152,395))return MERCS_GROUPS_PREVIOUS;
    if(mev_inside(x,y,153,347,247,395))return MERCS_GROUPS_NEXT;
    if(mev_inside(x,y,380,430,483,474))return MERCS_GROUPS_DEFAULT;
    if(!m)return MERCS_GROUPS_NONE;
    yy=mgv_top(s,h);
    for(i=0;i<14;i++)if(mev_i32(m+0x2f0+8*i)>=0){
        if(mev_inside(x,y,370,yy,570,yy+h)){*slot=i;return MERCS_GROUPS_WEAPON;}yy+=h;
    }
    return MERCS_GROUPS_NONE;
}
#endif
