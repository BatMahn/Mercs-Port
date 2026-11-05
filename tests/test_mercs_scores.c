/* Original score updates and durable native ending receipts. */
#include "mercsscores.h"
#include "mercsdebrief.h"
#include "mercsfile.h"
#include <assert.h>
#include <limits.h>
#include "../tests/fixtures/dialog/original-scores.h"

static uint64_t hash(const unsigned char *p,size_t size)
{uint64_t h=UINT64_C(14695981039346656037);size_t i;for(i=0;i<size;i++)h=(h^p[i])*UINT64_C(1099511628211);return h;}
static void put32(unsigned char *b,uint32_t x)
{unsigned i;for(i=0;i<4;i++)b[i]=(unsigned char)(x>>(8*i));}
static void initial(mercs_lords_table *t)
{
    unsigned i;memset(t,0,sizeof *t);t->count=10;
    for(i=0;i<10;i++){
        snprintf(t->rows[i].pilot,32,"Pilot %u",i);snprintf(t->rows[i].group,32,"Unit %u",i);
        t->rows[i].wealth=1000-(int32_t)i*100;t->rows[i].highlighted=(unsigned char)((i%3)+1);
    }
}
static mercs_lords_row earned(int32_t wealth)
{mercs_lords_row r;memset(&r,0,sizeof r);strcpy(r.pilot,"Earned Pilot");strcpy(r.group,"Earned Unit");r.wealth=wealth;r.highlighted=7;return r;}
static void exact_original(void)
{
    size_t i,j;unsigned char bytes[694],saved[694];mercs_lords_table t,decoded;
    for(i=0;i<sizeof mercs_score_fixtures/sizeof mercs_score_fixtures[0];i++){
        const mercs_score_fixture *f=&mercs_score_fixtures[i];mercs_lords_row candidate=earned(f->candidate);
        initial(&t);t.count=f->count;
        for(j=0;j<10;j++)t.rows[j].wealth=f->wealth[j];
        assert(!mercs_lords_encode(&t,bytes));assert(!mercs_lords_decode(bytes,sizeof bytes,&decoded));
        assert(!mercs_lords_encode(&decoded,saved)&&!memcmp(bytes,saved,sizeof bytes));
        assert(mercs_lords_submit(&t,&candidate)==f->wrote);
        assert(!mercs_lords_encode(&t,bytes));
        if(hash(bytes,sizeof bytes)!=f->hash)fprintf(stderr,"Original score case%zu mismatch\n",i);
        assert(hash(bytes,sizeof bytes)==f->hash);
    }
    initial(&t);decoded=t;
    {mercs_lords_row bad=earned(2000);memset(bad.pilot,'X',32);
     assert(mercs_lords_submit(&t,&bad)<0&&!memcmp(&t,&decoded,sizeof t));}
}
static void valuation(void)
{
    mercs_save_state *d=mercs_save_create();mercs_lords_row r;
    uint8_t *h,pilot[MERCS_SAVE_PILOT_BYTES]={0},items[2*MERCS_SAVE_ITEM_BYTES]={0};
    mercs_career_mech m={0};int32_t value;
    char token[33],again[33],changed[33];
    assert(d);h=mercs_save_header_edit(d);strcpy((char *)h+0x102,"Saved Unit");
    put32(h+0x122,3044);put32(h+0x12a,1000);strcpy((char *)pilot,"Saved Pilot");pilot[0x2064]=1;
    assert(!mercs_save_set_list(d,MERCS_SAVE_OWNED_PILOTS,pilot,1));
    put32(items+4,500);put32(items+MERCS_SAVE_ITEM_BYTES+4,700);
    assert(!mercs_save_set_list(d,MERCS_SAVE_OWNED_WEAPONS,items,2));
    assert(!mercs_save_set_list(d,MERCS_SAVE_OWNED_AMMO,items,1));
    assert(!mercs_scores_earned(d,&r)&&r.wealth==2700&&!strcmp(r.pilot,"Saved Pilot")&&!strcmp(r.group,"Saved Unit"));
    strcpy((char *)m.bytes+0x81c,"COM");
    assert(!mercs_debrief_mech_value(&m,3044,&value));
    assert(!mercs_save_set_list(d,MERCS_SAVE_OWNED_MECHS,m.bytes,1));
    assert(!mercs_scores_earned(d,&r)&&r.wealth==2700+value);
    put32(h+0x12a,UINT32_MAX);assert(!mercs_save_set_list(d,MERCS_SAVE_OWNED_MECHS,NULL,0));
    assert(!mercs_scores_earned(d,&r)&&r.wealth==1699);
    assert(!mercs_scores_document_token(d,token));
    assert(!mercs_scores_document_token(d,again)&&!strcmp(token,again));
    put32(h+0x12a,UINT32_MAX-1);
    assert(!mercs_scores_document_token(d,changed)&&strcmp(token,changed));
    put32(h+0x12a,UINT32_MAX);
    assert(!mercs_scores_document_token(d,again)&&!strcmp(token,again));
    assert(mercs_scores_document_token(NULL,again)<0);
    memset(h+0x102,'X',32);assert(mercs_scores_earned(d,&r)<0);mercs_save_free(d);
}
static void read_exact(const char *dir,const char *name,unsigned char *bytes,size_t size)
{
    char path[2048];FILE *f;assert(!dp_resolve_in(dir,name,path,sizeof path));f=fopen(path,"rb");assert(f);
    assert(fread(bytes,1,size,f)==size&&fgetc(f)==EOF);assert(!fclose(f));
}
static void remove_file(const char *dir,const char *name)
{char path[2048];assert(!dp_resolve_in(dir,name,path,sizeof path));assert(!remove(path));}

