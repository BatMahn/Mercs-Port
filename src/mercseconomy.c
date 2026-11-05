#include "mercseconomy.h"
#include "mercsdebrief.h"
#include <stdlib.h>
#include <string.h>

static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static int32_t s32(uint32_t v)
{ return v<=INT32_MAX?(int32_t)v:(int32_t)(-1-(int64_t)(UINT32_MAX-v)); }
static void p32(uint8_t *p,uint32_t v)
{ p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24); }

/* Only used on a private clone. Source order and destination append order
 * match the original pointer lists; records are never regenerated. */
static int move_row(mercs_save_state *s,enum mercs_save_list from,
                     enum mercs_save_list to,size_t index)
{
    size_t nf=mercs_save_list_count(s,from),nt=mercs_save_list_count(s,to);
    size_t stride=mercs_save_list_stride(from),i;
    uint8_t *a=NULL,*b=NULL;
    uint8_t extension[3];const uint8_t *extra;
    int rc=MERCS_ECONOMY_NOMEM,has_extra;
    if(index>=nf||stride!=mercs_save_list_stride(to))return MERCS_ECONOMY_INVALID;
    if(nt>=MERCS_SAVE_MAX_LIST_RECORDS)return MERCS_ECONOMY_CAPACITY;
    if(nf>1){a=malloc((nf-1)*stride);if(!a)return rc;}
    b=malloc((nt+1)*stride);if(!b){free(a);return rc;}
    for(i=0;i<nf;i++)if(i!=index)
        memcpy(a+(i-(i>index))*stride,mercs_save_list_record(s,from,i),stride);
    for(i=0;i<nt;i++)memcpy(b+i*stride,mercs_save_list_record(s,to,i),stride);
    memcpy(b+nt*stride,mercs_save_list_record(s,from,index),stride);
    extra=mercs_save_mech_extension(s,from,index);has_extra=extra!=NULL;if(extra)memcpy(extension,extra,3);
    if(!mercs_save_set_list(s,from,a,nf-1)&&!mercs_save_set_list(s,to,b,nt+1)){
        rc=has_extra?mercs_save_set_mech_extension(s,to,nt,extension):0;
    }
    free(a);free(b);return rc;
}

int mercs_economy_item_price(const uint8_t item[MERCS_SAVE_ITEM_BYTES],int buy,int32_t *out)
{
    int32_t value;
    double marked;
    if(!item||!out||(buy!=0&&buy!=1)||item[8]>2)return MERCS_ECONOMY_INVALID;
    value=s32(u32(item+4));
    if(!buy){*out=value;return 0;}
    /* Original x87 uses the binary64 constant 1.1 and truncates toward zero.
     * The invalid-integer result is 0x80000000, not a C overflowing cast. */
    marked=(double)value*1.1;
    *out=marked>=2147483648.0||marked< -2147483648.0?INT32_MIN:(int32_t)marked;
    return 0;
}

int mercs_economy_trade_item(mercs_save_state *state,int ammo,int buy,size_t index)
{
    enum mercs_save_list shop,owned,from,to;
    const uint8_t *row,*h;
    mercs_save_state *s;
    int32_t price;
    int rc;
    if(!state||(ammo!=0&&ammo!=1)||(buy!=0&&buy!=1))return MERCS_ECONOMY_INVALID;
    shop=ammo?MERCS_SAVE_SHOP_AMMO:MERCS_SAVE_SHOP_WEAPONS;
    owned=ammo?MERCS_SAVE_OWNED_AMMO:MERCS_SAVE_OWNED_WEAPONS;
    from=buy?shop:owned;to=buy?owned:shop;
    row=mercs_save_list_record(state,from,index);h=mercs_save_header(state);
    if(!row||((row[8]==1)!=ammo)||mercs_economy_item_price(row,buy,&price))return MERCS_ECONOMY_INVALID;
    if(buy&&s32(u32(h+0x12a))<price)return MERCS_ECONOMY_FUNDS;
    if(mercs_save_list_count(state,to)>=MERCS_SAVE_MAX_LIST_RECORDS)return MERCS_ECONOMY_CAPACITY;
    s=mercs_save_clone(state);if(!s)return MERCS_ECONOMY_NOMEM;
    if(!buy&&row[8]==0)p32(mercs_save_list_record_edit(s,from,index)+0xa,UINT32_MAX);
    rc=move_row(s,from,to,index);
    if(!rc){
        uint8_t *header=mercs_save_header_edit(s);
        p32(header+0x12a,buy?u32(header+0x12a)-(uint32_t)price:u32(header+0x12a)+(uint32_t)price);
        header[0]=1;mercs_save_swap(state,s);
    }
    mercs_save_free(s);return rc;
}

