/* mercssalvage_file.c - Titanium SALVAGE.MW2 layout and the engine's rand()
 * (see mercssalvage.h). No other dependencies: the mission links it. */
#include "mercssalvage.h"
#include <string.h>

static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void p32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}

int32_t mercs_salvage_rand(uint32_t *state)
{
    *state=*state*214013u+2531011u;
    return (int32_t)((*state>>16)&0x7fff);
}

int mercs_salvage_encode(const mercs_salvage_wreck *wr,size_t n,uint8_t *out,size_t cap,size_t *size)
{
    size_t i,at=4;int l,k;
    if((n&&!wr)||!out||!size||n>MERCS_SALVAGE_MAX_WRECKS||cap<4+n*MERCS_SALVAGE_RECORD_BYTES)return MERCS_SALVAGE_INVALID;
    memset(out,0,4+n*MERCS_SALVAGE_RECORD_BYTES);
    p32(out,(uint32_t)n);
    for(i=0;i<n;i++){
        const mercs_salvage_wreck *e=wr+i;
        memcpy(out+at,e->name,strnlen(e->name,MERCS_SALVAGE_NAME_BYTES));at+=MERCS_SALVAGE_NAME_BYTES;
        memcpy(out+at,e->title,strnlen(e->title,MERCS_SALVAGE_TITLE_BYTES));at+=MERCS_SALVAGE_TITLE_BYTES;
        at+=8;   /* two zero words (0x10059d0f writes local_1b0 twice) */
        for(l=0;l<8;l++){
            p32(out+at,(uint32_t)e->front[l]);p32(out+at+4,(uint32_t)e->rear[l]);p32(out+at+8,(uint32_t)e->internal[l]);
            at+=0x28;
        }
        at+=0x140;
        for(k=0;k<96;k++){p32(out+at,(uint32_t)e->rnd[k]);at+=4;}
    }
    *size=at;return MERCS_SALVAGE_OK;
}

int mercs_salvage_decode(const uint8_t *b,size_t size,mercs_salvage_wreck *wr,size_t cap,size_t *count)
{
    size_t n,i,at=4;int l,k;
    if(!b||!wr||!count||size<4)return MERCS_SALVAGE_INVALID;
    n=u32(b);
    if(n>MERCS_SALVAGE_MAX_WRECKS||n>cap||size<4+n*MERCS_SALVAGE_RECORD_BYTES)return MERCS_SALVAGE_INVALID;
    for(i=0;i<n;i++){
        mercs_salvage_wreck *e=wr+i;
        memset(e,0,sizeof *e);
        memcpy(e->name,b+at,MERCS_SALVAGE_NAME_BYTES);at+=MERCS_SALVAGE_NAME_BYTES;
        memcpy(e->title,b+at,MERCS_SALVAGE_TITLE_BYTES);at+=MERCS_SALVAGE_TITLE_BYTES;
        at+=8;
        for(l=0;l<8;l++){
            e->front[l]=(int32_t)u32(b+at);e->rear[l]=(int32_t)u32(b+at+4);e->internal[l]=(int32_t)u32(b+at+8);
            at+=0x28;
        }
        at+=0x140;
        for(k=0;k<96;k++){e->rnd[k]=(int32_t)u32(b+at);at+=4;}
    }
    *count=n;return MERCS_SALVAGE_OK;
}

