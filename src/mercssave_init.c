/* Authored DOS career initialization: 0x69074, 0x68150, 0x635c8.
 * Imported documents remain byte preserving. Undefined bytes in original
 * newly allocated item/player records are deliberately zero initialized. */
#include "mercssave.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put32(uint8_t *p,uint32_t v)
{ p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24); }
static int failure(char *error,size_t size,int code,const char *text)
{ if(error&&size)snprintf(error,size,"%s",text);return code; }
static int copy_string(uint8_t *out,size_t capacity,const uint8_t *in,size_t available)
{
    size_t n=0;
    while(n<available&&in[n])n++;
    if(n==available||n>=capacity)return MERCS_SAVE_INVALID;
    memcpy(out,in,n+1);return MERCS_SAVE_OK;
}

/* All supplied Mercs editions contain the same truncated BET4CAM1: its final
 * 12 bytes declare a 308-byte EQUP chunk, containing only the first item ID.
 * Original DOS0x68150 reads 74 words beyond the resource and appends 15 garbage
 * items. Accept ONLY this complete payload fingerprint, keeping its 33 fully
 * authored shop items and ignoring that incomplete suffix. This is a native
 * compatibility policy for original undefined behavior, not recovered missing
 * inventory. All other truncated records still fail transactionally.
 * Evidence: tests/fixtures/MERCS_BET4_CAMPAIGN_TAIL.md. */
static int known_bet4_tail(const prj_record *r,size_t at,size_t limit)
{
    return limit==1824&&at==1812&&prj_known_mercs_campaign_tail(r);
}
static int append(mercs_save_state *s,enum mercs_save_list list,const uint8_t *row)
{
    size_t count=mercs_save_list_count(s,list),stride=mercs_save_list_stride(list);
    uint8_t *all;int rc;
    if(count>=MERCS_SAVE_MAX_LIST_RECORDS)return MERCS_SAVE_INVALID;
    all=(uint8_t *)malloc((count+1)*stride);
    if(!all)return MERCS_SAVE_NOMEM;
    if(count)memcpy(all,mercs_save_list_record(s,list,0),count*stride);
    memcpy(all+count*stride,row,stride);
    rc=mercs_save_set_list(s,list,all,count+1);free(all);return rc;
}
static int allocate_identity(mercs_save_state *s,uint32_t *identity)
{
    uint8_t *h=mercs_save_header_edit(s);uint32_t next=get32(h+0x15e);
    if(next>INT32_MAX)return MERCS_SAVE_UNSUPPORTED;
    *identity=next;put32(h+0x15e,next+1);return MERCS_SAVE_OK;
}

/* Retail DOS weapon table 0xb9ef4/+4, stride61, indices0..65. */
static const int32_t prices[66][2]={
 {375000,30000},{262500,30000},{150000,30000},{45000,30000},
 {120000,27000},{90000,27000},{15000,27000},{180000,54000},
 {135000,54000},{22500,0},{7500,1000},{450000,20000},
 {225000,2000},{375000,9000},{600000,12000},{900000,20000},
 {180000,1000},{300000,9000},{480000,12000},{720000,20000},
 {11250,0},{450000,0},{300000,0},{120000,0},{16875,0},
 {262500,0},{90000,0},{24000,0},{150000,6000},{150000,2000},
 {-1,-1},{15000,13500},{-1,-1},{22500,54000},{675000,10000},
 {250000,30000},{175000,30000},{100000,30000},{30000,30000},
 {80000,27000},{60000,27000},{10000,27000},{15000,54000},
 {5000,1000},{300000,20000},{75000,1000},{125000,4500},
 {200000,6000},{300000,10000},{200000,9000},{400000,12000},
 {7500,0},{300000,0},{200000,0},{200000,0},{100000,0},
 {40000,0},{11250,0},{175000,0},{60000,0},{16000,0},
 {100000,6000},{100000,2000},{10000,1},{15000,13500},{450000,10000}
};
static const uint32_t equipment_codes[23]={7000,7050,7100,7150,7200,
 7250,7300,7350,7400,7450,7500,7550,7600,7650,7700,7750,7800,
 7850,7900,8000,8500,9000,9500};