int mercs_economy_mech_price(const mercs_save_state *state,int shop,size_t index,int32_t *out)
{
    const uint8_t *row;
    mercs_career_mech m;
    int32_t value;
    int64_t marked;
    if(!state||!out||(shop!=0&&shop!=1))return MERCS_ECONOMY_INVALID;
    row=mercs_save_list_record(state,shop?MERCS_SAVE_SHOP_MECHS:MERCS_SAVE_OWNED_MECHS,index);
    if(!row)return MERCS_ECONOMY_INVALID;
    memcpy(m.bytes,row,sizeof m.bytes);
    if(mercs_debrief_mech_value(&m,s32(u32(mercs_save_header(state)+0x122)),&value))
        return MERCS_ECONOMY_INVALID;
    /* Within the original signed integer range, truncating the binary64
     * 1.1 constant gives exactly this quotient, on every host FP format. */
    marked=shop?(int64_t)value*11/10:value;
    *out=marked>INT32_MAX||marked<INT32_MIN?INT32_MIN:(int32_t)marked;
    return 0;
}

int mercs_economy_viable(const mercs_save_state *state,int *viable)
{
    size_t i,j;int32_t minimum=100000000,value,year;uint32_t assets=0;
    const uint8_t *header;
    static const enum mercs_save_list items[]={MERCS_SAVE_OWNED_WEAPONS,MERCS_SAVE_OWNED_AMMO};
    if(!state||!viable||!(header=mercs_save_header(state)))return MERCS_ECONOMY_INVALID;
    if(mercs_save_list_count(state,MERCS_SAVE_OWNED_MECHS)){*viable=1;return 0;}
    year=s32(u32(header+0x122));
    for(j=0;j<sizeof items/sizeof items[0];j++)
        for(i=0;i<mercs_save_list_count(state,items[j]);i++)
            assets+=(uint32_t)mercs_debrief_item_value(mercs_save_list_record(state,items[j],i));
    for(i=0;i<mercs_save_list_count(state,MERCS_SAVE_SHOP_MECHS);i++){
        mercs_career_mech mech;double marked;
        memcpy(mech.bytes,mercs_save_list_record(state,MERCS_SAVE_SHOP_MECHS,i),sizeof mech.bytes);
        if(mercs_debrief_mech_value(&mech,year,&value))return MERCS_ECONOMY_INVALID;
        marked=(double)value*1.1;
        if((double)minimum>marked)
            minimum=marked>=2147483648.0||marked< -2147483648.0?INT32_MIN:(int32_t)marked;
    }
    *viable=s32(u32(header+0x12a)+assets)>=minimum;return 0;
}

static int mech_reference(const mercs_save_state *s,enum mercs_save_list list,uint32_t id,size_t *index)
{
    size_t i,found=0;
    for(i=0;i<mercs_save_list_count(s,list);i++)
        if(u32(mercs_save_list_record(s,list,i)+0x810)==id){*index=i;found++;}
    return found==1?0:MERCS_ECONOMY_INVALID;
}

