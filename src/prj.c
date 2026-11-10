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

/* SHA-256 of one record or campaign payload (up to 1 MB) - fingerprints for the known bad-checksum records. This private bounded helper is
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
    uint8_t *padded;
    uint64_t bit_length = (uint64_t)size * 8u;
    unsigned block, i, padded_size=(size+9u+63u)&~63u;
    memset(digest,0,32);
    if(size>(1u<<20)||!(padded=calloc(padded_size,1)))return;
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
    free(padded);
    for (i = 0; i < 32; i++) digest[i] = (uint8_t)(state[i/4u] >> ((3u-i%4u)*8u));
}

static int known_mercs_checksum_record(const uint8_t *record, uint32_t size, uint32_t sum)
{
    /* Records the editions shipped with a stored checksum that is not their byte sum: the DOS CD's ASA00STD, and in
     * the Titanium (16- and 8-bit) and 3Dfx archives a set of MEKs (MON00STD among them - IVA1's ambush, so those two
     * editions' careers stopped at IVA1 with "Cannot read actor loadout"), CELs, POLYs and MEKTABLE. The originals do
     * not check: MERCSW.DLL's record read (0x1001ead0) seeks past the header and reads the payload, nothing summed.
     * Each exception is the whole record's SHA-256, so any other mismatch - a changed byte - is still refused. */
    static const struct {
        uint32_t size, stored, computed;
        uint16_t id;
        char tag[5], name[17], filename[17];
        uint8_t fingerprint[32];
    } known[] = {
        {7750,0x1644aa,0xb8948,1235,"CEL\0","\x42\x39\x45\x58\x43\x46\x30\x33\x00\x00\x00\x00\x00\x00\x00\x00","\x42\x39\x45\x58\x43\x46\x30\x33\x2e\x58\x45\x4c\x00\x00\x00\x00",{0xbf,0x28,0x24,0xb7,0xb4,0x51,0xd5,0xdd,0x83,0x0d,0xaa,0x3f,0xd7,0x6f,0x4f,0x29,0x1c,0x1a,0x99,0x21,0x79,0xb3,0x5e,0xdb,0xb0,0x6e,0x7d,0x34,0x05,0x4d,0x4e,0x4a}}, /* titanium/MW2.PRJ */
        {4301,0xe5349,0x833a7,1271,"CEL\0","\x42\x39\x45\x58\x50\x31\x32\x35\x00\x00\x00\x00\x00\x00\x00\x00","\x42\x39\x45\x58\x50\x31\x32\x35\x2e\x58\x45\x4c\x00\x00\x00\x00",{0xab,0x93,0xfe,0x5f,0x85,0x78,0x24,0x29,0x03,0x1d,0xc3,0xd6,0x6b,0x8c,0xed,0x64,0xcf,0xcb,0x10,0xec,0x42,0xe1,0x1b,0xcb,0x97,0x68,0x54,0xdd,0x0f,0xd4,0xdc,0x8c}}, /* titanium/MW2.PRJ */
        {17462,0x2f98c0,0x2d2b9e,2371,"CEL\0","\x47\x52\x41\x31\x4d\x4e\x54\x53\x00\x00\x00\x00\x00\x00\x00\x00","\x47\x52\x41\x31\x4d\x4e\x54\x53\x2e\x54\x45\x58\x00\x00\x00\x00",{0x8e,0x01,0x88,0x24,0x9d,0xcf,0x5d,0x61,0xde,0x9e,0xf0,0xa7,0xa4,0xdd,0xf1,0x22,0x51,0xf5,0x44,0x8b,0xb9,0xc1,0x6e,0x55,0x3a,0x66,0x14,0x96,0x9e,0x30,0x5f,0x81}}, /* titanium/MW2.PRJ */
        {496,0x41f3,0x41f4,6,"MEK\0","\x41\x53\x41\x30\x30\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x41\x53\x41\x30\x30\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0xb6,0xe2,0xe8,0x19,0xe2,0x90,0x1c,0xda,0x90,0x0c,0xec,0x5b,0xa1,0x3f,0xab,0x5e,0xb6,0x0e,0xa8,0xb0,0xc6,0xdc,0x12,0x91,0xf4,0xac,0x7e,0xbe,0xbf,0x2c,0x3e,0xb5}}, /* titanium/MW2.PRJ */
        {480,0x33f9,0x33fa,288,"MEK\0","\x41\x53\x41\x30\x31\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x41\x53\x41\x30\x31\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0x00,0xce,0xf6,0x48,0xd0,0xa9,0xe0,0xff,0xb6,0x45,0xd2,0x9f,0x0e,0xd4,0xae,0xc3,0xcd,0x91,0xe4,0x88,0xa8,0xdd,0xe8,0xba,0xaf,0xa2,0x28,0xf9,0x2c,0x2f,0x03,0x37}}, /* titanium/MW2.PRJ */
        {472,0x3336,0x33fb,194,"MEK\0","\x41\x53\x41\x35\x30\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x41\x53\x41\x35\x30\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0x3e,0x23,0x3b,0x0b,0x48,0xd1,0xd2,0xf1,0x56,0x5b,0x38,0xa2,0x21,0x4a,0x90,0x0d,0x40,0xb2,0x8c,0x70,0x63,0xe8,0xde,0x63,0x8c,0xe9,0xef,0xf3,0x8a,0x9c,0xb7,0x21}}, /* titanium/MW2.PRJ */
        {488,0x3a9f,0x3b66,206,"MEK\0","\x43\x45\x4e\x35\x30\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x43\x45\x4e\x35\x30\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0x8c,0x5a,0xbd,0x4c,0x82,0xfa,0xd2,0x4b,0xe6,0x71,0xc1,0x15,0x82,0xbe,0x3d,0xbc,0x97,0x35,0x24,0x7a,0xbd,0xbd,0xb2,0xe5,0xaf,0x87,0xf1,0x48,0x2d,0x3c,0x67,0xd5}}, /* titanium/MW2.PRJ */
        {472,0x32fa,0x33c1,209,"MEK\0","\x43\x49\x43\x35\x30\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x43\x49\x43\x35\x30\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0x8c,0xab,0xfb,0xbf,0x2c,0x3f,0x20,0x4f,0x9e,0x36,0x39,0x18,0xe5,0x60,0xa8,0xa5,0xb2,0x07,0xbe,0x37,0x2d,0x9d,0xfd,0xf0,0xff,0x95,0x10,0xa8,0xfa,0xed,0x6b,0xcc}}, /* titanium/MW2.PRJ */
        {456,0x29e5,0x2a75,234,"MEK\0","\x43\x52\x42\x35\x30\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x43\x52\x42\x35\x30\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0x7d,0xe2,0x2a,0x92,0xa5,0xbb,0x8f,0x14,0x5e,0x96,0xfd,0x81,0xf6,0x24,0x24,0xd7,0x1f,0x6d,0x76,0x4d,0xbe,0x34,0x27,0xe4,0x6b,0x2d,0x5a,0xe7,0x28,0xb1,0x18,0xc9}}, /* titanium/MW2.PRJ */
        {480,0x3a5b,0x3aeb,236,"MEK\0","\x43\x59\x43\x35\x30\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x43\x59\x43\x35\x30\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0xb9,0x10,0x2e,0x9e,0x65,0x48,0xbe,0xb7,0x2d,0x18,0x5d,0x50,0xb3,0xa0,0xf2,0x79,0x02,0xc9,0x6a,0x25,0x81,0xc3,0x63,0xfe,0x97,0x7e,0x26,0xec,0xbe,0x60,0x42,0xca}}, /* titanium/MW2.PRJ */
        {496,0x3fc6,0x3fc8,238,"MEK\0","\x44\x52\x41\x35\x30\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x44\x52\x41\x35\x30\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0xc2,0xf2,0x88,0xe1,0xe2,0x09,0xd2,0x1f,0x3b,0x8e,0xfe,0x18,0x7b,0xf6,0x5d,0x13,0x8c,0xb9,0x43,0x06,0xff,0x73,0x7e,0xdb,0x1c,0xec,0xe3,0x1a,0xa8,0x65,0xc6,0x1d}}, /* titanium/MW2.PRJ */
        {464,0x2a84,0x2a90,290,"MEK\0","\x45\x4c\x45\x30\x31\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x45\x4c\x45\x30\x31\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0x87,0x60,0xee,0x7d,0xe6,0x5f,0x7c,0x13,0x20,0x66,0xa0,0x37,0xe4,0xfc,0x38,0xbc,0x17,0xae,0x10,0x22,0xe6,0xdb,0x67,0x76,0x13,0xb1,0xe3,0xb2,0x83,0xb9,0x6d,0xff}}, /* titanium/MW2.PRJ */
        {496,0x4c80,0x4c82,240,"MEK\0","\x46\x4c\x41\x35\x30\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x46\x4c\x41\x35\x30\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0x8d,0xb2,0x28,0x25,0x33,0x9f,0xb5,0xcd,0x41,0xe4,0xf6,0x81,0xde,0xe3,0xfe,0xb9,0xb1,0xe3,0xe7,0xb6,0x75,0x48,0xaf,0xa0,0x6f,0xfb,0x63,0x49,0xee,0xa4,0x62,0xd0}}, /* titanium/MW2.PRJ */
        {504,0x460c,0x4763,248,"MEK\0","\x4a\x45\x4e\x35\x30\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x4a\x45\x4e\x35\x30\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0x9d,0x24,0xd7,0x03,0x4b,0xca,0x03,0xee,0xd4,0x71,0xfc,0xec,0xb1,0xc6,0x81,0x86,0x5f,0xee,0x5c,0x22,0x5d,0x79,0x06,0x87,0x30,0x24,0xc7,0x30,0xc7,0x23,0x91,0x53}}, /* titanium/MW2.PRJ */
        {496,0x495c,0x495d,106,"MEK\0","\x4d\x4f\x4e\x30\x30\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x4d\x4f\x4e\x30\x30\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0xc4,0x3f,0x73,0xb5,0xa4,0x4a,0xf5,0x2a,0xbc,0xe8,0x0a,0xc8,0x94,0xa5,0x17,0x8d,0xf7,0xee,0x77,0x60,0xe9,0x3e,0xd2,0xb2,0x1a,0x1c,0x1e,0xbf,0xba,0x0c,0xeb,0x53}}, /* titanium/MW2.PRJ */
        {552,0x610c,0x6195,270,"MEK\0","\x54\x42\x52\x35\x30\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x54\x42\x52\x35\x30\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0xfb,0x58,0x3d,0xf5,0xc2,0x63,0x99,0x2c,0xdc,0x29,0xbd,0x6c,0xc8,0xcd,0x6d,0xe8,0xb4,0x4b,0x1f,0x6e,0x1e,0x7a,0x37,0x8a,0x7a,0x6b,0xcd,0x17,0x76,0x34,0x1d,0x04}}, /* titanium/MW2.PRJ */
        {496,0x4486,0x44a3,272,"MEK\0","\x54\x48\x47\x35\x30\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x54\x48\x47\x35\x30\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0xc9,0xab,0xf0,0xb7,0x36,0xa7,0xb7,0x50,0x8f,0x88,0x56,0x30,0xc9,0xef,0x63,0xeb,0x97,0xaf,0xad,0x6a,0x33,0xfc,0x8b,0xea,0xb4,0xda,0x67,0xb1,0xf7,0xa7,0xa4,0x7a}}, /* titanium/MW2.PRJ */
        {488,0x413f,0x4296,278,"MEK\0","\x56\x49\x43\x35\x30\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x56\x49\x43\x35\x30\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0x01,0x15,0x63,0xed,0x57,0x21,0x62,0x31,0x4c,0x7e,0xbb,0xc3,0x68,0xa7,0xf2,0xb5,0x03,0x79,0xc4,0x82,0xa3,0x76,0x1f,0x9b,0x24,0xf1,0x9b,0x03,0x2d,0x68,0x1d,0xdb}}, /* titanium/MW2.PRJ */
        {504,0x4e34,0x4ec6,282,"MEK\0","\x57\x48\x4b\x35\x30\x53\x54\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x57\x48\x4b\x35\x30\x53\x54\x44\x2e\x4d\x45\x4b\x00\x00\x00\x00",{0xb1,0xef,0x52,0x7e,0x49,0x08,0x6b,0x63,0x0a,0x75,0xb0,0x7f,0x3d,0x3c,0x93,0xd5,0x60,0xcd,0x7d,0xa0,0x56,0x5a,0x3f,0xb8,0x11,0x86,0xf9,0xfb,0x35,0xd7,0x57,0x2d}}, /* titanium/MW2.PRJ */
        {550,0xb105,0xb34f,4979,"POLY","\x54\x46\x31\x4c\x46\x4c\x45\x47\x00\x00\x00\x00\x00\x00\x00\x00","\x54\x46\x31\x4c\x46\x4c\x45\x47\x2e\x57\x54\x42\x00\x00\x00\x00",{0x0e,0x70,0x8e,0x49,0xb8,0x71,0xeb,0x62,0x82,0x79,0x8d,0x4e,0x50,0x7b,0xdd,0xe1,0x9d,0xef,0x2e,0x6c,0xef,0x1f,0x3d,0x84,0xb8,0x25,0x52,0xe1,0x44,0xff,0x43,0xf3}}, /* titanium/MW2.PRJ */
        {294,0x4163,0x4327,4983,"POLY","\x54\x46\x31\x54\x55\x52\x52\x54\x00\x00\x00\x00\x00\x00\x00\x00","\x54\x46\x31\x54\x55\x52\x52\x54\x2e\x57\x54\x42\x00\x00\x00\x00",{0x9b,0xca,0x51,0xce,0x9d,0x98,0xee,0xc0,0xb3,0xb1,0xd5,0x56,0xec,0x03,0x8d,0x58,0x1e,0x49,0xae,0xf8,0x41,0x11,0x4e,0x7e,0x8b,0x54,0x5b,0x96,0x5a,0x6a,0xa5,0x13}}, /* titanium/MW2.PRJ */
        {8386,0x8032d,0xd6f01,7493,"POLY","\x54\x4c\x42\x4d\x45\x53\x41\x43\x00\x00\x00\x00\x00\x00\x00\x00","\x54\x4c\x42\x4d\x45\x53\x41\x43\x2e\x57\x54\x42\x00\x00\x00\x00",{0x1b,0xfb,0x84,0x1d,0x34,0x3b,0xf0,0xda,0x40,0xc2,0xb6,0x64,0x8e,0x41,0x61,0x69,0xb7,0xf0,0x88,0x75,0xbf,0x15,0xd8,0x9e,0x86,0xbf,0x1d,0x8f,0xc6,0xd8,0xcc,0xf8}}, /* titanium/MW2.PRJ */
        {3324,0x2f854,0x4b18b,7494,"POLY","\x54\x4c\x42\x4d\x45\x53\x41\x44\x00\x00\x00\x00\x00\x00\x00\x00","\x54\x4c\x42\x4d\x45\x53\x41\x44\x2e\x57\x54\x42\x00\x00\x00\x00",{0xb1,0x12,0x0b,0x1d,0x18,0xa5,0x12,0x89,0xd8,0xca,0x96,0x86,0x24,0x05,0xb7,0x5c,0x56,0x7d,0xf3,0xdd,0x7a,0xd1,0xcf,0x85,0x60,0x8e,0xe3,0xa8,0xfe,0x90,0xe5,0x93}}, /* titanium/MW2.PRJ */
        {4598,0x4413c,0x7d729,7495,"POLY","\x54\x4c\x42\x4d\x45\x53\x41\x48\x00\x00\x00\x00\x00\x00\x00\x00","\x54\x4c\x42\x4d\x45\x53\x41\x48\x2e\x57\x54\x42\x00\x00\x00\x00",{0xa4,0xa8,0x20,0xbf,0xe2,0xc3,0x65,0xd7,0x4d,0x8e,0xe0,0x00,0x48,0x04,0xd1,0x47,0xa9,0x8b,0xf7,0xea,0x74,0xaf,0x5e,0x07,0x43,0xb7,0xc8,0xf9,0xcb,0x65,0xd6,0xbf}}, /* titanium/MW2.PRJ */
        {6180,0x52149,0xabe8d,7496,"POLY","\x54\x4c\x42\x4d\x45\x53\x41\x49\x00\x00\x00\x00\x00\x00\x00\x00","\x54\x4c\x42\x4d\x45\x53\x41\x49\x2e\x57\x54\x42\x00\x00\x00\x00",{0x88,0xa4,0x0f,0x01,0xee,0xd3,0x18,0x3a,0xbd,0x9e,0x66,0x70,0xf3,0x32,0xaa,0x01,0xcb,0x30,0xda,0x96,0x19,0xb0,0x23,0xcd,0x4b,0xde,0xc6,0x4f,0xb8,0x3e,0xac,0x75}}, /* titanium/MW2.PRJ */
        {3626,0x7345f,0x73708,8,"TABL","\x4d\x45\x4b\x54\x41\x42\x4c\x45\x00\x00\x00\x00\x00\x00\x00\x00","\x4d\x45\x4b\x54\x41\x42\x4c\x45\x2e\x54\x42\x4c\x00\x00\x00\x00",{0x8b,0x0d,0x47,0x0a,0x83,0x68,0xbe,0x1f,0x30,0xe9,0x48,0x37,0x95,0xb0,0x64,0x9f,0x0c,0xb9,0x9f,0x79,0x8c,0xf8,0xd1,0x96,0xc5,0x8b,0xd7,0x31,0xb2,0x8e,0xa4,0x51}}, /* titanium/8bit/MW2.PRJ */
    };
    uint8_t actual[32];size_t i;
    for(i=0;i<sizeof known/sizeof known[0];i++){
        if(size!=known[i].size||sum!=known[i].computed||rd32(record+8)!=known[i].stored||
           memcmp(record+12,known[i].tag,4)||rd16(record+24)!=known[i].id||
           memcmp(record+30,known[i].name,16)||memcmp(record+46,known[i].filename,16))continue;
        record_sha256_known(record,size,actual);
        if(!memcmp(actual,known[i].fingerprint,sizeof actual))return 1;
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
