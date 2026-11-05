#ifndef MERCS_MECHBAY_VIEW_H
#define MERCS_MECHBAY_VIEW_H

/* Retail DOS Mech Bay0x4b280/0x4b660 and Mech Ops0x4bb80/0x4c0e8.
 * Original asset names, coordinates, disabled room hotspots and routes.
 * The caller owns page transitions, sounds and transaction dialogs. */
#include "mercseconomy_view.h"
#include "mercsui_layout.h"

enum { MERCS_MECHBAY_NONE, MERCS_MECHBAY_OPS, MERCS_MECHBAY_OFFICE, MERCS_MECHBAY_CREW };
enum { MERCS_MECHOPS_NONE, MERCS_MECHOPS_ARMS, MERCS_MECHOPS_FACTORY,
       MERCS_MECHOPS_CUSTOMIZE, MERCS_MECHOPS_REPAIR, MERCS_MECHOPS_GROUPS,
       MERCS_MECHOPS_EXIT };

static inline int mercs_mechbay_hit(int x,int y)
{
    if(mev_inside(x,y,0,191,98,431))return MERCS_MECHBAY_OPS;
    if(mev_inside(x,y,0,432,639,479))return MERCS_MECHBAY_OFFICE;
    if(mev_inside(x,y,391,116,456,169))return MERCS_MECHBAY_CREW;
    return MERCS_MECHBAY_NONE;
}
static inline void mercs_mechbay_draw(mercs_art *a,int restart,int x,int y,int subtitles)
{
    int hit=mercs_mechbay_hit(x,y);
    mercs_art_background(a,13);
    mercs_art_clip_flags(a,"mmon",0,234,0x28,restart);
    mercs_art_clip_flags(a,"mlighta",0,192,0x28,restart);
    mercs_art_clip_flags(a,"mlightb",322,220,0x28,restart);
    mercs_art_clip_flags(a,"mlightc",455,246,0x28,restart);
    if(subtitles&&hit==MERCS_MECHBAY_OPS)mercs_art_text(a,MERCS_UI_FONT_MENU,12,214,"MECH OPS",22);
    if(subtitles&&hit==MERCS_MECHBAY_OFFICE)mercs_art_text(a,MERCS_UI_FONT_MENU,321,443,"OFFICE",22);
}
static inline int mercs_mechops_hit(int x,int y)
{
    static const int ys[5][2]={{7,97},{98,191},{192,286},{287,378},{379,469}};
    int i;
    if(mev_inside(x,y,512,426,618,471))return MERCS_MECHOPS_EXIT;
    for(i=0;i<5;i++)if(mev_inside(x,y,13,ys[i][0],238,ys[i][1]))return MERCS_MECHOPS_ARMS+i;
    return MERCS_MECHOPS_NONE;
}
static inline void mercs_mechops_draw(mercs_art *a,const mercs_save_state *s,int restart,int x,int y)
{
    static const char *const names[5]={"bmarms","bmmech","bmcust","bmrepr","bmwgrp"};
    static const int ys[5]={11,104,198,292,385};
    int i,hit=mercs_mechops_hit(x,y);
    mercs_art_background(a,17);mev_funds(a,s);
    mercs_art_clip_flags(a,"mbay",352,113,0x28,restart);
    for(i=0;i<5;i++)mercs_art_button(a,names[i],19,ys[i],hit==MERCS_MECHOPS_ARMS+i);
    mercs_art_button(a,"bmexit",525,437,hit==MERCS_MECHOPS_EXIT);
}

#endif
