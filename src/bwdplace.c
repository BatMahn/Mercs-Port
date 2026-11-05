/* Original Mercs block matrix and include-state traversal. See the executed
 * original fixtures in tests/fixtures/combat-completion/original-bwdplace.*. */
#include "bwdplace.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Executing Mercs DOS 0x485d0 with the x87 initialized produces this table.
 * The original sin interpolator is 0x48380 (Windows 0x10006b20). */
static const int32_t block_sin[257] = {
    0, 3294177, 6588231, 9882037, 13175471, 16468409, 19760727, 23052301,
    26343007, 29632721, 32921319, 36208678, 39494674, 42779183, 46062081, 49343245,
    52622551, 55899876, 59175096, 62448089, 65718730, 68986897, 72252467, 75515317,
    78775323, 82032364, 85286316, 88537057, 91784465, 95028417, 98268791, 101505466,
    104738318, 107967228, 111192073, 114412731, 117629082, 120841004, 124048377, 127251079,
    130448990, 133641990, 136829958, 140012775, 143190321, 146362475, 149529119, 152690133,
    155845399, 158994797, 162138209, 165275516, 168406601, 171531346, 174649633, 177761344,
    180866362, 183964571, 187055854, 190140094, 193217176, 196286983, 199349400, 202404311,
    205451603, 208491159, 211522865, 214546608, 217562273, 220569747, 223568917, 226559670,
    229541892, 232515473, 235480299, 238436260, 241383244, 244321140, 247249837, 250169225,
    253079195, 255979637, 258870441, 261751498, 264622701, 267483941, 270335111, 273176102,
    276006808, 278827123, 281636941, 284436154, 287224659, 290002350, 292769123, 295524873,
    298269497, 301002891, 303724952, 306435579, 309134668, 311822119, 314497829, 317161699,
    319813628, 322453516, 325081264, 327696773, 330299944, 332890680, 335468882, 338034455,
    340587300, 343127323, 345654427, 348168517, 350669499, 353157279, 355631762, 358092856,
    360540468, 362974506, 365394878, 367801493, 370194260, 372573090, 374937893, 377288580,
    379625062, 381947251, 384255060, 386548402, 388827191, 391091341, 393340766, 395575382,
    397795106, 399999852, 402189538, 404364083, 406523403, 408667418, 410796047, 412909209,
    415006826, 417088818, 419155107, 421205615, 423240265, 425258980, 427261684, 429248302,
    431218759, 433172981, 435110894, 437032426, 438937503, 440826055, 442698010, 444553298,
    446391848, 448213592, 450018461, 451806387, 453577303, 455331142, 457067838, 458787326,
    460489540, 462174418, 463841894, 465491908, 467124396, 468739297, 470336550, 471916095,
    473477873, 475021824, 476547892, 478056017, 479546144, 481018217, 482472179, 483907977,
    485325555, 486724862, 488105843, 489468448, 490812625, 492138322, 493445491, 494734082,
    496004046, 497255337, 498487905, 499701706, 500896694, 502072823, 503230049, 504368329,
    505487620, 506587880, 507669066, 508731140, 509774060, 510797787, 511802283, 512787509,
    513753430, 514700008, 515627208, 516534995, 517423335, 518292194, 519141539, 519971339,
    520781563, 521572179, 522343159, 523094472, 523826092, 524537989, 525230138, 525902513,
    526555087, 527187837, 527800739, 528393769, 528966906, 529520127, 530053412, 530566741,
    531060094, 531533454, 531986801, 532420119, 532833392, 533226604, 533599741, 533952788,
    534285731, 534598559, 534891260, 535163823, 535416236, 535648492, 535860581, 536052495,
    536224227, 536375770, 536507119, 536618269, 536709216, 536779956, 536830486, 536860805,
    536870912,
};

