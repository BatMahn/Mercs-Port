/* mercssalvage.c - see mercssalvage.h. Titanium MW2SHELL.DLL (16-bit set) and
 * mercsw.dll addresses are given for each rule. */
#include "mercssalvage.h"
#include "mercsmechlab.h"
#include "mercsdebrief.h"
#include "mercsdebrief_data.h"
#include "mek.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void p32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static int32_t r(const uint8_t *m,size_t at){return (int32_t)u32(m+at);}
static void w(uint8_t *m,size_t at,int32_t v){p32(m+at,(uint32_t)v);}

/* The debrief gate's per-mission table (campaign mode, the scene's three-letter
 * prefix and number): 1 = salvage opportunity. */
int mercs_salvage_allowed(const char *scene,int rights)
{
    static const struct { const char *p; int all; signed char n[8]; } T[]={
        {"bet",-1,{0,1,0,1,1,1}},{"gra",-1,{0,1,1,1,0,1}},{"iva",-1,{0,1,0,0,0,0}},
        {"lut",-1,{0,1,1,0,1,1,1}},{"mcc",-1,{0,0,0,1,1,0,1,1}},{"tra",-1,{0,0,0,0,1}},
        {"spa",1,{0}},{"wol",1,{0}},{"big",1,{0}},{"buz",0,{0}},{"dom",0,{0}},{"dou",1,{0}},
        {"dow",1,{0}},{"med",1,{0}},{"mou",1,{0}},{"myb",1,{0}},{"out",1,{0}},{"rec",0,{0}},
        {"rev",1,{0}},{"rog",1,{0}},{"sur",1,{0}},{"wou",0,{0}},{"ran",0,{0}},{"sol",1,{0}}
    };
    size_t i;int number;
    if(rights)return 1;
    if(!scene||strlen(scene)<4)return 0;
    if(mercs_salvage_is_out3(scene))return 1;
    number=scene[3]-'0';
    for(i=0;i<sizeof T/sizeof *T;i++){
        if(strncasecmp(scene,T[i].p,3))continue;
        if(T[i].all>=0)return T[i].all;
        return number>=0&&number<8?T[i].n[number]:0;
    }
    return 0;
}

int mercs_salvage_is_out3(const char *scene)
{ return scene&&!strncasecmp(scene,"out",3)&&scene[3]=='3'; }

int mercs_salvage_category(int32_t code)
{
    int d=code/100,t;
    if(d>=1&&d<=0x41){
        if(d>0x22)d-=0x23;
        if((d>=0&&d<10)||(d>0x1e&&d<0x23))return 0;
        if(d>9&&d<0x14)return 2;
        if(d>0x13&&d<0x1c)return 1;
    }else if(d>99)return 4;
    t=code/10*10;
    if(t==8000||t==8500||t==9550)return 3;
    return -1;
}

int mercs_salvage_condition(const uint8_t *m,int loc)
{
    const uint8_t *p=m+0x760+18*loc;
    int internal=r(p,0),front=r(p,8),rear=r(p,12),fm=p[0x10]&15,rm=(p[0x10]>>4)&15,a=0,b=0,worst;
    int in=internal<0?0:internal,f=front<0?0:front,rr=rear<0?0:rear;
    if(rm)b=15-(int)(((int64_t)(in+rr)*0x10000*3)/((int64_t)rm<<16));
    if(b<1)b=0;else if(b>15)b=15;
    if(fm)a=15-(int)(((int64_t)(in+f)*0x10000*3)/((int64_t)fm<<16));
    if(a<1)a=0;else if(a>15)a=15;
    worst=a>b?a:b;
    if(front<1&&rear<1&&internal<1)return 3;
    if(worst>=12)return 2;
    return worst>=1?1:0;
}

