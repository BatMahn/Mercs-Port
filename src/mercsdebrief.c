#include "mercsdebrief.h"
#include "mercsdebrief_data.h"
#include "mercsdebrief_value_data.h"

#include <string.h>

static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void p32(uint8_t *p,uint32_t v)
{ p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24); }
static uint32_t u16(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8; }
static int32_t signed_bits(uint32_t n)
{ int32_t v;memcpy(&v,&n,sizeof v);return v; }
static int mode_ok(int mode) { return mode==0||mode==1; }
static int terminated(const char *s,size_t n) { return memchr(s,0,n)!=NULL; }

int mercs_debrief_enter(int mode,unsigned restore,const mercs_car_record *car,
                        mercs_debrief_entry *out)
{
    mercs_debrief_entry p;
    unsigned status;
    if(!mode_ok(mode)||restore>255||!car||!out)return MERCS_DEBRIEF_INVALID;
    memset(&p,0,sizeof p);p.show_report=1;
    status=car->bytes[MERCS_CAR_PLAYER_STATUS_OFFSET];
    /* DOS constructor0x3b5d0 bypasses both movies for any nonzero restore byte. */
    if(restore==0){
        if(status==2){
            p.movie=MERCS_DEBRIEF_MOVIE_EJECT;
            if(mode==1)p.remove_player_mech=p.unassign_player=1;
        }else if(status==4){
            p.movie=MERCS_DEBRIEF_MOVIE_DEATH;
            if(mode==1){p.career_end=1;p.show_report=0;}
        }
        if(mode==0)p.restore_training_mechs=1;
    }
    *out=p;return MERCS_DEBRIEF_OK;
}

int32_t mercs_debrief_expense(const mercs_debrief_terms *t)
{
    uint32_t sum;
    if(!t)return 0;
    /* DOS0x389dc; its arithmetic deliberately wraps at each original DWORD. */
    sum=((uint32_t)t->per_mech_a+(uint32_t)t->per_mech_b)*t->owned_mech_count;
    sum+=1u+(uint32_t)t->fixed_a+(uint32_t)t->fixed_b+(uint32_t)t->fixed_c;
    sum+=(uint32_t)t->pilot_pay+(uint32_t)t->aero_pay;
    return signed_bits(sum*(uint32_t)t->expense_factor);
}

int mercs_debrief_build(const mercs_debrief_terms *t,const mercs_car_record *car,
                        const mercs_msn_record *msn,mercs_debrief_report *out)
{
    mercs_debrief_report p;
    uint32_t income=0;
    size_t i;
    if(!t||!car||!msn||!out||!mode_ok(t->mode)||t->restore_flag>255||
       msn->row_count>MERCS_MSN_ROWS)return MERCS_DEBRIEF_INVALID;
    if(msn->outcome!=0&&msn->outcome!=2&&msn->outcome!=3&&msn->outcome!=4)
        return MERCS_DEBRIEF_UNSUPPORTED;
    memset(&p,0,sizeof p);p.success=msn->outcome==2;
    p.outcome=msn->outcome;p.row_count=msn->row_count;
    p.confirmed_kills=u16(car->bytes+0x1e)+u16(car->bytes+0x4a);
    /* Original report0x38d64: reward is based on nonzero row state, including
     * values the textual status renderer calls Unknown. */
    for(i=0;i<msn->row_count;i++)if(msn->rows[i].succeeded!=0){
        switch(msn->rows[i].priority){
        case 1:p.row_reward[i]=t->target_pay;p.target_count++;break;
        case 2:p.row_reward[i]=t->bonus_pay;p.bonus_count++;break;
        case 4:p.row_reward[i]=t->bounty_pay;p.bounty_count++;break;
        default:break;
        }
        income+=(uint32_t)p.row_reward[i];
    }
    p.objective_income=signed_bits(income);
    if(t->mode==1){
        p.kill_income=signed_bits(p.confirmed_kills*(uint32_t)t->bounty_pay);
        p.contract_income=p.success?t->success_pay:t->failure_pay;
        income+=(uint32_t)p.kill_income+(uint32_t)p.contract_income;
        p.expense=mercs_debrief_expense(t);
        p.balance=signed_bits((uint32_t)t->funds+income-(uint32_t)p.expense);
        p.select_salvage=p.success&&t->restore_flag!=1;
    }else p.balance=t->funds;
    p.income=signed_bits(income);
    *out=p;return MERCS_DEBRIEF_OK;
}

