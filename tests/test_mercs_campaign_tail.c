/* Real campaign loader with faults injected after a verified archive read.
 * Never modifies the source PRJ. This checks both the exact authored exception
 * and atomic rejection of altered/unrecognized truncated payloads. */
#include "mercssave.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
static int injected_read(prj_archive *,const char *,const char *,prj_record *);
#define prj_read_named injected_read
#include "../src/mercssave_init.c"
#undef prj_read_named

static int fault;
static int injected_read(prj_archive *archive,const char *tag,const char *name,prj_record *out)
{
    int rc=prj_read_named(archive,tag,name,out);
    if(rc||strcmp(tag,"BWD")||strcasecmp(name,"BET4CAM1"))return rc;
    assert(out->size==1824);
    switch(fault){
    case 1:out->data[720]^=1;break; /* Changed byte in the complete NEWS text. */
    case 2:out->data[1820]^=1;break; /* Changed incomplete equipment ID. */
    case 3:out->size--;break;
    case 4:strcpy(out->name,"UNK4CAM1");break;
    case 5:
        out->data=realloc(out->data,out->size+1);assert(out->data);
        out->data[out->size++]=0;break;
    case 6:strcpy(out->tag,"XWD");break;
    default:break;
    }
    return 0;
}

int main(int argc,char **argv)
{
    prj_archive *archive;
    mercs_save_state *base=NULL,*state;
    uint8_t *before,*after;
    size_t before_size,after_size,i;
    char error[384];
    assert(argc==2);
    archive=prj_open(argv[1],error,sizeof error);assert(archive);
    assert(!mercs_save_initialize(archive,1,"TAIL CHECK","TEST COMPANY",&base,error,sizeof error));
    assert(!mercs_save_load_campaign(archive,base,"BET1CAM1",error,sizeof error));
    assert(!mercs_save_encode(base,1,&before,&before_size));
    for(fault=0;fault<=6;fault++){
        state=mercs_save_clone(base);assert(state);
        if(!fault){
            assert(!mercs_save_load_campaign(archive,state,"BET4CAM1",error,sizeof error));
            assert(mercs_save_list_count(state,MERCS_SAVE_SHOP_WEAPONS)==33);
            assert(mercs_save_list_count(state,MERCS_SAVE_SHOP_MECHS)==18);
            for(i=0;i<33;i++)
                assert(mercs_save_list_record(state,MERCS_SAVE_SHOP_WEAPONS,i)[8]==(i<17?0:2));
        }else{
            assert(mercs_save_load_campaign(archive,state,"BET4CAM1",error,sizeof error)==MERCS_SAVE_INVALID);
            assert(!mercs_save_encode(state,1,&after,&after_size));
            assert(after_size==before_size&&!memcmp(before,after,before_size));free(after);
        }
        mercs_save_free(state);
    }
    free(before);mercs_save_free(base);prj_close(archive);
    puts("PASS: exact BET4 keeps 33 complete items; six altered/unrecognized truncations reject without changing the career.");
    return 0;
}
