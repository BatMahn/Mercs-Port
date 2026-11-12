/* Retail DOS shell Mech Lab, recovered from0x30990..0x36960.
 * Original behavioral checks: tests/fixtures/mechlab/verify_original_mechlab.py.
 * Work is staged as a complete save so cancellation and allocation failures
 * cannot strand an installed item outside the owned inventory.
 */
#include "mercsmechlab.h"
#include "mercscareer_data.h"
#include "mercsdebrief_data.h"
#include "mek.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct mercs_mechlab_session {
    mercs_save_state *document, *entry;
    mercs_career_mech backup;
    size_t index;
    int trim_cursor, pending, component, changed;
};
static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static int32_t r(const uint8_t *p,size_t at)
{ uint32_t v=u32(p+at);return v<=INT32_MAX?(int32_t)v:(int32_t)(-1-(int64_t)(UINT32_MAX-v)); }
static void w(uint8_t *p,size_t at,int32_t value)
{ uint32_t v=(uint32_t)value;p+=at;p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24); }
static uint8_t *current(mercs_mechlab_session *s)
{ return s?mercs_save_list_record_edit(s->document,MERCS_SAVE_OWNED_MECHS,s->index):NULL; }
static int mode(const mercs_mechlab_session *s)
{ return r(mercs_save_header(s->document),0x23bb); }
static int valid_mech(const uint8_t *m)
{
    int i;
    if(!m||r(m,0x200)<1000||r(m,0x200)>10000||r(m,0x20c)<0||
       r(m,0x20c)>=20000||r(m,0x20c)%10000>=79||m[0x84f]>1)return 0;
    for(i=0;i<8;i++)if(r(m,0x760+18*i)<0||r(m,0x760+18*i)>1000||
        r(m,0x764+18*i)<0||r(m,0x764+18*i)>2000||
        r(m,0x768+18*i)<0||r(m,0x768+18*i)>2000||
        r(m,0x76c+18*i)<-1||r(m,0x76c+18*i)>2000)return 0;
    return 1;
}
/* Original0x583ac/0x5849c/0x58558. No CASE weight is included by the
 * original final sum. Equipment records retain this original convention. */
static int totals(uint8_t *m)
{
    int e=r(m,0x20c),tons=r(m,0x200),walk,jump,gyro;
    int64_t total;
    if(!valid_mech(m))return MERCS_MECHLAB_INVALID;
    w(m,0x210,career_engine[e%10000][0]);
    w(m,0x214,career_engine[e%10000][1]/(e>=10000?2:1));
    w(m,0x218,e>=10000?7:0);w(m,0x21c,tons-r(m,0x214));
    walk=r(m,0x210)*100/tons;jump=r(m,0x288);
    if(jump<0||jump>1000)return MERCS_MECHLAB_INVALID;
    if(jump>walk)jump=walk;
    w(m,0x280,walk);w(m,0x284,(walk*3+2)/2);w(m,0x288,jump);
    w(m,0x234,tons<=5500?50:tons<=8500?100:200);w(m,0x238,r(m,0x234)*jump);
    gyro=r(m,0x210);if(gyro%100)gyro+=100-gyro%100;
    w(m,0x220,gyro);w(m,0x224,300);w(m,0x244,tons/(r(m,0x240)?20:10));
    total=(int64_t)r(m,0x214)+gyro+300+r(m,0x22c)+r(m,0x238)+r(m,0x244)+
          r(m,0x250)+r(m,0x260)+r(m,0x268)+r(m,0x270);
    if(total<0||total>INT32_MAX)return MERCS_MECHLAB_INVALID;
    w(m,0x204,(int32_t)total);w(m,0x208,tons-(int32_t)total);return 0;
}
static void heat_count(uint8_t *m)
{
    int n=r(m,0x22c)/100+10-r(m,0x210)/25;
    w(m,0x230,n>0?n:0);
}
/*0x5c580 retains the original negative rear sentinel in the summation.
 * Separate volatile products enforce the DOS53-bit x87 precision. */
