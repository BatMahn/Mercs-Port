/* prj.c - see prj.h for the format description. */
#include "prj.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    uint32_t offset;
    uint32_t size;
} prj_slot;

typedef struct {
    char     name[PRJ_NAME_LEN + 1];
    uint16_t id;
} prj_symbol;

typedef struct {
    char        tag[5];
    prj_slot   *slots;
    int         slot_count;
    prj_symbol *symbols;
    int         symbol_count;
} prj_type;

struct prj_archive {
    FILE     *fp;
    uint64_t  file_size;
    prj_type *types;
    int       type_count;
    prj_record *generated;
    size_t generated_count;
};

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t ror32(uint32_t value, unsigned bits)
{
    return (value >> bits) | (value << (32u - bits));
}

/* SHA-256 of one known 496/504-byte record or 1824-byte campaign payload. This private bounded helper is
 * used only after an exact Mercs anomaly's size/name/sum checks pass.
 * It fingerprints all bytes, including unused header fields; no game payload
 * is embedded and no supplied archive is rewritten. */
static void record_sha256_known(const uint8_t *record, unsigned size, uint8_t digest[32])
{
    static const uint32_t constants[64] = {
        0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
        0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
        0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
        0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
        0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
        0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
        0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
        0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
    };
    uint32_t state[8] = {0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                         0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    uint8_t padded[1856] = {0};
    uint64_t bit_length = (uint64_t)size * 8u;
    unsigned block, i, padded_size=(size+9u+63u)&~63u;
    if(size!=496&&size!=504&&size!=1824){memset(digest,0,32);return;}
    memcpy(padded, record, size);
    padded[size] = 0x80;
    for (i = 0; i < 8; i++) padded[padded_size-1u-i] = (uint8_t)(bit_length >> (i * 8u));
    for (block = 0; block < padded_size/64u; block++) {
        uint32_t words[64], a, b, c, d, e, f, g, h;
        const uint8_t *p = padded + block * 64u;
        for (i = 0; i < 16; i++)
            words[i] = ((uint32_t)p[i*4u] << 24) | ((uint32_t)p[i*4u+1] << 16) |
                       ((uint32_t)p[i*4u+2] << 8) | p[i*4u+3];
        for (i = 16; i < 64; i++) {
            uint32_t x = words[i-15], y = words[i-2];
            words[i] = words[i-16] + (ror32(x,7) ^ ror32(x,18) ^ (x >> 3)) +
                       words[i-7] + (ror32(y,17) ^ ror32(y,19) ^ (y >> 10));
        }
        a=state[0]; b=state[1]; c=state[2]; d=state[3];
        e=state[4]; f=state[5]; g=state[6]; h=state[7];
        for (i = 0; i < 64; i++) {
            uint32_t t1 = h + (ror32(e,6) ^ ror32(e,11) ^ ror32(e,25)) +
                          ((e & f) ^ (~e & g)) + constants[i] + words[i];
            uint32_t t2 = (ror32(a,2) ^ ror32(a,13) ^ ror32(a,22)) +
                          ((a & b) ^ (a & c) ^ (b & c));
            h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
        state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
    }
    for (i = 0; i < 32; i++) digest[i] = (uint8_t)(state[i/4u] >> ((3u-i%4u)*8u));
}

static int known_mercs_checksum_record(const uint8_t *record, uint32_t size, uint32_t sum)
{
    static const struct {
        uint32_t size, stored, computed;
        uint16_t id;
        char name[16],filename[16];
        uint8_t fingerprint[32];
    } known[] = {
        {496,0x41f3,0x41f4,6,"ASA00STD","ASA00STD.MEK",{
            0xb6,0xe2,0xe8,0x19,0xe2,0x90,0x1c,0xda,0x90,0x0c,0xec,0x5b,0xa1,0x3f,0xab,0x5e,
            0xb6,0x0e,0xa8,0xb0,0xc6,0xdc,0x12,0x91,0xf4,0xac,0x7e,0xbe,0xbf,0x2c,0x3e,0xb5}},
        {504,0x460c,0x4763,248,"JEN50STD","JEN50STD.MEK",{
            0x9d,0x24,0xd7,0x03,0x4b,0xca,0x03,0xee,0xd4,0x71,0xfc,0xec,0xb1,0xc6,0x81,0x86,
            0x5f,0xee,0x5c,0x22,0x5d,0x79,0x06,0x87,0x30,0x24,0xc7,0x30,0xc7,0x23,0x91,0x53}}
    };
    uint8_t actual[32];size_t i;
    /* Keep each edition's supplied condition, including JEN50STD's different
     * head/leg armor. Match the entire record, not just its name or additive
     * sum. Original PRJ checksum policy remains unverified; these are narrow
     * port compatibility rules documented by the two checksum audit reports. */
    for(i=0;i<sizeof known/sizeof known[0];i++){
        if(size!=known[i].size||sum!=known[i].computed||rd32(record+8)!=known[i].stored||
           memcmp(record+12,"MEK\0",4)||rd16(record+24)!=known[i].id||
           memcmp(record+30,known[i].name,16)||memcmp(record+46,known[i].filename,16))continue;
        record_sha256_known(record,size,actual);
        return memcmp(actual,known[i].fingerprint,sizeof actual)==0;
    }
    return 0;
}

int prj_known_mercs_campaign_tail(const prj_record *r)
{
    static const uint8_t fingerprint[32]={
        0xd0,0xdb,0x99,0xa5,0x7a,0xc5,0x4e,0xad,0x1a,0xf0,0x2c,0x8e,0x12,0xc8,0x89,0x2c,
        0xfa,0x8a,0x48,0x8f,0x6a,0xc6,0xe4,0xef,0x9c,0x7d,0x1a,0x36,0x24,0xd3,0xe2,0x61};
    uint8_t actual[32];
    if(!r||!r->data||r->size!=1824||strcmp(r->tag,"BWD")||strcasecmp(r->name,"BET4CAM1"))return 0;
    record_sha256_known(r->data,1824,actual);
    return memcmp(actual,fingerprint,sizeof actual)==0;
}

static void copy_name(char *dst, const uint8_t *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i]; i++) dst[i] = (char)src[i];
    dst[i] = '\0';
}