int mercs_salvage_whole(const uint8_t *m,const int32_t rnd[96],int index)
{
    int n[4]={0,0,0,0},l,p=0,a,b,c,d;
    if(mercs_salvage_condition(m,2)==3)return 0;
    for(l=0;l<8;l++)n[mercs_salvage_condition(m,l)]++;
    a=n[0];b=n[1];c=n[2];d=n[3];
    if(a>=7)return 1;
    if((b==1&&a>5)||(b<5&&a>3&&b+a>5)||(b<6&&a>1&&b+a>5))return 1;
    if(c==1&&a+b>5)p=90;
    else if(b>=1&&a>=6)p=90;
    else if(b>=2&&b<=3&&a>=4)p=80;
    else if(b>=4&&b<=5&&a>=4)p=70;
    else if(b>=6)p=60;
    else if(c==1&&a+b>4)p=50;
    else if(c>=2&&c<=3&&a+b>=4)p=40;
    else if(c>=4&&c<=5&&a+b>=4)p=30;
    else if(d==1||c>6)p=30;
    else if(d==2)p=30;
    else if(d>2)p=5;
    /* 0x1006729d indexes the wreck's own draw block at the wreck's index */
    return index>=0&&index<96&&rnd[index]%100<p;
}

int mercs_salvage_item_slots(int32_t code)
{
    int c=mercs_salvage_category(code);
    if(c>=0&&c<=2&&code/100<mek_weapon_count)return mek_weapons[code/100].crits;
    return 1;
}

void mercs_salvage_item_name(int32_t code,char *out,size_t cap)
{
    /* ammunition: 10000 + its weapon's code (type x 100 + instance) x 100 + n;
     * the name is the weapon type's, as 0x1004a6fe derives it */
    if(code>=10000&&(code-10000)/10000<mek_weapon_count)snprintf(out,cap,"%s Ammo",mek_weapons[(code-10000)/10000].name);
    else if(code>0&&code<7000&&code/100<mek_weapon_count)snprintf(out,cap,"%s",mek_weapons[code/100].name);
    else if(code/10*10==8000)snprintf(out,cap,"Heat Sink");
    else if(code/10*10==8500)snprintf(out,cap,"Jump Jet");
    else if(code/10*10==9550)snprintf(out,cap,"CASE");
    else snprintf(out,cap,"-");
}

void mercs_salvage_row_name(unsigned kind,int32_t id,char *out,size_t cap)
{
    static const char *const EQ[7]={"Engine","Heat Sink","Double Heat Sink","Endo Steel","Ferro-Fibrous","CASE","Jump Jet"};
    if(kind==0&&id>=0&&id<mek_weapon_count)snprintf(out,cap,"%s",mek_weapons[id].name);
    else if(kind==1&&id>=0&&id<mek_weapon_count)snprintf(out,cap,"%s Ammo",mek_weapons[id].name);
    else if(kind==2&&id>=0&&id<7)snprintf(out,cap,"%s",EQ[id]);
    else snprintf(out,cap,"-");
}

int mercs_salvage_row_category(unsigned kind,int32_t id)
{
    if(kind==2)return id==0||id==1||id==5||id==6||id==10?3:-1;
    if(kind==1)return id>0&&id<0x42?4:-1;
    if(kind!=0)return -1;
    if(id>=1&&id<=0x41){
        int d=id>0x22?id-0x23:id;
        if((d>=0&&d<10)||(d>0x1e&&d<0x23))return 0;
        if(d>9&&d<0x14)return 2;
        if(d>0x13&&d<0x1c)return 1;
    }else if(id>99)return 4;
    return -1;
}

/* ---- building the wrecks (0x100675e8) ---- */

static int slots_in(int loc){return loc==0||loc==6||loc==7?6:12;}

static void rebuild_lists(uint8_t *m)
{
    int l,j,k,nw=0,na=0;
    for(k=0;k<14;k++){w(m,0x2f0+8*k,-1);w(m,0x2f4+8*k,-1);}
    for(k=0;k<25;k++)w(m,0x28c+4*k,-1);
    for(l=0;l<8;l++)for(j=0;j<slots_in(l);j++){
        int32_t id=r(m,0x368+48*l+4*j),c=mercs_salvage_category(id),seen=0;
        if(c>=0&&c<=2){
            for(k=0;k<14;k++)if(r(m,0x2f0+8*k)==id)seen=1;
            if(!seen&&nw<14){w(m,0x2f4+8*nw,l);w(m,0x2f0+8*nw,id);nw++;}
        }else if(c==4){
            for(k=0;k<25;k++)if(r(m,0x28c+4*k)==id)seen=1;
            if(!seen&&na<25){w(m,0x28c+4*na,id);na++;}
        }
    }
}