int mercs_debrief_continue(const mercs_debrief_terms *t,
                           const mercs_debrief_report *r,
                           mercs_debrief_commit *out)
{
    mercs_debrief_commit p;
    const char *next=NULL;
    char failure[sizeof t->failure_next];
    size_t i;
    if(!t||!r||!out||!mode_ok(t->mode)||
       !terminated(t->success_next,sizeof t->success_next)||
       !terminated(t->failure_next,sizeof t->failure_next)||
       (r->success!=0&&r->success!=1))return MERCS_DEBRIEF_INVALID;
    if(t->campaign_type<0||t->campaign_type>4)return MERCS_DEBRIEF_UNSUPPORTED;
    memcpy(failure,t->failure_next,sizeof failure);
    for(i=0;failure[i];i++)if(failure[i]>='a'&&failure[i]<='z')failure[i]-='a'-'A';
    memset(&p,0,sizeof p);p.funds_after=t->funds;
    if(t->mode==0){
        if(t->campaign_type==1&&r->success)next=t->success_next;
        else if((t->campaign_type==2||t->campaign_type==3)&&r->success)
            p.next=MERCS_DEBRIEF_NEXT_TRAINING_CLEANUP;
    }else{
        p.commit_funds=1;p.cleanup_lost=1;p.funds_after=r->balance;
        switch(t->campaign_type){
        case 0:next=t->success_next;break;
        case 1:
            if(r->success){p.transfer_salvage=1;next=t->success_next;}
            else if(!strcmp(failure,"DEATH"))p.next=MERCS_DEBRIEF_NEXT_CAREER_END;
            else next=failure;
            break;
        case 2:
            if(r->success)p.transfer_salvage=1;
            if(!r->success&&!strcmp(failure,"DEATH"))
                p.next=MERCS_DEBRIEF_NEXT_CAREER_END;
            else p.next=MERCS_DEBRIEF_NEXT_CONTRACT_END;
            break;
        case 3:
            if(r->success){p.transfer_salvage=1;next=t->success_next;}
            else p.next=MERCS_DEBRIEF_NEXT_CONTRACT_END;
            break;
        default:break;
        }
    }
    if(next){
        if(!*next)return MERCS_DEBRIEF_UNSUPPORTED;
        p.next=MERCS_DEBRIEF_NEXT_CAMPAIGN;
        memcpy(p.campaign,next,strlen(next)+1);
    }
    *out=p;return MERCS_DEBRIEF_OK;
}

int mercs_debrief_preflight(const mercs_debrief_terms *t,
                            const mercs_debrief_report *r,
                            const mercs_car_record *car,int32_t asset_value,
                            const char no_mech_campaign[16],
                            mercs_debrief_gate *out)
{
    mercs_debrief_gate p;
    size_t i;
    int dead;
    if(!t||!r||!car||!out||!mode_ok(t->mode)||t->restore_flag>255||
       (r->success!=0&&r->success!=1))return MERCS_DEBRIEF_INVALID;
    if(t->campaign_type<0||t->campaign_type>4)return MERCS_DEBRIEF_UNSUPPORTED;
    memset(&p,0,sizeof p);
    dead=car->bytes[MERCS_CAR_PLAYER_STATUS_OFFSET]==4;
    /* Handler3c384 invokes3bfe4,3c1d8,3bef4 in this order. */
    if(t->mode==1&&!dead&&r->balance<0){
        p.reason=MERCS_DEBRIEF_GATE_BANKRUPT;p.block_settlement=p.career_end=1;
        strcpy(p.movies[p.movie_count++],"mpoor");
    }else if(t->campaign_type==4&&(t->mode==1||r->success)){
        p.reason=MERCS_DEBRIEF_GATE_ENDING;p.block_settlement=p.career_end=1;
        if(t->mode==0||t->restore_flag==0)strcpy(p.movies[p.movie_count++],"mend");
        if(t->mode==1)strcpy(p.movies[p.movie_count++],
            signed_bits((uint32_t)t->funds+(uint32_t)asset_value)>=15000000?"mrich":"mpoor");
    }else if(t->mode==1&&!dead&&t->campaign_type!=4&&t->owned_mech_count==0){
        if(!no_mech_campaign||!terminated(no_mech_campaign,16))return MERCS_DEBRIEF_INVALID;
        if(!*no_mech_campaign)return MERCS_DEBRIEF_UNSUPPORTED;
        p.reason=MERCS_DEBRIEF_GATE_NO_MECHS;p.block_settlement=1;
        for(i=0;no_mech_campaign[i];i++){
            unsigned c=(unsigned char)no_mech_campaign[i];
            if(c>='a'&&c<='z')c-='a'-'A';
            p.campaign[i]=(char)c;
        }
        if(!strcmp(p.campaign,"DEATH")){p.career_end=1;p.campaign[0]=0;}
    }
    *out=p;return MERCS_DEBRIEF_OK;
}

