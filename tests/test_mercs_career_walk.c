/* Walks a whole career, contract by contract, with recorded wins (the shell's
 * accept -> return -> debriefing -> continue path, as test_mercs_campaign drives
 * it) until the original's career end. Picks the first authored contract each
 * time it has a choice, else the first offer. No mission is simulated: this
 * checks the career graph, settlement and the ending gate, not combat. */
#include "mercscampaign.h"
#include "mercsstore.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void controlled_return(mercs_shell_state *v,int selected)
{
    mercs_car_init_dos(&v->last_car);
    v->last_car.bytes[MERCS_CAR_PLAYER_STATUS_OFFSET]=0;v->last_car.bytes[0x1e]=2;
    memset(&v->last_msn,0,sizeof v->last_msn);v->last_msn.outcome=2;
    v->last_msn.row_count=1;v->last_msn.rows[0].succeeded=1;v->last_msn.rows[0].priority=1;
    strcpy((char *)v->last_msn.rows[0].text,"Objective");
    v->last_return_valid=1;v->last_return_mission=selected;
    strcpy(v->last_return_token,"0123456789abcdef0123456789abcdef");
}
static int walk(prj_archive *a,int mode,int verbose)
{
    mercs_campaign c={0};mercs_shell_state *v=calloc(1,sizeof *v);char e[384]={0};
    int step,i,pick;
    if(!v||mercs_campaign_new(a,mode,"WALKER","TEST UNIT",&c,v,e,sizeof e)){fprintf(stderr,"new: %s\n",e);return 1;}
    for(step=0;step<400;step++){
        if(c.phase==MERCS_CAMPAIGN_ENDED){printf("mode %d: career ended after %d contracts\n",mode,step);break;}
        if(c.phase!=MERCS_CAMPAIGN_READY){fprintf(stderr,"mode %d step %d: phase %d\n",mode,step,c.phase);return 1;}
        if(!c.active){
            pick=-1;
            for(i=0;i<v->mission_count&&pick<0;i++)if(strncasecmp(v->missions[i].campaign,"RAN",3))pick=i;
            if(pick<0)pick=0;
            if(v->mission_count<1){fprintf(stderr,"mode %d step %d: no contracts\n",mode,step);return 1;}
            if(mercs_campaign_accept(a,&c,v,pick,e,sizeof e)){fprintf(stderr,"mode %d step %d accept %s: %s\n",mode,step,v->missions[pick].campaign,e);return 1;}
        }
        if(!mode&&v->owned_index<0&&v->owned_count>0){
            if(mercs_shell_assign_owned(a,v,0,e,sizeof e)||mercs_campaign_sync(&c,v,e,sizeof e)){fprintf(stderr,"assign: %s\n",e);return 1;}
        }
        if(verbose)printf("mode %d step %d: %s (%s) funds %d mechs %d\n",mode,step,c.campaign,v->missions[c.accepted].scene,(int)v->funds,v->owned_count);
        controlled_return(v,c.accepted);
        if(mercs_campaign_return(&c,v,e,sizeof e)){fprintf(stderr,"mode %d step %d return: %s\n",mode,step,e);return 1;}
        if(mercs_campaign_enter_debrief(a,&c,v,e,sizeof e)){fprintf(stderr,"mode %d step %d debrief: %s\n",mode,step,e);return 1;}
        {   mercs_debrief_gate g;
            if(mercs_campaign_preflight(&c,v,&g,e,sizeof e)){fprintf(stderr,"mode %d step %d preflight: %s\n",mode,step,e);return 1;} }
        if(mercs_campaign_continue(a,&c,v,e,sizeof e)){fprintf(stderr,"mode %d step %d continue: %s\n",mode,step,e);return 1;}
    }
    if(c.phase!=MERCS_CAMPAIGN_ENDED){fprintf(stderr,"mode %d: no ending after %d steps (last %s)\n",mode,step,c.campaign);return 1;}
    mercs_campaign_free(&c);free(v);return 0;
}
int main(int argc,char **argv)
{
    prj_archive *a;char e[384];int rc;
    if(argc<2){fprintf(stderr,"usage: %s MW2.PRJ [-v]\n",argv[0]);return 2;}
    a=prj_open(argv[1],e,sizeof e);if(!a){fprintf(stderr,"%s\n",e);return 2;}
    rc=walk(a,0,argc>2)|walk(a,1,argc>2);
    prj_close(a);if(!rc)puts("PASS: both careers walked to the original ending");return rc;
}