static int load_stock(prj_archive *a,const char *name,mercs_career_mech_pair *pair,char resource[16])
{
    prj_record rec;int rc;char file[32];
    snprintf(resource,16,"%.5sSTD",name);
    if(prj_read_named(a,"MEK",resource,&rec)!=PRJ_OK){
        /* INFERRED: a name whose own STD design is absent falls back to the
         * chassis' 00 variant; Titanium's loader would skip the wreck. */
        snprintf(resource,16,"%.3s00STD",name);
        if(prj_read_named(a,"MEK",resource,&rec)!=PRJ_OK)return MERCS_SALVAGE_MISSING;
    }
    snprintf(file,sizeof file,"%.5sMER.MEK",resource);
    rc=mercs_career_pair_init(pair,rec.data,rec.size,file,0,-1);
    prj_record_free(&rec);
    return rc?MERCS_SALVAGE_MISSING:MERCS_SALVAGE_OK;
}

static void roll_components(mercs_salvage_entry *e,const int32_t rnd[96])
{
    uint8_t *m=e->current.bytes;const uint8_t *ref=e->listing.bytes;
    int l,j,l2,j2;
    for(l=0;l<8;l++)for(j=0;j<12;j++)e->status[l][j]=-1;
    for(l=0;l<8;l++){
        int cond=mercs_salvage_condition(m,l),n=slots_in(l);
        for(j=0;j<n;j++){
            int32_t id=r(ref,0x368+48*l+4*j);
            int c=id==-1?-1:mercs_salvage_category(id/100*100);
            int roll,dmg_ok;
            if(id==-1||c==-1||e->status[l][j]!=-1){
                if(cond==3)w(m,0x368+48*l+4*j,0);
                continue;
            }
            roll=rnd[l*8+j]%100;
            dmg_ok=mercs_salvage_category(id)!=4;
            switch(cond){
            case 0:
                if(roll>=0x38)e->status[l][j]=0;
                else if(roll>=0x15&&roll<=0x36)e->status[l][j]=dmg_ok?1:0;
                else if(roll<0x15){w(m,0x368+48*l+4*j,0);e->status[l][j]=2;}
                break;
            case 1:
                if(roll>=0x42)e->status[l][j]=0;
                else if(roll>=0x20&&roll<=0x40)e->status[l][j]=dmg_ok?1:0;
                else if(roll<0x1e){w(m,0x368+48*l+4*j,0);e->status[l][j]=2;}
                break;
            case 2:
                if(roll>=0x60)e->status[l][j]=0;
                else if(roll>=0x2e&&roll<=0x5e)e->status[l][j]=dmg_ok?1:0;
                else{w(m,0x368+48*l+4*j,0);e->status[l][j]=2;}
                break;
            default:
                w(m,0x368+48*l+4*j,0);e->status[l][j]=2;
            }
            /* every other critical slot of the same item shares the result */
            for(l2=0;l2<8;l2++)for(j2=0;j2<12;j2++){
                if((l2==l&&j2==j)||r(m,0x368+48*l2+4*j2)!=id)continue;
                e->status[l2][j2]=e->status[l][j];
                w(m,0x368+48*l2+4*j2,r(m,0x368+48*l+4*j));
            }
        }
        for(j=n;j<12;j++){w(m,0x368+48*l+4*j,-1);w(e->listing.bytes,0x368+48*l+4*j,-1);}
    }
    for(l=0;l<8;l++)if(mercs_salvage_condition(m,l)==3)for(j=0;j<12;j++)w(m,0x368+48*l+4*j,0);
    rebuild_lists(m);rebuild_lists(e->listing.bytes);
    mercs_mechlab_recalculate(m);
}