int32_t mercs_debrief_age_value(int32_t year,int32_t acquired,int32_t base)
{
    static const int32_t years[5]={3025,3039,3050,3055,3058};
    static const double change[5][5]={
        {0,.5,.75,0,0},{-.1,0,.5,.75,0},{-.25,-.1,0,.5,.75},
        {-.5,-.25,-.1,0,.5},{0,-.5,-.25,-.1,0}
    };
    int i,j;
    volatile double product;
    double value;
    for(i=0;i<5&&year!=years[i];i++);
    for(j=0;j<5&&acquired!=years[j];j++);
    if(i==5||j==5)return base;
    /* Separate double multiply and add match initialized DOS x87 precision;
     * the original's conversion helper truncates towards zero. */
    product=(double)base*change[i][j];value=(double)base+product;
    if(value>=2147483648.0||value< -2147483648.0)return INT32_MIN;
    return (int32_t)value;
}

int32_t mercs_debrief_item_value(const uint8_t item[26])
{return item?signed_bits(u32(item+4)):0;}

int mercs_debrief_mech_value(const mercs_career_mech *m,int32_t year,int32_t *out)
{
    uint32_t total;
    int32_t base=-1;
    uint8_t seen[8][12]={{0}};
    char name[8]={0};
    size_t i,j,k,l;
    if(!m||!out||!memchr(m->bytes+0x81c,0,7))return MERCS_DEBRIEF_INVALID;
    for(i=0;i<7&&m->bytes[0x81c+i];i++){
        unsigned c=m->bytes[0x81c+i];if(c>='a'&&c<='z')c-='a'-'A';name[i]=(char)c;
    }
    for(i=0;i<71;i++)if(!strcmp(name,debrief_value_models[i].name)){
        base=debrief_value_models[i].price;break;
    }
    if(base==-1)base=2000000;
    total=(uint32_t)mercs_debrief_age_value(year,signed_bits(u32(m->bytes+0x843)),base);
    for(i=0;i<8;i++){
        int32_t armor=signed_bits(u32(m->bytes+0x768+18*i));
        int32_t rear=signed_bits(u32(m->bytes+0x76c+18*i));
        int32_t internal=signed_bits(u32(m->bytes+0x760+18*i));
        if(armor>=0)total+=(uint32_t)armor*625u;
        if(rear>=0)total+=(uint32_t)rear*625u;
        if(internal>0)total+=(uint32_t)internal*200u;
        for(j=0;j<12;j++){
            int32_t critical=signed_bits(u32(m->bytes+0x368+48*i+4*j));
            if(critical>=7000&&critical<10000){
                for(k=23;k>0;k--)if(critical/50==debrief_value_equipment[k-1].critical/50){
                    total+=(uint32_t)debrief_value_equipment[k-1].price;break;
                }
            }
        }
    }
    for(i=0;i<8;i++)for(j=0;j<12;j++)if(!seen[i][j]){
        int32_t critical=signed_bits(u32(m->bytes+0x368+48*i+4*j));
        uint32_t id;
        seen[i][j]=1;
        if(critical<=0||(critical>=7000&&critical<10000))continue;
        id=(uint32_t)(critical<7000?critical/100:(critical-10000)/10000);
        if(id>=66)return MERCS_DEBRIEF_INVALID;
        /* DOS0x40af4 retains j+1 as the starting column for EACH remaining
         * location. Preserve that original quirk, rather than full-grid
         * deduplication or counting one price per occupied critical slot. */
        for(k=i;k<8;k++)for(l=j+1;l<12;l++)
            if(u32(m->bytes+0x368+48*k+4*l)==(uint32_t)critical)seen[k][l]=1;
        total+=(uint32_t)(critical<7000?debrief_weapon_price[id]:debrief_ammo_price[id]);
    }
    *out=signed_bits(total);return MERCS_DEBRIEF_OK;
}