static void tag_from(char dst[5], const uint8_t *src)
{
    copy_name(dst, src, 4);
}

static int read_at(prj_archive *a, uint64_t off, void *buf, size_t len)
{
    if (off > a->file_size || len > a->file_size - off) return PRJ_ERR_FORMAT;
    if (fseek(a->fp, (long)off, SEEK_SET) != 0) return PRJ_ERR_IO;
    if (fread(buf, 1, len, a->fp) != len) return PRJ_ERR_IO;
    return PRJ_OK;
}

/* Reads an INDX or SYMB block into a malloc'd buffer after validating its header. */
static int load_block(prj_archive *a, uint32_t off, uint32_t size, const char *magic,
                      const char *tag, uint8_t **out, uint16_t *capacity)
{
    uint8_t *buf;
    int rc;

    *out = NULL;
    if (size < 22) return PRJ_ERR_FORMAT;
    buf = (uint8_t *)malloc(size);
    if (!buf) return PRJ_ERR_NOMEM;
    rc = read_at(a, off, buf, size);
    if (rc != PRJ_OK) { free(buf); return rc; }
    if (memcmp(buf, magic, 4) != 0 || rd32(buf + 4) + 8u != size ||
        strncmp((const char *)buf + 12, tag, strlen(tag)) != 0) {
        free(buf);
        return PRJ_ERR_FORMAT;
    }
    *capacity = rd16(buf + 16);
    *out = buf;
    return PRJ_OK;
}