static int add_wreck(mercs_salvage_yard *y,prj_archive *a,const mercs_salvage_wreck *wr,size_t index,int out3)
{
    mercs_salvage_entry *e=y->entries+y->count;mercs_career_mech_pair pair;char resource[16];int l;
    memset(e,0,sizeof *e);
    if(load_stock(a,out3?"KDK00":wr->name,&pair,resource))return MERCS_SALVAGE_MISSING;
    mercs_mechlab_armor_condition(pair.current.bytes);   /* 0x1004b041 on the stock copy */
    e->current=pair.current;e->listing=pair.current;
    if(out3){
        /* OUT3: the Kodiak comes whole and undamaged (0x100675e8, local_674) */
        rebuild_lists(e->current.bytes);rebuild_lists(e->listing.bytes);
        for(l=0;l<8;l++){int j;for(j=0;j<12;j++)e->status[l][j]=-1;}
        e->whole=1;y->count++;return MERCS_SALVAGE_OK;
    }
    for(l=0;l<8;l++){
        w(e->current.bytes,0x760+18*l,wr->internal[l]);
        w(e->current.bytes,0x768+18*l,wr->front[l]);
        w(e->current.bytes,0x76c+18*l,l>=1&&l<=3?wr->rear[l]:-1);
    }
    if(wr->title[0]){size_t n=strnlen(wr->title,MERCS_SALVAGE_TITLE_BYTES);
        memset(e->current.bytes,0,0x100);memcpy(e->current.bytes,wr->title,n);}
    roll_components(e,wr->rnd);
    e->whole=mercs_salvage_whole(e->current.bytes,wr->rnd,(int)index);
    y->count++;return MERCS_SALVAGE_OK;
}

void mercs_salvage_yard_free(mercs_salvage_yard *y)
{
    if(!y)return;
    free(y->entries);free(y->discards);mercs_save_free(y->document);
    memset(y,0,sizeof *y);
}

int mercs_salvage_yard_open(mercs_salvage_yard *y,prj_archive *a,const mercs_salvage_wreck *wr,size_t n,
                            int out3,const mercs_save_state *doc)
{
    size_t i,owned;
    if(!y||!a||!doc||(n&&!wr)||n>MERCS_SALVAGE_MAX_WRECKS)return MERCS_SALVAGE_INVALID;
    memset(y,0,sizeof *y);
    owned=mercs_save_list_count(doc,MERCS_SAVE_OWNED_MECHS);
    /* room for every wreck, the Kodiak and every owned mech put back */
    y->entries=calloc(n+1+owned+1,sizeof *y->entries);
    y->document=mercs_save_clone(doc);
    if(!y->entries||!y->document){mercs_salvage_yard_free(y);return MERCS_SALVAGE_NOMEM;}
    for(i=0;i<n;i++)add_wreck(y,a,wr+i,i,0);
    if(out3)add_wreck(y,a,NULL,0,1);
    if(!y->count){mercs_salvage_yard_free(y);return MERCS_SALVAGE_MISSING;}
    return MERCS_SALVAGE_OK;
}

/* ---- the hold ---- */

static int list_append(mercs_save_state *s,enum mercs_save_list list,const uint8_t *row)
{
    size_t n=mercs_save_list_count(s,list),st=mercs_save_list_stride(list),i;uint8_t *p;int rc;
    if(n>=MERCS_SAVE_MAX_LIST_RECORDS)return MERCS_SALVAGE_FULL;
    p=malloc((n+1)*st);if(!p)return MERCS_SALVAGE_NOMEM;
    for(i=0;i<n;i++)memcpy(p+i*st,mercs_save_list_record(s,list,i),st);
    memcpy(p+n*st,row,st);rc=mercs_save_set_list(s,list,p,n+1);free(p);
    return rc?MERCS_SALVAGE_NOMEM:MERCS_SALVAGE_OK;
}

static int list_remove(mercs_save_state *s,enum mercs_save_list list,size_t at)
{
    size_t n=mercs_save_list_count(s,list),st=mercs_save_list_stride(list),i,k=0;uint8_t *p;int rc;
    if(at>=n)return MERCS_SALVAGE_INVALID;
    p=malloc(n*st+1);if(!p)return MERCS_SALVAGE_NOMEM;
    for(i=0;i<n;i++)if(i!=at)memcpy(p+(k++)*st,mercs_save_list_record(s,list,i),st);
    rc=mercs_save_set_list(s,list,p,k);free(p);
    return rc?MERCS_SALVAGE_NOMEM:MERCS_SALVAGE_OK;
}

int mercs_salvage_hold_used(const mercs_salvage_yard *y)
{
    size_t i,n;int used=0;
    if(!y||!y->document)return 0;
    n=mercs_save_list_count(y->document,MERCS_SAVE_OWNED_WEAPONS);
    for(i=0;i<n;i++){
        const uint8_t *p=mercs_save_list_record(y->document,MERCS_SAVE_OWNED_WEAPONS,i);
        int32_t id=(int32_t)u32(p);
        /* 0x10061c29: a weapon row its critical slots, anything else one */
        used+=p[8]==0&&id>=0&&id<mek_weapon_count&&mercs_salvage_category(id*100)>=0&&mercs_salvage_category(id*100)<=2?
              mek_weapons[id].crits:1;
    }
    return used+(int)mercs_save_list_count(y->document,MERCS_SAVE_OWNED_AMMO);
}

