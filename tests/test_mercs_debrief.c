#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mercsdebrief.h"
#include "mercs_debrief_original_fixtures.h"
#include "mercs_debrief_value_fixtures.h"
#include "mercs_debrief_sort_fixtures.h"

#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static int32_t sb(uint32_t x){int32_t v;memcpy(&v,&x,4);return v;}
static unsigned nyb(char c){return c>='0'&&c<='9'?(unsigned)(c-'0'):(unsigned)(c-'a'+10);}
static size_t unhex(const char *s,uint8_t *p){size_t n=0;for(;*s;s+=2)p[n++]=(uint8_t)((nyb(s[0])<<4)|nyb(s[1]));return n;}
static void bytes(debrief_hex h,uint8_t *p,size_t size){size_t n=unhex(h.a,p);n+=unhex(h.b,p+n);assert(n==size);}
static void p16(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void p32(uint8_t *p,uint32_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24);}

int main(void)
{
    size_t i,k;
    for(i=0;i<COUNT(debrief_entry_cases);i++){
        const debrief_entry_fixture *f=&debrief_entry_cases[i];mercs_car_record car;mercs_debrief_entry p;
        memset(&car,0,sizeof car);car.bytes[0x1d]=(uint8_t)f->status;
        assert(mercs_debrief_enter(f->mode,(unsigned)f->restore,&car,&p)==0);
        assert(p.movie==f->movie&&p.remove_player_mech==f->remove&&p.unassign_player==f->remove);
        assert(p.career_end==f->career_end&&p.show_report==f->show_report);
        assert(p.restore_training_mechs==(f->mode==0&&f->restore==0));
    }
    for(i=0;i<COUNT(debrief_report_cases);i++){
        const debrief_report_fixture *f=&debrief_report_cases[i];mercs_car_record car;mercs_msn_record msn;mercs_debrief_report p;
        memset(&car,0,sizeof car);memset(&msn,0,sizeof msn);msn.row_count=f->count;msn.outcome=2;
        p16(car.bytes+0x1e,f->kills[0]);p16(car.bytes+0x4a,f->kills[1]);
        for(k=0;k<f->count;k++){msn.rows[k].succeeded=f->rows[k][0];msn.rows[k].priority=f->rows[k][1];}
        assert(mercs_debrief_build(&f->terms,&car,&msn,&p)==0);
        assert(sb((uint32_t)p.income-(uint32_t)p.contract_income)==f->income);
        assert(p.target_count==f->counts[0]&&p.bonus_count==f->counts[1]&&p.bounty_count==f->counts[2]);
        assert(p.confirmed_kills==f->kills[0]+f->kills[1]);
    }
    for(i=0;i<COUNT(debrief_regular_cases);i++){
        const debrief_regular_fixture *f=&debrief_regular_cases[i];mercs_debrief_terms t=f->terms;
        mercs_car_record car;mercs_msn_record msn;mercs_debrief_report p;
        memset(&car,0,sizeof car);memset(&msn,0,sizeof msn);msn.row_count=1;msn.outcome=(uint32_t)f->outcome;
        msn.rows[0].succeeded=1;msn.rows[0].priority=1;t.target_pay=777;
        assert(mercs_debrief_build(&t,&car,&msn,&p)==0);
        assert(p.income==f->income&&p.contract_income==f->compensation&&p.success==f->success);
        assert(p.select_salvage==(f->success&&t.restore_flag!=1));
    }
    for(i=0;i<COUNT(debrief_novice_cases);i++){
        const debrief_novice_fixture *f=&debrief_novice_cases[i];mercs_debrief_terms t;mercs_car_record car;
        mercs_msn_record msn;mercs_debrief_report p;mercs_debrief_entry e;
        memset(&t,0,sizeof t);memset(&car,0,sizeof car);memset(&msn,0,sizeof msn);
        t.restore_flag=(unsigned)f->restore;msn.outcome=(uint32_t)f->outcome;
        assert(mercs_debrief_build(&t,&car,&msn,&p)==0&&p.success==f->success);
        assert(mercs_debrief_enter(0,(unsigned)f->restore,&car,&e)==0&&e.restore_training_mechs==f->restore_training);
    }
    for(i=0;i<COUNT(debrief_expense_cases);i++)assert(mercs_debrief_expense(&debrief_expense_cases[i].terms)==debrief_expense_cases[i].expense);
    for(i=0;i<COUNT(debrief_continue_cases);i++){
        const debrief_continue_fixture *f=&debrief_continue_cases[i];mercs_debrief_report r;mercs_debrief_commit p;
        memset(&r,0,sizeof r);r.success=f->success;
        r.balance=sb((uint32_t)f->terms.funds+(uint32_t)f->income-(uint32_t)mercs_debrief_expense(&f->terms));
        assert(mercs_debrief_continue(&f->terms,&r,&p)==0);
        assert(p.funds_after==f->funds&&p.next==f->next&&p.cleanup_lost==f->cleanup&&p.transfer_salvage==f->salvage);
        assert(!strcmp(p.campaign,f->campaign)&&p.commit_funds==(f->terms.mode==1));
    }
    for(i=0;i<COUNT(debrief_rng_cases);i++){
        const debrief_rng_fixture *f=&debrief_rng_cases[i];uint32_t state=f->seed;
        for(k=0;k<16;k++)assert(mercs_debrief_rand(&state)==f->values[k]);
        assert(state==f->final);
    }
    for(i=0;i<COUNT(debrief_roll_cases);i++){
        const debrief_roll_fixture *f=&debrief_roll_cases[i];uint32_t state=f->seed;
        mercs_salvage_choice items[7]={{-1,127},{0,127},{1,127},{49,127},{50,127},{99,127},{100,127}};
        mercs_salvage_choice mechs[4]={{0,127},{50,127},{99,127},{100,127}};
        assert(mercs_debrief_roll_salvage((unsigned)f->restore,items,7,mechs,4,&state)==0&&state==f->final);
        for(k=0;k<7;k++)assert(items[k].selected==f->selected[k]);
        for(k=0;k<4;k++)assert(mechs[k].selected==f->selected[7+k]);
    }
    for(i=0;i<COUNT(debrief_training_cases);i++){
        const debrief_training_fixture *f=&debrief_training_cases[i];mercs_career_mech_pair p,expected;
        bytes(f->current,p.current.bytes,sizeof p.current.bytes);bytes(f->reference,p.reference.bytes,sizeof p.reference.bytes);
        bytes(f->after,expected.current.bytes,sizeof expected.current.bytes);bytes(f->reference_after,expected.reference.bytes,sizeof expected.reference.bytes);
        assert(mercs_debrief_restore_training_pair(&p)==0&&!memcmp(&p,&expected,sizeof p));
    }
    for(i=0;i<COUNT(debrief_salvage_cases);i++){
        const debrief_salvage_fixture *f=&debrief_salvage_cases[i];mercs_career_mech c,r;mercs_career_mech_pair p,expected;
        bytes(f->candidate,c.bytes,sizeof c.bytes);bytes(f->stock,r.bytes,sizeof r.bytes);
        bytes(f->current_after,expected.current.bytes,sizeof expected.current.bytes);bytes(f->reference_after,expected.reference.bytes,sizeof expected.reference.bytes);
        assert(mercs_debrief_salvage_pair(&c,&r,"PLY07MER.MEK",&p)==0&&!memcmp(&p,&expected,sizeof p));
    }
    for(i=0;i<COUNT(debrief_item_cases);i++){
        const debrief_item_fixture *f=&debrief_item_cases[i];uint8_t in[26],out[26],expected[26];int dest=-1;
        assert(unhex(f->input,in)==26&&unhex(f->output,expected)==26);
        assert(mercs_debrief_salvage_item(in,out,&dest)==0&&!memcmp(out,expected,26));
        assert(dest==(in[8]==1?MERCS_DEBRIEF_INVENTORY_AMMO:MERCS_DEBRIEF_INVENTORY_WEAPON));
    }
    for(i=0;i<COUNT(debrief_gate_cases);i++){
        const debrief_gate_fixture *f=&debrief_gate_cases[i];mercs_debrief_report r;mercs_car_record car;mercs_debrief_gate p;
        char fallback[16]={0};memset(&r,0,sizeof r);memset(&car,0,sizeof car);
        r.success=f->success;r.balance=sb((uint32_t)f->terms.funds+(uint32_t)f->income-(uint32_t)mercs_debrief_expense(&f->terms));
        car.bytes[0x1d]=(uint8_t)f->status;strcpy(fallback,f->fallback);
        assert(mercs_debrief_preflight(&f->terms,&r,&car,f->asset_value,fallback,&p)==0);
        assert(p.reason==f->reason&&p.block_settlement==(f->reason!=0)&&p.career_end==f->career_end);
        assert(!strcmp(p.campaign,f->campaign)&&p.movie_count==f->movie_count);
        for(k=0;k<2;k++)assert(!strcmp(p.movies[k],f->movies[k]));
    }
    for(i=0;i<COUNT(debrief_age_cases);i++)
        assert(mercs_debrief_age_value(debrief_age_cases[i].year,debrief_age_cases[i].acquired,debrief_age_cases[i].base)==debrief_age_cases[i].value);
    for(i=0;i<COUNT(debrief_value_cases);i++){
        mercs_career_mech m;int32_t value;
        bytes(debrief_value_cases[i].record,m.bytes,sizeof m.bytes);
        assert(mercs_debrief_mech_value(&m,debrief_value_cases[i].year,&value)==0&&value==debrief_value_cases[i].value);
    }
    for(i=0;i<COUNT(debrief_item_value_cases);i++){
        uint8_t item[26]={0};p32(item+4,(uint32_t)debrief_item_value_cases[i].input);
        assert(mercs_debrief_item_value(item)==debrief_item_value_cases[i].value);
    }
    /* Portable rejection is atomic, unlike unchecked original pointer/index access. */
    {
        mercs_debrief_terms t;mercs_car_record car;mercs_msn_record msn;mercs_debrief_report p,before;
        memset(&t,0,sizeof t);memset(&car,0,sizeof car);memset(&msn,0,sizeof msn);memset(&p,0xa5,sizeof p);before=p;
        msn.outcome=1;assert(mercs_debrief_build(&t,&car,&msn,&p)<0&&!memcmp(&p,&before,sizeof p));
        msn.outcome=2;msn.row_count=49;assert(mercs_debrief_build(&t,&car,&msn,&p)<0&&!memcmp(&p,&before,sizeof p));
        {uint8_t in[26]={0},out[26],old[26];int dest=123;memset(out,0xa5,26);memcpy(old,out,26);
         assert(mercs_debrief_salvage_item(in,out,&dest)==MERCS_DEBRIEF_SKIPPED&&!memcmp(old,out,26)&&dest==123);
         in[8]=2;in[9]=1;in[0]=255;assert(mercs_debrief_salvage_item(in,out,&dest)<0&&!memcmp(old,out,26)&&dest==123);}
    }
    assert(mercs_debrief_compare_time(-1,-1)==1&&mercs_debrief_compare_time(0,-1)==-1&&mercs_debrief_compare_time(3,3)==0);
    for(i=0;i<COUNT(debrief_sort_cases);i++) {
        const debrief_sort_fixture *f=&debrief_sort_cases[i];mercs_msn_record msn;
        uint32_t order[MERCS_MSN_ROWS],before[MERCS_MSN_ROWS];int result;
        memset(&msn,0,sizeof msn);msn.row_count=f->count;
        for(k=0;k<f->count;k++)msn.rows[k].resolved_seconds=f->times[k];
        memset(order,0xa5,sizeof order);memcpy(before,order,sizeof before);
        result=mercs_debrief_order_rows(&msn,order);
        if(f->nonprogress)assert(result==MERCS_DEBRIEF_UNSUPPORTED&&!memcmp(order,before,sizeof order));
        else {
            if(result||memcmp(order,f->order,f->count*sizeof order[0])) {
                fprintf(stderr,"Original row order differs at fixture %u (%u rows)\n",(unsigned)i,f->count);assert(0);
            }
        }
    }
    puts("PASS: original DOS debrief entry, rewards, expenses, continuation, ending gates, asset values, RNG, salvage, training restore and bounded failures");
    return 0;
}
