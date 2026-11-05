#ifndef MERCS_FACTORY_VIEW_H
#define MERCS_FACTORY_VIEW_H

/* Original retail DOS Mech Factory0x4d800/0x4dfb8, including its authored
 * art, clips, six hitboxes, row builder0x4d29c and detail callbacks0x4c614.
 * Selection and preview are read-only. The shell owns confirmed transactions. */
#include "mercsinventory_view.h"

enum { MERCS_FACTORY_NONE,MERCS_FACTORY_EXIT,MERCS_FACTORY_SELL,MERCS_FACTORY_BUY,
       MERCS_FACTORY_DETAILS,MERCS_FACTORY_NEXT,MERCS_FACTORY_PREVIOUS,MERCS_FACTORY_SELECT };
typedef struct mercs_factory_view {
    int side,details,restart,scroll[2],detail_scroll;
    size_t selected,count[2];
} mercs_factory_view;
static inline enum mercs_save_list mfv_list(int side)
{ return side?MERCS_SAVE_SHOP_MECHS:MERCS_SAVE_OWNED_MECHS; }
static inline void mercs_factory_view_prepare(mercs_factory_view *v,const mercs_save_state *s,int fresh)
{
    if(fresh){memset(v,0,sizeof *v);v->side=-1;}
    v->count[0]=mercs_save_list_count(s,MERCS_SAVE_OWNED_MECHS);
    v->count[1]=mercs_save_list_count(s,MERCS_SAVE_SHOP_MECHS);
    if(fresh)v->side=v->count[1]?1:v->count[0]?0:-1;
    else v->side=-1; /* Original transaction clears selection and price. */
    v->selected=0;v->restart=1;v->detail_scroll=0;
}
static inline int mercs_factory_view_select(mercs_factory_view *v,int side,size_t index)
{
    if(side<0||side>1||index>=v->count[side])return -1;
    v->side=side;v->selected=index;v->restart=1;v->detail_scroll=0;return 0;
}
static inline void mfv_money(mercs_art *a,int y,const char *label,int32_t value)
{
    char b[24];mev_money(value,b);mercs_art_text(a,6,490,y,label,17);
    mercs_art_text(a,6,620-mercs_art_width(a,6,b),y,b,17);
}
static inline void mfv_mark_equal(const uint8_t *m,unsigned char used[96],int loc,int slot,int id)
{
    int l,k;
    /* Preserve the original nested-loop bounds (the slot does not reset
     * when advancing a location), rather than changing its listing policy. */
    for(l=loc;l<8;l++)for(k=slot+1;k<12;k++)
        if(mev_i32(m+0x368+l*48+k*4)==id)used[l*12+k]=1;
}
static inline int mfv_type(int id)
{ return id<=0?3:id<7000?0:id<10000?2:1; }
static inline int mfv_item(int id,int type)
{ return type==1?(id-10000)/10000:id/100; }
static inline void mfv_detail_rows(const uint8_t *m,char rows[21][320],int *count)
{
    unsigned char used[96]={0};int l,k,j,h,jets=0,sinks=0,weight=mev_i32(m+0x204),max=mev_i32(m+0x200),armor=mev_i32(m+0x250);
    char name[257];const char *nm=miv_mech_name(m,name);int n=0;
    snprintf(rows[n++],320,"%s %d.%d%d/%d.%d%d T",nm,weight/100,(weight/10)%10,weight%10,max/100,(max/10)%10,max%10);
    snprintf(rows[n++],320,"walk: %1.1f kph run: %1.1f kph",mev_i32(m+0x280)*10.8,mev_i32(m+0x284)*10.8);
    for(j=0;j<96;j++){int id=mev_i32(m+0x368+j*4);if(id/50==8500/50)jets++;if(id/50==8000/50)sinks++;}
    if(jets)snprintf(rows[n++],320,"Equipped w/%d Jump Jets",jets);else snprintf(rows[n++],320,"No Jump Jets");
    /* DOS0x4ca4c's second-sink search compares id/100 to8000. No valid
     * component can enter that branch; its authored display counts singles. */
    if(sinks)snprintf(rows[n++],320,"Equipped w/%d single heat sinks",sinks);else snprintf(rows[n++],320,"No heat sinks");
    snprintf(rows[n++],320,"Armor: %d.%d%d T",armor/100,(armor/10)%10,armor%10);
    for(h=0;h<16;h++){
        int done=0;
        for(l=0;l<8&&!done;l++)for(k=0;k<12&&!done;k++){
            int id,type,item,qty=0,ll,kk;
            if(used[l*12+k])continue;
            used[l*12+k]=1;
            id=mev_i32(m+0x368+l*48+k*4);type=mfv_type(id);
            if(type==2||type==3)continue;
            item=mfv_item(id,type);if(item<0||item>=mek_weapon_count)continue;
            mfv_mark_equal(m,used,l,k,id);
            for(ll=l;ll<8;ll++)for(kk=k;kk<12;kk++){
                int other=mev_i32(m+0x368+ll*48+kk*4);
                if(used[ll*12+kk]||mfv_type(other)!=type||mfv_item(other,type)!=item)continue;
                used[ll*12+kk]=1;qty++;mfv_mark_equal(m,used,ll,kk,other);
            }
            qty++;
            if(type==1)snprintf(rows[n++],320,qty==1?"%d ton %s Ammo":"%d tons %s Ammo",qty,mek_weapons[item].name);
            else snprintf(rows[n++],320,"%d %s",qty,mek_weapons[item].name);
            done=1;
        }
        if(!done)break;
    }
    *count=n;
}
static inline void mercs_factory_view_draw(mercs_factory_view *v,mercs_art *a,const mercs_save_state *s,int x,int y)
{
    int side,h=mercs_art_height(a,10),selected=v->side>=0;size_t i;
    const uint8_t *m=selected?mercs_save_list_record(s,mfv_list(v->side),v->selected):NULL;
    mercs_art_background(a,16);
    mercs_art_button(a,"mfbs",495,386,mev_inside(x,y,488,381,583,417));
    mercs_art_button(a,"mfbb",292,386,mev_inside(x,y,284,381,380,415));
    mercs_art_button(a,"mfbe",510,440,mev_inside(x,y,498,432,601,473));
    mercs_art_button(a,"mfbp",47,428,mev_inside(x,y,34,422,129,462));
    mercs_art_button(a,"mfbn",136,430,mev_inside(x,y,130,422,225,462));
    mercs_art_button(a,v->details?"mfbc":"mfbd",v->details?49:50,v->details?133:135,mev_inside(x,y,45,148,209,185));
    if(mev_u32(mercs_save_header(s)+0x23bb)==1){
        int32_t funds=mev_i32(mercs_save_header(s)+0x12a),price;
        mfv_money(a,10,"Funds:",funds);
        if(m&&!mercs_economy_mech_price(s,v->side,v->selected,&price)){
            uint32_t after=v->side?(uint32_t)funds-(uint32_t)price:(uint32_t)funds+(uint32_t)price;
            int32_t balance=after<=INT32_MAX?(int32_t)after:(int32_t)(-1-(int64_t)(UINT32_MAX-after));
            mfv_money(a,23,"Price:",price);mfv_money(a,36,"Bal:",balance);
        }
    }
    if(m){
        char name[257];const char *title=miv_mech_name(m,name);
        if(mercs_art_width(a,5,title)>164){const char *space=strchr(title,' ');if(space&&space[1])title=space+1;}
        mercs_art_center(a,5,126,93,title,17);
        if(!v->details){
            char prefix[8],clip[16];memcpy(prefix,m+0x81c,7);prefix[7]=0;snprintf(clip,sizeof clip,"a%s",prefix);
            mercs_art_clip_flags(a,clip,44,207,0x28,(unsigned)v->restart);v->restart=0;
        }else{
            char rows[21][320];int count,j,yy=206;
            mfv_detail_rows(m,rows,&count);
            for(j=v->detail_scroll;j<count;j++){
                const char *p=rows[j];int yyrow=yy;
                do{
                    char line[320];const char *end=strchr(p,'\n');size_t n=end?(size_t)(end-p):strlen(p);
                    if(n>=sizeof line)n=sizeof line-1;
                    memcpy(line,p,n);line[n]=0;
                    if(yyrow+h<=413)mercs_art_text(a,10,43,yyrow,line,22);
                    yyrow+=h;p=end?end+1:NULL;
                }while(p);
                yy=yyrow+1;if(yy+h>413)break;
            }
        }
    }
    for(side=0;side<2;side++)for(i=(size_t)v->scroll[side];i<v->count[side];i++){
        char name[257];int yy=94+(int)(i-(size_t)v->scroll[side])*h,ink=v->side==side&&v->selected==i?17:22;
        if(yy+h>375)break;
        mercs_art_text(a,10,side?257:461,yy,miv_mech_name(mercs_save_list_record(s,mfv_list(side),i),name),ink);
    }
}
static inline int mercs_factory_view_hit(const mercs_factory_view *v,mercs_art *a,int x,int y,int *side,size_t *index)
{
    static const int bounds[6][4]={{498,432,601,473},{488,381,583,417},{284,381,380,415},{45,148,209,185},{130,422,225,462},{34,422,129,462}};
    int i,h=mercs_art_height(a,10);
    for(i=0;i<6;i++)if(mev_inside(x,y,bounds[i][0],bounds[i][1],bounds[i][2],bounds[i][3]))return MERCS_FACTORY_EXIT+i;
    for(i=0;i<2;i++)if(h>0&&x>=(i?257:461)&&x<(i?357:561)&&y>=94&&y<375){
        size_t n=(size_t)((y-94)/h)+(size_t)v->scroll[i];
        if(n<v->count[i]){*side=i;*index=n;return MERCS_FACTORY_SELECT;}
    }
    return MERCS_FACTORY_NONE;
}
static inline void mercs_factory_view_step(mercs_factory_view *v,int direction)
{
    size_t i;if(v->side<0||!v->count[v->side])return;
    i=v->selected;
    if(direction>0&&i+1<v->count[v->side])i++;else if(direction<0&&i)i--;
    (void)mercs_factory_view_select(v,v->side,i);
    if((int)i<v->scroll[v->side])v->scroll[v->side]=(int)i;
    if((int)i>=v->scroll[v->side]+28)v->scroll[v->side]=(int)i-27;
}
static inline void mercs_factory_view_scroll(mercs_factory_view *v,int x,int rows)
{
    int side=x<230?-1:x<440?1:0,max;
    if(side<0){v->detail_scroll+=rows;if(v->detail_scroll<0)v->detail_scroll=0;if(v->detail_scroll>20)v->detail_scroll=20;return;}
    max=(int)v->count[side]-28;if(max<0)max=0;
    v->scroll[side]+=rows;if(v->scroll[side]<0)v->scroll[side]=0;if(v->scroll[side]>max)v->scroll[side]=max;
}
#endif