int mercs_economy_trade_mech(mercs_save_state *state,int buy,size_t index)
{
    enum mercs_save_list from=buy?MERCS_SAVE_SHOP_MECHS:MERCS_SAVE_OWNED_MECHS;
    enum mercs_save_list to=buy?MERCS_SAVE_OWNED_MECHS:MERCS_SAVE_SHOP_MECHS;
    enum mercs_save_list rf=buy?MERCS_SAVE_SHOP_REFERENCES:MERCS_SAVE_OWNED_REFERENCES;
    enum mercs_save_list rt=buy?MERCS_SAVE_OWNED_REFERENCES:MERCS_SAVE_SHOP_REFERENCES;
    const uint8_t *row;
    mercs_save_state *next;
    size_t ref=0,i,n;uint32_t id;int32_t price,value;
    int rc,number=0,found;
    uint8_t *m,*r,*h;
    if(!state||(buy!=0&&buy!=1)||!(row=mercs_save_list_record(state,from,index))||
       mercs_economy_mech_price(state,buy,index,&price))return MERCS_ECONOMY_INVALID;
    id=u32(row+0x810);
    if(mech_reference(state,rf,id,&ref))return MERCS_ECONOMY_INVALID;
    n=mercs_save_list_count(state,to);
    if(n>=MERCS_SAVE_MAX_LIST_RECORDS||n!=mercs_save_list_count(state,rt))return MERCS_ECONOMY_CAPACITY;
    if(buy){
        mercs_career_mech raw;
        memcpy(raw.bytes,row,sizeof raw.bytes);
        if(mercs_debrief_mech_value(&raw,s32(u32(mercs_save_header(state)+0x122)),&value))
            return MERCS_ECONOMY_INVALID;
        /* The DOS guard compares to the untruncated x87 product. 1.1 is
         * slightly above eleven tenths: an exact positive integer price
         * therefore still needs one additional C-Bill for this guard. */
        if((int64_t)s32(u32(mercs_save_header(state)+0x12a))*10<(int64_t)value*11 ||
           (value>0&&(int64_t)s32(u32(mercs_save_header(state)+0x12a))*10==(int64_t)value*11))
            return MERCS_ECONOMY_FUNDS;
        if(!memchr(row+0x823,0,MERCS_CAREER_FILENAME_BYTES)||strlen((const char*)row+0x823)<5)
            return MERCS_ECONOMY_INVALID;
        do{
            found=0;
            for(i=0;i<n;i++){
                const uint8_t *name=mercs_save_list_record(state,to,i)+0x826;
                char two[3]={(char)name[0],(char)name[1],0};
                if(atoi(two)==number){found=1;number++;break;}
            }
        }while(found&&number<100);
        if(number>=100)return MERCS_ECONOMY_CAPACITY;
    }
    next=mercs_save_clone(state);if(!next)return MERCS_ECONOMY_NOMEM;
    m=mercs_save_list_record_edit(next,from,index);r=mercs_save_list_record_edit(next,rf,ref);
    h=mercs_save_header_edit(next);
    if(buy){
        m[0x826]=(uint8_t)('0'+number/10);m[0x827]=(uint8_t)('0'+number%10);
        memcpy(r+0x823,m+0x823,strlen((const char*)m+0x823)+1);
        p32(m+0x810,u32(h+0x15e));p32(r+0x810,u32(h+0x15e));p32(h+0x15e,u32(h+0x15e)+1u);
    }else{
        for(i=0;i<mercs_save_list_count(next,MERCS_SAVE_OWNED_PILOTS);i++){
            uint8_t *pilot=mercs_save_list_record_edit(next,MERCS_SAVE_OWNED_PILOTS,i);
            if(u32(pilot+0x20)==id){p32(pilot+0x20,UINT32_MAX);break;}
        }
    }
    p32(m+0x80c,UINT32_MAX);p32(r+0x80c,UINT32_MAX);
    rc=move_row(next,from,to,index);if(!rc)rc=move_row(next,rf,rt,ref);
    if(!rc){
        p32(h+0x12a,buy?u32(h+0x12a)-(uint32_t)price:u32(h+0x12a)+(uint32_t)price);
        h[0]=1;mercs_save_swap(state,next);
    }
    mercs_save_free(next);return rc;
}