int mercs_debrief_restore_training_pair(mercs_career_mech_pair *pair)
{
    mercs_career_mech ref;
    if(!pair||u32(pair->current.bytes+0x810)!=u32(pair->reference.bytes+0x810))
        return MERCS_DEBRIEF_INVALID;
    ref=pair->reference;
    memcpy(ref.bytes+0x80c,pair->current.bytes+0x80c,4);
    pair->reference=ref;pair->current=ref;
    return MERCS_DEBRIEF_OK;
}

uint32_t mercs_debrief_rand(uint32_t *state)
{
    if(!state)return 0;
    *state=*state*UINT32_C(0x41c64e6d)+UINT32_C(0x3039);
    return (*state>>16)&UINT32_C(0x7fff);
}

int mercs_debrief_roll_salvage(unsigned restore,mercs_salvage_choice *items,
                              size_t ni,mercs_salvage_choice *mechs,size_t nm,
                              uint32_t *state)
{
    size_t i;
    if(restore>255||!state||(ni&&!items)||(nm&&!mechs)||ni>4096||nm>4096)
        return MERCS_DEBRIEF_INVALID;
    if(restore==1)return MERCS_DEBRIEF_OK;
    for(i=0;i<ni;i++)items[i].selected=(uint8_t)(items[i].threshold>=
                                      (int32_t)(mercs_debrief_rand(state)%100));
    for(i=0;i<nm;i++)mechs[i].selected=(uint8_t)(mechs[i].threshold>=
                                      (int32_t)(mercs_debrief_rand(state)%100));
    return MERCS_DEBRIEF_OK;
}

int mercs_debrief_salvage_pair(const mercs_career_mech *candidate,
                               const mercs_career_mech *reference,
                               const char *filename,mercs_career_mech_pair *out)
{
    mercs_career_mech_pair p;
    size_t i,n;
    if(!candidate||!reference||!filename||!out)return MERCS_DEBRIEF_INVALID;
    for(n=0;n<MERCS_CAREER_FILENAME_BYTES&&filename[n];n++){
        unsigned c=(unsigned char)filename[n];
        if(c<33||c>126||strchr("/\\:*?\"<>|",(int)c))return MERCS_DEBRIEF_INVALID;
    }
    if(n<8||n>=MERCS_CAREER_FILENAME_BYTES||strstr(filename,"..")||
       candidate->bytes[0x84e]!=1)return MERCS_DEBRIEF_INVALID;
    /* Caller loaded the matching stock chassis. */
    for(i=0;i<3;i++){
        unsigned a=candidate->bytes[0x823+i],b=reference->bytes[0x823+i],c=(unsigned char)filename[i];
        if(a>='a'&&a<='z')a-='a'-'A';
        if(b>='a'&&b<='z')b-='a'-'A';
        if(c>='a'&&c<='z')c-='a'-'A';
        if(a!=b||a!=c)return MERCS_DEBRIEF_INVALID;
    }
    p.current=*candidate;p.reference=*reference;
    memcpy(p.current.bytes+0x823,filename,n+1);
    p32(p.reference.bytes+0x810,u32(candidate->bytes+0x810));
    for(i=0;i<8;i++)memcpy(p.current.bytes+0x770+18*i,p.reference.bytes+0x770+18*i,2);
    *out=p;return MERCS_DEBRIEF_OK;
}

int mercs_debrief_compare_time(int32_t a,int32_t b)
{
    if(a<0)return 1;
    if(b<0)return -1;
    return a>b?1:a<b?-1:0;
}

static int row_compare(const mercs_msn_record *msn,uint32_t a,uint32_t b)
{ return mercs_debrief_compare_time(msn->rows[a].resolved_seconds,msn->rows[b].resolved_seconds); }
static void row_swap(uint32_t *a,uint32_t *b)
{ uint32_t value=*a;*a=*b;*b=value; }

/* Exact branch order of median helper0x7b402 matters for equal/negative times. */
static int row_median(const mercs_msn_record *msn,const uint32_t *order,
                      int a,int b,int c)
{
    if(row_compare(msn,order[a],order[b])>0) {
        if(row_compare(msn,order[a],order[c])>0)
            return row_compare(msn,order[b],order[c])>0?b:c;
        return a;
    }
    if(row_compare(msn,order[a],order[c])>=0)return a;
    return row_compare(msn,order[b],order[c])>0?c:b;
}