/* Mercs DOS 0x485d0 generated inverse-tangent table (Q16 degrees). */
static const int32_t block_atan[257] = {
    0,14667,29334,44001,58666,73329,87990,102648,
    117303,131955,146602,161245,175883,190516,205143,219764,
    234378,248985,263585,278176,292759,307333,321898,336454,
    350999,365533,380057,394569,409070,423558,438033,452496,
    466945,481380,495800,510206,524597,538973,553332,567676,
    582002,596312,610604,624878,639134,653372,667590,681790,
    695969,710129,724268,738386,752483,766559,780613,794645,
    808654,822640,836604,850543,864459,878351,892219,906061,
    919879,933671,947437,961178,974892,988580,1002241,1015874,
    1029481,1043059,1056610,1070133,1083627,1097092,1110528,1123935,
    1137313,1150661,1163979,1177266,1190524,1203750,1216946,1230111,
    1243244,1256346,1269416,1282455,1295461,1308435,1321376,1334285,
    1347160,1360003,1372813,1385589,1398332,1411041,1423716,1436357,
    1448964,1461537,1474075,1486579,1499048,1511482,1523881,1536245,
    1548574,1560868,1573126,1585349,1597536,1609687,1621802,1633882,
    1645925,1657933,1669904,1681839,1693737,1705599,1717425,1729214,
    1740967,1752683,1764362,1776004,1787609,1799178,1810710,1822205,
    1833662,1845083,1856467,1867813,1879123,1890395,1901630,1912828,
    1923989,1935113,1946199,1957249,1968261,1979235,1990173,2001073,
    2011937,2022763,2033551,2044303,2055017,2065695,2076335,2086938,
    2097504,2108033,2118525,2128980,2139399,2149780,2160124,2170432,
    2180703,2190937,2201134,2211295,2221419,2231506,2241557,2251572,
    2261550,2271492,2281398,2291267,2301100,2310897,2320659,2330384,
    2340073,2349727,2359345,2368927,2378473,2387984,2397460,2406900,
    2416305,2425675,2435009,2444309,2453574,2462803,2471998,2481159,
    2490284,2499376,2508432,2517455,2526443,2535397,2544317,2553203,
    2562055,2570873,2579657,2588408,2597126,2605810,2614461,2623079,
    2631663,2640215,2648733,2657219,2665672,2674093,2682481,2690837,
    2699160,2707452,2715711,2723938,2732134,2740297,2748429,2756530,
    2764599,2772637,2780644,2788619,2796564,2804478,2812360,2820213,
    2828035,2835826,2843587,2851318,2859018,2866689,2874330,2881941,
    2889522,2897074,2904597,2912090,2919554,2926989,2934394,2941771,
    2949120,
};