static int pilot_assignment(const mercs_save_state *s,size_t pilot_index,int *owned)
{
    const uint8_t *p=mercs_save_list_record(s,MERCS_SAVE_OWNED_PILOTS,pilot_index);
    size_t i,j,n=mercs_save_list_count(s,MERCS_SAVE_OWNED_MECHS),pilots;
    uint32_t id,assignment;
    if(!p||n>INT32_MAX)return MERCS_ECONOMY_INVALID;
    id=u32(p+0x24);assignment=u32(p+0x20);*owned=-1;
    if(id==UINT32_MAX)return MERCS_ECONOMY_INVALID;
    pilots=mercs_save_list_count(s,MERCS_SAVE_OWNED_PILOTS);
    for(i=0;i<pilots;i++)if(i!=pilot_index&&u32(mercs_save_list_record(s,MERCS_SAVE_OWNED_PILOTS,i)+0x24)==id)
        return MERCS_ECONOMY_INVALID;
    for(i=0;i<n;i++){
        const uint8_t *m=mercs_save_list_record(s,MERCS_SAVE_OWNED_MECHS,i);
        uint32_t mid=u32(m+0x810),owner=u32(m+0x80c);
        if(mid==UINT32_MAX)return MERCS_ECONOMY_INVALID;
        for(j=0;j<i;j++)if(u32(mercs_save_list_record(s,MERCS_SAVE_OWNED_MECHS,j)+0x810)==mid)
            return MERCS_ECONOMY_INVALID;
        if((mid==assignment)!=(owner==id))return MERCS_ECONOMY_INVALID;
        if(mid==assignment)*owned=(int)i;
    }
    return assignment!=UINT32_MAX&&*owned<0?MERCS_ECONOMY_INVALID:0;
}

int mercs_economy_assign_pilot(mercs_save_state *state,size_t pilot_index,int mech_index)
{
    int old,rc;mercs_save_state *next;uint8_t *pilot,*m;
    size_t count=mercs_save_list_count(state,MERCS_SAVE_OWNED_MECHS);
    uint32_t id,mid=UINT32_MAX;
    if(!state||mech_index< -1||(mech_index>=0&&(size_t)mech_index>=count))return MERCS_ECONOMY_INVALID;
    if((rc=pilot_assignment(state,pilot_index,&old)))return rc;
    id=u32(mercs_save_list_record(state,MERCS_SAVE_OWNED_PILOTS,pilot_index)+0x24);
    if(mech_index>=0){
        const uint8_t *target=mercs_save_list_record(state,MERCS_SAVE_OWNED_MECHS,(size_t)mech_index);
        if(u32(target+0x80c)!=UINT32_MAX&&u32(target+0x80c)!=id)return MERCS_ECONOMY_ASSIGNED;
        mid=u32(target+0x810);
    }
    next=mercs_save_clone(state);if(!next)return MERCS_ECONOMY_NOMEM;
    if(old>=0)p32(mercs_save_list_record_edit(next,MERCS_SAVE_OWNED_MECHS,(size_t)old)+0x80c,UINT32_MAX);
    pilot=mercs_save_list_record_edit(next,MERCS_SAVE_OWNED_PILOTS,pilot_index);p32(pilot+0x20,mid);
    if(mech_index>=0){m=mercs_save_list_record_edit(next,MERCS_SAVE_OWNED_MECHS,(size_t)mech_index);p32(m+0x80c,id);}
    mercs_save_header_edit(next)[0]=1;mercs_save_swap(state,next);mercs_save_free(next);return 0;
}

