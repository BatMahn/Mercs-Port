#ifndef MERCS_MECHLAB_VIEW_H
#define MERCS_MECHLAB_VIEW_H

/* DOS0x35964/0x36960 original Mech Lab: DATABASE resource25 (index24), original body SHPs,
 * category/action clips, fonts and authored23 hit rectangles. The view keeps
 * presentation/selection only. Career persistence remains with the shell. */
#include "mercsmechlab.h"
#include "mercstrading_view.h"
#include "mercsrepair_view.h"
#include "mercsdebrief_data.h"

enum mercs_mechlab_view_action {
    MERCS_MECHLAB_VIEW_NONE, MERCS_MECHLAB_VIEW_EXIT,
    MERCS_MECHLAB_VIEW_PREV, MERCS_MECHLAB_VIEW_NEXT,
    MERCS_MECHLAB_VIEW_CATEGORY, MERCS_MECHLAB_VIEW_SELECT,
    MERCS_MECHLAB_VIEW_LOCATION, MERCS_MECHLAB_VIEW_CRITICAL,
    MERCS_MECHLAB_VIEW_ADD, MERCS_MECHLAB_VIEW_REMOVE,
    MERCS_MECHLAB_VIEW_ADJUST, MERCS_MECHLAB_VIEW_PLACE,
    MERCS_MECHLAB_VIEW_RENAME, MERCS_MECHLAB_VIEW_TECHNOLOGY
};
typedef struct mercs_mechlab_view {
    int category,slot,scroll,panel,technology; /* panel0 criticals,1 engine,2 armor */
    size_t selected,count;
    mercs_item_group *groups;
    const char *name_override; /* borrowed in-game rename text; NULL otherwise */
} mercs_mechlab_view;