static int32_t r29(uint64_t n)
{
    return (int32_t)((uint32_t)(n >> 29) + (uint32_t)((n >> 28) & 1));
}
static int32_t product(int32_t a, int32_t b)
{
    return r29((uint64_t)((int64_t)a * b));
}
static int32_t sum(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static int32_t neg(int32_t a) { return (int32_t)(0u-(uint32_t)a); }
static int32_t sine(int32_t angle)
{
    uint32_t phase=(uint32_t)product(angle,0x5b05b05b), f=phase, index=(phase>>14)&0x3fc;
    uint64_t p;
    int32_t v;
    if(phase&0x1000000u) { f=~f;index^=0x3fc; }
    index>>=2;
    p=(uint64_t)(uint32_t)(block_sin[index+1]-block_sin[index])*(f&65535);
    v=(int32_t)((uint32_t)block_sin[index]+(uint32_t)(p>>16)+(uint32_t)((p>>15)&1));
    return phase&0x2000000u?neg(v):v;
}
static int32_t cosine(int32_t a) { return sine(sum(a,0x5a0000)); }

int32_t bwd_angle_sin(int32_t angle)
{
    return sine(angle);
}

int32_t bwd_angle_asin(int32_t value)
{
    uint32_t v=value<0?0u-(uint32_t)value:(uint32_t)value, phase;
    int lo=0,hi=256,negative=value<0;
    if(v>=0x20000000u)return value<0?-0x5a0000:0x5a0000;
    while(hi-lo>1) {
        int mid=(lo+hi)/2;
        if((uint32_t)block_sin[mid]<=v)lo=mid;else hi=mid;
    }
    phase=(uint32_t)lo<<16;
    v-=(uint32_t)block_sin[lo];
    if(v)phase|=(uint32_t)(((uint64_t)v<<16)/(uint32_t)(block_sin[lo+1]-block_sin[lo]));
    value=(int32_t)(((uint64_t)phase*0x5a000000u)>>32);
    return negative?-value:value;
}

int32_t bwd_angle_atan2(int32_t y,int32_t x)
{
    uint32_t a=y<0?0u-(uint32_t)y:(uint32_t)y;
    uint32_t b=x<0?0u-(uint32_t)x:(uint32_t)x,ratio,index;
    int32_t angle;
    int flip=a>b;
    if(a==b)angle=45*65536; /* original includes the degenerate (0,0) case */
    else {
        if(flip){uint32_t t=a;a=b;b=t;}
        ratio=a?(uint32_t)(((uint64_t)a<<24)/b):0;
        index=ratio>>16;
        angle=block_atan[index]+(int32_t)(((uint64_t)(ratio&65535)*
              (uint32_t)(block_atan[index+1]-block_atan[index])+32768)>>16);
        if(flip)angle=90*65536-angle;
    }
    if(x<0)angle=180*65536-angle;
    return y<0?-angle:angle;
}

void bwd_transform_angles(const bwd_transform *t,int32_t angles[3])
{
    const int32_t *m=t->m;
    if(m[5]<0x1ff7ceda && m[5]>-0x1ff7ceda)angles[0]=-bwd_angle_asin(m[5]);
    else {
        /* DOS 0x4872c truncates the x87 square root to an integer. */
        int32_t length=(int32_t)sqrtl((long double)m[2]*m[2]+(long double)m[8]*m[8]);
        if(length<2000001) {
            angles[0]=0;angles[1]=m[5]<0?90*65536:-90*65536;
            angles[2]=bwd_angle_atan2(m[1],m[0]);return;
        }
        angles[0]=90*65536-bwd_angle_asin(length);
        if(m[5]<0)angles[0]=-angles[0];
    }
    angles[1]=bwd_angle_atan2(m[2],m[8]);
    angles[2]=bwd_angle_atan2(m[3],m[4]);
}

void bwd_transform_identity(bwd_transform *out)
{
    memset(out,0,sizeof *out);out->m[0]=out->m[4]=out->m[8]=0x20000000;out->block=-1;
}
void bwd_transform_point(const bwd_transform *t, int32_t xyz[3])
{
    int32_t v[3];int i,k;
    for(i=0;i<3;i++) {
        uint64_t n=0;
        for(k=0;k<3;k++)n+=(uint64_t)((int64_t)t->m[i*3+k]*xyz[k]);
        v[i]=sum(r29(n),t->m[9+i]);
    }
    memcpy(xyz,v,sizeof v);
}
void bwd_transform_multiply(bwd_transform *out,const bwd_transform *a,const bwd_transform *b)
{
    bwd_transform r;int i,j,k;
    bwd_transform_identity(&r);r.block=b->block;
    for(i=0;i<3;i++)for(j=0;j<3;j++) {
        uint64_t n=0;
        for(k=0;k<3;k++)n+=(uint64_t)((int64_t)a->m[i*3+k]*b->m[k*3+j]);
        r.m[i*3+j]=r29(n);
    }
    memcpy(r.m+9,b->m+9,12);bwd_transform_point(a,r.m+9);*out=r;
}
static void rotation(bwd_transform *t,int32_t a,int32_t b,int32_t c)
{
    int32_t *m=t->m;
    bwd_transform_identity(t);
    if(!a&&!b) {if(c){m[0]=m[4]=cosine(c);m[3]=sine(c);m[1]=neg(m[3]);}}
    else if(!a&&!c) {m[0]=m[8]=cosine(b);m[2]=sine(b);m[6]=neg(m[2]);}
    else if(!b&&!c) {m[4]=m[8]=cosine(a);m[7]=sine(a);m[5]=neg(m[7]);}
    else {
        int32_t ca=cosine(a),cb=cosine(b),cc=cosine(c),sa=sine(a),sb=sine(b),sc=sine(c);
        m[0]=sum(product(cc,cb),product(product(sc,sa),sb));
        m[3]=product(ca,sc);
        m[6]=sum(product(product(cb,sc),sa),neg(product(cc,sb)));
        m[2]=product(ca,sb);m[5]=neg(sa);m[8]=product(ca,cb);
        m[1]=r29((uint64_t)((int64_t)m[5]*m[6])-(uint64_t)((int64_t)m[3]*m[8]));
        m[4]=r29((uint64_t)((int64_t)m[8]*m[0])-(uint64_t)((int64_t)m[6]*m[2]));
        m[7]=r29((uint64_t)((int64_t)m[2]*m[3])-(uint64_t)((int64_t)m[0]*m[5]));
    }
}
void bwd_transform_torso(bwd_transform *out,int32_t pitch,int32_t twist)
{
    int32_t *m=out->m,cp,sp,cy,sy;
    if(!pitch||!twist) { rotation(out,pitch,twist,0);return; }
    bwd_transform_identity(out);
    cp=cosine(pitch);sp=sine(pitch);cy=cosine(twist);sy=sine(twist);
    m[0]=cy;m[3]=product(sp,sy);m[6]=neg(product(cp,sy));
    m[2]=sy;m[5]=neg(product(sp,cy));m[8]=product(cp,cy);
    /* The original fills the remaining column by a Q29 cross product
     * (Windows 0x100891d0), including its rounding. */
    m[1]=r29((uint64_t)((int64_t)m[5]*m[6])-(uint64_t)((int64_t)m[3]*m[8]));
    m[4]=r29((uint64_t)((int64_t)m[8]*m[0])-(uint64_t)((int64_t)m[6]*m[2]));
    m[7]=r29((uint64_t)((int64_t)m[2]*m[3])-(uint64_t)((int64_t)m[0]*m[5]));
}
void bwd_transform_block(bwd_transform *out,const int32_t bounds[6],const int32_t x[9])
{
    int i;int32_t center[3],minus[3];
    rotation(out,x[3],x[4],x[5]);
    for(i=0;i<3;i++) {
        int32_t lo=bounds[i],extent=bounds[3+i],hi;
        if(extent<0){lo=sum(lo,extent);extent=neg(extent);}
        hi=sum(lo,extent);center[i]=sum(lo,hi)/2;minus[i]=neg(center[i]);
    }
    bwd_transform_point(out,minus);
    for(i=0;i<3;i++)out->m[9+i]=sum(sum(minus[i],x[6+i]),center[i]);
}

static int32_t read32(const unsigned char *p)
{
    return (int32_t)((uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24);
}
#define BLOCK_MAX 32
#define INCLUDE_MAX 128
typedef struct {
    const bwd_mission *m; bwd_place_visitor visit;void *user;
    bwd_transform identity,blocks[BLOCK_MAX];
    int parents[BLOCK_MAX],stack[BLOCK_MAX+1],depth,count,active;
    int32_t pending[9]; int path[INCLUDE_MAX],rec_depth;
} walk_state;
static void reset_pending(walk_state *s)
{
    memset(s->pending,0,sizeof s->pending);s->pending[0]=s->pending[1]=s->pending[2]=1;
}
static const bwd_transform *active(const walk_state *s)
{
    return s->active<0?&s->identity:&s->blocks[s->active];
}
static int walk_record(walk_state *s,int ri)
{
    const bwd_record *r=&s->m->records[ri];int k,rc;
    if(s->rec_depth>=INCLUDE_MAX)return -1;
    for(k=0;k<s->rec_depth;k++)if(s->path[k]==ri)return -1;
    s->path[s->rec_depth++]=ri;
    if((rc=s->visit(s->m,ri,-1,active(s),s->user)))return rc;
    for(k=0;k<r->chunk_count;k++) {
        const bwd_chunk *c=&r->chunks[k];int i;
        if(!strcmp(c->tag,"BLKX")) {
            if(c->size<36)return -1;
            for(i=0;i<9;i++)s->pending[i]=read32(c->data+i*4);
        } else if(!strcmp(c->tag,"BLK")) {
            int32_t bounds[6];int parent;
            if(c->size<24)return -1;
            if(s->count>=BLOCK_MAX)continue; /* original allocation guard */
            for(i=0;i<6;i++)bounds[i]=read32(c->data+i*4);
            parent=s->active;s->parents[s->count]=parent;
            bwd_transform_block(&s->blocks[s->count],bounds,s->pending);
            /* The original loops through all ancestor records, including
             * ancestors whose matrix is already composed. Preserve that
             * nested-block behavior rather than silently simplifying it. */
            while(parent>=0) {
                bwd_transform_multiply(&s->blocks[s->count],&s->blocks[parent],&s->blocks[s->count]);
                parent=s->parents[parent];
            }
            s->blocks[s->count].block=s->count;
            s->stack[++s->depth]=s->count;s->active=s->count++;
            reset_pending(s);
        } else if(!strcmp(c->tag,"ENDB")) {
            if(s->depth<1)return -1;
            s->active=s->stack[--s->depth];
        } else if(!strcmp(c->tag,"ELSB")) {
            /* Original handler 0x4378c is a no-op. */
        } else {
            if((rc=s->visit(s->m,ri,k,active(s),s->user)))return rc;
            if(!strcmp(c->tag,"INCL")) {
                uint16_t id;char name[11];
                if(bwd_incl(c,&id,name))return -1;
                for(i=0;i<s->m->record_count;i++)if(!strcasecmp(s->m->records[i].name,name))break;
                if(i<s->m->record_count&&(rc=walk_record(s,i)))return rc;
            }
        }
    }
    if((rc=s->visit(s->m,ri,-2,active(s),s->user)))return rc;
    s->rec_depth--;return 0;
}
int bwd_place_walk(const bwd_mission *m,bwd_place_visitor visit,void *user)
{
    walk_state s;int rc;
    if(!m||!visit||m->record_count<1)return -1;
    memset(&s,0,sizeof s);s.m=m;s.visit=visit;s.user=user;s.active=-1;s.stack[0]=-1;
    bwd_transform_identity(&s.identity);reset_pending(&s);
    rc=walk_record(&s,0);
    return rc?rc:(s.depth?-1:0);
}
typedef struct {const char *name;bwd_navpoint *out;} nav_search;
static int find_nav(const bwd_mission *m,int r,int k,const bwd_transform *t,void *user)
{
    nav_search *q=user;int32_t p[3];
    if(k<0||strcasecmp(m->records[r].name,q->name)||bwd_navp(&m->records[r].chunks[k],q->out))return 0;
    p[0]=q->out->x;p[1]=q->out->y;p[2]=q->out->z;bwd_transform_point(t,p);
    q->out->x=p[0];q->out->y=p[1];q->out->z=p[2];return 1;
}
int bwd_place_nav(const bwd_mission *m,const char *name,bwd_navpoint *out)
{
    nav_search s={name,out};
    if(!name||!out)return -1;
    return bwd_place_walk(m,find_nav,&s)==1?0:-1;
}