static int set_price(uint8_t *row,int ammo)
{
    uint32_t id=get32(row);int32_t value;
    if(id>=66)return MERCS_SAVE_UNSUPPORTED;
    value=prices[id][ammo];if(value<=0)value=10000;
    put32(row+4,(uint32_t)value);
    if(ammo){put32(row+0x16,100);put32(row+0x12,0);}
    else{put32(row+0x12,100);put32(row+0x0a,UINT32_MAX);put32(row+0x0e,0);}
    return MERCS_SAVE_OK;
}

static int mech_row(prj_archive *archive,mercs_save_state *s,
                    const uint8_t *row,int salvage)
{
    char name[17],filename[32];prj_record record;mercs_career_mech_pair pair;
    uint32_t id;size_t length;int rc;
    if(copy_string((uint8_t *)name,sizeof name,row,16))return MERCS_SAVE_INVALID;
    length=strlen(name);
    if(length!=8)return MERCS_SAVE_UNSUPPORTED;
    memcpy(filename,name,8);memcpy(filename+5,"MER.MEK",8);
    rc=prj_read_named(archive,"MEK",name,&record);
    /* 0x58ec5 returns NULL for an absent stock design; the MHLT handler
     * 0x68965 skips it without consuming an identity. RANVCAM1 includes
     * ALT01STD, which is absent from the supplied DOS archive. */
    if(rc==PRJ_ERR_NOT_FOUND)return MERCS_SAVE_OK;
    if(rc)return MERCS_SAVE_INVALID;
    rc=allocate_identity(s,&id);
    if(!rc)rc=mercs_career_pair_init(&pair,record.data,record.size,filename,id,-1);
    prj_record_free(&record);
    if(rc)return MERCS_SAVE_UNSUPPORTED;
    mercs_career_set_acquisition(&pair.current,(int32_t)get32(row+(salvage?20:16)),
                                (int32_t)get32(row+(salvage?24:20)),
                                salvage?(int32_t)get32(row+16):0);
    pair.reference=pair.current;
    if(salvage)return append(s,MERCS_SAVE_SALVAGE_MECHS,pair.current.bytes);
    rc=append(s,MERCS_SAVE_SHOP_MECHS,pair.current.bytes);
    if(!rc)rc=append(s,MERCS_SAVE_SHOP_REFERENCES,pair.reference.bytes);
    return rc;
}

static int person_row(prj_archive *archive,mercs_save_state *s,
                      const uint8_t *name_bytes,int tech)
{
    char name[17];prj_record resource;uint8_t person[MERCS_SAVE_PILOT_BYTES];
    size_t at,limit,bytes=tech?MERCS_SAVE_TECH_BYTES:MERCS_SAVE_PILOT_BYTES;
    int found=0,rc=MERCS_SAVE_INVALID;
    if(copy_string((uint8_t *)name,sizeof name,name_bytes,16))return rc;
    rc=prj_read_named(archive,"BWD",name,&resource);
    /* DOS0x685da/0x68682 skip absent authored personnel resources. Retail
     * NOVICE9, for example, names MCC1PIL/TEC entries that are not in PRJ. */
    if(rc==PRJ_ERR_NOT_FOUND)return MERCS_SAVE_OK;
    if(rc)return MERCS_SAVE_INVALID;
    rc=MERCS_SAVE_INVALID;
    if(resource.size<12||memcmp(resource.data,"BWD",3))goto done;
    limit=get32(resource.data+4);
    if(limit>resource.size||limit<12)goto done;
    memset(person,0,sizeof person);
    for(at=12;at<limit;){
        const uint8_t *chunk=resource.data+at,*p=chunk+8;size_t length,n;
        if(limit-at<8)goto done;
        length=get32(chunk+4);
        if(length<8||length>limit-at)goto done;
        if(!memcmp(chunk,"PILT",4)||(tech&&!memcmp(chunk,"TECH",4))){
            if(length<92||found)goto done;
            /* Decoder strncpy(name,31), then explicit fields. */
            for(n=0;n<31&&p[n];n++)person[n]=p[n];
            put32(person+0x20,UINT32_MAX);
            if(tech){
                memcpy(person+0x2d,p+0x20,16);memcpy(person+0x24,p+0x30,4);
                if(copy_string(person+0x3d,16,p+0x34,16)||
                   copy_string(person+0x4d,16,p+0x44,16))goto done;
            }else{
                put32(person+0x24,UINT32_MAX);memcpy(person+0x28,p+0x20,16);
                memcpy(person+0x3c,p+0x30,4);
                if(copy_string(person+0x44,16,p+0x34,16)||
                   copy_string(person+0x54,16,p+0x44,16))goto done;
            }
            found=1;at+=92;continue;
        }
        if(!memcmp(chunk,"DSSR",4)){
            size_t base=tech?0x5e:0x64;
            if(!found||length>=bytes-base)goto done;
            /* Original strncpy size is chunk length minus one. The source
             * is a terminated string; reject unsafe overreads explicitly. */
            n=0;while(n<length-8&&p[n])n++;
            if(n==length-8)goto done;
            memcpy(person+base,p,n);memset(person+base+n,0,length-1-n);
            person[base+length]=0;
        }
        at+=length;
    }
    if(!found)goto done;
    rc=append(s,tech?MERCS_SAVE_SHOP_TECHS:MERCS_SAVE_SHOP_PILOTS,person);
done:
    prj_record_free(&resource);return rc;
}

