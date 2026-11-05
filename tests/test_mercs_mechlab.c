/* Original-execution numerical fixtures plus transaction/regression checks.
 * No original executable, image, sound or MEK file is needed by this target. */
#include "mercsmechlab.h"
#include "mek.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fixtures/mercs_mechlab_original.h"

static size_t checks;
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#x);abort();} checks++; } while(0)
static uint32_t get(const uint8_t *p,size_t at)
{ return (uint32_t)p[at]|(uint32_t)p[at+1]<<8|(uint32_t)p[at+2]<<16|(uint32_t)p[at+3]<<24; }
static void put(uint8_t *p,size_t at,uint32_t n)
{ p[at]=(uint8_t)n;p[at+1]=(uint8_t)(n>>8);p[at+2]=(uint8_t)(n>>16);p[at+3]=(uint8_t)(n>>24); }
static uint64_t hash(const uint8_t *p,size_t n)
{ uint64_t h=UINT64_C(14695981039346656037);size_t i;for(i=0;i<n;i++)h=(h^p[i])*UINT64_C(1099511628211);return h; }
static int compare_item(const void *a,const void *b){return memcmp(a,b,26);}
static uint64_t item_hash(const mercs_save_state *d,enum mercs_save_list list)
{
    size_t n=mercs_save_list_count(d,list),i;uint8_t *p=malloc(n*26+1);uint64_t h;
    CHECK(p);for(i=0;i<n;i++)memcpy(p+26*i,mercs_save_list_record(d,list,i),26);
    qsort(p,n,26,compare_item);h=hash(p,n*26);free(p);return h;
}
static uint64_t document_hash(const mercs_save_state *d)
{
    uint8_t *p=NULL;size_t n=0;uint64_t h;
    CHECK(!mercs_save_encode(d,1,&p,&n));h=hash(p,n);free(p);return h;
}
static mercs_save_state *seed(int mode,int clan)
{
    mercs_save_state *d=mercs_save_create();mercs_career_mech_pair pair;
    CHECK(d);memcpy(pair.current.bytes,mlab_seed,sizeof pair.current.bytes);
    if(clan)pair.current.bytes[0x84f]=0;
    pair.reference=pair.current;CHECK(!mercs_save_set_owned(d,&pair,1));
    CHECK(!mercs_save_set_list(d,MERCS_SAVE_OWNED_WEAPONS,mlab_input_weapons,sizeof mlab_input_weapons/26));
    CHECK(!mercs_save_set_list(d,MERCS_SAVE_OWNED_AMMO,mlab_input_ammo,sizeof mlab_input_ammo/26));
    put(mercs_save_header_edit(d),0x12a,100000000);put(mercs_save_header_edit(d),0x23bb,(uint32_t)mode);
    return d;
}
static int apply(mercs_mechlab_session *s,const mlab_op *a)
{
    if(a->op==0)return mercs_mechlab_select_location(s,a->a);
    if(a->op==1)return mercs_mechlab_add(s,(unsigned)a->a,a->b);
    if(a->op==2)return mercs_mechlab_remove(s,a->a);
    if(a->op==3)return mercs_mechlab_adjust(s,(enum mercs_mechlab_control)a->a,a->b);
    if(a->op==4)return mercs_mechlab_place(s,a->a);
    return -999;
}
static void original_fixtures(void)
{
    size_t i,j;
    for(i=0;i<sizeof mlab_fixtures/sizeof mlab_fixtures[0];i++){
        const mlab_fixture *f=mlab_fixtures+i;mercs_save_state *d=seed(f->mode,f->clan),*cancel=NULL;
        mercs_mechlab_session *s=NULL;uint64_t before=document_hash(d),actual;
        CHECK(!mercs_mechlab_begin(d,0,&s));
        for(j=0;j<f->count;j++){
            int rc=apply(s,f->ops+j);
            if(rc)fprintf(stderr,"%s action%zu returned%d\n",f->name,j,rc);
            CHECK(rc==0);
        }
        actual=hash(mercs_mechlab_current(s)->bytes,0x850);
        if(actual!=f->mech)fprintf(stderr,"%s mech expected%016" PRIx64 " actual%016" PRIx64 "\n",f->name,f->mech,actual);
        CHECK(actual==f->mech);CHECK(mercs_mechlab_cost(s)==f->cost);CHECK(mercs_mechlab_pending(s,NULL)==f->pending);
        CHECK(mercs_save_list_count(mercs_mechlab_document(s),MERCS_SAVE_OWNED_WEAPONS)==f->nw);
        CHECK(mercs_save_list_count(mercs_mechlab_document(s),MERCS_SAVE_OWNED_AMMO)==f->na);
        actual=item_hash(mercs_mechlab_document(s),MERCS_SAVE_OWNED_WEAPONS);
        if(actual!=f->weapons)fprintf(stderr,"%s weapons expected%016" PRIx64 " actual%016" PRIx64 "\n",f->name,f->weapons,actual);
        CHECK(actual==f->weapons);CHECK(item_hash(mercs_mechlab_document(s),MERCS_SAVE_OWNED_AMMO)==f->ammo);
        CHECK(document_hash(d)==before);CHECK(!mercs_mechlab_finish(s,0,&cancel));CHECK(document_hash(cancel)==before);
        mercs_save_free(cancel);mercs_mechlab_free(s);mercs_save_free(d);
    }
}
static void transaction_checks(void)
{
    mercs_save_state *d=seed(1,0),*out=NULL,*decoded=NULL;mercs_mechlab_session *s=NULL;
    uint8_t *bytes=NULL,raw[MERCS_CAREER_DEPLOY_MAX_BYTES],sentinel[32];size_t n,written;
    uint64_t before;uint32_t mass;mek_def loadout;
    CHECK(!mercs_mechlab_begin(d,0,&s));CHECK(!mercs_mechlab_select_location(s,2));
    before=document_hash(mercs_mechlab_document(s));
    CHECK(mercs_mechlab_add(s,0,24)==MERCS_MECHLAB_NO_SPACE);CHECK(document_hash(mercs_mechlab_document(s))==before);
    CHECK(mercs_mechlab_add(s,2,2)==MERCS_MECHLAB_HEATSINK_TYPE);CHECK(document_hash(mercs_mechlab_document(s))==before);
    CHECK(mercs_mechlab_remove(s,0)==MERCS_MECHLAB_FIXED);CHECK(document_hash(mercs_mechlab_document(s))==before);
    CHECK(mercs_mechlab_select_location(s,-1)==MERCS_MECHLAB_INVALID);CHECK(document_hash(mercs_mechlab_document(s))==before);
    CHECK(!mercs_mechlab_select_location(s,1));mass=get(mercs_mechlab_current(s)->bytes,0x204);
    CHECK(!mercs_mechlab_add(s,2,10));CHECK(mercs_mechlab_add(s,2,10)==MERCS_MECHLAB_NO_ITEM);
    CHECK(!mercs_mechlab_remove(s,1));CHECK(get(mercs_mechlab_current(s)->bytes,0x204)==mass);
    CHECK(!get(mercs_mechlab_current(s)->bytes,0x7f4));CHECK(!mercs_mechlab_add(s,2,10));
    CHECK(!mercs_mechlab_rollback(s));CHECK(!mercs_mechlab_dirty(s));
    CHECK(!mercs_mechlab_add(s,2,5));CHECK(mercs_mechlab_add(s,2,5)==MERCS_MECHLAB_NO_ITEM);
    CHECK(!mercs_mechlab_remove(s,1));CHECK(!mercs_mechlab_rollback(s));
    CHECK(!mercs_mechlab_add(s,2,8));CHECK(mercs_mechlab_pending(s,NULL)==14);
    CHECK(mercs_mechlab_finish(s,1,&out)==MERCS_MECHLAB_PLACEMENT&&!out);
    CHECK(mercs_mechlab_adjust(s,MERCS_MECHLAB_ENGINE,1)==MERCS_MECHLAB_PLACEMENT);
    CHECK(!mercs_mechlab_place(s,1));CHECK(!mercs_mechlab_remove(s,1));CHECK(mercs_mechlab_pending(s,NULL)==14);
    CHECK(!mercs_mechlab_rollback(s));CHECK(!mercs_mechlab_pending(s,NULL));
    CHECK(!mercs_mechlab_rename(s,"  new design  "));CHECK(!strcmp((const char *)mercs_mechlab_current(s)->bytes+0x100,"NEW DESIGN"));
    before=document_hash(mercs_mechlab_document(s));CHECK(mercs_mechlab_rename(s,"1234567890123456789")==MERCS_MECHLAB_LIMIT);
    CHECK(document_hash(mercs_mechlab_document(s))==before);CHECK(!mercs_mechlab_rollback(s));
    mercs_mechlab_free(s);s=NULL;
    put(mercs_save_header_edit(d),0x12a,50000);CHECK(!mercs_mechlab_begin(d,0,&s));
    CHECK(!mercs_mechlab_select_location(s,4));CHECK(!mercs_mechlab_remove(s,4));
    CHECK(mercs_mechlab_cost(s)==50000);CHECK(!mercs_mechlab_finish(s,1,&out));
    CHECK(get(mercs_save_header(out),0x12a)==0);CHECK(get(mercs_save_header(d),0x12a)==50000);
    CHECK(mercs_save_list_record(out,MERCS_SAVE_OWNED_MECHS,0)[0x84c]==0);
    CHECK(get(mercs_save_list_record(out,MERCS_SAVE_OWNED_MECHS,0),0x814)==1);
    CHECK(mercs_save_list_record(out,MERCS_SAVE_OWNED_REFERENCES,0)[0x84c]==1);
    CHECK(!mercs_save_encode(out,1,&bytes,&n));CHECK(!mercs_save_decode(bytes,n,&decoded));
    CHECK(document_hash(out)==document_hash(decoded));free(bytes);mercs_save_free(decoded);mercs_save_free(out);out=NULL;
    mercs_mechlab_free(s);s=NULL;
    put(mercs_save_header_edit(d),0x12a,49999);CHECK(!mercs_mechlab_begin(d,0,&s));
    CHECK(!mercs_mechlab_select_location(s,4));CHECK(!mercs_mechlab_remove(s,4));before=document_hash(mercs_mechlab_document(s));
    CHECK(mercs_mechlab_finish(s,1,&out)==MERCS_MECHLAB_FUNDS&&!out);CHECK(document_hash(mercs_mechlab_document(s))==before);
    mercs_mechlab_free(s);s=NULL;
    { mercs_career_mech m;memcpy(m.bytes,mlab_seed,sizeof m.bytes);
      CHECK(!mercs_career_deployment_bytes(&m,raw,sizeof raw,&n)); }
    CHECK(!mercs_mechlab_begin_mek(raw,n,"cmd00usr.mek",&s));CHECK(!mercs_mechlab_select_location(s,4));
    CHECK(!mercs_mechlab_remove(s,4));CHECK(!mercs_mechlab_rename(s,"INSTANT DESIGN"));
    memset(sentinel,0xa5,sizeof sentinel);written=999;
    CHECK(mercs_mechlab_finish_mek(s,1,sentinel,sizeof sentinel,&written)==MERCS_MECHLAB_LIMIT&&written==999);
    for(n=0;n<sizeof sentinel;n++)CHECK(sentinel[n]==0xa5);
    CHECK(!mercs_mechlab_finish_mek(s,1,raw,sizeof raw,&n));CHECK(!mek_parse(raw,n,&loadout));
    CHECK(mercs_mechlab_cost(s)==50000);CHECK(get(mercs_save_header(mercs_mechlab_document(s)),0x12a)==0);
    mercs_mechlab_free(s);mercs_save_free(d);
}
int main(void)
{
    original_fixtures();transaction_checks();
    printf("PASS: %zu original Mech Lab scenarios and %zu record/inventory/transaction checks\n",
        sizeof mlab_fixtures/sizeof mlab_fixtures[0],checks);return 0;
}
