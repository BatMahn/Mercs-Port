#ifndef MERCS_ROSTER_VIEW_H
#define MERCS_ROSTER_VIEW_H

/* Retail DOS Duty Roster: row builder0x3e724, assignment callback0x3e528,
 * preview0x3e3a4 and AeroTech callbacks0x3e160/0x3e1e4. All eight owned
 * pilots remain visible in authored order. Only assignment cells select. */
#include "mercsinventory_view.h"

typedef struct mercs_roster_view { size_t selected; int restart; } mercs_roster_view;
static inline void mercs_roster_view_prepare(mercs_roster_view *v,const mercs_save_state *s,int fresh)
{
    if(fresh||v->selected>=mercs_save_list_count(s,MERCS_SAVE_OWNED_PILOTS))v->selected=0;
    v->restart=1;
}
static inline const uint8_t *mrv_assigned(const mercs_save_state *s,const uint8_t *pilot)
{
    size_t i;if(!pilot)return NULL;
    for(i=0;i<mercs_save_list_count(s,MERCS_SAVE_OWNED_MECHS);i++){
        const uint8_t *m=mercs_save_list_record(s,MERCS_SAVE_OWNED_MECHS,i);
        if(mev_u32(m+0x80c)==mev_u32(pilot+0x24))return m;
    }
    return NULL;
}
static inline void mrv_aero_text(const mercs_save_state *s,char text[64])
{
    const uint8_t *p=mercs_save_list_record(s,MERCS_SAVE_OWNED_TECHS,0);
    if(!p){text[0]=0;return;}
    snprintf(text,64,"%.31s %s",(const char*)p,p[0x2c]==1?"assigned":"not assigned");
}
static inline void mercs_roster_view_draw(mercs_roster_view *v,mercs_art *a,const mercs_save_state *s)
{
    size_t i;const uint8_t *m;
    mercs_art_center(a,6,210,89,"MechWarrior Assignment",22);
    for(i=0;i<mercs_save_list_count(s,MERCS_SAVE_OWNED_PILOTS);i++){
        const uint8_t *p=mercs_save_list_record(s,MERCS_SAVE_OWNED_PILOTS,i);
        char name[32],assignment[280],design[257];int y=115+(int)i*10;
        memcpy(name,p,31);name[31]=0;m=mrv_assigned(s,p);
        mercs_art_text(a,10,37,y,name,22);
        if(m)snprintf(assignment,sizeof assignment,"piloting %s",miv_mech_name(m,design));
        else strcpy(assignment,"no mech assigned");
        mercs_art_text(a,10,150,y,assignment,i==v->selected?17:22);
    }
    m=mrv_assigned(s,mercs_save_list_record(s,MERCS_SAVE_OWNED_PILOTS,v->selected));
    if(m){char clip[12];snprintf(clip,sizeof clip,"a%.3s",(const char*)m+0x81c);
        mercs_art_clip_flags(a,clip,440,114,0x68,(unsigned)v->restart);}
    v->restart=0;
    if(mev_i32(mercs_save_header(s)+0x2df7)>=3&&mercs_save_list_count(s,MERCS_SAVE_OWNED_TECHS)){
        const uint8_t *p=mercs_save_list_record(s,MERCS_SAVE_OWNED_TECHS,0);char text[64];
        mrv_aero_text(s,text);mercs_art_text(a,10,37,314,"Aerotech:",22);
        mercs_art_text(a,10,37,358,text,p[0x2c]==1?17:22);
    }
}
/* Return1 pilot,2 support toggle,0 no hit. Original overlapping15px
 * assignment rows are searched in table order, not rounded by row height. */
static inline int mercs_roster_view_hit(const mercs_save_state *s,mercs_art *a,int x,int y,size_t *index)
{
    size_t i;
    for(i=0;i<mercs_save_list_count(s,MERCS_SAVE_OWNED_PILOTS);i++)
        if(mev_inside(x,y,150,115+(int)i*10,380,130+(int)i*10)){*index=i;return 1;}
    if(mev_i32(mercs_save_header(s)+0x2df7)>=3&&mercs_save_list_count(s,MERCS_SAVE_OWNED_TECHS)){
        char text[64];mrv_aero_text(s,text);
        if(mev_inside(x,y,37,358,37+mercs_art_width(a,10,text),358+mercs_art_height(a,10))){*index=0;return 2;}
    }
    return 0;
}
#endif