int mercs_economy_cycle_pilot(mercs_save_state *state,size_t pilot_index,int direction)
{
    int old,choice,rc;size_t i,n,free_count=0;
    if(!state||(direction!=1&&direction!=-1))return MERCS_ECONOMY_INVALID;
    if((rc=pilot_assignment(state,pilot_index,&old)))return rc;
    n=mercs_save_list_count(state,MERCS_SAVE_OWNED_MECHS);
    for(i=0;i<n;i++)if(u32(mercs_save_list_record(state,MERCS_SAVE_OWNED_MECHS,i)+0x80c)==UINT32_MAX)free_count++;
    if(old<0&&!free_count)return MERCS_ECONOMY_EMPTY;
    choice=old<0?(int)n:old;
    for(i=0;i<=n;i++){
        choice+=direction;if(choice<0)choice=(int)n;if((size_t)choice>n)choice=0;
        if((size_t)choice==n)return mercs_economy_assign_pilot(state,pilot_index,-1);
        if(u32(mercs_save_list_record(state,MERCS_SAVE_OWNED_MECHS,(size_t)choice)+0x80c)==UINT32_MAX)
            return mercs_economy_assign_pilot(state,pilot_index,choice);
    }
    return MERCS_ECONOMY_INVALID;
}

int mercs_economy_toggle_aerotech(mercs_save_state *state,size_t index)
{
    mercs_save_state *next;uint8_t *tech;
    if(!state||!mercs_save_list_record(state,MERCS_SAVE_OWNED_TECHS,index))return MERCS_ECONOMY_INVALID;
    if(s32(u32(mercs_save_header(state)+0x2df7))<3)return MERCS_ECONOMY_LIMIT;
    next=mercs_save_clone(state);if(!next)return MERCS_ECONOMY_NOMEM;
    tech=mercs_save_list_record_edit(next,MERCS_SAVE_OWNED_TECHS,index);tech[0x2c]=(uint8_t)(tech[0x2c]!=1);
    mercs_save_header_edit(next)[0]=1;mercs_save_swap(state,next);mercs_save_free(next);return 0;
}

int mercs_economy_roster_ready(const mercs_save_state *state,int *mechmates,int *aerotech)
{
    size_t i;int player=0,mates=0,techs=0,assigned,rc,limit;
    if(!state||!mechmates||!aerotech)return MERCS_ECONOMY_INVALID;
    for(i=0;i<mercs_save_list_count(state,MERCS_SAVE_OWNED_PILOTS);i++){
        const uint8_t *p=mercs_save_list_record(state,MERCS_SAVE_OWNED_PILOTS,i);
        if((rc=pilot_assignment(state,i,&assigned)))return rc;
        if(p[0x2064]){if(player||assigned<0)return MERCS_ECONOMY_PLAYER;player=1;}
        else if(assigned>=0)mates++;
    }
    limit=s32(u32(mercs_save_header(state)+0x2df7));
    if(limit>=3)for(i=0;i<mercs_save_list_count(state,MERCS_SAVE_OWNED_TECHS);i++)
        if(mercs_save_list_record(state,MERCS_SAVE_OWNED_TECHS,i)[0x2c]==1)techs++;
    *mechmates=mates;*aerotech=techs;
    if(!player)return MERCS_ECONOMY_PLAYER;
    return mates>2||techs>1||mates+techs>limit?MERCS_ECONOMY_LIMIT:0;
}

int mercs_economy_item_category(const uint8_t item[MERCS_SAVE_ITEM_BYTES])
{
    static const signed char classes[66]={
        0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,0,2,-1,0,-1,
        0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,0,2,0,0,0
    };
    uint32_t id;
    if(!item)return -1;
    if(item[8]==2)return MERCS_ITEM_EQUIPMENT;
    if(item[8]==1)return MERCS_ITEM_AMMO;
    if(item[8]!=0)return -1;
    id=u32(item);return id<66?classes[id]:-1;
}

static int group_compare(const void *a,const void *b)
{
    const mercs_item_group *aa=a,*bb=b;
    if(aa->id<bb->id)return -1;
    if(aa->id>bb->id)return 1;
    return aa->first_index<bb->first_index?-1:aa->first_index>bb->first_index?1:0;
}

