#include "mercscontrols.h"
#include "datapath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../tests/fixtures/controls-completion/original-controls-fixtures.h"
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(int argc,char **argv)
{
    mercs_control_config c,old;
    uint8_t encoded[MC_CPC_BYTES];
    char map[65536],path[128];
    size_t i,j;
    int selected[MC_DEVICES];
    if (argc!=2) { fprintf(stderr,"usage: %s GAME-CD-ROOT\n",argv[0]); return 2; }
    dp_clear_roots(); CHECK(!dp_add_root(argv[1]));
    for (i=0;i<sizeof mc_original_cases/sizeof mc_original_cases[0];i++) {
        const mc_original_case *r=mc_original_cases+i;
        unsigned char *raw=NULL; size_t len=0;
        snprintf(path,sizeof path,"GIDDI/%s",r->name);
        CHECK(!dp_read_file(path,&raw,&len));
        CHECK(!mercs_controls_decode(&c,raw,len));
        mercs_controls_encode(&c,encoded); CHECK(len==sizeof encoded && !memcmp(encoded,raw,len));
        free(raw);
        CHECK(!mercs_controls_remap(&c));
        CHECK(!memcmp(c.b,r->bindings,sizeof c.b));
        mercs_controls_selected(&c,selected); CHECK(!memcmp(selected,r->selected,sizeof selected));
        CHECK(mercs_controls_generate(&c,map,sizeof map,NULL));
        if (strcmp(map,r->map)) { fprintf(stderr,"map differs: %s\n",r->name); puts(map); return 1; }
        checks++;
    }
    for (i=0;i<sizeof mc_synthetic_cases/sizeof mc_synthetic_cases[0];i++) {
        const mc_original_case *r=mc_synthetic_cases+i;
        mercs_controls_clear(&c); memcpy(c.b,r->bindings,sizeof c.b); old=c;
        CHECK(mercs_controls_generate(&c,map,sizeof map,NULL));
        if (strcmp(map,r->map)) { fprintf(stderr,"synthetic map differs: %s\n",r->name); puts(map); return 1; }
        CHECK(!memcmp(&c,&old,sizeof c));
        CHECK(!mercs_controls_generate(&c,map,4,NULL));
    }
    mercs_controls_clear(&c); mercs_controls_encode(&c,encoded); old=c;
    CHECK(mercs_controls_decode(&c,encoded,sizeof encoded-1)); CHECK(!memcmp(&c,&old,sizeof c));
    encoded[324]=16; encoded[325]=encoded[326]=encoded[327]=0;
    CHECK(mercs_controls_decode(&c,encoded,sizeof encoded)); CHECK(!memcmp(&c,&old,sizeof c));
    for (i=0;i<4;i++) { static const uint32_t mods[]={1,2,4,0}; mercs_control_cycle_modifier(&c.b[0][0]); CHECK(c.b[0][0].flags==mods[i]); }
    for (j=0;j<MC_DEVICES;j++) { const mercs_control_device *d=mercs_control_device_at((int)j); if (d) CHECK(d->naxis<=8 && d->nbutton<=128); }
    printf("PASS Mercs controls: %d checks, %zu original maps, %zu synthetic original maps\n",checks,
        sizeof mc_original_cases/sizeof mc_original_cases[0],sizeof mc_synthetic_cases/sizeof mc_synthetic_cases[0]);
    return 0;
}