static int load_type(prj_archive *a, prj_type *t, const uint8_t *ent)
{
    uint32_t indx_off = rd32(ent + 4), indx_size = rd32(ent + 8);
    uint32_t symb_off = rd32(ent + 12), symb_size = rd32(ent + 16);
    uint8_t *blk;
    uint16_t cap;
    int rc, i;

    tag_from(t->tag, ent);
    if (strcmp(t->tag, "FREE") == 0) return PRJ_OK; /* free list: nothing to index */

    rc = load_block(a, indx_off, indx_size, "INDX", t->tag, &blk, &cap);
    if (rc != PRJ_OK) return rc;
    if (22u + (uint32_t)cap * 8u > indx_size) { free(blk); return PRJ_ERR_FORMAT; }
    t->slots = (prj_slot *)calloc(cap ? cap : 1, sizeof *t->slots);
    if (!t->slots) { free(blk); return PRJ_ERR_NOMEM; }
    t->slot_count = cap;
    for (i = 0; i < cap; i++) {
        t->slots[i].offset = rd32(blk + 22 + i * 8);
        t->slots[i].size = rd32(blk + 22 + i * 8 + 4);
    }
    free(blk);

    rc = load_block(a, symb_off, symb_size, "SYMB", t->tag, &blk, &cap);
    if (rc != PRJ_OK) return rc;
    if (22u + (uint32_t)cap * 18u > symb_size) { free(blk); return PRJ_ERR_FORMAT; }
    {
        uint16_t count = rd16(blk + 18);
        if (count > cap) count = cap;
        t->symbols = (prj_symbol *)calloc(count ? count : 1, sizeof *t->symbols);
        if (!t->symbols) { free(blk); return PRJ_ERR_NOMEM; }
        for (i = 0; i < count; i++) {
            const uint8_t *e = blk + 22 + i * 18;
            copy_name(t->symbols[i].name, e, PRJ_NAME_LEN);
            t->symbols[i].id = rd16(e + 16);
        }
        t->symbol_count = count;
    }
    free(blk);
    return PRJ_OK;
}

prj_archive *prj_open(const char *path, char *err, size_t errlen)
{
    prj_archive *a;
    uint8_t hdr[26];
    uint8_t *ents = NULL;
    int count, i, rc;

#define FAIL(msg) do { if (err) snprintf(err, errlen, "%s", msg); goto fail; } while (0)

    a = (prj_archive *)calloc(1, sizeof *a);
    if (!a) { if (err) snprintf(err, errlen, "out of memory"); return NULL; }
    a->fp = fopen(path, "rb");
    if (!a->fp) FAIL("cannot open archive");
    if (fseek(a->fp, 0, SEEK_END) != 0) FAIL("seek failed");
    a->file_size = (uint64_t)ftell(a->fp);

    if (read_at(a, 0, hdr, sizeof hdr) != PRJ_OK) FAIL("file too short");
    if (memcmp(hdr, "PROJ", 4) != 0) FAIL("not a PROJ archive");
    if (rd32(hdr + 4) + 8u != a->file_size) FAIL("size field does not match file size");
    if (memcmp(hdr + 12, "DDIT", 4) != 0) FAIL("missing DDIT directory");

    count = rd16(hdr + 24);
    if (count <= 0 || count > 256) FAIL("implausible type count");
    ents = (uint8_t *)malloc((size_t)count * 24);
    if (!ents) FAIL("out of memory");
    if (read_at(a, 26, ents, (size_t)count * 24) != PRJ_OK) FAIL("truncated type table");

    a->types = (prj_type *)calloc((size_t)count, sizeof *a->types);
    if (!a->types) FAIL("out of memory");
    a->type_count = count;
    for (i = 0; i < count; i++) {
        rc = load_type(a, &a->types[i], ents + i * 24);
        if (rc != PRJ_OK) {
            if (err) snprintf(err, errlen, "type %d (%.4s): %s", i, (const char *)(ents + i * 24),
                              prj_strerror(rc));
            goto fail;
        }
    }
    free(ents);
    return a;
#undef FAIL
fail:
    free(ents);
    prj_close(a);
    return NULL;
}

void prj_close(prj_archive *a)
{
    int i;
    if (!a) return;
    for (i=0;i<(int)a->generated_count;i++) prj_record_free(&a->generated[i]);
    free(a->generated);
    if (a->types) {
        for (i = 0; i < a->type_count; i++) {
            free(a->types[i].slots);
            free(a->types[i].symbols);
        }
        free(a->types);
    }
    if (a->fp) fclose(a->fp);
    free(a);
}

int prj_type_count(const prj_archive *a) { return a->type_count; }

const char *prj_type_tag(const prj_archive *a, int type)
{
    return (type >= 0 && type < a->type_count) ? a->types[type].tag : NULL;
}

int prj_find_type(const prj_archive *a, const char *tag)
{
    int i;
    for (i = 0; i < a->type_count; i++)
        if (strcasecmp(a->types[i].tag, tag) == 0) return i;
    return -1;
}

