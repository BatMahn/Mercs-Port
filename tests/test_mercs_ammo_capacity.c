/* Native prelaunch protection for the original ten-index ammunition pool.
 * These are disposable shell/overlay fixtures, not normal careers, legal
 * Mech Lab designs, engine frames or campaign victories. Actual original
 * editor/loader/return evidence is in tests/fixtures/ammo-pool-capacity/original.
 * Usage: test_mercs_ammo_capacity ARCHIVE [ARCHIVE ...]
 */
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mercsshell_data.h"
#include "mercsreturn.h"
#include "datapath.h"
#include "mek.h"

static unsigned checks,failures,cases_run;
static const char *case_name;
static void check(int ok,const char *label)
{
    checks++;
    if(!ok){failures++;fprintf(stderr,"FAIL %s: %s\n",case_name,label);}
}
static void require(int ok,const char *label)
{ if(!ok){fprintf(stderr,"Fixture setup failed: %s\n",label);exit(2);} }
static void join(char *out,size_t cap,const char *a,const char *b)
{ int n=snprintf(out,cap,"%s/%s",a,b);require(n>=0&&(size_t)n<cap,"path length"); }
static uint64_t hash_bytes(uint64_t h,const void *data,size_t n)
{ const unsigned char *p=data;while(n--){h^=*p++;h*=UINT64_C(1099511628211);}return h; }
static int cmp_name(const void *a,const void *b)
{ return strcmp(*(char *const *)a,*(char *const *)b); }
static uint64_t tree(const char *path,uint64_t h,int remove_tree)
{
    DIR *d=opendir(path);struct dirent *entry;char *names[32];size_t n=0,i;
    require(d!=NULL,"read disposable directory");
    while((entry=readdir(d))!=NULL)if(strcmp(entry->d_name,".")&&strcmp(entry->d_name,"..")){
        require(n<32,"bounded disposable directory entries");
        names[n]=strdup(entry->d_name);require(names[n]!=NULL,"directory name allocation");n++;
    }
    require(!closedir(d),"close disposable directory");qsort(names,n,sizeof *names,cmp_name);
    for(i=0;i<n;i++){
        char full[1024];struct stat st;join(full,sizeof full,path,names[i]);require(!lstat(full,&st),"stat disposable entry");
        h=hash_bytes(h,names[i],strlen(names[i])+1);
        if(S_ISDIR(st.st_mode)){unsigned char tag='D';h=hash_bytes(h,&tag,1);h=tree(full,h,remove_tree);}
        else{
            unsigned char tag='F',buf[1024];size_t used;FILE *f;
            require(S_ISREG(st.st_mode),"only regular disposable files");h=hash_bytes(h,&tag,1);
            f=fopen(full,"rb");require(f!=NULL,"read disposable file");
            while((used=fread(buf,1,sizeof buf,f))>0)h=hash_bytes(h,buf,used);
            require(!ferror(f)&&!fclose(f),"finish disposable file");
            if(remove_tree)require(!unlink(full),"remove disposable file");
        }
        free(names[i]);
    }
    if(remove_tree)require(!rmdir(path),"remove disposable directory");
    return h;
}
static void write_file(const char *root,const char *name,const void *bytes,size_t size)
{
    char path[1024];FILE *f;join(path,sizeof path,root,name);f=fopen(path,"wb");require(f!=NULL,"create disposable sentinel");
    require(fwrite(bytes,1,size,f)==size&&!fclose(f),"write disposable sentinel");
}
static void check_file(const char *root,const char *logical,const void *want,size_t n)
{
    char path[1024];FILE *f;unsigned char got[MERCS_CAREER_DEPLOY_MAX_BYTES];size_t used=0;int extra=0;
    join(path,sizeof path,root,logical);f=fopen(path,"rb");
    if(f){used=fread(got,1,sizeof got,f);extra=fgetc(f);require(!ferror(f)&&!fclose(f),"close deployed fixture");}
    check(f!=NULL&&used==n&&extra==EOF&&!memcmp(got,want,n),"deployment bytes preserved exactly");
}