static int row_sort(const mercs_msn_record *msn,uint32_t *order,int count)
{
    int a,b,c,d,i,j,gap,median,step,left,right,amount;
    uint32_t pivot;
    if(count<2)return 0;
    if(count<16) {
        /* Original0x7b4ba advances the outer cursor by the gap, too. This is
         * deliberately not ordinary insertion sort or libc's small sorter. */
        for(gap=3;gap>0;gap-=2)
            for(i=gap;i<count;i+=gap)
                for(j=i;j>0&&row_compare(msn,order[j-gap],order[j])>0;j-=gap)
                    row_swap(&order[j-gap],&order[j]);
        return 0;
    }
    median=count/2;
    if(count>29) {
        a=0;c=count-1;
        if(count>42) {
            step=count/8;
            a=row_median(msn,order,0,step,2*step);
            median=row_median(msn,order,median-step,median,median+step);
            c=row_median(msn,order,c-2*step,c-step,c);
        }
        median=row_median(msn,order,a,median,c);
    }
    /* The DOS array has aligned DWORD elements: 0x7b661 copies the pivot into
     * a local DWORD without swapping the selected row to the start. */
    pivot=order[median];a=b=0;c=d=count-1;
    for(;;) {
        while(b<=c&&(i=row_compare(msn,order[b],pivot))<=0) {
            if(i==0)row_swap(&order[a++],&order[b]);
            b++;
        }
        while(b<=c&&(i=row_compare(msn,order[c],pivot))>=0) {
            if(i==0)row_swap(&order[c],&order[d--]);
            c--;
        }
        if(b>c)break;
        row_swap(&order[b++],&order[c--]);
    }
    amount=a<b-a?a:b-a;
    for(i=0;i<amount;i++)row_swap(&order[i],&order[b-amount+i]);
    amount=d-c<count-d-1?d-c:count-d-1;
    for(i=0;i<amount;i++)row_swap(&order[b+i],&order[count-amount+i]);
    left=b-a;right=d-c;
    if(left>=count||right>=count)return MERCS_DEBRIEF_UNSUPPORTED;
    /* Original processes the smaller side first and stacks the larger side. */
    if(right>=left) {
        if(row_sort(msn,order,left))return MERCS_DEBRIEF_UNSUPPORTED;
        return row_sort(msn,order+count-right,right);
    }
    if(row_sort(msn,order+count-right,right))return MERCS_DEBRIEF_UNSUPPORTED;
    return row_sort(msn,order,left);
}

int mercs_debrief_order_rows(const mercs_msn_record *msn,uint32_t out[MERCS_MSN_ROWS])
{
    uint32_t order[MERCS_MSN_ROWS],i;
    int result;
    if(!msn||!out||msn->row_count>MERCS_MSN_ROWS)return MERCS_DEBRIEF_INVALID;
    for(i=0;i<msn->row_count;i++)order[i]=i;
    result=row_sort(msn,order,(int)msn->row_count);
    if(result)return result;
    memcpy(out,order,msn->row_count*sizeof order[0]);
    return MERCS_DEBRIEF_OK;
}

int mercs_debrief_salvage_item(const uint8_t candidate[26],
                              uint8_t owned[26],int *destination)
{
    uint8_t p[26];
    uint32_t id;
    int32_t value;
    int dest;
    if(!candidate||!owned||!destination)return MERCS_DEBRIEF_INVALID;
    if(candidate[9]!=1||candidate[8]>2)return MERCS_DEBRIEF_SKIPPED;
    id=u32(candidate);
    if(candidate[8]!=0&&id>=66)return MERCS_DEBRIEF_INVALID;
    memcpy(p,candidate,sizeof p);
    dest=MERCS_DEBRIEF_INVENTORY_WEAPON;
    /* Preserve the original dispatch, including its type-byte convention:
     * SWNL type0 uses the equipment helper, type2 the weapon helper. Neither
     * helper converts the type byte when appending to the owned list. */
    if(p[8]==0){
        value=10000;
        if(id==1||id==2||id==5||id==6)value=debrief_equipment_price[id];
        else if(id==3||id==4)value=50000;
    }else if(p[8]==1){
        value=debrief_ammo_price[id];if(value<1)value=10000;
        p32(p+0x16,100);p32(p+0x12,0);
        dest=MERCS_DEBRIEF_INVENTORY_AMMO;
    }else{
        value=debrief_weapon_price[id];if(value<1)value=10000;
        p32(p+0x12,100);p32(p+0xa,UINT32_MAX);p32(p+0xe,0);
    }
    p32(p+4,(uint32_t)value);
    memcpy(owned,p,sizeof p);*destination=dest;
    return MERCS_DEBRIEF_OK;
}
