#ifndef MERCS_INSTANT_VIEW_H
#define MERCS_INSTANT_VIEW_H
#include "mercsinstant.h"
#include "mercsart.h"

/* Authored table0xc0c4c /0xc0fe8. Bounds are inclusive in the original
 * hotspot consumer; listbox0xc0dbc uses its separate width/height test. */
static const int mia_main_spots[9][4]={
    {428,292,522,342},{523,292,618,342},{428,202,522,241},{523,202,618,241},
    {499,432,605,473},{427,380,576,430},{16,433,123,471},{143,431,359,473},{395,432,498,473}};
static const int mia_param_spots[10][4]={
    {500,430,602,474},{148,430,354,474},{178,71,241,105},{178,55,243,91},{244,55,307,91},
    {308,55,372,91},{178,92,268,126},{269,92,357,126},{358,92,447,126},{188,127,280,162}};
static const int mia_rows[6]={54,98,142,268,312,356};
static inline int mia_inside(const int r[4],int x,int y)
{return x>=r[0]&&x<=r[2]&&y>=r[1]&&y<=r[3];}
static inline int mercs_instant_view_hit(int parameters,int x,int y)
{
    int i;if(parameters){for(i=0;i<10;i++)if(i!=1&&i!=2&&mia_inside(mia_param_spots[i],x,y))return i;}
    else for(i=0;i<8;i++)if(mia_inside(mia_main_spots[i],x,y))return i;
    return -1;
}
static inline int mercs_instant_view_row(int x,int y)
{int i;for(i=0;i<6;i++)if(x>=32&&x<232&&y>=mia_rows[i]&&y<mia_rows[i]+21)return i;return -1;}
static inline void mercs_instant_view_draw(mercs_art *a,mercs_instant *s,int parameters,int x,int y)
{
    int i;
    if(parameters){
        static const char *const names[]={"laaeasy","laamed","laahard","laadest","laadefd","laarecn"};
        static const int bx[]={183,247,311,183,272,361};
        mercs_art_background(a,21);
        mercs_art_button(a,"iabexit",510,440,mia_inside(mia_param_spots[0],x,y));
        for(i=0;i<6;i++)mercs_art_button(a,names[i],bx[i],i<3?62:98,
            i<3?s->parameters.skill==i+1:s->parameters.kind==i-2);
        mercs_art_text(a,6,205,134,s->parameters.planet,22);
        mercs_art_clip_flags(a,"solarsys",88,193,0x28u,s->restart);
        mercs_art_clip_flags(a,"ships",356,193,0x28u,s->restart);
    }else{
        static const struct {const char *name;int x,y,spot;} buttons[]={
            {"iaaprev",437,297,0},{"iaanext",528,300,1},{"iaaprev",437,208,2},{"iaanext",528,212,3},
            {"iaaexit",510,440,4},{"iaamisp",157,440,7},{"iaalnch",428,393,5},{"iaamlab",25,440,6}};
        mercs_art_background(a,20);
        for(i=0;i<8;i++)mercs_art_button(a,buttons[i].name,buttons[i].x,buttons[i].y,mia_inside(mia_main_spots[buttons[i].spot],x,y));
        for(i=0;i<6;i++){
            const mercs_instant_slot *p=&s->slots[i];char label[160];
            if(p->chassis<0)snprintf(label,sizeof label,"Point %d: %s",i,p->chassis==-1?"NO MECH ASSIGNED":"RANDOM MECH");
            else if(p->custom_size){mek_def m;
                if(!mek_parse(p->custom,p->custom_size,&m))snprintf(label,sizeof label,"Point %d: Mech: %s",i,m.config_name);
                else snprintf(label,sizeof label,"Point %d: Mech: %s",i,mercs_instant_chassis_name(p->chassis));
            }else snprintf(label,sizeof label,"Point %d: Mech: %s",i,mercs_instant_chassis_name(p->chassis));
            mercs_art_text(a,6,32,mia_rows[i],label,s->selected==i?17:22);
        }
        if(s->selected>=0&&s->selected<6&&s->slots[s->selected].chassis>=0){char clip[16];
            snprintf(clip,sizeof clip,"a%s",mercs_instant_prefix(s->slots[s->selected].chassis));
            mercs_art_clip_flags(a,clip,441,26,0x68u,s->restart);
        }
    }
    s->restart=0;
}
#endif