static void install_fixture(prj_archive *archive,mercs_shell_state *state,int at,
                            int ac5,int srm,int orphan)
{
    prj_record raw;mek_def def;unsigned char bytes[MEK_MAX_SERIALIZED_BYTES];
    char filename[32];size_t size;int i,l,k,pos=0,links=ac5+srm;
    require(ac5>=0&&srm>=0&&links<=25,"fixture link count");
    require(!prj_read_named(archive,"MEK","STK00STD",&raw),"read authored chassis");
    require(!mek_parse(raw.data,raw.size,&def),"parse authored chassis");prj_record_free(&raw);
    memset(def.items,0,sizeof def.items);def.item_count=orphan?1:3;def.link_count=links;
    if(orphan)def.items[0].id=5601;
    else{def.items[0].id=4601;def.items[1].id=4602;def.items[2].id=3901;}
    for(i=0;i<def.item_count;i++)def.items[i].linked_id=-1;
    for(i=0;i<ac5;i++){
        def.items[def.item_count+i].id=(uint32_t)(14601+i);
        /* Alternating instances must still share a single type pool. */
        def.items[def.item_count+i].linked_id=4601+(i&1);
    }
    for(i=0;i<srm;i++){
        def.items[def.item_count+ac5+i].id=(uint32_t)(13901+i);
        def.items[def.item_count+ac5+i].linked_id=3901;
    }
    for(l=0;l<8;l++)for(k=0;k<MEK_SLOTS;k++){
        def.loc[l].slots[k]=0;
        if(k<def.loc[l].slot_count&&pos<def.item_count+def.link_count)
            def.loc[l].slots[k]=(uint16_t)def.items[pos++].id;
    }
    require(pos==def.item_count+def.link_count,"fixture critical storage");
    size=mek_write(&def,bytes,sizeof bytes);require(size>0,"serialize declared fixture");
    snprintf(filename,sizeof filename,"STK%02dMER.MEK",at);
    require(!mercs_career_pair_init(&state->owned[at],bytes,size,filename,(uint32_t)(700+at),
        at?(int32_t)(800+at):(int32_t)state->pilot_instance_id),"initialize disposable owned record");
}

typedef struct {
    const char *name,*campaign;
    int ac5,srm,orphan,late_member,preexisting,expect_ok;
} input_case;
static const input_case inputs[]={
    {"ten-shared-ac5","BET1CAM1",10,1,0,0,0,1},
    {"eleven-shared-ac5","BET1CAM1",11,1,0,0,0,0},
    {"fourteen-shared-ac5","BET1CAM1",14,1,0,0,0,0},
    {"ten-per-two-types","BET1CAM1",10,10,0,0,0,1},
    {"eleven-second-type","BET1CAM1",1,11,0,0,0,0},
    {"ten-orphan-bins","BET1CAM1",10,0,1,0,0,1},
    {"eleven-orphan-bins","BET1CAM1",11,0,1,0,0,0},
    {"late-lance-member-empty-overlay","BET1CAM1",14,1,0,1,0,0},
    {"late-lance-member-existing-overlay","BET1CAM1",14,1,0,1,1,0},
    {"mcc4-unused-owned-overflow","MCC4CAM1",14,1,0,1,0,1},
    {"sig2-unused-owned-overflow","SIG2CAM1",14,1,0,1,0,1},
    {"out3-unused-owned-overflow","OUT3CAM1",14,1,0,1,0,1}
};