static inline void mercs_mechlab_view_init(mercs_mechlab_view *v)
{ memset(v,0,sizeof *v);v->selected=(size_t)-1;v->slot=-1;v->technology=1; }
static inline void mercs_mechlab_view_free(mercs_mechlab_view *v)
{ if(v){free(v->groups);mercs_mechlab_view_init(v);} }
static inline int mercs_mechlab_view_prepare(mercs_mechlab_view *v,const mercs_mechlab_session *s,int category)
{
    mercs_item_group *groups=NULL;size_t count=0,i;int32_t old=-1;int changed;
    const mercs_save_state *doc=mercs_mechlab_document(s);int game_mode;
    if(!v||!doc||category<0||category>4)return -1;
    changed=v->category!=category;
    if(!changed&&v->selected<v->count)old=v->groups[v->selected].id;
    game_mode=mev_i32(mercs_save_header(doc)+0x23bb);
    if(game_mode==2){
        /* DOS0x57480 technology selector: weapon63 belongs to Clan even
         * though its table index follows the Inner Sphere equipment. */
        groups=calloc(77,sizeof *groups);if(!groups)return -1;
        for(i=0;i<(category==MERCS_ITEM_EQUIPMENT?11u:66u);i++){
            uint8_t row[26]={0};unsigned type=category==MERCS_ITEM_EQUIPMENT?2u:category==MERCS_ITEM_AMMO?1u:0u;
            row[0]=(uint8_t)i;row[8]=(uint8_t)type;
            if(type<2&&((i>=35&&i!=63)?1:0)!=v->technology)continue;
            if(type==1&&debrief_ammo_price[i]<=0)continue;
            if(mercs_economy_item_category(row)!=category)continue;
            groups[count].id=(int32_t)i;groups[count].type=type;groups[count].count=1;count++;
        }
    }else if(mercs_economy_item_groups(doc,0,(enum mercs_item_category)category,&groups,&count))return -1;
    if(category==MERCS_ITEM_EQUIPMENT){
        int present=0;for(i=0;i<count;i++)if(groups[i].id==9)present=1;
        if(!present){
            mercs_item_group *p=realloc(groups,(count+1)*sizeof *p);
            if(!p){free(groups);return -1;}groups=p;memset(groups+count,0,sizeof *groups);
            groups[count].id=9;groups[count].type=2;groups[count].count=1;count++;
        }
    }
    free(v->groups);v->groups=groups;v->count=count;v->selected=(size_t)-1;
    if(changed){v->scroll=0;v->panel=0;v->slot=-1;}v->category=category;
    for(i=0;i<count;i++)if(groups[i].id==old){v->selected=i;break;}
    if(v->scroll>(int)count-1)v->scroll=count?(int)count-1:0;
    return 0;
}
static inline void mlv_number(mercs_art *a,int x,int y,int32_t value,int right)
{
    char text[24];snprintf(text,sizeof text,"%ld",(long)value);
    mercs_art_text(a,10,x-(right?mercs_art_width(a,10,text):0),y,text,22);
}
static inline int mlv_crits(const mercs_item_group *g,const uint8_t *m)
{
    static const int equipment[11]={-1,1,2,-1,-1,1,1,-1,-1,-1,1};
    (void)m;
    if(g->type==1)return 1;
    if(g->type==0)return g->id>=0&&g->id<66?mek_weapons[g->id].crits:0;
    return g->id>=0&&g->id<11?equipment[g->id]:0;
}
/* DOS engine table0xb99c8, sixteen-byte rows (rating index). */
static const char *const mlv_manufacturer[79]={
    "Omni",
    "GM",
    "Pitban",
    "Omni",
    "Nissan",
    "VOX",
    "GM",
    "GM",
    "DAV",
    "VOX",
    "Leenex",
    "Nissan",
    "Omni",
    "GM",
    "VOX",
    "DAV",
    "DAV",
    "Nissan",
    "Hermes",
    "DAV",
    "GM",
    "GM",
    "GM",
    "Vlar",
    "Magna",
    "Hermes",
    "Leenex",
    "Omni",
    "GM",
    "GM",
    "LTV",
    "VOX",
    "DAV",
    "Omni",
    "GM",
    "GM",
    "DAV",
    "Nissan",
    "Nissan",
    "Vlar",
    "GM",
    "Core Tex",
    "DAV",
    "VOX",
    "Leenex",
    "GM",
    "Pitban",
    "Magna",
    "Magna",
    "Strand",
    "Magna",
    "Vlar",
    "GM",
    "Core Tex",
    "VOX",
    "Pitban",
    "Omni",
    "GM",
    "Vlar",
    "GM",
    "Magna",
    "GM",
    "Pitban",
    "VOX",
    "VOX",
    "Leenex",
    "VOX",
    "Vlar",
    "Magna",
    "LTV",
    "Hermes",
    "Hermes",
    "Magna",
    "GM",
    "GM",
    "LTV",
    "Magna",
    "Hermes",
    "LTV"
};
static inline void mercs_mechlab_view_draw(mercs_mechlab_view *v,mercs_art *a,const mercs_mechlab_session *s)
{
    static const char *const locations[8]={"Head","Right Torso","Center Torso","Left Torso","Right Arm","Left Arm","Right Leg","Left Leg"};
    static const char *const body[8]={"head","rtor","ctor","ltor","rarm","larm","rleg","lleg"};
    static const int bx[8]={105,75,88,120,26,154,11,129},by[8]={122,108,154,107,126,126,173,173};
    static const char *const buttons[5]={"bcmiss","bcenrg","bcproj","bcequp","bcammo"};
    static const int buttonx[5]={15,105,194,284,373};
    const mercs_career_mech *mech=mercs_mechlab_current(s);const uint8_t *m,*h;
    char text[320],name[257],number[24];int i,loc,height,step,pending,component,game_mode;
    if(!v||!a||!mech)return;
    m=mech->bytes;h=mercs_save_header(mercs_mechlab_document(s));
    game_mode=mev_i32(h+0x23bb);loc=mev_i32(m+0x364);if(loc<0||loc>7)loc=0;
    height=mercs_art_height(a,10);if(height<1)return;step=height+1;
    pending=mercs_mechlab_pending(s,&component);
    mercs_art_background(a,game_mode==2?22:24);
    for(i=0;i<5;i++)mercs_art_button(a,buttons[i],buttonx[i],12,i==v->category);
    mercs_art_button(a,"bcexit",509,440,0);mercs_art_button(a,"bcadd",496,382,0);
    mercs_art_button(a,"bcrem",291,382,0);mercs_art_button(a,"bcprev",41,393,0);mercs_art_button(a,"bcnext",131,395,0);
    if(game_mode==2){mercs_art_button(a,"mlbi",236,440,v->technology==1);mercs_art_button(a,"mlbc",359,440,v->technology==0);}
    for(i=0;i<8;i++){
        int state=mercs_repair_view_condition(mech,i);
        if(state==3&&i!=loc)continue;
        mercs_art_named_shape(a,body[i],state+(i==loc?3:0),bx[i],by[i]);
    }
    if(v->name_override)snprintf(name,sizeof name,"%s_",v->name_override);
    else {memcpy(name,m+(m[0x100]?0x100:0),256);name[256]=0;}
    mercs_art_center(a,5,120,63,name,17);
    snprintf(text,sizeof text,"Mass: %ld.%02ld T  Max: %ld.%02ld T",(long)(mev_i32(m+0x204)/100),
             (long)(mev_i32(m+0x204)%100),(long)(mev_i32(m+0x200)/100),(long)(mev_i32(m+0x200)%100));
    mercs_art_text(a,10,258,352,text,22);
    if(game_mode==1){
        static const char *const labels[3]={"Funds:","Cost:","Balance:"};
        int32_t cost=mercs_mechlab_cost(s),funds=mev_i32(h+0x12a),values[3]={funds,cost,funds-cost};
        for(i=0;i<3;i++){
            mev_money(values[i],number);mercs_art_text(a,10,490,10+13*i,labels[i],22);
            mercs_art_text(a,10,620-mercs_art_width(a,10,number),10+13*i,number,22);
        }
    }
    if(v->panel!=2){
        mercs_art_text(a,10,255,90,locations[loc],17);
        for(i=0;i<(loc==0||loc>=6?6:12);i++){
            int32_t id=mev_i32(m+0x368+48*loc+4*i);int selected=v->slot>=0&&
                (i==v->slot||(id>0&&id==mev_i32(m+0x368+48*loc+4*v->slot)));
            if(id/10==955)snprintf(text,sizeof text,"CASE");else mercs_repair_view_critical_name(id,text);
            mercs_art_text(a,10,255,90+2*step+i*step,text,selected?17:22);
        }
    }
    mercs_art_text(a,10,460,92,"Weapon:",17);
    if(game_mode==1)mercs_art_text(a,10,611-mercs_art_width(a,10,"In Stock"),92,"In Stock",17);
    {const char *label=v->category==MERCS_ITEM_AMMO?"Crits/Ton":"Crits";
     int x=game_mode==1?561:611;mercs_art_text(a,10,x-mercs_art_width(a,10,label),92,label,17);}
    for(i=v->scroll;i<(int)v->count;i++){
        const mercs_item_group *g=v->groups+i;char label[64];int y=92+2*height+(i-v->scroll)*height;
        if(y+height>(v->panel||pending?222:373))break;
        snprintf(text,sizeof text,"%s%s",mtv_item_name(g,label),g->type==2?"":":");
        mercs_art_text(a,10,460,y,text,(size_t)i==v->selected?17:22);
        if(game_mode==1)mlv_number(a,611,y,(int32_t)g->count,1);
        if(mlv_crits(g,m)>=0)mlv_number(a,game_mode==1?561:611,y,mlv_crits(g,m),1);
    }
    if(pending){
        /* Original0x317ac regenerates one row per remaining critical. */
        mercs_art_text(a,10,460,232,"Assign Crits",17);
        for(i=0;i<pending;i++)mercs_art_text(a,10,460,232+(i+2)*height,
            component==9000?"Endo Steel (1)":"Ferro-Fibrous (1)",17);
    }else if(v->panel==1){
        static const char *const labels[6]={"Rating","Type","Manufacturer","Mass","Walking Speed","Running Speed"};
        mercs_art_text(a,10,460,232,"ENGINE",22);
        for(i=0;i<6;i++){
            int y=232+(2+i)*step;
            mercs_art_text(a,10,460,y,labels[i],22);
            if(i==0)snprintf(text,sizeof text,"%ld%s",(long)mev_i32(m+0x210),mev_i32(m+0x20c)>=10000?"XL":"");
            else if(i==1)snprintf(text,sizeof text,"%s",mev_i32(m+0x20c)>=10000?"XL":"Std");
            else if(i==2)snprintf(text,sizeof text,"%s",mlv_manufacturer[mev_i32(m+0x20c)%10000]);
            else if(i==3)snprintf(text,sizeof text,"%ld.%02ld T",(long)(mev_i32(m+0x214)/100),(long)(mev_i32(m+0x214)%100));
            else snprintf(text,sizeof text,"%.1f kph",mev_i32(m+(i==4?0x280:0x284))*10.8);
            mercs_art_text(a,10,560,y,text,i==1?17:22);
        }
        mercs_art_text(a,10,460,232+9*step,"FASTER",17);mercs_art_text(a,10,460,232+10*step,"SLOWER",17);
    }else if(v->panel==2){
        static const char *const labels[4]={"Factor","Allocated","Mass","Type"};
        mercs_art_text(a,10,460,232,"ARMOR",22);
        for(i=0;i<4;i++){
            int y=232+(2+i)*step;mercs_art_text(a,10,460,y,labels[i],22);
            if(i<2)snprintf(text,sizeof text,"%ld",(long)mev_i32(m+(i==0?0x258:0x25c)));
            else if(i==2)snprintf(text,sizeof text,"%ld.%02ld T",(long)(mev_i32(m+0x250)/100),(long)(mev_i32(m+0x250)%100));
            else snprintf(text,sizeof text,"%s",mev_i32(m+0x24c)?"Ferro":"Standard");
            mercs_art_text(a,10,560,y,text,22);
        }
        mercs_art_text(a,10,460,232+7*step,"ADD",17);mercs_art_text(a,10,460,232+8*step,"DELETE",17);
        mercs_art_text(a,10,567,324,"+",17);mercs_art_text(a,10,597,324,"+",17);
        mercs_art_text(a,10,567,358,"-",17);mercs_art_text(a,10,597,358,"-",17);
        mlv_number(a,570,232+10*step,mev_i32(m+0x768+18*loc),0);
        if(mev_i32(m+0x76c+18*loc)>=0)mlv_number(a,600,232+10*step,mev_i32(m+0x76c+18*loc),0);
        mercs_art_text(a,10,259,232,"ARMOR ALLOCATION",22);
        for(i=0;i<8;i++){
            int y=232+(2+i)*step,front=mev_i32(m+0x768+18*i),rear=mev_i32(m+0x76c+18*i);
            mercs_art_text(a,10,259,y,locations[i],22);
            if(rear<0)snprintf(text,sizeof text,"%d",front);else snprintf(text,sizeof text,"%d (%d)",front,rear);
            mercs_art_text(a,10,379,y,text,22);
        }
    }
}
static inline int mercs_mechlab_view_hit(const mercs_mechlab_view *v,mercs_art *a,
                                         const mercs_mechlab_session *s,int x,int y,int *value)
{
    static const int bounds[23][4]={
        {502,432,604,473},{399,432,501,473},{8,8,100,44},{101,8,190,44},{191,8,280,44},
        {281,8,369,44},{370,8,462,44},{28,384,124,431},{125,384,219,431},{281,378,386,418},
        {485,378,590,418},{18,434,234,472},{104,122,135,157},{164,121,218,228},
        {18,121,67,228},{99,156,140,229},{135,101,163,156},{74,101,104,156},
        {143,228,231,381},{7,228,107,381},{223,431,335,474},{346,431,458,474},{23,55,217,87}};
    static const int body[8]={0,5,4,2,3,1,7,6};
    int i,height,step,pending,loc,mode_;const uint8_t *m;
    if(!v||!a||!s||!value)return 0;
    m=mercs_mechlab_current(s)->bytes;
    loc=mev_i32(m+0x364);height=mercs_art_height(a,10);if(height<1)return 0;step=height+1;
    pending=mercs_mechlab_pending(s,NULL);mode_=mev_i32(mercs_save_header(mercs_mechlab_document(s))+0x23bb);
    for(i=0;i<23;i++)if(mev_inside(x,y,bounds[i][0],bounds[i][1],bounds[i][2],bounds[i][3])){
        if(i==0)return MERCS_MECHLAB_VIEW_EXIT;
        if(i>=2&&i<=6){*value=i-2;return MERCS_MECHLAB_VIEW_CATEGORY;}
        if(i==7)return MERCS_MECHLAB_VIEW_PREV;
        if(i==8)return MERCS_MECHLAB_VIEW_NEXT;
        if(i==9)return MERCS_MECHLAB_VIEW_REMOVE;
        if(i==10)return MERCS_MECHLAB_VIEW_ADD;
        if(i>=12&&i<=19){*value=body[i-12];return MERCS_MECHLAB_VIEW_LOCATION;}
        if(mode_==2&&(i==20||i==21)){*value=i==20;return MERCS_MECHLAB_VIEW_TECHNOLOGY;}
        if(i==22)return MERCS_MECHLAB_VIEW_RENAME;
    }
    if(pending&&mev_inside(x,y,460,232+2*height,620,232+(pending+2)*height-1)){
        *value=loc;return MERCS_MECHLAB_VIEW_PLACE;
    }
    if(!pending&&v->panel==1){
        if(mev_inside(x,y,560,232+3*step,620,232+4*step)){*value=MERCS_MECHLAB_XL*2+1;return MERCS_MECHLAB_VIEW_ADJUST;}
        if(mev_inside(x,y,460,232+9*step,530,232+10*step)){*value=MERCS_MECHLAB_ENGINE*2+1;return MERCS_MECHLAB_VIEW_ADJUST;}
        if(mev_inside(x,y,460,232+10*step,530,232+11*step)){*value=MERCS_MECHLAB_ENGINE*2;return MERCS_MECHLAB_VIEW_ADJUST;}
    }
    if(!pending&&v->panel==2){
        if(mev_inside(x,y,460,232+7*step,530,232+8*step)){*value=MERCS_MECHLAB_ARMOR_WEIGHT*2+1;return MERCS_MECHLAB_VIEW_ADJUST;}
        if(mev_inside(x,y,460,232+8*step,530,232+9*step)){*value=MERCS_MECHLAB_ARMOR_WEIGHT*2;return MERCS_MECHLAB_VIEW_ADJUST;}
        for(i=0;i<4;i++)if(mev_inside(x,y,567+30*(i%2),324+34*(i/2),581+30*(i%2),324+34*(i/2)+height)){
            *value=(MERCS_MECHLAB_FRONT_ARMOR+i%2)*2+(i<2);return MERCS_MECHLAB_VIEW_ADJUST;
        }
    }
    if(v->panel!=2&&x>=255&&x<430){
        if(y>=90&&y<90+height){*value=(loc+1)%8;return MERCS_MECHLAB_VIEW_LOCATION;}
        i=(y-(90+2*step))/step;
        if(y>=90+2*step&&i>=0&&i<(loc==0||loc>=6?6:12)){
            *value=i;return MERCS_MECHLAB_VIEW_CRITICAL;
        }
    }
    if(!pending&&x>=460&&x<=630&&y>=92+2*height&&y<(v->panel?222:373)){
        i=(y-(92+2*height))/height+v->scroll;
        if(i>=0&&(size_t)i<v->count){*value=i;return MERCS_MECHLAB_VIEW_SELECT;}
    }
    return 0;
}
/* Dispatch local view/editor actions. EXIT/PREV/NEXT/RENAME remain with the
 * shell because they require settlement, durable storage or text input. */
