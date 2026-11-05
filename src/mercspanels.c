#include "mercspanels.h"
#include <stdlib.h>
#include <string.h>

static uint32_t panel_u32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }

static uint64_t panel_fingerprint(const unsigned char *p, size_t n)
{
    uint64_t value = UINT64_C(14695981039346656037);
    size_t i;
    for (i = 0; i < n; i++) value = (value ^ p[i])*UINT64_C(1099511628211);
    return value;
}

int mercs_lords_decode(const unsigned char *bytes, size_t length, mercs_lords_table *out)
{
    mercs_lords_table next;
    unsigned i;
    if (!bytes || !out || length != MERCS_LORDS_FILE_BYTES) return MERCS_PANELS_INVALID;
    memset(&next,0,sizeof next);
    next.count = panel_u32(bytes);
    if (next.count > MERCS_LORDS_ROWS) return MERCS_PANELS_INVALID;
    for (i = 0; i < MERCS_LORDS_ROWS; i++) {
        const unsigned char *row = bytes+4+69*i;
        uint32_t wealth = panel_u32(row+64);
        if (i < next.count && (!memchr(row,0,32) || !memchr(row+32,0,32))) return MERCS_PANELS_INVALID;
        memcpy(next.rows[i].pilot,row,32);
        memcpy(next.rows[i].group,row+32,32);
        memcpy(&next.rows[i].wealth,&wealth,sizeof wealth);
        next.rows[i].highlighted = row[68];
    }
    *out = next;
    return MERCS_PANELS_OK;
}

static void panel_p32(unsigned char *p,uint32_t value)
{ p[0]=(unsigned char)value;p[1]=(unsigned char)(value>>8);p[2]=(unsigned char)(value>>16);p[3]=(unsigned char)(value>>24); }

int mercs_lords_encode(const mercs_lords_table *table,unsigned char out[MERCS_LORDS_FILE_BYTES])
{
    unsigned char bytes[MERCS_LORDS_FILE_BYTES];
    unsigned i;
    if (!table || !out || table->count>MERCS_LORDS_ROWS) return MERCS_PANELS_INVALID;
    panel_p32(bytes,table->count);
    for (i=0;i<MERCS_LORDS_ROWS;i++) {
        const mercs_lords_row *r=&table->rows[i];
        unsigned char *b=bytes+4+69*i;
        if (i<table->count && (!memchr(r->pilot,0,32) || !memchr(r->group,0,32))) return MERCS_PANELS_INVALID;
        memcpy(b,r->pilot,32);memcpy(b+32,r->group,32);
        panel_p32(b+64,(uint32_t)r->wealth);b[68]=r->highlighted;
    }
    memcpy(out,bytes,sizeof bytes);return MERCS_PANELS_OK;
}

int mercs_lords_submit(mercs_lords_table *table,const mercs_lords_row *earned)
{
    mercs_lords_table next;
    unsigned i,j,gap;
    int lowest=-1;
    int32_t minimum=500000000;
    unsigned char valid[MERCS_LORDS_FILE_BYTES];
    if (!earned || !memchr(earned->pilot,0,32) || !memchr(earned->group,0,32) ||
        mercs_lords_encode(table,valid)) return MERCS_PANELS_INVALID;
    next=*table;
    for (i=0;i<MERCS_LORDS_ROWS;i++) {
        next.rows[i].highlighted=0;
        if (next.rows[i].wealth<minimum) {minimum=next.rows[i].wealth;lowest=(int)i;}
    }
    if (lowest<0 || earned->wealth<=minimum) {*table=next;return 0;}
    next.rows[lowest]=*earned;next.rows[lowest].highlighted=1;
    /* Watcom qsort0x7b45a, count<16: gap3 followed by gap1. Its first pass
     * visits multiples of3, so libc qsort/stable insertion alone can change
     * the order of equally wealthy pilots. */
    for (gap=3;gap>0;gap=gap==3?1:0)
        for (i=gap;i<next.count;i+=gap)
            for (j=i;j>=gap && next.rows[j-gap].wealth<next.rows[j].wealth;j-=gap) {
                mercs_lords_row swap=next.rows[j];next.rows[j]=next.rows[j-gap];next.rows[j-gap]=swap;
            }
    *table=next;return 1;
}

/* The known DOS LE image's second object is based at VA0xa0000. Its first
 * physical page starts at file0xc5600; all 38 pages are consecutive. Static
 * credit pointers contain object-relative offsets before LE relocation.
 * Recognizing the whole image first keeps these constants version-bounded. */
static int panel_string(const unsigned char *exe, size_t length, uint32_t offset,
                         char *out, size_t capacity)
{
    size_t at = 0xc5600u+(size_t)offset, n = 0;
    if (at >= length || !capacity) return MERCS_PANELS_INVALID;
    while (at+n < length && n < capacity && exe[at+n]) {
        if (exe[at+n] < 32 || exe[at+n] >= 127) return MERCS_PANELS_INVALID;
        n++;
    }
    if (at+n >= length || n == capacity) return MERCS_PANELS_INVALID;
    memcpy(out,exe+at,n+1);
    return MERCS_PANELS_OK;
}

int mercs_panels_decode(const unsigned char *exe, size_t length, mercs_panels *out)
{
    mercs_panels *next;
    unsigned i;
    static const uint32_t headings[3] = {0x9ef4,0x9efa,0x9f0a};
    if (!exe || !out) return MERCS_PANELS_INVALID;
    if (length != 962175u || panel_fingerprint(exe,length) != UINT64_C(0x5c4bc66f9129d76c))
        return MERCS_PANELS_UNSUPPORTED;
    if (memcmp(exe,"MZ",2) || panel_u32(exe+0x3c) != 0x2e10u ||
        memcmp(exe+0x2e10,"LE",2)) return MERCS_PANELS_INVALID;
    next = calloc(1,sizeof *next);
    if (!next) return MERCS_PANELS_INVALID;
    for (i = 0; i < MERCS_CIRCUIT_ROWS; i++) {
        uint32_t offset = panel_u32(exe+0xda59cu+4*i);
        if (offset && panel_string(exe,length,offset,next->credits[i],sizeof next->credits[i]))
            goto invalid;
    }
    for (i = 0; i < 3; i++)
        if (panel_string(exe,length,headings[i],next->headings[i],sizeof next->headings[i])) goto invalid;
    if (panel_string(exe,length,0x9f37,next->scores_filename,sizeof next->scores_filename) ||
        mercs_lords_decode(exe+0xdd098,MERCS_LORDS_FILE_BYTES,&next->defaults)) goto invalid;
    /* DOS46d78 sorts missing-file defaults by signed wealth descending. This
     * recognized table has distinct values already in that order. */
    for (i = 1; i < next->defaults.count; i++)
        if (next->defaults.rows[i-1].wealth <= next->defaults.rows[i].wealth) goto invalid;
    *out = *next;
    free(next);
    return MERCS_PANELS_OK;
invalid:
    free(next);
    return MERCS_PANELS_INVALID;
}
