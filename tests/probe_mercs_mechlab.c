/* File-driven original comparison probe; no game bytes are embedded. */
#include "mercsmechlab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void p32(FILE *f,unsigned v){unsigned char b[4]={v,v>>8,v>>16,v>>24};fwrite(b,1,4,f);}
int main(int argc,char **argv)
{
    FILE *f;long n;unsigned char *b;mercs_save_state *d=NULL;
    mercs_mechlab_session *s=NULL;int i,rc,a,z;size_t j,k;
    if(argc<3)return 2;
    f=fopen(argv[1],"rb");if(!f)return 3;
    fseek(f,0,SEEK_END);n=ftell(f);rewind(f);b=malloc(n);if(!b||fread(b,1,n,f)!=(size_t)n)return 4;fclose(f);
    rc=mercs_save_decode(b,n,&d);free(b);if(rc)return 5;
    rc=mercs_mechlab_begin(d,0,&s);mercs_save_free(d);if(rc){fprintf(stderr,"begin %d\n",rc);return 6;}
    f=fopen(argv[2],"wb");if(!f)return 7;
    for(i=3;i<argc;i++){
        if(sscanf(argv[i],"loc:%d",&a)==1)rc=mercs_mechlab_select_location(s,a);
        else if(sscanf(argv[i],"add:%d:%d",&a,&z)==2)rc=mercs_mechlab_add(s,(unsigned)a,z);
        else if(sscanf(argv[i],"remove:%d",&a)==1)rc=mercs_mechlab_remove(s,a);
        else if(sscanf(argv[i],"adjust:%d:%d",&a,&z)==2)rc=mercs_mechlab_adjust(s,(enum mercs_mechlab_control)a,z);
        else if(sscanf(argv[i],"place:%d",&a)==1)rc=mercs_mechlab_place(s,a);
        else return 8;
        fprintf(stderr,"%s=%d\n",argv[i],rc);p32(f,(unsigned)rc);
    }
    p32(f,(unsigned)mercs_mechlab_cost(s));
    p32(f,(unsigned)mercs_mechlab_pending(s,NULL));
    fwrite(mercs_mechlab_current(s)->bytes,1,MERCS_CAREER_MECH_BYTES,f);
    for(k=MERCS_SAVE_OWNED_WEAPONS;k<=MERCS_SAVE_OWNED_AMMO;k++){
        size_t count=mercs_save_list_count(mercs_mechlab_document(s),(enum mercs_save_list)k);
        p32(f,(unsigned)count);
        for(j=0;j<count;j++)fwrite(mercs_save_list_record(mercs_mechlab_document(s),(enum mercs_save_list)k,j),1,26,f);
    }
    fclose(f);mercs_mechlab_free(s);return 0;
}
