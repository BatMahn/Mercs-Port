/* DOS0x694b4/0x69bb0 and retail Windows0x100396a0/0x10039c60. */
#include "mercssave.h"
#include <stdlib.h>
#include <string.h>

struct mercs_save_array { uint8_t *data,*extension; size_t count; };
struct mercs_save_state {
    uint8_t header[MERCS_SAVE_HEADER_BYTES];
    struct mercs_save_array lists[MERCS_SAVE_LIST_COUNT];
    int windows;
};
static const uint16_t offsets[MERCS_SAVE_LIST_COUNT] = {
    0x156,0x15a,0x142,0x142,0x146,0x14a,0x14e,0x152,
    0x12e,0x12e,0x132,0x136,0x13a,0x13e
};
static const uint16_t strides[MERCS_SAVE_LIST_COUNT] = {
    0x850,0x1a,0x850,0x850,0x2066,0x205e,0x1a,0x1a,
    0x850,0x850,0x2066,0x205e,0x1a,0x1a
};
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put32(uint8_t *p,uint32_t v)
{ p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24); }
static int valid_list(enum mercs_save_list list)
{ return list>=0 && list<MERCS_SAVE_LIST_COUNT; }
mercs_save_state *mercs_save_create(void)
{ return (mercs_save_state *)calloc(1,sizeof(mercs_save_state)); }
void mercs_save_free(mercs_save_state *s)
{ int i; if(!s)return;for(i=0;i<MERCS_SAVE_LIST_COUNT;i++){free(s->lists[i].data);free(s->lists[i].extension);}free(s); }
void mercs_save_swap(mercs_save_state *left,mercs_save_state *right)
{
    mercs_save_state temporary;
    if(!left||!right)return;
    temporary=*left;*left=*right;*right=temporary;
}
mercs_save_state *mercs_save_clone(const mercs_save_state *s)
{
    mercs_save_state *n;int i;
    if(!s)return NULL;
    n=mercs_save_create();if(!n)return NULL;
    n->windows=s->windows;memcpy(n->header,s->header,sizeof n->header);
    for(i=0;i<MERCS_SAVE_LIST_COUNT;i++){
        if(mercs_save_set_list(n,(enum mercs_save_list)i,s->lists[i].data,s->lists[i].count)){
            mercs_save_free(n);return NULL;
        }
        if(s->lists[i].extension&&s->lists[i].count)
            memcpy(n->lists[i].extension,s->lists[i].extension,3*s->lists[i].count);
    }
    /* set_list updates counts; imported header bytes remain authoritative. */
    memcpy(n->header,s->header,sizeof n->header);return n;
}
const uint8_t *mercs_save_header(const mercs_save_state *s){return s?s->header:NULL;}
uint8_t *mercs_save_header_edit(mercs_save_state *s){return s?s->header:NULL;}
size_t mercs_save_list_stride(enum mercs_save_list l){return valid_list(l)?strides[l]:0;}
size_t mercs_save_list_count(const mercs_save_state *s,enum mercs_save_list l)
{return s&&valid_list(l)?s->lists[l].count:0;}
int mercs_save_windows(const mercs_save_state *s){return s?s->windows:0;}
const uint8_t *mercs_save_mech_extension(const mercs_save_state *s,enum mercs_save_list l,size_t i)
{return s&&s->windows&&valid_list(l)&&strides[l]==MERCS_CAREER_MECH_BYTES&&i<s->lists[l].count?
    s->lists[l].extension+3*i:NULL;}
