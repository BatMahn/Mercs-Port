#include "mercscareer.h"
#include "mercs_career_original_fixtures.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(x) (sizeof(x)/sizeof((x)[0]))
#define CHECK(x) do { if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);} } while(0)
static size_t unhex(career_hex hex,void *out,size_t capacity)
{
    size_t i,n,k,written=0;uint8_t *p=out;const char *parts[]={hex.a,hex.b};
    for(k=0;k<2;k++) {
        const char *s=parts[k];n=strlen(s);
        CHECK(!(n&1)&&n/2<=capacity-written);
        for(i=0;i<n/2;i++){unsigned v;CHECK(sscanf(s+2*i,"%2x",&v)==1);p[written++]=(uint8_t)v;}
    }
    return written;
}
static void same(const void *a,const void *b,size_t n,const char *name)
{
    size_t i;const uint8_t *x=a,*y=b;
    for(i=0;i<n;i++)if(x[i]!=y[i]){fprintf(stderr,"%s differs at0x%zx: native%02x original%02x\n",name,i,x[i],y[i]);exit(1);}
}
static void put32(uint8_t *p,uint32_t v)
{ p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24); }

static void loaders(void)
{
    size_t i;
    for(i=0;i<COUNT(career_loader_fixtures);i++) {
        const career_loader_fixture *f=&career_loader_fixtures[i];
        uint8_t mek[1024],expected[MERCS_CAREER_DEPLOY_MAX_BYTES],actual[sizeof expected];
        mercs_career_mech_pair pair,before;mercs_career_mech record;
        size_t n=unhex(f->mek,mek,sizeof mek),want,written=0;char name[32];
        CHECK(mercs_career_pair_init(&pair,mek,n,f->filename,17,-1)==0);
        CHECK(unhex(f->record,record.bytes,sizeof record.bytes)==sizeof record.bytes);
        same(&pair.current,&record,sizeof record,f->name);
        same(&pair.reference,&record,sizeof record,f->name);
        CHECK(mercs_career_instance_id(&pair.current)==17);
        CHECK(mercs_career_association_id(&pair.current)==-1);
        CHECK(mercs_career_filename(&pair.current,name,sizeof name)==0&&!strcmp(name,f->filename));
        before=pair;
        want=unhex(f->deployment,expected,sizeof expected);
        CHECK(mercs_career_deployment_bytes(&pair.current,actual,sizeof actual,&written)==0&&written==want);
        same(actual,expected,want,f->name);same(&pair,&before,sizeof pair,f->name);
    }
}
static void cycles(void)
{
    size_t i;
    for(i=0;i<COUNT(career_cycle_fixtures);i++) {
        const career_cycle_fixture *f=&career_cycle_fixtures[i];
        mercs_career_mech_pair pair,before,after;mercs_career_mech expected;
        mercs_career_import_result result;
        uint8_t returned[1024],out[MERCS_CAREER_DEPLOY_MAX_BYTES],want[sizeof out];
        size_t n,written=0,want_size;
        CHECK(unhex(f->current,pair.current.bytes,sizeof pair.current.bytes)==sizeof pair.current.bytes);
        CHECK(unhex(f->reference,pair.reference.bytes,sizeof pair.reference.bytes)==sizeof pair.reference.bytes);
        before=pair;n=unhex(f->returned,returned,sizeof returned);
        CHECK(unhex(f->after,expected.bytes,sizeof expected.bytes)==sizeof expected.bytes);
        CHECK(mercs_career_import_return(&pair,0,returned,n,&result)==0);
        CHECK(result.action==MERCS_CAREER_IMPORT_APPLIED&&result.request_delete==1&&!strcmp(result.filename,f->filename));
        same(&pair.current,&expected,sizeof expected,f->name);
        same(&pair.reference,&before.reference,sizeof pair.reference,f->name);
        CHECK(!mercs_career_deployed(&pair.current));
        CHECK(mercs_career_instance_id(&pair.current)==mercs_career_instance_id(&before.current));
        CHECK(mercs_career_association_id(&pair.current)==mercs_career_association_id(&before.current));
        want_size=unhex(f->deployment,want,sizeof want);
        CHECK(mercs_career_deployment_bytes(&pair.current,out,sizeof out,&written)==0&&written==want_size);
        same(out,want,want_size,f->name);
        after=pair;
        /* Repeated consumption ignores even malformed bytes once marker clears. */
        CHECK(mercs_career_import_return(&pair,0,returned,1,&result)==0);
        CHECK(result.action==MERCS_CAREER_IMPORT_SKIPPED&&!result.request_delete);
        same(&pair,&after,sizeof pair,f->name);
        /* Original restore_mode2 follows the normal path, unlike mode1. */
        pair=before;CHECK(mercs_career_import_return(&pair,2,returned,n,&result)==0);
        same(&pair,&after,sizeof pair,f->name);
    }
}
static void markers_and_loss(void)
{
    size_t i;
    for(i=0;i<COUNT(career_marker_fixtures);i++) {
        const career_marker_fixture *f=&career_marker_fixtures[i];
        mercs_career_mech_pair pair,before;mercs_career_mech expected;
        mercs_career_import_result result;uint8_t invalid=0;
        unhex(f->current,pair.current.bytes,sizeof pair.current.bytes);
        memset(&pair.reference,0xa7,sizeof pair.reference);before=pair;
        unhex(f->after,expected.bytes,sizeof expected.bytes);
        CHECK(mercs_career_import_return(&pair,f->restore,f->available?&invalid:NULL,f->available?1:0,&result)==0);
        CHECK(result.action==(f->restore==1?MERCS_CAREER_IMPORT_RESTORE:
              f->marker==1?MERCS_CAREER_IMPORT_MISSING:MERCS_CAREER_IMPORT_SKIPPED));
        CHECK(!result.request_delete);same(&pair.current,&expected,sizeof expected,"marker branch");
        same(&pair.reference,&before.reference,sizeof pair.reference,"marker reference");
    }
    for(i=0;i<COUNT(career_loss_fixtures);i++) {
        mercs_career_mech m;unhex(career_loss_fixtures[i].record,m.bytes,sizeof m.bytes);
        CHECK(mercs_career_mech_lost(&m)==career_loss_fixtures[i].lost);
    }
}
static void invalid_is_transactional(void)
{
    uint8_t mek[1024],bad[1024],out[1024],old_out[1024];
    size_t n=unhex(career_loader_fixtures[0].mek,mek,sizeof mek),written=937;
    mercs_career_mech_pair pair,before;mercs_career_import_result result,old_result;
    CHECK(mercs_career_pair_init(&pair,mek,n,"PLY00MER.MEK",11,-1)==0);
    mercs_career_set_acquisition(&pair.current,2000000,3039,100);
    mercs_career_set_acquisition(&pair.reference,2000000,3039,100);
    mercs_career_set_association(&pair.current,12);
    CHECK(mercs_career_association_id(&pair.reference)==-1);
    mercs_career_set_deployed(&pair.current,1);CHECK(mercs_career_deployed(&pair.current));before=pair;
    memset(&result,0xa5,sizeof result);old_result=result;
    memcpy(bad,mek,n);put32(bad+16,15);
    CHECK(mercs_career_import_return(&pair,0,bad,n,&result)==MERCS_CAREER_UNSUPPORTED);
    same(&pair,&before,sizeof pair,"invalid import");same(&result,&old_result,sizeof result,"invalid result");
    CHECK(mercs_career_import_return(&pair,0,NULL,1,&result)==MERCS_CAREER_INVALID);
    same(&pair,&before,sizeof pair,"missing length");
    CHECK(mercs_career_pair_init(&pair,mek,n-1,"PLY00MER.MEK",11,-1)==MERCS_CAREER_INVALID);
    same(&pair,&before,sizeof pair,"truncated initialization");
    CHECK(mercs_career_pair_init(&pair,mek,n,"../PLY00.MEK",11,-1)==MERCS_CAREER_INVALID);
    same(&pair,&before,sizeof pair,"filename initialization");
    memcpy(bad,mek,n);put32(bad+20,26);
    CHECK(mercs_career_pair_init(&pair,bad,n,"PLY00MER.MEK",11,-1)==MERCS_CAREER_UNSUPPORTED);
    same(&pair,&before,sizeof pair,"ammo bound");
    memcpy(bad,mek,n);bad[24+36]=13;
    CHECK(mercs_career_pair_init(&pair,bad,n,"PLY00MER.MEK",11,-1)==MERCS_CAREER_INVALID);
    same(&pair,&before,sizeof pair,"slot bound");
    memset(out,0x39,sizeof out);memcpy(old_out,out,sizeof out);
    CHECK(mercs_career_deployment_bytes(&pair.current,out,1,&written)==MERCS_CAREER_NO_SPACE&&written==937);
    same(out,old_out,sizeof out,"small output");same(&pair,&before,sizeof pair,"small output state");
    mercs_career_set_deployed(&pair.current,0);CHECK(!mercs_career_deployed(&pair.current));
}
int main(void)
{
    loaders();cycles();markers_and_loss();invalid_is_transactional();
    printf("Mercs career consumer: %zu loader/deploy, %zu return/redeploy, %zu marker, %zu loss original goldens; transactional bounds passed\n",
           COUNT(career_loader_fixtures),COUNT(career_cycle_fixtures),COUNT(career_marker_fixtures),COUNT(career_loss_fixtures));
    return 0;
}