static int recalculate(uint8_t *m)
{
    int i,weight=0,ammo=0,armor=0,aw;
    volatile double product;
    if(!valid_mech(m))return MERCS_MECHLAB_INVALID;
    for(i=0;i<14;i++){
        int id=r(m,0x2f0+8*i);
        if(id==-1)continue;
        if(id<0||id/100>=66)return MERCS_MECHLAB_INVALID;
        weight+=career_weapon_weight[id/100];
    }
    for(i=0;i<25;i++)if(r(m,0x28c+4*i)!=-1)ammo+=100;
    for(i=0;i<8;i++)armor+=r(m,0x768+18*i)+r(m,0x76c+18*i);
    product=(double)armor*(r(m,0x24c)?(m[0x84f]?0.05580357142857143:0.052083333333333336):0.0625);
    product=product*100.0;
    if(product<INT32_MIN/2+50||product>INT32_MAX/2-50)return MERCS_MECHLAB_INVALID;
    aw=(int)product;aw=aw*2+50;aw=(aw-aw%100)/2;
    w(m,0x250,aw);w(m,0x260,weight);w(m,0x268,ammo);
    return totals(m);
}
static int armor_capacity(mercs_mechlab_session *s,uint8_t *m)
{
    int i,count=0,cap,weight=r(m,0x250),guard=0;
    volatile double product;
    if(weight<0||weight>100000)return MERCS_MECHLAB_LIMIT;
    product=(double)(weight*16)*(m[0x84f]?1.12:1.2);
    cap=(r(m,0x24c)?(int)product:weight*16)+49;cap/=100;
    for(i=0;i<8;i++)count+=r(m,0x768+18*i)+(r(m,0x76c+18*i)>=0?r(m,0x76c+18*i):0);
    while(count>cap){
        size_t a=0x768+18*s->trim_cursor;
        if(++guard>1000000)return MERCS_MECHLAB_INVALID;
        if(r(m,a)>0){w(m,a,r(m,a)-1);count--;}
        if(count<=cap)break;
        if(r(m,a+4)>0){w(m,a+4,r(m,a+4)-1);count--;}
        s->trim_cursor=(s->trim_cursor+1)%8;
    }
    w(m,0x258,cap);w(m,0x25c,count);return 0;
}
/* DOS0x5c488 resets the quantized armor condition baselines after an
 * allocation edit; the picture condition is not a reference-mech ratio. */