static int item_rows(mercs_save_state *s,const uint8_t *tag,const uint8_t *p,size_t size)
{
    size_t stride,at,repeat,i;uint8_t item[MERCS_SAVE_ITEM_BYTES];int rc;
    enum mercs_save_list list=MERCS_SAVE_SHOP_WEAPONS;
    if(!memcmp(tag,"WNLT",4))stride=12;
    else if(!memcmp(tag,"EQUP",4))stride=20;
    else stride=16;
    if(size%stride)return MERCS_SAVE_INVALID;
    for(at=0;at<size;at+=stride){
        const uint8_t *r=p+at;uint32_t encoded=get32(r),kind=0,id=encoded;
        memset(item,0,sizeof item);repeat=1;list=MERCS_SAVE_SHOP_WEAPONS;
        if(!memcmp(tag,"WNLT",4)){
            put32(item,id);rc=set_price(item,0);if(rc)return rc;
            memcpy(item+0xe,r+8,4);
        }else if(!memcmp(tag,"AMMO",4)){
            /* DOS0x688xx compares the loop index with a signed quantity.
             * Authored regular contracts use -1 here: zero iterations. */
            repeat=get32(r+4);if(!repeat||repeat>INT32_MAX)continue;
            if(repeat>MERCS_SAVE_MAX_LIST_RECORDS)return MERCS_SAVE_INVALID;
            put32(item,id);put32(item+0xe,1);item[8]=1;
            rc=set_price(item,1);if(rc)return rc;
            memcpy(item+0x12,r+12,4);list=MERCS_SAVE_SHOP_AMMO;
        }else if(!memcmp(tag,"EQUP",4)){
            memcpy(item,r,8);item[8]=2;memcpy(item+0xa,r+8,12);
        }else{
            list=MERCS_SAVE_SALVAGE_ITEMS;
            if(encoded==0||encoded==UINT32_MAX)continue;
            if(encoded>=10000){kind=1;id=(encoded-10000)/10000;}
            else if(encoded>=7000){
                int j;kind=2;id=UINT32_MAX;
                for(j=22;j>=0;j--)if(encoded/50==equipment_codes[j]/50){id=(uint32_t)j;break;}
                if(id==UINT32_MAX)return MERCS_SAVE_UNSUPPORTED;
            }
            item[8]=(uint8_t)kind;put32(item,id);put32(item+0xa,UINT32_MAX);
            if(kind==1)put32(item+0xe,1);
            /* The original price initializer overwrites authored candidate
             * threshold/year. Preserve that observed behavior. */
            rc=set_price(item,kind==1);if(rc)return rc;
        }
        for(i=0;i<repeat;i++){rc=append(s,list,item);if(rc)return rc;}
    }
    return MERCS_SAVE_OK;
}