static inline int mercs_mechlab_view_apply(mercs_mechlab_view *v,mercs_mechlab_session *s,int action,int value)
{
    int rc=0;
    if(action==MERCS_MECHLAB_VIEW_CATEGORY)return mercs_mechlab_view_prepare(v,s,value)?MERCS_MECHLAB_NOMEM:0;
    if(action==MERCS_MECHLAB_VIEW_SELECT){
        if(value<0||(size_t)value>=v->count)return MERCS_MECHLAB_INVALID;
        v->selected=(size_t)value;v->slot=-1;return 0;
    }
    if(action==MERCS_MECHLAB_VIEW_CRITICAL){v->slot=value;v->selected=(size_t)-1;return 0;}
    if(action==MERCS_MECHLAB_VIEW_LOCATION){
        v->slot=-1;return mercs_mechlab_select_location(s,value);
    }
    if(action==MERCS_MECHLAB_VIEW_PLACE)rc=mercs_mechlab_place(s,value);
    else if(action==MERCS_MECHLAB_VIEW_ADD){
        if(v->selected>=v->count)return MERCS_MECHLAB_NO_ITEM;
        rc=mercs_mechlab_add(s,v->groups[v->selected].type,v->groups[v->selected].id);
        if(rc==MERCS_MECHLAB_OPEN_ENGINE||rc==MERCS_MECHLAB_OPEN_ARMOR){v->panel=rc==MERCS_MECHLAB_OPEN_ENGINE?1:2;return 0;}
        if(!rc)v->panel=0;
    }else if(action==MERCS_MECHLAB_VIEW_REMOVE){
        if(v->slot<0)return MERCS_MECHLAB_NO_ITEM;
        rc=mercs_mechlab_remove(s,v->slot);if(!rc)v->slot=-1;
    }else if(action==MERCS_MECHLAB_VIEW_ADJUST)
        rc=mercs_mechlab_adjust(s,(enum mercs_mechlab_control)(value/2),(value&1)?1:-1);
    else if(action==MERCS_MECHLAB_VIEW_TECHNOLOGY){v->technology=!!value;}
    else return MERCS_MECHLAB_INVALID;
    if(!rc&&mercs_mechlab_view_prepare(v,s,v->category))return MERCS_MECHLAB_NOMEM;
    return rc;
}
static inline void mercs_mechlab_view_scroll(mercs_mechlab_view *v,mercs_art *a,int rows)
{
    int h=mercs_art_height(a,10),max,visible;if(h<1)return;
    visible=((v->panel?222:373)-(92+2*h))/h;max=(int)v->count-visible;if(max<0)max=0;
    v->scroll+=rows;if(v->scroll<0)v->scroll=0;if(v->scroll>max)v->scroll=max;
}
#endif