static void run_case(prj_archive *a,const input_case *input)
{
    char root[]="/tmp/mercs-ammo-capacity-XXXXXX",error[512],names[1][16],path[1024];
    mercs_shell_state *state=calloc(1,sizeof *state),*before=malloc(sizeof *before);
    mercs_exchange_launch manifest,manifest_before;
    mercs_shell_deploy_pilot pilots[3];
    unsigned char deployed[3][MERCS_CAREER_DEPLOY_MAX_BYTES];size_t sizes[3];
    uint64_t filesystem_before,filesystem_after;int i,j,rc;size_t pilot_count=input->late_member?3u:1u;
    unsigned fails_before=failures;
    case_name=input->name;cases_run++;
    require(state&&before&&mkdtemp(root),"allocate disposable shell fixture");
    require(!mercs_shell_register(a,state,1,"Capacity Pilot","Capacity Fixture",error,sizeof error),error);
    state->owned_count=3;
    for(i=0;i<3;i++)install_fixture(a,state,i,1,1,0);
    install_fixture(a,state,input->late_member?2:0,input->ac5,input->srm,input->orphan);
    require(!mercs_shell_assign_owned(a,state,0,error,sizeof error),error);
    strcpy(names[0],input->campaign);require(!mercs_shell_set_missions(a,state,names,1,error,sizeof error),error);
    require(state->mission_count==1&&state->missions[0].launchable,"fixture mission available");
    memset(pilots,0,sizeof pilots);
    for(i=0;i<3;i++){
        pilots[i].owned_index=i;pilots[i].pilot_instance_id=i?(uint32_t)(800+i):state->pilot_instance_id;
        snprintf(pilots[i].name,sizeof pilots[i].name,"Pilot%d",i);
        for(j=0;j<8;j++)pilots[i].attributes[j]=-1;
        require(!mercs_career_deployment_bytes(&state->owned[i].current,deployed[i],sizeof deployed[i],&sizes[i]),"prepare expected unchanged MEK");
    }
    if(input->preexisting){
        join(path,sizeof path,root,"MEK");require(!mkdir(path,0700),"create sentinel MEK directory");
        for(i=0;i<3;i++){char filename[32];snprintf(filename,sizeof filename,"STK%02dMER.MEK",i);write_file(path,filename,"UNCHANGED MEK",13);}
        write_file(root,"USERSTAR.BWD","UNCHANGED USERSTAR",18);write_file(root,"marker","UNCHANGED MARKER",16);
    }
    dp_clear_roots();require(!dp_add_root(root),"set disposable overlay read root");
    memset(&manifest,0xa5,sizeof manifest);manifest_before=manifest;*before=*state;
    filesystem_before=tree(root,UINT64_C(14695981039346656037),0);
    rc=mercs_shell_prepare_roster(a,state,0,root,pilots,pilot_count,NULL,&manifest,error,sizeof error);
    filesystem_after=tree(root,UINT64_C(14695981039346656037),0);
    check((rc==0)==input->expect_ok,"expected launch admission");
    if(!input->expect_ok){
        check(error[0]&&strstr(error,"10")&&strstr(error,"ammunition bins per weapon type")&&strstr(error,"Mech Lab"),"actionable ten-bin-per-type shell error");
        check(!memcmp(state,before,sizeof *state),"all shell/owned/reference/deployed state unchanged");
        check(!memcmp(&manifest,&manifest_before,sizeof manifest),"manifest unchanged on rejection");
        check(filesystem_before==filesystem_after,"complete overlay tree unchanged on rejection");
    }else{
        check(!error[0],"successful launch has no error");
        check(manifest.unit_count==pilot_count,"successful manifest unit count");
        for(i=0;i<(int)pilot_count;i++){
            char logical[64];snprintf(logical,sizeof logical,"MEK/STK%02dMER.MEK",i);check_file(root,logical,deployed[i],sizes[i]);
            check(manifest.units[i].actor_index==-i-1&&manifest.units[i].controller_flags==2,"owned manifest identity");
            mercs_career_set_deployed(&before->owned[i].current,1);
        }
        check(!memcmp(state,before,sizeof *state),"only expected deployment markers change");
        join(path,sizeof path,root,"USERSTAR.BWD");{struct stat st;check(!stat(path,&st)&&st.st_size>0,"normal USERSTAR written");}
    }
    printf("CASE %s: %s (%s)\n",input->name,failures==fails_before?"PASS":"FAIL",input->expect_ok?"admitted":"refused before writes");
    dp_clear_roots();(void)tree(root,UINT64_C(14695981039346656037),1);free(before);free(state);
}
int main(int argc,char **argv)
{
    int i;size_t j;char error[512];
    if(argc<2){fprintf(stderr,"Usage: %s ARCHIVE [ARCHIVE ...]\n",argv[0]);return 2;}
    require(MERCS_RETURN_POOL_BINS==10,"original pool bound remains ten");
    for(i=1;i<argc;i++){
        prj_archive *a=prj_open(argv[i],error,sizeof error);require(a!=NULL,error);
        printf("ARCHIVE %s\n",argv[i]);
        for(j=0;j<sizeof inputs/sizeof inputs[0];j++)run_case(a,&inputs[j]);
        prj_close(a);
    }
    printf("{\"checks\":%u,\"failures\":%u,\"cases\":%u,\"archives\":%d,\"engine_steps\":0,\"campaign_wins\":0}\n",checks,failures,cases_run,argc-1);
    return failures?1:0;
}