static int load_campaign_inplace(prj_archive *archive,mercs_save_state *s,const char *name)
{
    prj_record resource;uint8_t *h=mercs_save_header_edit(s);size_t at=0,limit;int rc,found=0,i,procedural;
    if(!archive||!name||!*name||strlen(name)>15)return MERCS_SAVE_INVALID;
    procedural=strlen(name)>=4&&toupper((unsigned char)name[0])=='R'&&
       toupper((unsigned char)name[1])=='A'&&toupper((unsigned char)name[2])=='N';
    rc=prj_read_named(archive,"BWD",name,&resource);if(rc)return MERCS_SAVE_INVALID;
    rc=MERCS_SAVE_INVALID;
    if(resource.size<12||memcmp(resource.data,"BWD",3))goto done;
    limit=get32(resource.data+4);if(limit<12||limit>resource.size)goto done;
    for(i=MERCS_SAVE_SHOP_MECHS;i<=MERCS_SAVE_SHOP_AMMO;i++){
        rc=mercs_save_set_list(s,(enum mercs_save_list)i,NULL,0);if(rc)goto done;
    }
    h[0x239d]=0;
    for(at=12;at<limit;){
        const uint8_t *chunk=resource.data+at,*p=chunk+8;size_t length,n,stride=0;
        rc=MERCS_SAVE_INVALID;
        if(limit-at<8)goto done;
        length=get32(chunk+4);if(length<8)goto done;
        if(length>limit-at){
            if(known_bet4_tail(&resource,at,limit))break;
            goto done;
        }
        if(!memcmp(chunk,"CPGN",4)){
            static const uint16_t fields[][2]={{4,0x122},{8,0x126},{92,0x1e6},{96,0x1ea},
                {124,0x1ee},{128,0x1f2},{132,0x1f6},{104,0x1fa},{0,0x1fe}};
            static const uint16_t strings[][2]={{12,0x166},{28,0x176},{44,0x186},
                {60,0x196},{76,0x1b6},{136,0x1c6},{108,0x1d6}};
            if(length<160||found)goto done;
            for(n=0;n<sizeof fields/sizeof fields[0];n++)memcpy(h+fields[n][1],p+fields[n][0],4);
            for(n=0;n<sizeof strings/sizeof strings[0];n++)
                if(copy_string(h+strings[n][1],16,p+strings[n][0],16))goto done;
            if(procedural)snprintf((char *)h+0x166,16,"ran%dscn1",isdigit((unsigned char)name[3])?name[3]-'0':0);
            h[0x1b6]=0;found=1;at+=160;continue;
        }
        if(!memcmp(chunk,"SLST",4)||!memcmp(chunk,"FLST",4)){
            size_t base=!memcmp(chunk,"SLST",4)?0x202:0x2a2;
            size_t count=(length-8)/16;
            if((length-8)%16||count>10)goto done;
            put32(h+(base==0x202?0x2392:0x2396),(uint32_t)count);
            for(n=0;n<count;n++)if(copy_string(h+base+n*16,16,p+n*16,16))goto done;
        }else if(!memcmp(chunk,"NEWS",4)||!memcmp(chunk,"CTNT",4)){
            size_t base=!memcmp(chunk,"NEWS",4)?0x342:0x1342,copy=length-1,available=limit-at-8;
            if(length>=4096)goto done;
            /* Original memcpy includes seven bytes after the text chunk.
             * Preserve available original bytes; only the beyond-resource
             * part is zeroed instead of reading arbitrary heap contents. */
            if(copy>available)copy=available;
            memcpy(h+base,p,copy);
            if(copy<length-1)memset(h+base+copy,0,length-1-copy);
            h[base+length]=0;
        }else if(!memcmp(chunk,"MHLT",4)||!memcmp(chunk,"SMHL",4)){
            int salvage=!memcmp(chunk,"SMHL",4);stride=salvage?28:24;
            if((length-8)%stride)goto done;
            for(n=0;n<length-8;n+=stride){rc=mech_row(archive,s,p+n,salvage);if(rc)goto done;}
        }else if(!memcmp(chunk,"PTLT",4)||!memcmp(chunk,"THLT",4)){
            if((length-8)%16)goto done;
            for(n=0;n<length-8;n+=16){rc=person_row(archive,s,p+n,!memcmp(chunk,"THLT",4));if(rc)goto done;}
        }else if(!memcmp(chunk,"WNLT",4)||!memcmp(chunk,"AMMO",4)||
                 !memcmp(chunk,"EQUP",4)||!memcmp(chunk,"SWNL",4)){
            rc=item_rows(s,chunk,p,length-8);if(rc)goto done;
        }
        at+=length;
    }
    rc=found?MERCS_SAVE_OK:MERCS_SAVE_INVALID;
    /* DOS0x67fc8 overlays ONLY the generated scene and contract text. Pay,
     * calendar, shop, roster and successor lists remain from the RAN CPGN.
     * The original lookup replaces just the RAN prefix. The native generator
     * aliases lettered contracts to their numeric preview resource. */
    if(!rc&&procedural){
        char companion[17];prj_record random;size_t pos;
        snprintf(companion,sizeof companion,"RND%s",name+3);
        i=prj_read_named(archive,"BWD",companion,&random);
        if(!i){
            if(random.size<12||memcmp(random.data,"BWD",3)||get32(random.data+4)!=random.size)rc=MERCS_SAVE_INVALID;
            else for(pos=12;pos<random.size;){
                const uint8_t *q=random.data+pos;size_t z;
                if(random.size-pos<8||(z=get32(q+4))<8||z>random.size-pos){rc=MERCS_SAVE_INVALID;break;}
                if(!memcmp(q,"CPGN",4)){
                    if(z<160||copy_string(h+0x166,16,q+20,16)){rc=MERCS_SAVE_INVALID;break;}
                    pos+=160;continue;
                }
                if(!memcmp(q,"CTNT",4)&&copy_string(h+0x1342,4096,q+8,z-8)){rc=MERCS_SAVE_INVALID;break;}
                pos+=z;
            }
            prj_record_free(&random);
        }
        else if(i!=PRJ_ERR_NOT_FOUND)rc=MERCS_SAVE_INVALID;
    }
done:
    prj_record_free(&resource);return rc;
}