/* An owned inventory row as the port's Mech Lab returns one (mercsmechlab.c
 * returned()); Titanium's 0x10064537 builds the same kinds. The damaged flag is
 * Titanium's +0x16 (weapons and equipment; ammunition is never damaged). */
static void make_row(uint8_t p[MERCS_SAVE_ITEM_BYTES],unsigned kind,int32_t id,int damaged)
{
    int32_t value;
    memset(p,0,MERCS_SAVE_ITEM_BYTES);p32(p,(uint32_t)id);p[8]=(uint8_t)kind;
    if(kind==0){value=id>=0&&id<66?debrief_weapon_price[id]:0;w(p,0xa,-1);w(p,0xe,0);w(p,0x12,100);w(p,0x16,damaged?1:0);}
    else if(kind==1){value=id>=0&&id<66?debrief_ammo_price[id]:0;w(p,0xe,1);w(p,0x12,0);w(p,0x16,100);}
    else{value=10000;if(id==1||id==2||id==5||id==6)value=debrief_equipment_price[id];
         if(id==3||id==4)value=50000;
         w(p,0xa,-1);w(p,0xe,-1);w(p,0x12,-1);w(p,0x16,damaged?1:0);}
    p32(p+4,(uint32_t)(value>0?value:10000));
}

static int row_damaged(const uint8_t *p){return p[8]!=1&&u32(p+0x16)==1;}

int mercs_salvage_take_component(mercs_salvage_yard *y,size_t entry,int loc,int slot)
{
    mercs_salvage_entry *e;int32_t id,t;int count=0,damaged=0,l,j,rc;unsigned kind;int32_t row_id;
    uint8_t row[MERCS_SAVE_ITEM_BYTES];
    if(!y||entry>=y->count||loc<0||loc>7||slot<0||slot>11)return MERCS_SALVAGE_INVALID;
    e=y->entries+entry;
    id=r(e->listing.bytes,0x368+48*loc+4*slot);
    if(id<=0||mercs_salvage_category(id)<0||mercs_salvage_condition(e->current.bytes,loc)==3)return MERCS_SALVAGE_INVALID;
    if(r(e->current.bytes,0x368+48*loc+4*slot)!=id)return MERCS_SALVAGE_INVALID;   /* destroyed or taken */
    if(mercs_salvage_hold_used(y)+mercs_salvage_item_slots(id)>=MERCS_SALVAGE_HOLD_SLOTS+1)return MERCS_SALVAGE_FULL;
    /* 0x100652b8's per-category limit (18) counts a table it never fills: it
     * cannot trigger, so it is not applied. */
    t=id/10*10;
    if((t>=7000&&t<=7900)||t==9000||t==9500)return MERCS_SALVAGE_REFUSED;
    for(l=0;l<8;l++)for(j=0;j<12;j++){
        if(r(e->listing.bytes,0x368+48*l+4*j)==id)count++;
        if(r(e->current.bytes,0x368+48*l+4*j)==id&&e->status[l][j]==1)damaged=1;
    }
    if(id<7000){kind=0;row_id=id/100;}
    else if(id<10000){kind=2;row_id=t==8000?(count==1?1:2):t==8500?6:5;}
    /* Ammo bins carry 10000 + weapon code (type x 100 + instance) x 100 + n
     * (MEK loader 0x10047700 / 0x1004839b); Titanium's item-type helper
     * 0x1004a6fe takes (code-10000)/10000 as the ammo row's weapon type.
     * Titanium's salvage copy 0x10064537 uses (code-10000)/100, which yields
     * the weapon code (e.g. 401), past its 66-entry weapon table: the port
     * uses the type, as every other Titanium ammo path does. Salvaged ammo
     * is never marked damaged (+0x16 = 0). */
    else{kind=1;row_id=(id-10000)/10000;damaged=0;}
    make_row(row,kind,row_id,damaged);
    rc=list_append(y->document,kind==1?MERCS_SAVE_OWNED_AMMO:MERCS_SAVE_OWNED_WEAPONS,row);
    if(rc)return rc;
    for(l=0;l<8;l++)for(j=0;j<12;j++){
        if(r(e->listing.bytes,0x368+48*l+4*j)==id)w(e->listing.bytes,0x368+48*l+4*j,0);
        if(r(e->current.bytes,0x368+48*l+4*j)==id)w(e->current.bytes,0x368+48*l+4*j,0);
    }
    rebuild_lists(e->current.bytes);mercs_mechlab_recalculate(e->current.bytes);
    return MERCS_SALVAGE_OK;
}