int mercs_economy_item_groups(const mercs_save_state *state,int shop,
                              enum mercs_item_category category,
                              mercs_item_group **groups,size_t *count)
{
    enum mercs_save_list list;
    mercs_item_group *p;
    size_t n,i,used=0,out=0;
    if(!state||!groups||!count||(shop!=0&&shop!=1)||category<0||category>MERCS_ITEM_AMMO)
        return MERCS_ECONOMY_INVALID;
    list=category==MERCS_ITEM_AMMO?(shop?MERCS_SAVE_SHOP_AMMO:MERCS_SAVE_OWNED_AMMO):
        (shop?MERCS_SAVE_SHOP_WEAPONS:MERCS_SAVE_OWNED_WEAPONS);
    n=mercs_save_list_count(state,list);p=n?calloc(n,sizeof *p):NULL;
    if(n&&!p)return MERCS_ECONOMY_NOMEM;
    for(i=0;i<n;i++){
        const uint8_t *item=mercs_save_list_record(state,list,i);
        if(mercs_economy_item_category(item)!=(int)category)continue;
        p[used].id=s32(u32(item));p[used].first_index=i;p[used].count=1;p[used].type=item[8];
        if(mercs_economy_item_price(item,shop,&p[used].price)){free(p);return MERCS_ECONOMY_INVALID;}
        used++;
    }
    if(used)qsort(p,used,sizeof *p,group_compare);
    for(i=0;i<used;i++){
        if(out&&p[out-1].id==p[i].id){p[out-1].count++;continue;}
        p[out++]=p[i];
    }
    *groups=p;*count=out;return 0;
}

static int personnel_lists(enum mercs_personnel_kind kind,enum mercs_save_list *shop,
                            enum mercs_save_list *owned)
{
    if(kind!=MERCS_PERSONNEL_MECHWARRIOR&&kind!=MERCS_PERSONNEL_AEROTECH)return -1;
    *shop=kind==MERCS_PERSONNEL_AEROTECH?MERCS_SAVE_SHOP_TECHS:MERCS_SAVE_SHOP_PILOTS;
    *owned=kind==MERCS_PERSONNEL_AEROTECH?MERCS_SAVE_OWNED_TECHS:MERCS_SAVE_OWNED_PILOTS;
    return 0;
}

int mercs_economy_hire(mercs_save_state *state,enum mercs_personnel_kind kind,size_t index)
{
    enum mercs_save_list shop,owned;
    mercs_save_state *s;
    uint8_t *row,*h;
    int rc;
    if(!state||personnel_lists(kind,&shop,&owned)||!mercs_save_list_record(state,shop,index))
        return MERCS_ECONOMY_INVALID;
    if(mercs_save_list_count(state,owned)>=(kind==MERCS_PERSONNEL_AEROTECH?1u:8u))
        return MERCS_ECONOMY_CAPACITY;
    s=mercs_save_clone(state);if(!s)return MERCS_ECONOMY_NOMEM;
    row=mercs_save_list_record_edit(s,shop,index);h=mercs_save_header_edit(s);
    p32(row+(kind==MERCS_PERSONNEL_AEROTECH?0x20:0x24),u32(h+0x15e));
    p32(h+0x15e,u32(h+0x15e)+1u);
    if(kind==MERCS_PERSONNEL_MECHWARRIOR)p32(row+0x20,UINT32_MAX);
    rc=move_row(s,shop,owned,index);
    if(!rc){h[0]=1;mercs_save_swap(state,s);}
    mercs_save_free(s);return rc;
}

