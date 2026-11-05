/* Pure original table decoding and view composition. The external DOS EXE is
 * required: original credits and default score data are not shipped in tests.
 * Draw coordinates/styles/widths/text hashes come from executed original code. */
#include "../tools/mercspanels_view.h"
#include "../tests/fixtures/dialog/panels_original_fixtures.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>

static const panel_fixture *fixture;
static unsigned drawn;
static int background;

static uint64_t hash_update(uint64_t hash,const unsigned char *b,size_t n)
{
    size_t i;
    for (i=0;i<n;i++) hash=(hash^b[i])*UINT64_C(1099511628211);
    return hash;
}
static uint64_t text_hash(const char *s)
{ return hash_update(UINT64_C(14695981039346656037),(const unsigned char *)s,strlen(s)); }

int mercs_art_background(mercs_art *a,int entry)
{ (void)a; background=entry; drawn=0; return 0; }
int mercs_art_width(mercs_art *a,int font,const char *s)
{
    int i;
    uint64_t h=text_hash(s);
    (void)a;
    for (i=0;i<fixture->count;i++)
        if (fixture->text[i].font==font && fixture->text[i].fingerprint==h)
            return fixture->text[i].width;
    assert(!"Missing original font-width fixture");
    return -1;
}
void mercs_art_text(mercs_art *a,int font,int x,int y,const char *s,int ink)
{
    const panel_text *t;
    (void)a;
    assert(drawn<(unsigned)fixture->count);
    t=&fixture->text[drawn++];
    assert(t->x==x && t->y==y && t->font==font && t->ink==ink);
    assert(t->fingerprint==text_hash(s));
}
void mercs_art_text_clipped(mercs_art *a,int font,int x,int y,const char *s,int ink,int top,int bottom)
{
    assert(top==125 && bottom==460);
    mercs_art_text(a,font,x,y,s,ink);
}

static void put32(unsigned char *p,uint32_t v)
{ p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24); }

static void score_fixture(unsigned char raw[694])
{
    unsigned i;
    static const int32_t wealth[4]={0,INT32_MIN,INT32_MAX,1000};
    static const unsigned char flags[4]={0,1,2,1};
    memset(raw,0,694);put32(raw,4);
    for (i=0;i<4;i++) {
        snprintf((char *)raw+4+69*i,32,"Pilot %c",'A'+(int)i);
        snprintf((char *)raw+4+69*i+32,32,"Group %c",'A'+(int)i);
        put32(raw+4+69*i+64,(uint32_t)wealth[i]);raw[4+69*i+68]=flags[i];
    }
}

static void bounds(const unsigned char *exe,size_t length,mercs_panels *data)
{
    mercs_panels *before=malloc(sizeof *before);
    mercs_lords_table lords,old;
    unsigned char raw[695],*changed=malloc(length);
    assert(before && changed);*before=*data;
    assert(mercs_panels_decode(NULL,length,data)==MERCS_PANELS_INVALID);
    assert(mercs_panels_decode(exe,length,NULL)==MERCS_PANELS_INVALID);
    assert(mercs_panels_decode(exe,0,data)==MERCS_PANELS_UNSUPPORTED);
    assert(mercs_panels_decode(exe,length-1,data)==MERCS_PANELS_UNSUPPORTED);
    assert(mercs_panels_decode(exe,length+1,data)==MERCS_PANELS_UNSUPPORTED);
    memcpy(changed,exe,length);changed[0xda59c]^=1;
    assert(mercs_panels_decode(changed,length,data)==MERCS_PANELS_UNSUPPORTED);
    memcpy(changed,exe,length);changed[length-1]^=1;
    assert(mercs_panels_decode(changed,length,data)==MERCS_PANELS_UNSUPPORTED);
    assert(!memcmp(data,before,sizeof *data));
    score_fixture(raw);
    assert(!mercs_lords_decode(raw,694,&lords));old=lords;
    assert(mercs_lords_decode(raw,693,&lords)==MERCS_PANELS_INVALID);
    assert(mercs_lords_decode(raw,695,&lords)==MERCS_PANELS_INVALID);
    put32(raw,11);
    assert(mercs_lords_decode(raw,694,&lords)==MERCS_PANELS_INVALID);
    put32(raw,UINT32_MAX);
    assert(mercs_lords_decode(raw,694,&lords)==MERCS_PANELS_INVALID);
    score_fixture(raw);memset(raw+4,'X',32);
    assert(mercs_lords_decode(raw,694,&lords)==MERCS_PANELS_INVALID);
    score_fixture(raw);memset(raw+36,'X',32);
    assert(mercs_lords_decode(raw,694,&lords)==MERCS_PANELS_INVALID);
    assert(!memcmp(&lords,&old,sizeof lords));
    /* Unused records are not rendered; the original file retains ten slots. */
    memset(raw,255,694);put32(raw,0);
    assert(!mercs_lords_decode(raw,694,&lords) && lords.count==0);
    free(changed);free(before);
}

static void goldens(const mercs_panels *data)
{
    unsigned char raw[694];
    mercs_lords_table custom,empty;
    uint64_t hash=UINT64_C(14695981039346656037);
    unsigned i,nonempty=0,maxlen=0;
    score_fixture(raw);assert(!mercs_lords_decode(raw,sizeof raw,&custom));
    memset(raw,0,sizeof raw);assert(!mercs_lords_decode(raw,sizeof raw,&empty));
    assert(data->defaults.count==10 && !strcmp(data->scores_filename,"mw2high.cfg"));
    for (i=0;i<MERCS_CIRCUIT_ROWS;i++) {
        unsigned len=(unsigned)strlen(data->credits[i]);
        hash=hash_update(hash,(const unsigned char *)data->credits[i],len+1);
        if (len) nonempty++;
        if (len>maxlen) maxlen=len;
    }
    assert(hash==PANEL_CREDITS_FNV64 && nonempty==269 && maxlen==78);
    for (i=0;i<sizeof panel_fixtures/sizeof panel_fixtures[0];i++) {
        fixture=&panel_fixtures[i];
        if (i<3) assert(!mercs_lords_draw((mercs_art *)1,data,i==0 ? &data->defaults : i==1 ? &custom : &empty));
        else assert(!mercs_circuit_draw((mercs_art *)1,data,(unsigned)fixture->scroll));
        assert(background==fixture->background && drawn==(unsigned)fixture->count);
    }
    assert(!mercs_circuit_draw((mercs_art *)1,data,UINT_MAX) && drawn==0);
}

int main(int argc,char **argv)
{
    FILE *file;
    unsigned char *exe;
    long length;
    mercs_panels *data=malloc(sizeof *data);
    if (argc!=2) { fprintf(stderr,"Usage: %s original-DOS-MW2SHELL.EXE\n",argv[0]);return 2; }
    assert(data);file=fopen(argv[1],"rb");assert(file);
    assert(!fseek(file,0,SEEK_END));length=ftell(file);assert(length>0 && length<2000000);
    rewind(file);exe=malloc((size_t)length);assert(exe);
    assert(fread(exe,1,(size_t)length,file)==(size_t)length);assert(!fclose(file));
    assert(!mercs_panels_decode(exe,(size_t)length,data));
    bounds(exe,(size_t)length,data);goldens(data);
    free(exe);free(data);
    puts("PASS: original external DOS tables; 3 Merc Lords layouts and 36 First Circuit frames; corrupt/truncated/unknown input rejection");
    return 0;
}
