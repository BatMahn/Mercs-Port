#include "bwdplace.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../tests/fixtures/combat-completion/original-bwdplace.h"

static void write32(unsigned char *p,int32_t n)
{uint32_t v=(uint32_t)n;p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static void words(unsigned char *p,const int32_t *v,int n)
{int i;for(i=0;i<n;i++)write32(p+i*4,v[i]);}
static bwd_chunk chunk(const char *tag,const unsigned char *p,size_t n)
{bwd_chunk c;memset(&c,0,sizeof c);memcpy(c.tag,tag,4);c.data=p;c.size=n;return c;}
static int original_math(void)
{
    size_t i;int count=0;
    for(i=0;i<sizeof original_blocks/sizeof original_blocks[0];i++) {
        const original_block_case *c=&original_blocks[i];bwd_transform t;int32_t p[3];
        bwd_transform_block(&t,c->bounds,c->transform);
        if(memcmp(t.m,c->matrix,sizeof t.m)){int j;for(j=0;j<12;j++)if(t.m[j]!=c->matrix[j])fprintf(stderr,"fixture%zu matrix%d native%d original%d\n",i,j,t.m[j],c->matrix[j]);assert(0);}
        memcpy(p,c->point,sizeof p);bwd_transform_point(&t,p);assert(!memcmp(p,c->result,sizeof p));count++;
    }
    return count;
}
typedef struct {int enters,leaves,n;int32_t positions[8][3];} observed;
static int observe(const bwd_mission *m,int r,int k,const bwd_transform *t,void *user)
{
    observed *o=user;bwd_navpoint nav;
    if(k==-1){o->enters++;return 0;}if(k==-2){o->leaves++;return 0;}
    if(!bwd_navp(&m->records[r].chunks[k],&nav)) {
        assert(o->n<8);o->positions[o->n][0]=nav.x;o->positions[o->n][1]=nav.y;o->positions[o->n][2]=nav.z;
        bwd_transform_point(t,o->positions[o->n]);o->n++;
    }
    return 0;
}
static void ordered_includes(void)
{
    unsigned char x[36],b[24],n[72]={0},inc[12]={0xff,0xff,'S','T','A','R','T'},inner[12]={0xff,0xff,'I','N','N','E','R'};
    const int32_t placement[9]={1,1,1,0,0,0,6500,0,-89500},box[6]={0,0,0,1,1,1},point[3]={37500,0,37500};
    bwd_chunk root[7],start[1],sub[2];bwd_record records[3];bwd_mission m;observed o={0};bwd_navpoint nav;
    words(x,placement,9);words(b,box,6);words(n,point,3);write32(n+12,90<<16);
    root[0]=chunk("BLKX",x,sizeof x);root[1]=chunk("INCL",inc,sizeof inc);root[2]=chunk("BLK",b,sizeof b);
    root[3]=chunk("INCL",inner,sizeof inner);root[4]=chunk("INCL",inc,sizeof inc);root[5]=chunk("ENDB",NULL,0);root[6]=chunk("INCL",inc,sizeof inc);
    start[0]=chunk("NAVP",n,sizeof n);sub[0]=chunk("INCL",inc,sizeof inc);sub[1]=chunk("ELSB",NULL,0);
    memset(records,0,sizeof records);strcpy(records[0].name,"ROOT");records[0].chunks=root;records[0].chunk_count=7;
    strcpy(records[1].name,"START");records[1].chunks=start;records[1].chunk_count=1;
    strcpy(records[2].name,"INNER");records[2].chunks=sub;records[2].chunk_count=2;
    memset(&m,0,sizeof m);m.records=records;m.record_count=3;
    assert(!bwd_place_walk(&m,observe,&o));assert(o.n==4&&o.enters==6&&o.leaves==6);
    assert(o.positions[0][0]==37500&&o.positions[0][2]==37500); /* pending alone */
    assert(o.positions[1][0]==44000&&o.positions[1][2]==-52000);
    assert(!memcmp(o.positions[1],o.positions[2],sizeof point)); /* repeated include */
    assert(!memcmp(o.positions[0],o.positions[3],sizeof point)); /* ENDB restores */
    assert(!bwd_place_nav(&m,"start",&nav)&&nav.x==37500&&nav.heading==90);
    root[1]=chunk("BLK",b,sizeof b);root[2]=chunk("ELSB",NULL,0);
    assert(!bwd_place_nav(&m,"START",&nav)&&nav.x==44000&&nav.z==-52000&&nav.heading==90);
    /* BLK/ENDB can themselves cross include file boundaries. */
    root[2]=chunk("ENDB",NULL,0);root[5]=chunk("ELSB",NULL,0);
    assert(!bwd_place_nav(&m,"START",&nav)&&nav.x==37500);
    {unsigned char self[12]={0xff,0xff,'R','O','O','T'};root[0]=chunk("INCL",self,sizeof self);assert(bwd_place_walk(&m,observe,&o)==-1);}
}
static void nested_blocks(void)
{
    unsigned char x[3][36],box[24],nav[72]={0};bwd_chunk c[12];bwd_record r; bwd_mission m;observed o={0};int i,k=0;
    const int32_t b[6]={0,0,0,1,1,1};words(box,b,6);
    for(i=0;i<3;i++) {
        int32_t p[9]={1,1,1,0,0,0,i==0?1:i==1?10:100,0,0};words(x[i],p,9);
        c[k++]=chunk("BLKX",x[i],36);c[k++]=chunk("BLK",box,24);c[k++]=chunk("NAVP",nav,72);
    }
    for(i=0;i<3;i++)c[k++]=chunk("ENDB",NULL,0);
    memset(&r,0,sizeof r);strcpy(r.name,"NESTED");r.chunks=c;r.chunk_count=k;
    memset(&m,0,sizeof m);m.records=&r;m.record_count=1;
    assert(!bwd_place_walk(&m,observe,&o)&&o.n==3);
    for(i=0;i<3;i++)assert(o.positions[i][0]==original_nested_points[i]);
    r.chunk_count--;assert(bwd_place_walk(&m,observe,&o)==-1);
}
static int noop(const bwd_mission *m,int r,int c,const bwd_transform *t,void *u)
{(void)m;(void)r;(void)c;(void)t;(void)u;return 0;}
static void archive_scenes(const char *path)
{
    char error[256];prj_archive *a=prj_open(path,error,sizeof error);int type,i,total=0;
    assert(a);type=prj_find_type(a,"BWD");assert(type>=0);
    for(i=0;i<prj_symbol_count(a,type);i++) {
        const char *name=prj_symbol_name(a,type,i);bwd_mission m;
        if(!strstr(name,"SCN"))continue;
        assert(!bwd_mission_load(a,name,&m));
        if(bwd_place_walk(&m,noop,NULL)){fprintf(stderr,"unbalanced scene %s\n",name);assert(0);}
        bwd_mission_free(&m);total++;
    }
    printf("PASS original include/block scope across %d archive scene trees\n",total);prj_close(a);
}
int main(int argc,char **argv)
{
    int n=original_math();ordered_includes();nested_blocks();
    if(argc>1)archive_scenes(argv[1]);
    printf("PASS %d executed-original DOS/Windows block matrices and NAVP positions; pending, nested, repeated includes, block scope, raw heading and cycle guards\n",n);return 0;
}
