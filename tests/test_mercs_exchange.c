#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "mercsexchange.h"

static mercs_exchange_launch make_launch(void)
{
    mercs_exchange_launch l;
    memset(&l,0,sizeof l);
    strcpy(l.scene,"MERC1SCN1"); strcpy(l.edition,"dos");
    l.unit_count=1; l.units[0].actor_index=-1;
    l.units[0].controller_flags=2; strcpy(l.units[0].name,"PLY00MER");
    return l;
}
static void result_path(char *out,size_t cap,const char *overlay,const mercs_exchange_launch *l,const char *name)
{
    int n=snprintf(out,cap,"%s/MERCS-RETURNS/%s/%s",overlay,l->token,name);
    assert(n>0 && (size_t)n<cap);
}
static void overwrite(const char *path,const void *data,size_t length)
{
    FILE *f=fopen(path,"wb"); assert(f);
    assert(fwrite(data,1,length,f)==length); assert(!fclose(f));
}
int main(int argc,char **argv)
{
    mercs_exchange_launch l=make_launch(),loaded,other,wrong;
    mercs_exchange_result result,readback,untouched;
    mercs_car_record car;
    mercs_msn_star star;
    size_t n,raw_size;
    char path[4096], retained[4096];
    int path_size;
    unsigned char raw[20000],damaged[20000];
    FILE *f;
    assert(argc==2);
    memset(&result,0,sizeof result); memset(&star,0,sizeof star);
    assert(!mercs_car_init_dos(&car)); memcpy(result.career,car.bytes,sizeof result.career);
    star.started_seconds=0; star.resolved_seconds=-1;
    star.node_count=1; star.nodes[0].visible=1; star.nodes[0].runtime_state=3;
    star.nodes[0].started_seconds=0; star.nodes[0].resolved_seconds=-1;
    strcpy(star.nodes[0].text,"Return test");
    assert(!mercs_msn_encode_selected(&star,1,0,result.mission,sizeof result.mission,&n));
    /* Deliberately opaque, bounded MEK payload: this test verifies transport.
     * Original MEK validity and shell acceptance have separate oracle tests. */
    result.mech_count=1; result.mechs[0].actor_index=-1;
    strcpy(result.mechs[0].name,"PLY00MER"); result.mechs[0].size=394;
    memset(result.mechs[0].bytes,0x6b,result.mechs[0].size);
    assert(!mercs_exchange_begin(argv[1],&l));
    assert(strlen(l.token)==32);
    assert(!mercs_exchange_load(argv[1],l.token,&loaded));
    assert(!strcmp(loaded.token,l.token) && !strcmp(loaded.scene,l.scene));
    assert(loaded.unit_count==1 && loaded.units[0].actor_index==-1 && loaded.units[0].controller_flags==2);
    memset(&readback,0xa5,sizeof readback); untouched=readback;
    assert(mercs_exchange_read(argv[1],&l,&readback)==MERCS_EXCHANGE_PENDING);
    assert(!memcmp(&readback,&untouched,sizeof readback));
    /* Temporarily unavailable return storage must stay distinguishable from
     * an invalid packet or an ordinary missing RESULT.DAT. */
    path_size=snprintf(path,sizeof path,"%s/MERCS-RETURNS/%s",argv[1],l.token);
    assert(path_size>0 && (size_t)path_size<sizeof path);
    path_size=snprintf(retained,sizeof retained,"%s.retained-for-io-test",path);
    assert(path_size>0 && (size_t)path_size<sizeof retained);
    assert(!rename(path,retained)); wrong=loaded;
    assert(mercs_exchange_load(argv[1],l.token,&loaded)==MERCS_EXCHANGE_IO);
    assert(!memcmp(&loaded,&wrong,sizeof loaded));
    assert(mercs_exchange_read(argv[1],&l,&readback)==MERCS_EXCHANGE_IO);
    assert(!memcmp(&readback,&untouched,sizeof readback));
    assert(!rename(retained,path));
    assert(!mercs_exchange_load(argv[1],l.token,&loaded));
    assert(mercs_exchange_read(argv[1],&l,&readback)==MERCS_EXCHANGE_PENDING);
    assert(!mercs_exchange_publish(argv[1],&l,&result));
    assert(!mercs_exchange_read(argv[1],&l,&readback));
    assert(!memcmp(readback.career,result.career,sizeof result.career));
    assert(!memcmp(readback.mission,result.mission,sizeof result.mission));
    assert(readback.mech_count==1 && readback.mechs[0].size==394 &&
           !memcmp(readback.mechs[0].bytes,result.mechs[0].bytes,394));
    assert(mercs_exchange_publish(argv[1],&l,&result)==MERCS_EXCHANGE_INVALID);
    wrong=l; strcpy(wrong.scene,"OTHER");
    readback=untouched;
    assert(mercs_exchange_read(argv[1],&wrong,&readback)==MERCS_EXCHANGE_INVALID);
    assert(!memcmp(&readback,&untouched,sizeof readback));

    result_path(path,sizeof path,argv[1],&l,"RESULT.DAT");
    f=fopen(path,"rb"); assert(f); raw_size=fread(raw,1,sizeof raw,f);
    assert(raw_size>3000 && feof(f)); assert(!fclose(f));
    memcpy(damaged,raw,raw_size); damaged[raw_size-12]^=0x20;
    overwrite(path,damaged,raw_size);
    assert(mercs_exchange_read(argv[1],&l,&readback)==MERCS_EXCHANGE_INVALID);
    assert(!memcmp(&readback,&untouched,sizeof readback));
    overwrite(path,raw,raw_size-1);
    assert(mercs_exchange_read(argv[1],&l,&readback)==MERCS_EXCHANGE_INVALID);
    overwrite(path,raw,raw_size);
    assert(!mercs_exchange_acknowledge(argv[1],&l));
    assert(mercs_exchange_read(argv[1],&l,&readback)==MERCS_EXCHANGE_APPLIED);
    assert(mercs_exchange_acknowledge(argv[1],&l)==MERCS_EXCHANGE_APPLIED);
    assert(!memcmp(&readback,&untouched,sizeof readback));

    other=make_launch(); assert(!mercs_exchange_begin(argv[1],&other));
    assert(strcmp(l.token,other.token));
    result_path(path,sizeof path,argv[1],&other,"RESULT.DAT.TMP");
    overwrite(path,raw,97); /* killed child: partial staging is never consumed */
    assert(mercs_exchange_read(argv[1],&other,&readback)==MERCS_EXCHANGE_PENDING);
    result_path(path,sizeof path,argv[1],&other,"RESULT.DAT");
    overwrite(path,raw,raw_size); /* even a complete result from another launch */
    assert(mercs_exchange_read(argv[1],&other,&readback)==MERCS_EXCHANGE_INVALID);
    assert(!memcmp(&readback,&untouched,sizeof readback));

    other=make_launch(); assert(!mercs_exchange_begin(argv[1],&other));
    result.mech_count=0; /* original rb guard may skip the owned MEK */
    assert(!mercs_exchange_publish(argv[1],&other,&result));
    assert(!mercs_exchange_read(argv[1],&other,&readback) && readback.mech_count==0);
    wrong=make_launch(); strcpy(wrong.units[0].name,"../BAD");
    assert(mercs_exchange_begin(argv[1],&wrong)==MERCS_EXCHANGE_INVALID && !wrong.token[0]);
    assert(mercs_exchange_load(argv[1],"../BAD",&loaded)==MERCS_EXCHANGE_INVALID);
    wrong=make_launch(); wrong.unit_count=17;
    assert(mercs_exchange_begin(argv[1],&wrong)==MERCS_EXCHANGE_INVALID);
#ifndef _WIN32
    other=make_launch(); assert(!mercs_exchange_begin(argv[1],&other));
    result_path(path,sizeof path,argv[1],&other,"RESULT.DAT");
    assert(!symlink("/dev/zero",path));
    assert(mercs_exchange_read(argv[1],&other,&readback)==MERCS_EXCHANGE_IO);
#endif
    puts("PASS: complete launch-bound exchange, exact original payloads, stale/corrupt/partial rejection, missing MEK, once-only receipt, bounded paths, unavailable-directory retry");
    return 0;
}