static void armor_condition(uint8_t *m)
{
    int i;
    for(i=0;i<8;i++){
        int front=(r(m,0x760+18*i)+r(m,0x768+18*i))/5;
        int rear=(r(m,0x760+18*i)+r(m,0x76c+18*i))/5;
        if(front<0)front=0;
        if(front>15)front=15;
        if(rear<0)rear=0;
        if(rear>15)rear=15;
        m[0x770+18*i]=(uint8_t)(front|(rear<<4));m[0x771+18*i]|=8;
    }
}
static void dirty(mercs_mechlab_session *s)
{ current(s)[0x84c]=1;s->changed=1; }
static int list_remove(mercs_save_state *s,enum mercs_save_list list,size_t at)
{
    size_t n=mercs_save_list_count(s,list),stride=mercs_save_list_stride(list),i;
    uint8_t *p=NULL;int rc;
    if(at>=n)return MERCS_MECHLAB_INVALID;
    if(n>1){p=malloc((n-1)*stride);if(!p)return MERCS_MECHLAB_NOMEM;}
    for(i=0;i<n;i++)if(i!=at)memcpy(p+(i-(i>at))*stride,mercs_save_list_record(s,list,i),stride);
    rc=mercs_save_set_list(s,list,p,n-1);free(p);return rc?MERCS_MECHLAB_NOMEM:0;
}
static int list_append(mercs_save_state *s,enum mercs_save_list list,const uint8_t *row)
{
    size_t n=mercs_save_list_count(s,list),stride=mercs_save_list_stride(list),i;
    uint8_t *p;int rc;
    if(n>=MERCS_SAVE_MAX_LIST_RECORDS)return MERCS_MECHLAB_LIMIT;
    p=malloc((n+1)*stride);if(!p)return MERCS_MECHLAB_NOMEM;
    for(i=0;i<n;i++)memcpy(p+i*stride,mercs_save_list_record(s,list,i),stride);
    memcpy(p+n*stride,row,stride);rc=mercs_save_set_list(s,list,p,n+1);free(p);
    return rc?MERCS_MECHLAB_NOMEM:0;
}
static int stock(mercs_mechlab_session *s,unsigned type,int id,int consume)
{
    enum mercs_save_list list=type==1?MERCS_SAVE_OWNED_AMMO:MERCS_SAVE_OWNED_WEAPONS;
    size_t i,n=mercs_save_list_count(s->document,list);
    if(mode(s)==2)return 0;
    for(i=0;i<n;i++){
        const uint8_t *p=mercs_save_list_record(s->document,list,i);
        /* Titanium 0x10049802 installs only stock without the salvage
         * damage mark; DOS rows never carry it */
        if(p[8]==type&&r(p,0)==id&&(p[8]==1||r(p,0x16)!=1))
            return consume&&mode(s)==1?list_remove(s->document,list,i):0;
    }
    return MERCS_MECHLAB_NO_ITEM;
}
static int returned(mercs_mechlab_session *s,unsigned type,int id)
{
    uint8_t p[MERCS_SAVE_ITEM_BYTES]={0};int value;
    if(type>2||id<0||(type<2&&id>=66))return MERCS_MECHLAB_INVALID;
    w(p,0,id);p[8]=(uint8_t)type;
    if(type==0){
        value=debrief_weapon_price[id];w(p,0xa,-1);w(p,0xe,0);w(p,0x12,100);
    }else if(type==1){
        value=debrief_ammo_price[id];w(p,0xe,1);w(p,0x12,0);w(p,0x16,100);
    }else{
        value=10000;if(id==1||id==2||id==5||id==6)value=debrief_equipment_price[id];
        if(id==3||id==4)value=50000;
        w(p,0xa,-1);w(p,0xe,-1);w(p,0x12,-1);
    }
    w(p,4,value>0?value:10000);
    return list_append(s->document,type==1?MERCS_SAVE_OWNED_AMMO:MERCS_SAVE_OWNED_WEAPONS,p);
}
static int unique(const uint8_t *m,int base,int divisor,int max)
{
    int instance,i;
    for(instance=1;instance<=max;instance++){
        int used=0;
        for(i=0;i<96;i++)if(r(m,0x368+4*i)==base+instance){used=1;break;}
        if(!used)return base+instance;
    }
    (void)divisor;return 0;
}
static int fill(uint8_t *m,int location,int code,int count)
{
    int i,pass,other=-1;
    if(code/100==34||code/100==65){
        if(location==1)other=4;else if(location==4)other=1;
        else if(location==3)other=5;else if(location==5)other=3;
        else return MERCS_MECHLAB_LOCATION;
    }
    for(pass=0;pass<(other>=0?2:1)&&count;pass++){
        int loc=pass?other:location;
        for(i=0;i<12&&count;i++)if(!r(m,0x368+48*loc+4*i)){
            w(m,0x368+48*loc+4*i,code);count--;
        }
    }
    return count?MERCS_MECHLAB_NO_SPACE:0;
}
static int remove_code(mercs_mechlab_session *s,int code,int selected)
{
    uint8_t *m=current(s);int i,rc=0,kind,id;
    (void)selected;
    if(code<=0)return MERCS_MECHLAB_LIMIT;
    if(code>=7300&&code<8000)return MERCS_MECHLAB_FIXED;
    if(code/10==900||code/10==950){
        if(s->pending){
            /* DOS0x339ec removes the first matching critical in the current
             * location while placement is active, regardless of clicked row. */
            for(i=0;i<12;i++)if(r(m,0x368+48*r(m,0x364)+4*i)==code){
                w(m,0x368+48*r(m,0x364)+4*i,0);s->pending++;break;
            }
            return 0;
        }
        for(i=0;i<96;i++)if(r(m,0x368+4*i)/10==code/10)w(m,0x368+4*i,0);
        w(m,code/10==900?0x240:0x24c,0);return recalculate(m);
    }
    for(i=0;i<96;i++)if(r(m,0x368+4*i)==code)w(m,0x368+4*i,0);
    if(code<7000){
        kind=0;id=code/100;
        for(i=0;i<14;i++)if(r(m,0x2f0+8*i)==code)w(m,0x2f0+8*i,-1);
    }else if(code>=10000){
        kind=1;id=(code-10000)/10000;
        for(i=0;i<25;i++)if(r(m,0x28c+4*i)==code)w(m,0x28c+4*i,-1);
    }else{
        kind=2;
        if(code/100==80){id=r(m,0x228)?2:1;w(m,0x22c,r(m,0x22c)-100);heat_count(m);}
        else if(code/100==85){id=6;w(m,0x288,r(m,0x288)-1);}
        else if(code/10==955){
            id=5;if(m[0x84f])w(m,0x278,r(m,0x278)-50);
            m[0x771+18*r(m,0x364)]&=(uint8_t)~1;
        }else if(code==7000){
            id=10;
            /* The DOS removal leaves stale MASC mass/present metadata. The
             * native editor must release it so a removed MASC can be refit. */
            w(m,0x270,0);w(m,0x7f4,0);
        }else return MERCS_MECHLAB_FIXED;
    }
    rc=returned(s,(unsigned)kind,id);if(rc)return rc;
    return recalculate(current(s));
}
static int add_inner(mercs_mechlab_session *s,unsigned type,int id)
{
    uint8_t *m=current(s);int location=r(m,0x364),i,rc,code=0,crits=1;
    if(type>2||id<0||(type<2&&id>=66)||(type==2&&id>10))return MERCS_MECHLAB_INVALID;
    if(s->pending)return MERCS_MECHLAB_PLACEMENT;
    if(type==2&&(id==0||id==9)){
        if(id==0&&(rc=stock(s,type,id,0)))return rc;
        return id==0?MERCS_MECHLAB_OPEN_ENGINE:MERCS_MECHLAB_OPEN_ARMOR;
    }
    if(location<0||location>=8||!r(m,0x760+18*location))return MERCS_MECHLAB_LOCATION;
    if((rc=stock(s,type,id,0)))return rc;
    if(type==0){
        code=unique(m,id*100,100,99);crits=mek_weapons[id].crits;
        if(!code||crits<1||crits>24)return MERCS_MECHLAB_LIMIT;
        for(i=0;i<14&&r(m,0x2f0+8*i)!=-1;i++);
        if(i==14)return MERCS_MECHLAB_LIMIT;
        w(m,0x2f0+8*i,code);w(m,0x2f4+8*i,-1);
    }else if(type==1){
        code=unique(m,id*10000+10100,100,20);
        for(i=0;i<25&&r(m,0x28c+4*i)!=-1;i++);
        if(!code||i==25)return MERCS_MECHLAB_LIMIT;
        w(m,0x28c+4*i,code);
    }else if(id==3||id==4){
        int count=0;
        if((id==3)==!!r(m,0x228))return MERCS_MECHLAB_HEATSINK_TYPE;
        if(id==3){
            for(i=0;i<96;i++)if(r(m,0x368+4*i)/100==80){w(m,0x368+4*i,0);count++;}
            w(m,0x228,1);
            for(i=0;i<count;i++)if((rc=returned(s,2,2)))return rc;
        }else{
            int j;
            for(i=0;i<96;i++)if(r(m,0x368+4*i)/100==80){
                code=r(m,0x368+4*i);
                for(j=i+1;j<96;j++)if(r(m,0x368+4*j)==code){w(m,0x368+4*j,0);count++;}
            }
            if(!count)return MERCS_MECHLAB_LIMIT;
            w(m,0x228,0);
        }
        dirty(s);return 0;
    }else if(id==7||id==8){
        int free_slots=0;
        if(r(m,id==7?0x24c:0x240))return MERCS_MECHLAB_DUPLICATE;
        crits=m[0x84f]?14:7;
        for(i=0;i<96;i++)if(!r(m,0x368+4*i))free_slots++;
        if(free_slots<crits)return MERCS_MECHLAB_NO_SPACE;
        s->pending=crits;s->component=id==7?9500:9000;dirty(s);return 0;
    }else if(id==1||id==2){
        if((id==2)!=!!r(m,0x228))return MERCS_MECHLAB_HEATSINK_TYPE;
        code=unique(m,8000,100,99);crits=id==1?1:(m[0x84f]?3:2);
        w(m,0x22c,r(m,0x22c)+100);heat_count(m);
    }else if(id==6){
        if(location==0||location==4||location==5)return MERCS_MECHLAB_LOCATION;
        if(r(m,0x288)>=r(m,0x280))return MERCS_MECHLAB_LIMIT;
        code=unique(m,8500,100,99);w(m,0x288,r(m,0x288)+1);
    }else if(id==5){
        for(i=0;i<12;i++)if(r(m,0x368+48*location+4*i)/10==955)return MERCS_MECHLAB_DUPLICATE;
        code=unique(m,9550,10,9);m[0x771+18*location]|=1;w(m,0x7f0,1);
        if(m[0x84f])w(m,0x278,r(m,0x278)+50);
    }else if(id==10){
        if(r(m,0x7f4))return MERCS_MECHLAB_DUPLICATE;
        code=7000;w(m,0x270,r(m,0x808)*100);w(m,0x7f4,1);
    }else return MERCS_MECHLAB_INVALID;
    if(!code)return MERCS_MECHLAB_LIMIT;
    if((rc=fill(m,location,code,crits)))return rc;
    if((rc=stock(s,type,id,1)))return rc;
    if((rc=recalculate(current(s))))return rc;
    dirty(s);return 0;
}
static int adjust_inner(mercs_mechlab_session *s,enum mercs_mechlab_control control,int direction)
{
    uint8_t *m=current(s);int location=r(m,0x364),value,other,cap,used,rc=0,i;
    size_t at;
    if(s->pending)return MERCS_MECHLAB_PLACEMENT;
    if(direction!=-1&&direction!=1)return MERCS_MECHLAB_INVALID;
    if(control==MERCS_MECHLAB_ENGINE){
        value=r(m,0x200)/100*(r(m,0x280)+direction);
        if(value<10||value>400)return MERCS_MECHLAB_LIMIT;
        w(m,0x20c,(value-10)/5+(r(m,0x20c)>=10000?10000:0));
        rc=totals(m);heat_count(m);
    }else if(control==MERCS_MECHLAB_XL){
        if(r(m,0x20c)<10000){
            static const int locations[2]={3,1};
            w(m,0x20c,r(m,0x20c)+10000);
            for(i=0;i<6;i++){
                at=0x368+48*locations[i/3]+4*(i%3);value=r(m,at);
                if(value>0){
                    /* Original engine fitting removes any movable component
                     * sharing those criticals before reserving the side slots. */
                    int selected=r(m,0x364);w(m,0x364,locations[i/3]);
                    rc=remove_code(s,value,i%3);m=current(s);w(m,0x364,selected);
                    if(rc&&rc!=MERCS_MECHLAB_FIXED)return rc;
                }
                w(m,at,7850);
            }
        }else{
            w(m,0x20c,r(m,0x20c)-10000);
            for(i=0;i<12;i++){
                if(r(m,0x398+4*i)==7850)w(m,0x398+4*i,0);
                if(r(m,0x3f8+4*i)==7850)w(m,0x3f8+4*i,0);
            }
        }
        rc=totals(m);
    }else if(control==MERCS_MECHLAB_ARMOR_WEIGHT){
        value=r(m,0x250)+direction*50;
        if(value<0||value>r(m,0x200))return MERCS_MECHLAB_LIMIT;
        w(m,0x250,value);rc=armor_capacity(s,m);if(!rc)rc=totals(m);
    }else if(control==MERCS_MECHLAB_FRONT_ARMOR||control==MERCS_MECHLAB_REAR_ARMOR){
        if(location<0||location>=8)return MERCS_MECHLAB_LOCATION;
        at=0x768+18*location+(control==MERCS_MECHLAB_REAR_ARMOR?4:0);
        value=r(m,at);other=r(m,0x768+18*location+(control==MERCS_MECHLAB_REAR_ARMOR?0:4));
        if(value<0)return MERCS_MECHLAB_LOCATION;
        used=r(m,0x25c);cap=r(m,0x258);
        if(direction<0){if(!value)return MERCS_MECHLAB_LIMIT;w(m,at,value-1);w(m,0x25c,used-1);}
        else if(used<cap&&value+(other>0?other:0)<r(m,0x764+18*location)){
            w(m,at,value+1);w(m,0x25c,used+1);
        }else if(other>0){w(m,at,value+1);w(m,0x768+18*location+(control==MERCS_MECHLAB_REAR_ARMOR?0:4),other-1);}
        else return MERCS_MECHLAB_LIMIT;
    }else return MERCS_MECHLAB_INVALID;
    if(!rc){if(control>=MERCS_MECHLAB_ARMOR_WEIGHT)armor_condition(m);dirty(s);}
    return rc;
}
/* Each public mutation stages a second document. This also rolls back partial
 * ArrowIV placement, inventory OOM and engine replacement as one operation. */