int prj_symbol_count(const prj_archive *a, int type)
{
    return (type >= 0 && type < a->type_count) ? a->types[type].symbol_count : 0;
}

const char *prj_symbol_name(const prj_archive *a, int type, int index)
{
    if (type < 0 || type >= a->type_count) return NULL;
    if (index < 0 || index >= a->types[type].symbol_count) return NULL;
    return a->types[type].symbols[index].name;
}

int prj_symbol_id(const prj_archive *a, int type, int index)
{
    if (type < 0 || type >= a->type_count) return -1;
    if (index < 0 || index >= a->types[type].symbol_count) return -1;
    return a->types[type].symbols[index].id;
}

int prj_find_id(const prj_archive *a, int type, const char *name)
{
    int i;
    if (type < 0 || type >= a->type_count) return -1;
    for(i=0;i<(int)a->generated_count;i++)
        if(!strcasecmp(a->generated[i].tag,a->types[type].tag)&&
           !strcasecmp(a->generated[i].name,name))return a->generated[i].id;
    for (i = 0; i < a->types[type].symbol_count; i++)
        if (strcasecmp(a->types[type].symbols[i].name, name) == 0) return a->types[type].symbols[i].id;
    return -1;
}

uint32_t prj_record_size(const prj_archive *a, int type, int id)
{
    if (type < 0 || type >= a->type_count) return 0;
    if(id>=0xf000&&id<0xf000+(int)a->generated_count&&
       !strcasecmp(a->generated[id-0xf000].tag,a->types[type].tag))
        return (uint32_t)a->generated[id-0xf000].size+PRJ_RECORD_HEADER;
    if (id <= 0 || id >= a->types[type].slot_count) return 0;
    return a->types[type].slots[id].offset ? a->types[type].slots[id].size : 0;
}

int prj_read(prj_archive *a, int type, int id, prj_record *out)
{
    const prj_type *t;
    uint8_t *buf;
    uint32_t off, size, sum = 0;
    size_t i;
    int rc;

    memset(out, 0, sizeof *out);
    if (type < 0 || type >= a->type_count) return PRJ_ERR_NOT_FOUND;
    t = &a->types[type];
    if(id>=0xf000&&id<0xf000+(int)a->generated_count&&
       !strcasecmp(a->generated[id-0xf000].tag,t->tag)){
        const prj_record *g=&a->generated[id-0xf000];
        uint8_t *copy=malloc(g->size?g->size:1);
        if(!copy)return PRJ_ERR_NOMEM;
        memcpy(copy,g->data,g->size);*out=*g;out->data=copy;return PRJ_OK;
    }
    if (id <= 0 || id >= t->slot_count || t->slots[id].offset == 0) return PRJ_ERR_NOT_FOUND;
    off = t->slots[id].offset;
    size = t->slots[id].size;
    if (size < PRJ_RECORD_HEADER) return PRJ_ERR_FORMAT;

    buf = (uint8_t *)malloc(size);
    if (!buf) return PRJ_ERR_NOMEM;
    rc = read_at(a, off, buf, size);
    if (rc != PRJ_OK) { free(buf); return rc; }

    if (memcmp(buf, "DATA", 4) != 0 || rd32(buf + 4) + 8u != size ||
        strncmp((const char *)buf + 12, t->tag, strlen(t->tag)) != 0 || rd16(buf + 24) != id) {
        free(buf);
        return PRJ_ERR_FORMAT;
    }
    for (i = 12; i < size; i++) sum += buf[i];
    out->checksum = rd32(buf + 8);
    out->computed_checksum = sum;
    if (sum != out->checksum) {
        if (!known_mercs_checksum_record(buf, size, sum)) { free(buf); return PRJ_ERR_CHECKSUM; }
        out->checksum_compat = 1;
    }

    tag_from(out->tag, buf + 12);
    memcpy(out->stamp, buf + 16, 4);
    out->year = rd32(buf + 20);
    out->id = (uint16_t)id;
    copy_name(out->name, buf + 30, PRJ_NAME_LEN);
    copy_name(out->filename, buf + 46, PRJ_NAME_LEN);

    out->size = size - PRJ_RECORD_HEADER;
    memmove(buf, buf + PRJ_RECORD_HEADER, out->size);
    out->data = buf;
    return PRJ_OK;
}