int mercs_save_load_campaign(prj_archive *archive,mercs_save_state *s,const char *campaign,
                             char *error,size_t error_size)
{
    mercs_save_state *copy;int rc;
    if(!s)return failure(error,error_size,MERCS_SAVE_INVALID,"No career document");
    copy=mercs_save_clone(s);if(!copy)return failure(error,error_size,MERCS_SAVE_NOMEM,"Out of memory");
    rc=load_campaign_inplace(archive,copy,campaign);
    if(!rc)mercs_save_swap(s,copy);
    mercs_save_free(copy);
    return failure(error,error_size,rc,rc==MERCS_SAVE_UNSUPPORTED?
        "Original campaign contains unsupported procedural or roster data":rc?"Invalid original CPGN resource":"");
}

static int training_mechs(mercs_save_state *s,int include_shop)
{
    size_t i,j,count=mercs_save_list_count(s,MERCS_SAVE_OWNED_MECHS),kept=0;
    size_t shops=include_shop?mercs_save_list_count(s,MERCS_SAVE_SHOP_MECHS):0;
    mercs_career_mech_pair *pairs;int rc;
    if(count+shops>MERCS_SAVE_MAX_LIST_RECORDS)return MERCS_SAVE_INVALID;
    pairs=(mercs_career_mech_pair *)calloc(count+shops?count+shops:1,sizeof *pairs);
    if(!pairs)return MERCS_SAVE_NOMEM;
    for(i=0;i<count;i++){
        const uint8_t *row=mercs_save_list_record(s,MERCS_SAVE_OWNED_MECHS,i),*reference=NULL;
        if((int32_t)get32(row+0x814)<=0)continue;
        for(j=0;j<mercs_save_list_count(s,MERCS_SAVE_OWNED_REFERENCES);j++){
            const uint8_t *candidate=mercs_save_list_record(s,MERCS_SAVE_OWNED_REFERENCES,j);
            if(get32(candidate+0x810)==get32(row+0x810)){if(reference){free(pairs);return MERCS_SAVE_INVALID;}reference=candidate;}
        }
        if(!reference){free(pairs);return MERCS_SAVE_INVALID;}
        memcpy(pairs[kept].current.bytes,row,MERCS_CAREER_MECH_BYTES);
        memcpy(pairs[kept++].reference.bytes,reference,MERCS_CAREER_MECH_BYTES);
    }
    for(i=0;i<shops;i++){
        memcpy(pairs[kept].current.bytes,mercs_save_list_record(s,MERCS_SAVE_SHOP_MECHS,i),MERCS_CAREER_MECH_BYTES);
        pairs[kept].reference=pairs[kept].current;kept++;
    }
    rc=mercs_save_set_owned(s,pairs,kept);
    /* Original training transfer creates a full reference copy of each
     * shop current record. Retain the Windows extension in both new rows. */
    if(!rc&&mercs_save_windows(s))for(i=0;i<shops;i++){
        const uint8_t *extra=mercs_save_mech_extension(s,MERCS_SAVE_SHOP_MECHS,i);
        if(!extra||mercs_save_set_mech_extension(s,MERCS_SAVE_OWNED_MECHS,kept-shops+i,extra)||
           mercs_save_set_mech_extension(s,MERCS_SAVE_OWNED_REFERENCES,kept-shops+i,extra)){rc=MERCS_SAVE_INVALID;break;}
    }
    free(pairs);return rc;
}
static int transfer_training_inplace(mercs_save_state *s)
{
    size_t i,j,count;uint8_t person[MERCS_SAVE_PILOT_BYTES];
    int rc=training_mechs(s,1);if(rc)return rc;
    rc=mercs_save_set_list(s,MERCS_SAVE_SHOP_MECHS,NULL,0);if(rc)return rc;
    rc=mercs_save_set_list(s,MERCS_SAVE_SHOP_REFERENCES,NULL,0);if(rc)return rc;
    count=mercs_save_list_count(s,MERCS_SAVE_SHOP_PILOTS);
    for(i=0;i<count;i++){
        uint32_t id;rc=allocate_identity(s,&id);if(rc)return rc;
        memcpy(person,mercs_save_list_record(s,MERCS_SAVE_SHOP_PILOTS,i),sizeof person);
        put32(person+0x24,id);rc=append(s,MERCS_SAVE_OWNED_PILOTS,person);if(rc)return rc;
    }
    rc=mercs_save_set_list(s,MERCS_SAVE_SHOP_PILOTS,NULL,0);if(rc)return rc;
    for(j=0;j<2;j++){
        enum mercs_save_list from=j?MERCS_SAVE_SHOP_AMMO:MERCS_SAVE_SHOP_WEAPONS;
        enum mercs_save_list to=j?MERCS_SAVE_OWNED_AMMO:MERCS_SAVE_OWNED_WEAPONS;
        count=mercs_save_list_count(s,from);
        for(i=0;i<count;i++){rc=append(s,to,mercs_save_list_record(s,from,i));if(rc)return rc;}
        rc=mercs_save_set_list(s,from,NULL,0);if(rc)return rc;
    }
    return MERCS_SAVE_OK;
}
int mercs_save_transfer_training(mercs_save_state *s)
{
    mercs_save_state *copy;int rc;
    if(!s||get32(mercs_save_header(s)+0x23bb)!=0)return MERCS_SAVE_INVALID;
    copy=mercs_save_clone(s);if(!copy)return MERCS_SAVE_NOMEM;
    rc=transfer_training_inplace(copy);if(!rc)mercs_save_swap(s,copy);
    mercs_save_free(copy);return rc;
}
int mercs_save_training_cleanup(mercs_save_state *s)
{
    mercs_save_state *copy;size_t player;uint8_t row[MERCS_SAVE_PILOT_BYTES];int rc;
    if(!s||get32(mercs_save_header(s)+0x23bb)!=0)return MERCS_SAVE_INVALID;
    rc=mercs_save_player(s,&player,NULL,NULL,0);if(rc)return rc;
    copy=mercs_save_clone(s);if(!copy)return MERCS_SAVE_NOMEM;
    rc=training_mechs(copy,0);
    if(!rc){
        memcpy(row,mercs_save_list_record(copy,MERCS_SAVE_OWNED_PILOTS,player),sizeof row);
        put32(row+0x20,UINT32_MAX);
        rc=mercs_save_set_list(copy,MERCS_SAVE_OWNED_PILOTS,row,1);
    }
    if(!rc)mercs_save_swap(s,copy);
    mercs_save_free(copy);return rc;
}