static int edit(mercs_mechlab_session *s,int operation,unsigned type,int value,int direction)
{
    mercs_mechlab_session next;int rc;uint8_t *m;
    if(!s)return MERCS_MECHLAB_INVALID;
    next=*s;next.document=mercs_save_clone(s->document);
    if(!next.document)return MERCS_MECHLAB_NOMEM;
    if(operation==0)rc=add_inner(&next,type,value);
    else if(operation==1){
        m=current(&next);
        if(value<0||value>=12||r(m,0x364)<0||r(m,0x364)>7)rc=MERCS_MECHLAB_INVALID;
        else{rc=remove_code(&next,r(m,0x368+48*r(m,0x364)+4*value),value);if(!rc)dirty(&next);}
    }else rc=adjust_inner(&next,(enum mercs_mechlab_control)value,direction);
    if(!rc){mercs_save_free(s->document);*s=next;}else mercs_save_free(next.document);
    return rc;
}
int mercs_mechlab_begin(const mercs_save_state *source,size_t index,mercs_mechlab_session **out)
{
    mercs_mechlab_session *s;const uint8_t *m;int rc;
    if(!source||!out||!(m=mercs_save_list_record(source,MERCS_SAVE_OWNED_MECHS,index))||!valid_mech(m))return MERCS_MECHLAB_INVALID;
    s=calloc(1,sizeof *s);if(!s)return MERCS_MECHLAB_NOMEM;
    s->document=mercs_save_clone(source);s->entry=mercs_save_clone(source);s->index=index;
    if(!s->document||!s->entry){mercs_mechlab_free(s);return MERCS_MECHLAB_NOMEM;}
    rc=recalculate(current(s));if(rc){mercs_mechlab_free(s);return rc;}
    if(r(current(s),0x364)<0||r(current(s),0x364)>7)w(current(s),0x364,0);
    memcpy(s->backup.bytes,current(s),MERCS_CAREER_MECH_BYTES);*out=s;return 0;
}
int mercs_mechlab_begin_mek(const uint8_t *mek,size_t length,const char *filename,
                             mercs_mechlab_session **out)
{
    mercs_career_mech_pair pair;mercs_save_state *doc;int rc;
    if(!out||mercs_career_pair_init(&pair,mek,length,filename,0,-1))return MERCS_MECHLAB_INVALID;
    doc=mercs_save_create();if(!doc)return MERCS_MECHLAB_NOMEM;
    w(mercs_save_header_edit(doc),0x23bb,2);
    rc=mercs_save_set_owned(doc,&pair,1)?MERCS_MECHLAB_NOMEM:mercs_mechlab_begin(doc,0,out);
    mercs_save_free(doc);return rc;
}
void mercs_mechlab_free(mercs_mechlab_session *s)
{ if(s){mercs_save_free(s->document);mercs_save_free(s->entry);free(s);} }
const mercs_save_state *mercs_mechlab_document(const mercs_mechlab_session *s)
{ return s?s->document:NULL; }
const mercs_career_mech *mercs_mechlab_current(const mercs_mechlab_session *s)
{ return s?(const mercs_career_mech *)mercs_save_list_record(s->document,MERCS_SAVE_OWNED_MECHS,s->index):NULL; }
const mercs_career_mech *mercs_mechlab_backup(const mercs_mechlab_session *s)
{ return s?&s->backup:NULL; }
size_t mercs_mechlab_index(const mercs_mechlab_session *s){return s?s->index:0;}
int mercs_mechlab_dirty(const mercs_mechlab_session *s){return s?s->changed:0;}
int32_t mercs_mechlab_cost(const mercs_mechlab_session *s)
{
    const uint8_t *m;int i,j,critical=0,armor=0;
    if(!s)return 0;
    m=mercs_mechlab_current(s)->bytes;
    for(i=0;i<8;i++){
        int difference=r(s->backup.bytes,0x768+18*i)-r(m,0x768+18*i)+r(s->backup.bytes,0x76c+18*i)-r(m,0x76c+18*i);
        armor+=difference<0?-difference:difference;
        for(j=0;j<12;j++){
            size_t at=0x368+48*i+4*j;int before=r(s->backup.bytes,at),after=r(m,at);
            if(before!=after&&(after!=0||before!=-1))critical++;
        }
    }
    return critical*50000+armor*625;
}
int mercs_mechlab_pending(const mercs_mechlab_session *s,int *component)
{ if(component)*component=s?s->component:0;return s?s->pending:0; }
int mercs_mechlab_select_location(mercs_mechlab_session *s,int location)
{
    if(!s||location<0||location>7)return MERCS_MECHLAB_INVALID;
    w(current(s),0x364,location);return 0;
}
int mercs_mechlab_add(mercs_mechlab_session *s,unsigned type,int id){return edit(s,0,type,id,0);}
int mercs_mechlab_remove(mercs_mechlab_session *s,int slot){return edit(s,1,0,slot,0);}
int mercs_mechlab_adjust(mercs_mechlab_session *s,enum mercs_mechlab_control control,int direction)
{ return edit(s,2,0,(int)control,direction); }
int mercs_mechlab_place(mercs_mechlab_session *s,int location)
{
    uint8_t *m;int i,rc;mercs_mechlab_session next;
    if(!s||location<0||location>7)return MERCS_MECHLAB_INVALID;
    if(!s->pending)return mercs_mechlab_select_location(s,location);
    next=*s;next.document=mercs_save_clone(s->document);if(!next.document)return MERCS_MECHLAB_NOMEM;
    m=current(&next);w(m,0x364,location);
    for(i=0;i<12&&r(m,0x368+48*location+4*i)!=0;i++);
    rc=i==12?MERCS_MECHLAB_NO_SPACE:0;
    if(!rc){
        w(m,0x368+48*location+4*i,next.component);next.pending--;
        if(!next.pending){w(m,next.component==9000?0x240:0x24c,1);rc=recalculate(m);}
    }
    if(!rc){dirty(&next);mercs_save_free(s->document);*s=next;}else mercs_save_free(next.document);
    return rc;
}
int mercs_mechlab_rename(mercs_mechlab_session *s,const char *name)
{
    size_t a=0,b,n,i;uint8_t *m;
    if(!s||!name)return MERCS_MECHLAB_INVALID;
    n=strlen(name);while(a<n&&isspace((unsigned char)name[a]))a++;
    b=n;while(b>a&&isspace((unsigned char)name[b-1]))b--;
    if(b==a||b-a>18)return MERCS_MECHLAB_LIMIT;
    m=current(s);memset(m+0x100,0,256);
    for(i=a;i<b;i++)m[0x100+i-a]=(uint8_t)toupper((unsigned char)name[i]);
    dirty(s);return 0;
}
int mercs_mechlab_rollback(mercs_mechlab_session *s)
{
    mercs_save_state *p;int loc;
    if(!s)return MERCS_MECHLAB_INVALID;
    p=mercs_save_clone(s->entry);if(!p)return MERCS_MECHLAB_NOMEM;
    loc=r(current(s),0x364);mercs_save_free(s->document);s->document=p;
    memcpy(current(s),s->backup.bytes,MERCS_CAREER_MECH_BYTES);w(current(s),0x364,loc);
    s->pending=s->component=s->changed=0;s->trim_cursor=0;return 0;
}
int mercs_mechlab_finish(mercs_mechlab_session *s,int accept,mercs_save_state **out)
{
    mercs_save_state *p;uint8_t *m,*h;size_t i,n,ref=0,found=0;
    int rc,cost;uint8_t copy[MERCS_CAREER_MECH_BYTES];
    if(!s||!out)return MERCS_MECHLAB_INVALID;
    if(!accept){p=mercs_save_clone(s->entry);if(!p)return MERCS_MECHLAB_NOMEM;*out=p;return 0;}
    if(s->pending)return MERCS_MECHLAB_PLACEMENT;
    p=mercs_save_clone(s->document);if(!p)return MERCS_MECHLAB_NOMEM;
    m=mercs_save_list_record_edit(p,MERCS_SAVE_OWNED_MECHS,s->index);h=mercs_save_header_edit(p);
    rc=0;cost=mercs_mechlab_cost(s);
    if(s->changed){
        rc=recalculate(m);
        if(!rc&&r(m,0x204)>r(m,0x200))rc=MERCS_MECHLAB_OVERWEIGHT;
        if(!rc&&mode(s)==1&&r(h,0x12a)<cost)rc=MERCS_MECHLAB_FUNDS;
        n=mercs_save_list_count(p,MERCS_SAVE_OWNED_REFERENCES);
        for(i=0;i<n;i++)if(r(mercs_save_list_record(p,MERCS_SAVE_OWNED_REFERENCES,i),0x810)==r(m,0x810)){ref=i;found++;}
        if(!rc&&found!=1)rc=MERCS_MECHLAB_INVALID;
        if(!rc){
            w(m,0x814,1);memcpy(copy,m,sizeof copy);
            rc=list_remove(p,MERCS_SAVE_OWNED_REFERENCES,ref);
            if(!rc)rc=list_append(p,MERCS_SAVE_OWNED_REFERENCES,copy);
            if(!rc&&mercs_save_windows(p))
                rc=mercs_save_set_mech_extension(p,MERCS_SAVE_OWNED_REFERENCES,n-1,
                       mercs_save_mech_extension(p,MERCS_SAVE_OWNED_MECHS,s->index));
            if(!rc){
                if(mode(s)==1)w(h,0x12a,r(h,0x12a)-cost);
                h[0]=1;m[0x84c]=0;
            }
        }
    }
    if(rc){mercs_save_free(p);return rc;}*out=p;return 0;
}
int mercs_mechlab_finish_mek(mercs_mechlab_session *s,int accept,uint8_t *out,size_t capacity,size_t *written)
{
    mercs_save_state *doc=NULL;uint8_t bytes[MERCS_CAREER_DEPLOY_MAX_BYTES];size_t n;
    mercs_career_mech mech;int rc;
    if(!s||!out||!written||mode(s)!=2)return MERCS_MECHLAB_INVALID;
    rc=mercs_mechlab_finish(s,accept,&doc);if(rc)return rc;
    memcpy(mech.bytes,mercs_save_list_record(doc,MERCS_SAVE_OWNED_MECHS,s->index),sizeof mech.bytes);
    mercs_save_free(doc);
    if(mercs_career_deployment_bytes(&mech,bytes,sizeof bytes,&n))return MERCS_MECHLAB_INVALID;
    if(n>capacity)return MERCS_MECHLAB_LIMIT;
    memcpy(out,bytes,n);*written=n;return 0;
}

/* Shared with the Titanium Salvage Yard (mercssalvage.c): the original weight
 * recalculation (0x5c580) and the armor condition baselines (0x5c488 /
 * Titanium 0x1004b041). Both act on a bare 0x850-byte career mech. */
int mercs_mechlab_recalculate(uint8_t *mech){return recalculate(mech);}
void mercs_mechlab_armor_condition(uint8_t *mech){if(mech)armor_condition(mech);}

