/* Asset-backed original-oracle companion; writes only its requested output. */
#include "mercssave.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv)
{
    prj_archive *a;mercs_save_state *s=NULL;uint8_t *data=NULL;size_t length=0;
    char error[256];FILE *f;int rc,i,mode;
    if(argc<4)return 2;
    mode=atoi(argv[2]);
    a=prj_open(argv[1],error,sizeof error);if(!a){fprintf(stderr,"%s\n",error);return 1;}
    rc=mercs_save_initialize(a,mode,"Test Pilot","Test Unit",&s,error,sizeof error);
    for(i=4;!rc&&i<argc;i++) {
        rc=mercs_save_load_campaign(a,s,argv[i],error,sizeof error);
        /* Initial novice acceptance transfers the authored roster. Chained
         * type-1 CPGN loads do not invoke the original transfer functions. */
        if(!rc&&!mode&&i==4)rc=mercs_save_transfer_training(s);
    }
    if(!rc)rc=mercs_save_encode(s,1,&data,&length);
    if(rc){fprintf(stderr,"save probe error %d: %s\n",rc,error);mercs_save_free(s);prj_close(a);return 1;}
    f=fopen(argv[3],"wb");if(!f)return 1;
    rc=fwrite(data,1,length,f)==length?0:1;if(fclose(f))rc=1;
    free(data);mercs_save_free(s);prj_close(a);return rc;
}