/* 0x1006581f: the first row of the item, but a damaged one if any follows. */
int mercs_salvage_leave_item(mercs_salvage_yard *y,unsigned kind,int32_t id)
{
    enum mercs_save_list list=kind==1?MERCS_SAVE_OWNED_AMMO:MERCS_SAVE_OWNED_WEAPONS;
    size_t i,n,at=SIZE_MAX;void *grown;
    if(!y||kind>2)return MERCS_SALVAGE_INVALID;
    n=mercs_save_list_count(y->document,list);
    for(i=0;i<n;i++){
        const uint8_t *p=mercs_save_list_record(y->document,list,i);
        if(p[8]!=kind||(int32_t)u32(p)!=id)continue;
        if(at==SIZE_MAX)at=i;
        else if(row_damaged(p)){at=i;break;}
    }
    if(at==SIZE_MAX)return MERCS_SALVAGE_INVALID;
    grown=realloc(y->discards,(y->discard_count+1)*sizeof *y->discards);
    if(!grown)return MERCS_SALVAGE_NOMEM;
    y->discards=grown;
    memcpy(y->discards[y->discard_count],mercs_save_list_record(y->document,list,at),MERCS_SAVE_ITEM_BYTES);
    if(list_remove(y->document,list,at))return MERCS_SALVAGE_NOMEM;
    y->discard_count++;return MERCS_SALVAGE_OK;
}

/* 0x1006565a: the first discarded row of the item, but an intact one if any follows. */
int mercs_salvage_take_discard(mercs_salvage_yard *y,unsigned kind,int32_t id)
{
    size_t i,at=SIZE_MAX;int rc;
    if(!y||kind>2)return MERCS_SALVAGE_INVALID;
    for(i=0;i<y->discard_count;i++){
        const uint8_t *p=y->discards[i];
        if(p[8]!=kind||(int32_t)u32(p)!=id)continue;
        if(at==SIZE_MAX)at=i;
        else if(!row_damaged(p)){at=i;break;}
    }
    if(at==SIZE_MAX)return MERCS_SALVAGE_INVALID;
    rc=list_append(y->document,kind==1?MERCS_SAVE_OWNED_AMMO:MERCS_SAVE_OWNED_WEAPONS,y->discards[at]);
    if(rc)return rc;
    memmove(y->discards+at,y->discards+at+1,(y->discard_count-at-1)*sizeof *y->discards);
    y->discard_count--;return MERCS_SALVAGE_OK;
}

int mercs_salvage_row_damaged(const uint8_t *row){return row?row_damaged(row):0;}

static int owned_pairs(const mercs_save_state *s,mercs_career_mech_pair **out,size_t *n)
{
    size_t i,c=mercs_save_list_count(s,MERCS_SAVE_OWNED_MECHS);
    mercs_career_mech_pair *p=calloc(c+1,sizeof *p);
    if(!p)return MERCS_SALVAGE_NOMEM;
    for(i=0;i<c;i++){
        memcpy(p[i].current.bytes,mercs_save_list_record(s,MERCS_SAVE_OWNED_MECHS,i),sizeof p[i].current.bytes);
        memcpy(p[i].reference.bytes,mercs_save_list_record(s,MERCS_SAVE_OWNED_REFERENCES,i),sizeof p[i].reference.bytes);
    }
    *out=p;*n=c;return MERCS_SALVAGE_OK;
}