int mercs_save_set_mech_extension(mercs_save_state *s,enum mercs_save_list l,size_t i,const uint8_t extra[3])
{
    if(!s||!extra||!valid_list(l)||strides[l]!=MERCS_CAREER_MECH_BYTES||i>=s->lists[l].count)return MERCS_SAVE_INVALID;
    if(!s->windows)return MERCS_SAVE_UNSUPPORTED;
    memcpy(s->lists[l].extension+3*i,extra,3);return 0;
}
const uint8_t *mercs_save_list_record(const mercs_save_state *s,enum mercs_save_list l,size_t i)
{return s&&valid_list(l)&&i<s->lists[l].count?s->lists[l].data+i*strides[l]:NULL;}
uint8_t *mercs_save_list_record_edit(mercs_save_state *s,enum mercs_save_list l,size_t i)
{return s&&valid_list(l)&&i<s->lists[l].count?s->lists[l].data+i*strides[l]:NULL;}
static int retained_extensions(const mercs_save_state *s,enum mercs_save_list l,
                                 const uint8_t *records,size_t count,uint8_t **out)
{
    size_t i,j;uint8_t *extra=NULL;
    *out=NULL;if(!s->windows||strides[l]!=MERCS_CAREER_MECH_BYTES||!count)return 0;
    extra=calloc(count,3);if(!extra)return MERCS_SAVE_NOMEM;
    for(i=0;i<count;i++){
        int found=0;uint32_t id=get32(records+i*strides[l]+0x810);
        for(j=0;j<s->lists[l].count;j++)if(get32(s->lists[l].data+j*strides[l]+0x810)==id){
            if(found){free(extra);return MERCS_SAVE_INVALID;}
            found=1;memcpy(extra+3*i,s->lists[l].extension+3*j,3);
        }
    }
    *out=extra;return 0;
}
int mercs_save_set_list(mercs_save_state *s,enum mercs_save_list l,const uint8_t *data,size_t count)
{
    uint8_t *copy=NULL,*extra=NULL;size_t bytes;int rc;
    if(!s||!valid_list(l)||count>MERCS_SAVE_MAX_LIST_RECORDS||(!data&&count))return MERCS_SAVE_INVALID;
    bytes=count*strides[l];if(bytes>MERCS_SAVE_MAX_BYTES)return MERCS_SAVE_INVALID;
    if(bytes){copy=(uint8_t *)malloc(bytes);if(!copy)return MERCS_SAVE_NOMEM;memcpy(copy,data,bytes);}
    rc=retained_extensions(s,l,data,count,&extra);if(rc){free(copy);return rc;}
    free(s->lists[l].data);free(s->lists[l].extension);
    s->lists[l].data=copy;s->lists[l].extension=extra;s->lists[l].count=count;
    put32(s->header+offsets[l],(uint32_t)count);return MERCS_SAVE_OK;
}
int mercs_save_set_owned(mercs_save_state *s,const mercs_career_mech_pair *pairs,size_t count)
{
    uint8_t *current=NULL,*reference=NULL,*ce=NULL,*re=NULL;size_t i,bytes;int rc;
    if(!s||count>MERCS_SAVE_MAX_LIST_RECORDS||(!pairs&&count))return MERCS_SAVE_INVALID;
    bytes=count*MERCS_CAREER_MECH_BYTES;
    if(bytes){current=(uint8_t *)malloc(bytes);reference=(uint8_t *)malloc(bytes);
        if(!current||!reference){free(current);free(reference);return MERCS_SAVE_NOMEM;}}
    for(i=0;i<count;i++){
        memcpy(current+i*MERCS_CAREER_MECH_BYTES,pairs[i].current.bytes,MERCS_CAREER_MECH_BYTES);
        memcpy(reference+i*MERCS_CAREER_MECH_BYTES,pairs[i].reference.bytes,MERCS_CAREER_MECH_BYTES);
    }
    rc=retained_extensions(s,MERCS_SAVE_OWNED_MECHS,current,count,&ce);
    if(!rc)rc=retained_extensions(s,MERCS_SAVE_OWNED_REFERENCES,reference,count,&re);
    if(rc){free(current);free(reference);free(ce);free(re);return rc;}
    free(s->lists[MERCS_SAVE_OWNED_MECHS].data);free(s->lists[MERCS_SAVE_OWNED_REFERENCES].data);
    free(s->lists[MERCS_SAVE_OWNED_MECHS].extension);free(s->lists[MERCS_SAVE_OWNED_REFERENCES].extension);
    s->lists[MERCS_SAVE_OWNED_MECHS].data=current;s->lists[MERCS_SAVE_OWNED_MECHS].count=count;
    s->lists[MERCS_SAVE_OWNED_REFERENCES].data=reference;s->lists[MERCS_SAVE_OWNED_REFERENCES].count=count;
    s->lists[MERCS_SAVE_OWNED_MECHS].extension=ce;s->lists[MERCS_SAVE_OWNED_REFERENCES].extension=re;
    put32(s->header+0x12e,(uint32_t)count);return MERCS_SAVE_OK;
}
int mercs_save_decode(const uint8_t *data,size_t length,mercs_save_state **out)
{
    mercs_save_state *s;size_t counts[MERCS_SAVE_LIST_COUNT],total=MERCS_SAVE_HEADER_BYTES;
    size_t windows_total=MERCS_SAVE_HEADER_BYTES,at,j;int i,rc;
    if(!data||!out||length<MERCS_SAVE_HEADER_BYTES||length>MERCS_SAVE_MAX_BYTES)return MERCS_SAVE_INVALID;
    for(i=0;i<MERCS_SAVE_LIST_COUNT;i++){
        counts[i]=get32(data+offsets[i]);
        if(counts[i]>MERCS_SAVE_MAX_LIST_RECORDS||counts[i]>(MERCS_SAVE_MAX_BYTES-total)/strides[i])return MERCS_SAVE_INVALID;
        total+=counts[i]*strides[i];
        windows_total+=counts[i]*(strides[i]+(strides[i]==MERCS_CAREER_MECH_BYTES?3u:0u));
    }
    if(total!=length&&windows_total!=length)return MERCS_SAVE_INVALID;
    s=mercs_save_create();if(!s)return MERCS_SAVE_NOMEM;
    s->windows=total!=length;
    memcpy(s->header,data,MERCS_SAVE_HEADER_BYTES);at=MERCS_SAVE_HEADER_BYTES;
    for(i=0;i<MERCS_SAVE_LIST_COUNT;i++){
        if(s->windows&&strides[i]==MERCS_CAREER_MECH_BYTES){
            size_t n=counts[i]*strides[i];uint8_t *common=n?malloc(n):NULL;
            if(n&&!common){mercs_save_free(s);return MERCS_SAVE_NOMEM;}
            for(j=0;j<counts[i];j++)memcpy(common+j*strides[i],data+at+j*(strides[i]+3u),strides[i]);
            rc=mercs_save_set_list(s,(enum mercs_save_list)i,common,counts[i]);free(common);
            if(rc){mercs_save_free(s);return rc;}
            for(j=0;j<counts[i];j++)memcpy(s->lists[i].extension+3*j,data+at+j*(strides[i]+3u)+strides[i],3);
            at+=counts[i]*(strides[i]+3u);
        }else{
            rc=mercs_save_set_list(s,(enum mercs_save_list)i,data+at,counts[i]);
            if(rc){mercs_save_free(s);return rc;}at+=counts[i]*strides[i];
        }
    }
    *out=s;return MERCS_SAVE_OK;
}
int mercs_save_encode(const mercs_save_state *s,int original,uint8_t **bytes,size_t *length)
{
    size_t total=MERCS_SAVE_HEADER_BYTES,at,count,stride,j;uint8_t *b;int i;
    if(!s||!bytes||!length||(original!=0&&original!=1))return MERCS_SAVE_INVALID;
    if(s->lists[MERCS_SAVE_SHOP_MECHS].count!=s->lists[MERCS_SAVE_SHOP_REFERENCES].count||
       s->lists[MERCS_SAVE_OWNED_MECHS].count!=s->lists[MERCS_SAVE_OWNED_REFERENCES].count)return MERCS_SAVE_INVALID;
    for(i=0;i<MERCS_SAVE_LIST_COUNT;i++){
        count=s->lists[i].count;stride=strides[i]+(s->windows&&strides[i]==MERCS_CAREER_MECH_BYTES?3u:0u);
        if(count>MERCS_SAVE_MAX_LIST_RECORDS||count>(MERCS_SAVE_MAX_BYTES-total)/stride||
           (count&&stride>strides[i]&&!s->lists[i].extension)||
           (count&&!s->lists[i].data)||(!original&&count!=get32(s->header+offsets[i])))return MERCS_SAVE_INVALID;
        total+=count*stride;
    }
    b=(uint8_t *)malloc(total);if(!b)return MERCS_SAVE_NOMEM;
    memcpy(b,s->header,MERCS_SAVE_HEADER_BYTES);at=MERCS_SAVE_HEADER_BYTES;
    if(original){b[0]=0;memcpy(b+0x23c7,b+0x23c3,4);}
    for(i=0;i<MERCS_SAVE_LIST_COUNT;i++){
        count=s->lists[i].count;
        if(original)put32(b+offsets[i],(uint32_t)count);
        if(s->windows&&strides[i]==MERCS_CAREER_MECH_BYTES){
            for(j=0;j<count;j++){
                memcpy(b+at,s->lists[i].data+j*strides[i],strides[i]);at+=strides[i];
                memcpy(b+at,s->lists[i].extension+3*j,3);at+=3;
            }
        }else{if(count)memcpy(b+at,s->lists[i].data,count*strides[i]);at+=count*strides[i];}
    }
    *bytes=b;*length=total;return MERCS_SAVE_OK;
}
void mercs_save_restore_markers(mercs_save_state *s)
{
    size_t i;if(!s)return;s->header[0]=0;
    for(i=0;i<s->lists[MERCS_SAVE_OWNED_MECHS].count;i++)s->lists[MERCS_SAVE_OWNED_MECHS].data[i*MERCS_CAREER_MECH_BYTES+0x84b]=0;
}
int mercs_save_player(const mercs_save_state *s,size_t *index,uint32_t *identity,
                      char *name,size_t name_size)
{
    size_t i,found=0,which=0,length;const uint8_t *p=NULL;
    if(!s)return MERCS_SAVE_INVALID;
    for(i=0;i<s->lists[MERCS_SAVE_OWNED_PILOTS].count;i++){
        const uint8_t *row=s->lists[MERCS_SAVE_OWNED_PILOTS].data+i*MERCS_SAVE_PILOT_BYTES;
        if(row[0x2064]==1){p=row;which=i;found++;}
    }
    if(found!=1)return MERCS_SAVE_INVALID;
    for(length=0;length<32&&p[length];length++){}
    if(length==32||(name&&name_size<=length))return MERCS_SAVE_INVALID;
    if(index)*index=which;
    if(identity)*identity=get32(p+0x24);
    if(name)memcpy(name,p,length+1);
    return MERCS_SAVE_OK;
}
int mercs_save_player_assignment(mercs_save_state *s,int32_t mech_identity)
{
    size_t index;int rc=mercs_save_player(s,&index,NULL,NULL,0);
    if(rc)return rc;
    put32(mercs_save_list_record_edit(s,MERCS_SAVE_OWNED_PILOTS,index)+0x20,(uint32_t)mech_identity);
    return MERCS_SAVE_OK;
}