int mercs_save_initialize(prj_archive *archive,int mode,const char *pilot,const char *unit,
                          mercs_save_state **out,char *error,size_t error_size)
{
    mercs_save_state *s;uint8_t *h;uint8_t player[MERCS_SAVE_PILOT_BYTES]={0};
    uint32_t player_id;size_t i,count;int rc;
    if(!archive||!out||!pilot||!*pilot||strlen(pilot)>31||!unit||strlen(unit)>31||mode<0||mode>1)
        return failure(error,error_size,MERCS_SAVE_INVALID,"Invalid authored-career initialization");
    s=mercs_save_create();if(!s)return failure(error,error_size,MERCS_SAVE_NOMEM,"Out of memory");
    h=mercs_save_header_edit(s);h[0x239c]=1;put32(h+0x23bb,(uint32_t)mode);put32(h+0x2df7,3);
    for(i=0;i<20;i++)put32(h+0x2342+i*4,UINT32_MAX);
    rc=load_campaign_inplace(archive,s,mode?"CAMP":"NOVICE");if(rc)goto done;
    if(mode){
        memcpy(h+0x12a,h+0x1e6,4);put32(h+0x23a3,10000);put32(h+0x23a7,2000);
        put32(h+0x23ab,1000);put32(h+0x23af,500);put32(h+0x23b3,720);
        count=mercs_save_list_count(s,MERCS_SAVE_SALVAGE_ITEMS);
        for(i=0;i<count;i++){
            const uint8_t *item=mercs_save_list_record(s,MERCS_SAVE_SALVAGE_ITEMS,i);
            rc=append(s,item[8]==1?MERCS_SAVE_OWNED_AMMO:MERCS_SAVE_OWNED_WEAPONS,item);if(rc)goto done;
        }
        rc=mercs_save_set_list(s,MERCS_SAVE_SALVAGE_ITEMS,NULL,0);if(rc)goto done;
        count=mercs_save_list_count(s,MERCS_SAVE_SALVAGE_MECHS);
        for(i=0;i<count;i++){
            uint8_t mech[MERCS_CAREER_MECH_BYTES];char name[32];
            /* New career owns no prior files; original allocator chooses
             * the first unused two-digit filename number in order. */
            if(i>99){rc=MERCS_SAVE_UNSUPPORTED;goto done;}
            memcpy(mech,mercs_save_list_record(s,MERCS_SAVE_SALVAGE_MECHS,i),sizeof mech);
            if(!memchr(mech+0x828,0,27)){rc=MERCS_SAVE_INVALID;goto done;}
            snprintf(name,sizeof name,"%.3s%02u%.26s",(char *)mech+0x823,(unsigned)i,(char *)mech+0x828);
            if(copy_string(mech+0x823,32,(const uint8_t *)name,sizeof name)){rc=MERCS_SAVE_INVALID;goto done;}
            rc=append(s,MERCS_SAVE_OWNED_MECHS,mech);if(rc)goto done;
            rc=append(s,MERCS_SAVE_OWNED_REFERENCES,mech);if(rc)goto done;
        }
        rc=mercs_save_set_list(s,MERCS_SAVE_SALVAGE_MECHS,NULL,0);if(rc)goto done;
        h[0x239d]=1;h[0x23a2]=0;
    }
    rc=allocate_identity(s,&player_id);if(rc)goto done;
    for(i=0;pilot[i];i++)player[i]=(uint8_t)toupper((unsigned char)pilot[i]);
    put32(player+0x20,UINT32_MAX);put32(player+0x24,player_id);
    memset(player+0x28,0xff,16);player[0x2064]=1;
    rc=append(s,MERCS_SAVE_OWNED_PILOTS,player);if(rc)goto done;
    memcpy(h+0x102,unit,strlen(unit)+1);h[0]=1;h[1]=1;
    if(!mode){rc=transfer_training_inplace(s);if(rc)goto done;}
    else if(mercs_save_list_count(s,MERCS_SAVE_OWNED_MECHS)){
        uint8_t *mech=mercs_save_list_record_edit(s,MERCS_SAVE_OWNED_MECHS,0);
        put32(mech+0x80c,player_id);
        rc=mercs_save_player_assignment(s,(int32_t)get32(mech+0x810));if(rc)goto done;
    }
    *out=s;return failure(error,error_size,MERCS_SAVE_OK,"");
done:
    mercs_save_free(s);return failure(error,error_size,rc,
        rc==MERCS_SAVE_UNSUPPORTED?"Original career requires unsupported authored/procedural data":"Cannot initialize original career resources");
}