static void persistence(const char *overlay)
{
    static const char token[]="00000000000000000000000000000001";
    static const char receipt[]="00000000000000000000000000000001.DAT";
    mercs_lords_table defaults,loaded;mercs_lords_row candidate=earned(1100),low=earned(-1);
    unsigned char original[694],applied[694],again[694],journal[1510];
    char receipts[2048],error[384],path[2048];
    initial(&defaults);assert(!mercs_lords_encode(&defaults,original));
    assert(!mercs_scores_load(overlay,NULL,&defaults,&loaded,error,sizeof error));
    assert(dp_resolve_in(overlay,"mw2high.cfg",path,sizeof path)!=0);
    assert(!mercs_scores_commit(overlay,NULL,&defaults,token,&candidate,error,sizeof error));
    read_exact(overlay,"mw2high.cfg",applied,sizeof applied);
    assert(!mercs_scores_commit(overlay,NULL,&defaults,token,&candidate,error,sizeof error));
    read_exact(overlay,"mw2high.cfg",again,sizeof again);assert(!memcmp(applied,again,sizeof again));
    assert(mercs_scores_commit(overlay,NULL,&defaults,token,&low,error,sizeof error)<0);
    assert(!mercs_file_directory(overlay,"MERCS-SCORES",receipts,sizeof receipts,error,sizeof error));
    read_exact(receipts,receipt,journal,sizeof journal);
    /* Crash after journal creation, before score rename: a failed recovery
     * must preserve the pending bytes and retry from the original table. */
    remove_file(receipts,receipt);remove_file(overlay,"mw2high.cfg");
    assert(!mercs_atomic_write(receipts,"PENDING.DAT",journal,sizeof journal,error,sizeof error));
    assert(!mercs_file_directory(overlay,"mw2high.cfg",path,sizeof path,error,sizeof error));
    assert(mercs_scores_commit(overlay,NULL,&defaults,token,&candidate,error,sizeof error)<0);
    assert(!rmdir(path));
    assert(!mercs_scores_commit(overlay,NULL,&defaults,token,&candidate,error,sizeof error));
    read_exact(overlay,"mw2high.cfg",again,sizeof again);assert(!memcmp(applied,again,sizeof again));
    /* Crash after score rename but before receipt: retry must not add a
     * second copy of this same earned result. */
    remove_file(receipts,receipt);
    assert(!mercs_atomic_write(receipts,"PENDING.DAT",journal,sizeof journal,error,sizeof error));
    assert(!mercs_scores_commit(overlay,NULL,&defaults,token,&candidate,error,sizeof error));
    read_exact(overlay,"mw2high.cfg",again,sizeof again);assert(!memcmp(applied,again,sizeof again));
    /* A changed score file during interrupted commitment is preserved. */
    remove_file(receipts,receipt);
    assert(!mercs_atomic_write(receipts,"PENDING.DAT",journal,sizeof journal,error,sizeof error));
    assert(!mercs_atomic_write(overlay,"mw2high.cfg",original,sizeof original,error,sizeof error));
    assert(mercs_scores_commit(overlay,NULL,&defaults,token,&candidate,error,sizeof error)<0);
    read_exact(overlay,"mw2high.cfg",again,sizeof again);assert(!memcmp(original,again,sizeof again));
    assert(!mercs_atomic_write(overlay,"mw2high.cfg",applied,sizeof applied,error,sizeof error));
    assert(!mercs_scores_commit(overlay,NULL,&defaults,token,&candidate,error,sizeof error));
    /* A nonqualifying result gets a receipt but leaves disk highlights and
     * all original table bytes untouched. */
    assert(!mercs_scores_commit(overlay,NULL,&defaults,"00000000000000000000000000000002",&low,error,sizeof error));
    read_exact(overlay,"mw2high.cfg",again,sizeof again);assert(!memcmp(applied,again,sizeof again));
    assert(mercs_scores_commit(overlay,NULL,&defaults,"../invalid",&candidate,error,sizeof error)<0);
    assert(!mercs_atomic_write(overlay,"mw2high.cfg",applied,693,error,sizeof error));
    assert(mercs_scores_load(overlay,NULL,&defaults,&loaded,error,sizeof error)<0);
    error[0]=0;
    assert(mercs_scores_commit(overlay,NULL,&defaults,"00000000000000000000000000000003",&candidate,error,sizeof error)<0);
    assert(strstr(error,"score file"));
}
int main(int argc,char **argv)
{
    assert(argc==2);exact_original();valuation();persistence(argv[1]);
    puts("PASS:128 executed original full-score comparisons; saved asset/funds valuation; no duplicate ending, interrupted pre/post-rename recovery, I/O failure preservation, conflicting data and corrupt-score rejection.");
    return 0;
}