int mercs_economy_dismiss(mercs_save_state *state,enum mercs_personnel_kind kind,size_t index)
{
    enum mercs_save_list shop,owned;
    mercs_save_state *s;
    const uint8_t *row;
    size_t i,mech=0,found=0;
    int rc;
    if(!state||personnel_lists(kind,&shop,&owned)||!(row=mercs_save_list_record(state,owned,index)))
        return MERCS_ECONOMY_INVALID;
    if(kind==MERCS_PERSONNEL_MECHWARRIOR){
        if(row[0x2064])return MERCS_ECONOMY_PLAYER;
        if(u32(row+0x20)!=UINT32_MAX){
            for(i=0;i<mercs_save_list_count(state,MERCS_SAVE_OWNED_MECHS);i++){
                const uint8_t *m=mercs_save_list_record(state,MERCS_SAVE_OWNED_MECHS,i);
                if(u32(m+0x80c)==u32(row+0x24)){mech=i;found++;}
            }
            if(found!=1)return MERCS_ECONOMY_INVALID;
        }
    }
    s=mercs_save_clone(state);if(!s)return MERCS_ECONOMY_NOMEM;
    if(found){
        p32(mercs_save_list_record_edit(s,MERCS_SAVE_OWNED_MECHS,mech)+0x80c,UINT32_MAX);
        p32(mercs_save_list_record_edit(s,owned,index)+0x20,UINT32_MAX);
    }
    rc=move_row(s,owned,shop,index);
    if(!rc){mercs_save_header_edit(s)[0]=1;mercs_save_swap(state,s);}
    mercs_save_free(s);return rc;
}

int mercs_economy_weapon_group(mercs_save_state *state,size_t mech_index,int slot)
{
    const uint8_t *m;
    mercs_save_state *next;
    uint8_t *edit;
    int i;
    if(!state||slot< -1||slot>=14||
       !(m=mercs_save_list_record(state,MERCS_SAVE_OWNED_MECHS,mech_index))||
       (slot>=0&&s32(u32(m+0x2f0+8*slot))<0))return MERCS_ECONOMY_INVALID;
    next=mercs_save_clone(state);if(!next)return MERCS_ECONOMY_NOMEM;
    edit=mercs_save_list_record_edit(next,MERCS_SAVE_OWNED_MECHS,mech_index);
    if(slot<0){for(i=0;i<14;i++)edit[0x2f5+8*i]&=0xf0;}
    else{
        unsigned group=((u32(edit+0x2f4+8*slot)>>8)&7u)+1u;
        if(group>4)group=0;
        edit[0x2f5+8*slot]=(uint8_t)((edit[0x2f5+8*slot]&0xf0u)|group);
    }
    mercs_save_header_edit(next)[0]=1;mercs_save_swap(state,next);mercs_save_free(next);return 0;
}

int mercs_economy_finance(const mercs_save_state *state,mercs_finance *out)
{
    const uint8_t *h;
    uint32_t n,pa=0,aa=0,total;
    size_t i;
    mercs_finance v;
    if(!state||!out)return MERCS_ECONOMY_INVALID;
    h=mercs_save_header(state);n=(uint32_t)mercs_save_list_count(state,MERCS_SAVE_OWNED_MECHS);
    v.maintenance=s32(u32(h+0x23a3)*n);
    v.technicians=s32(u32(h+0x23a7)*n);
    v.technicians_display=s32((uint32_t)v.technicians+1u);
    v.travel=s32(u32(h+0x23ab));v.broker=s32(u32(h+0x23af));v.guild=s32(u32(h+0x23b3));
    for(i=0;i<mercs_save_list_count(state,MERCS_SAVE_OWNED_PILOTS);i++){
        const uint8_t *p=mercs_save_list_record(state,MERCS_SAVE_OWNED_PILOTS,i);
        if(p[0x2064]==0)pa+=u32(p+0x3c);
    }
    for(i=0;i<mercs_save_list_count(state,MERCS_SAVE_OWNED_TECHS);i++)
        aa+=u32(mercs_save_list_record(state,MERCS_SAVE_OWNED_TECHS,i)+0x24);
    v.pilot_salaries=s32(pa);v.aero_salaries=s32(aa);
    total=(uint32_t)v.maintenance+(uint32_t)v.technicians_display+(uint32_t)v.travel+
        (uint32_t)v.broker+(uint32_t)v.guild+pa+aa;
    v.monthly_total=s32(total);v.funds=s32(u32(h+0x12a));*out=v;return 0;
}
