#include "mercsinstant.h"
#include "datapath.h"
#include "bwd.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "../tests/fixtures/combat-completion/original-instant.h"
#include "../tests/fixtures/combat-completion/original-instant-rmg.h"

static int checks;
#define T(c) do{checks++;if(!(c)){fprintf(stderr,"Instant Action check %d failed at line%d: %s\n",checks,__LINE__,#c);abort();}}while(0)
static void ok(int rc,const char *e,int line){checks++;if(rc){fprintf(stderr,"Instant Action failed line%d: %s (%d)\n",line,e,rc);abort();}}
#define OK(call) ok((call),error,__LINE__)
static uint64_t hash(const uint8_t *p,size_t n)
{uint64_t h=UINT64_C(14695981039346656037);size_t i;for(i=0;i<n;i++)h=(h^p[i])*UINT64_C(1099511628211);return h;}
static void selection(void)
{
    size_t i;
    for(i=0;i<sizeof original_ia_cycles/sizeof original_ia_cycles[0];i++){
        const original_ia_cycle *f=&original_ia_cycles[i];T(mercs_instant_cycle_chassis(f->slot,f->initial,f->step)==f->result);}
    for(i=0;i<sizeof original_ia_variants/sizeof original_ia_variants[0];i++){
        const original_ia_variant *f=&original_ia_variants[i];int a=f->active,index=f->index;
        mercs_instant_cycle_variant(&a,f->count,&index,f->step);T(a==f->result_active&&index==f->result_index);}
    for(i=0;i<15;i++)T(mercs_instant_planet_index(mercs_instant_planet((int)i))==(int)i);
}
static void generator(prj_archive *a)
{
    size_t i,j,k;char error[384]={0};
    for(i=0;i<sizeof original_ia_rmg_cases/sizeof original_ia_rmg_cases[0];i++){
        const original_ia_rmg *f=&original_ia_rmg_cases[i];mercs_rmg_config c={0};mercs_rmg_bundle b={0};
        c.number=0;c.year=-1;c.seed=f->seed;c.kind=f->kind;c.skill=f->skill;strcpy(c.planet,f->planet);
        OK(mercs_rmg_generate(a,&c,&b,error,sizeof error));T(b.config.year==f->year);T(b.count==f->count+1);
        for(j=0;j<f->count;j++){
            const original_ia_file *want=&original_ia_files[f->first+j];
            for(k=0;k<b.count;k++){char name[32];snprintf(name,sizeof name,"%s.%s",b.files[k].name,b.files[k].tag);if(!strcasecmp(name,want->name))break;}
            if(k>=b.count)fprintf(stderr,"missing original file%s\n",want->name);
            T(k<b.count);T(b.files[k].size==want->size&&hash(b.files[k].data,b.files[k].size)==want->hash);
        }
        mercs_rmg_free(&b);
    }
}
static void validate_graph(prj_archive *a,int friends)
{
    bwd_mission m;int i,j,players=0,allies=0,enemies=0;
    T(!bwd_mission_load(a,"RAN0SCN1",&m));
    if(m.missing_count)for(i=0;i<m.missing_count;i++)fprintf(stderr,"missing %s\n",m.missing[i]);
    T(m.missing_count==0&&m.nav_count>0);
    for(i=0;i<m.record_count;i++)for(j=0;j<m.records[i].chunk_count;j++){
        bwd_gps g;mek_def load;const bwd_chunk *p=&m.records[i].chunks[j];if(strcmp(p->tag,"GPS"))continue;
        T(!bwd_gps_decode(p,&g));T(!mek_load(a,g.loadout,&load));T(g.skeleton[0]);
        if(g.is_player)players++;else if(!g.table)allies++;else enemies++;
    }
    T(players==1&&allies==friends&&enemies>0);bwd_mission_free(&m);
}
static void prepare(prj_archive *a,const char *overlay,const char *edition,int child)
{
    mercs_instant *s=calloc(1,sizeof *s);mercs_instant_launch *l=calloc(1,sizeof *l),*copy=calloc(1,sizeof *copy);
    char error[384]={0};uint8_t bytes[MEK_MAX_SERIALIZED_BYTES];size_t size;int i;
    T(s&&l&&copy);mercs_instant_init(s);T(s->slots[0].chassis==0&&s->slots[1].chassis==-1&&s->slots[3].chassis==-2);
    OK(mercs_instant_generate(a,s,1,error,sizeof error));T(s->request.year==-1&&s->parameters.year>=3044&&s->parameters.year<=3054);
    T(!prj_is_generated(a,"BWD","USERSTAR"));
    mercs_instant_choose(s,1,1);mercs_instant_choose(s,2,1);T(s->slots[1].chassis==0&&s->slots[2].chassis==0);
    for(i=3;i<6;i++){s->slots[i].chassis=0;mercs_instant_choose(s,i,1);T(s->slots[i].chassis==1);}
    /* A custom player design is carried inside the capsule, never loaded
     * from a career MER loadout or a stale loose USERSTAR. */
    mercs_instant_choose(s,0,0);OK(mercs_instant_loadout(a,s,0,bytes,sizeof bytes,&size));
    OK(mercs_instant_design(s,bytes,size,overlay,error,sizeof error));T(s->slots[0].custom_size==size&&strstr(s->slots[0].loadout,"USR"));
    OK(mercs_instant_variant(s,1,overlay,error,sizeof error));T(s->slots[0].variant_active);
    OK(mercs_instant_prepare(s,edition,1,l,error,sizeof error));OK(mercs_instant_begin(overlay,l,error,sizeof error));
    OK(mercs_instant_read(overlay,l->token,edition,copy,error,sizeof error));T(!memcmp(l,copy,sizeof *l));
    T(mercs_instant_read(overlay,l->token,!strcmp(edition,"dos")?"3dfx":"dos",copy,error,sizeof error));
    T(mercs_instant_read(overlay,"../bad",edition,copy,error,sizeof error));error[0]=0;
    OK(mercs_instant_install(a,l,error,sizeof error));validate_graph(a,2);
    for(i=1;i<=5;i++){
        char name[17];prj_record r;snprintf(name,sizeof name,"EN%02dSTAR",i);T(!prj_read_named(a,"BWD",name,&r));
        T(r.generated&&r.size==352&&r.data[52+12]==i&&r.data[52+16]==5);prj_record_free(&r);
    }
    if(child)printf("INSTANT_DEPLOYED scene=RAN0SCN1 edition=%s token=%s overlay=%s\n",edition,l->token,overlay);
    else{
        char path[2048];FILE *f;int byte;
        snprintf(path,sizeof path,"%s/MERCS-INSTANT/%s/LAUNCH.DAT",overlay,l->token);
        f=fopen(path,"rb+");T(f!=NULL);T(!fseek(f,60,SEEK_SET));byte=fgetc(f);T(byte!=EOF);T(!fseek(f,60,SEEK_SET));T(fputc(byte^1,f)!=EOF);T(!fclose(f));
        T(mercs_instant_read(overlay,l->token,edition,copy,error,sizeof error));
        s->slots[0].chassis=-1;T(mercs_instant_prepare(s,edition,1,l,error,sizeof error));
    }
    mercs_instant_free(s);free(s);free(l);free(copy);
}
int main(int argc,char **argv)
{
    char error[384]={0},root[2048];prj_archive *a;char *slash;int child=argc==6;
    if(argc!=4&&argc!=6){fprintf(stderr,"usage:%s MW2.PRJ CD_ROOT OVERLAY [--prepare-launch EDITION]\n",argv[0]);return 2;}
    T(!child||!strcmp(argv[4],"--prepare-launch"));a=prj_open(argv[1],error,sizeof error);T(a!=NULL);
    snprintf(root,sizeof root,"%s",argv[1]);slash=strrchr(root,'/');T(slash!=NULL);*slash=0;
    mkdir(argv[3],0700);OK(dp_add_root(argv[3]));OK(dp_set_write_root(argv[3]));OK(dp_add_root(root));OK(dp_add_root(argv[2]));
    selection();if(!child)generator(a);prepare(a,argv[3],child?argv[5]:"dos",child);
    prj_close(a);printf("Mercs Instant Action: %d checks passed; original selection/year graph, custom designs, six-slot launch and capsule rejection.\n",checks);return 0;
}