int mercs_salvage_recover(mercs_salvage_yard *y,prj_archive *a,size_t entry)
{
    mercs_salvage_entry *e;mercs_career_mech_pair *pairs,pair,stock;size_t n,i;int number,rc;
    char name[32],existing[32],resource[16];uint8_t *h;uint32_t id;
    if(!y||entry>=y->count)return MERCS_SALVAGE_INVALID;
    e=y->entries+entry;
    if(!e->whole)return MERCS_SALVAGE_REFUSED;
    if(owned_pairs(y->document,&pairs,&n))return MERCS_SALVAGE_NOMEM;
    if(n+1>=MERCS_SALVAGE_MECH_LIMIT+1){free(pairs);return MERCS_SALVAGE_MECHS_FULL;}
    if(e->owned)pair=e->pair;
    else{
        /* As the port's debrief salvage transfer: the next free NN in
         * XXXnnMER.MEK, the stock reference and a fresh identity. */
        mercs_career_mech candidate=e->current;
        if(mercs_career_filename(&e->current,name,sizeof name)){free(pairs);return MERCS_SALVAGE_INVALID;}
        for(number=0;number<100;number++){
            for(i=0;i<n;i++){
                if(mercs_career_filename(&pairs[i].current,existing,sizeof existing))continue;
                if(isdigit((unsigned char)existing[3])&&isdigit((unsigned char)existing[4])&&
                   (existing[3]-'0')*10+existing[4]-'0'==number)break;
            }
            if(i==n)break;
        }
        if(number==100){free(pairs);return MERCS_SALVAGE_FULL;}
        if(load_stock(a,name,&stock,resource)){free(pairs);return MERCS_SALVAGE_MISSING;}
        snprintf(name+3,sizeof name-3,"%02dMER.MEK",number);
        h=mercs_save_header_edit(y->document);id=u32(h+0x15e);
        p32(stock.current.bytes+0x810,id);p32(stock.reference.bytes+0x810,id);
        candidate.bytes[0x84e]=1;
        rc=mercs_debrief_salvage_pair(&candidate,&stock.current,name,&pair);
        if(rc){free(pairs);return MERCS_SALVAGE_INVALID;}
        p32(pair.current.bytes+0x810,id);p32(pair.reference.bytes+0x810,id);
        pair.current.bytes[0x84e]=0;
        p32(pair.current.bytes+0x80c,UINT32_MAX);p32(pair.reference.bytes+0x80c,UINT32_MAX);
        p32(h+0x15e,id+1);
    }
    pairs[n++]=pair;
    rc=mercs_save_set_owned(y->document,pairs,n);free(pairs);
    if(rc)return MERCS_SALVAGE_NOMEM;
    memmove(y->entries+entry,y->entries+entry+1,(y->count-entry-1)*sizeof *y->entries);
    y->count--;return MERCS_SALVAGE_OK;
}

int mercs_salvage_discard_owned(mercs_salvage_yard *y,size_t index)
{
    mercs_career_mech_pair *pairs;size_t n,i;mercs_salvage_entry *e;uint32_t mid;int rc;
    if(!y||owned_pairs(y->document,&pairs,&n))return MERCS_SALVAGE_NOMEM;
    if(index>=n){free(pairs);return MERCS_SALVAGE_INVALID;}
    if(n==1){free(pairs);return MERCS_SALVAGE_LAST_MECH;}
    e=y->entries+y->count;memset(e,0,sizeof *e);
    e->pair=pairs[index];e->owned=1;e->whole=1;
    e->current=pairs[index].current;e->listing=pairs[index].current;
    for(i=0;i<8;i++){int j;for(j=0;j<12;j++)e->status[i][j]=-1;}
    mid=u32(pairs[index].current.bytes+0x810);
    p32(e->pair.current.bytes+0x80c,UINT32_MAX);p32(e->pair.reference.bytes+0x80c,UINT32_MAX);
    for(i=0;i<mercs_save_list_count(y->document,MERCS_SAVE_OWNED_PILOTS);i++){
        uint8_t *pilot=mercs_save_list_record_edit(y->document,MERCS_SAVE_OWNED_PILOTS,i);
        if(u32(pilot+0x20)==mid)p32(pilot+0x20,UINT32_MAX);
    }
    memmove(pairs+index,pairs+index+1,(n-index-1)*sizeof *pairs);
    rc=mercs_save_set_owned(y->document,pairs,n-1);free(pairs);
    if(rc)return MERCS_SALVAGE_NOMEM;
    y->count++;return MERCS_SALVAGE_OK;
}