int prj_read_named(prj_archive *a, const char *tag, const char *name, prj_record *out)
{
    int type = prj_find_type(a, tag);
    int id = prj_find_id(a, type, name);
    if (type < 0 || id < 0) { memset(out, 0, sizeof *out); return PRJ_ERR_NOT_FOUND; }
    return prj_read(a, type, id, out);
}

void prj_record_free(prj_record *r)
{
    if (!r) return;
    free(r->data);
    r->data = NULL;
    r->size = 0;
}

int prj_is_generated(const prj_archive *a,const char *tag,const char *name)
{
    size_t i;if(!a||!tag||!name)return 0;
    for(i=0;i<a->generated_count;i++)
        if(!strcasecmp(a->generated[i].tag,tag)&&!strcasecmp(a->generated[i].name,name))return 1;
    return 0;
}

void prj_free_generated(prj_record *records,size_t count)
{size_t i;for(i=0;i<count;i++)prj_record_free(&records[i]);free(records);}
void prj_restore_generated(prj_archive *a,prj_record *records,size_t count)
{if(a){prj_free_generated(a->generated,a->generated_count);a->generated=records;a->generated_count=count;}}
int prj_get_generated(prj_archive *a,prj_record **records,size_t *count)
{
    prj_record *r=NULL;size_t i;
    if(!a||!records||!count)return PRJ_ERR_FORMAT;
    if(a->generated_count){r=calloc(a->generated_count,sizeof *r);if(!r)return PRJ_ERR_NOMEM;}
    for(i=0;i<a->generated_count;i++){
        int type=prj_find_type(a,a->generated[i].tag);
        if(prj_read(a,type,a->generated[i].id,&r[i])){prj_free_generated(r,a->generated_count);return PRJ_ERR_NOMEM;}
    }
    *records=r;*count=a->generated_count;return PRJ_OK;
}

int prj_set_generated(prj_archive *a,const prj_record *records,size_t count)
{
    prj_record *copy=NULL;size_t i,j,total=0;
    if(!a||count>64||(count&&!records))return PRJ_ERR_FORMAT;
    for(i=0;i<(size_t)a->type_count;i++)if(a->types[i].slot_count>=0xf000)return PRJ_ERR_FORMAT;
    if(count){copy=calloc(count,sizeof *copy);if(!copy)return PRJ_ERR_NOMEM;}
    for(i=0;i<count;i++){
        const prj_record *r=&records[i];size_t len;
        if(!memchr(r->name,0,sizeof r->name)||!(len=strlen(r->name))||
           strspn(r->name,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")!=len||
           !memchr(r->tag,0,sizeof r->tag)||prj_find_type(a,r->tag)<0||
           r->size>8u*1024u*1024u-total||(r->size&&!r->data))goto bad;
        for(j=0;j<i;j++)if(!strcasecmp(copy[j].name,r->name)&&!strcasecmp(copy[j].tag,r->tag))goto bad;
        copy[i]=*r;copy[i].data=malloc(r->size?r->size:1);
        if(!copy[i].data)goto bad;
        memcpy(copy[i].data,r->data,r->size);copy[i].id=(uint16_t)(0xf000+i);
        copy[i].generated=1;copy[i].checksum=copy[i].computed_checksum=0;copy[i].checksum_compat=0;
        total+=r->size;
    }
    for(i=0;i<a->generated_count;i++)prj_record_free(&a->generated[i]);
    free(a->generated);a->generated=copy;a->generated_count=count;return PRJ_OK;
bad:
    for(j=0;j<count;j++)free(copy[j].data);
    free(copy);return PRJ_ERR_FORMAT;
}

void prj_decode_text(uint8_t *buf, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) buf[i] = (uint8_t)(0u - buf[i]);
}

const char *prj_strerror(int code)
{
    switch (code) {
    case PRJ_OK: return "ok";
    case PRJ_ERR_IO: return "I/O error";
    case PRJ_ERR_FORMAT: return "malformed archive data";
    case PRJ_ERR_NOT_FOUND: return "resource not found";
    case PRJ_ERR_CHECKSUM: return "checksum mismatch";
    case PRJ_ERR_NOMEM: return "out of memory";
    default: return "unknown error";
    }
}
