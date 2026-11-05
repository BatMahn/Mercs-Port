#include <assert.h>
#include "../tools/mercsdebrief_view.h"
#include "mercs_debrief_view_fixtures.h"

#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
/* Same declared width boundary as the original instruction oracle. Original
 * font pixels/layout are verified by the frontend asset fixture separately. */
int mercs_art_width(mercs_art *art,int font,const char *text)
{ (void)art;(void)font;return (int)strlen(text)*6; }
static void put32(uint8_t *p,uint32_t n)
{ p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24); }
static void put16(uint8_t *p,unsigned n)
{ p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8); }
static void compare(const char *actual,const char *expected)
{
    if(strcmp(actual,expected)) {
        fprintf(stderr,"Actual: %s\nExpected: %s\n",actual,expected);assert(0);
    }
}
int main(void)
{
    size_t i,j;
    for(i=0;i<COUNT(mdv_report_cases);i++) {
        const mdv_report_fixture *f=&mdv_report_cases[i];mercs_msn_record msn;
        mercs_car_record car;mercs_debrief_report report;mdv_buffer output={0},finance={0};
        memset(&msn,0,sizeof msn);memset(&car,0,sizeof car);msn.outcome=2;msn.row_count=f->count;
        put16(car.bytes+0x1e,f->kills[0]);put16(car.bytes+0x4a,f->kills[1]);
        for(j=0;j<f->count;j++) {
            msn.rows[j].priority=f->rows[j].priority;msn.rows[j].succeeded=f->rows[j].state;
            msn.rows[j].resolved_seconds=f->rows[j].time;strcpy((char *)msn.rows[j].text,f->rows[j].text);
        }
        assert(!mercs_debrief_build(&f->terms,&car,&msn,&report));
        assert(!mdv_objectives(&output,NULL,&f->terms,&report,&msn));compare(output.data,f->objectives);
        if(f->terms.mode==1) {
            assert(!mdv_finances(&finance,NULL,&f->terms,&report));compare(finance.data,f->finances);
        }
        free(output.data);free(finance.data);
    }
    for(i=0;i<COUNT(mdv_salvage_cases);i++) {
        const mdv_salvage_fixture *f=&mdv_salvage_cases[i];mercs_save_state *doc=mercs_save_create();
        uint8_t items[3*26]={0},mechs[3*MERCS_CAREER_MECH_BYTES]={0};mdv_buffer output={0};
        assert(doc);
        for(j=0;j<f->ni;j++) {
            uint8_t *p=items+j*26;put32(p,(uint32_t)f->items[j][0]);
            p[8]=(uint8_t)f->items[j][1];p[9]=(uint8_t)f->items[j][2];
        }
        for(j=0;j<f->nm;j++) {
            uint8_t *p=mechs+j*MERCS_CAREER_MECH_BYTES;
            snprintf((char *)p+0x100,256,"MECH%u",(unsigned)j);p[0x84e]=(uint8_t)f->flags[j];
        }
        assert(!mercs_save_set_list(doc,MERCS_SAVE_SALVAGE_ITEMS,items,f->ni));
        assert(!mercs_save_set_list(doc,MERCS_SAVE_SALVAGE_MECHS,mechs,f->nm));
        assert(!mdv_salvage(&output,doc));compare(output.data,f->text);
        for(j=0;j<f->ni;j++)assert(!memcmp(items+j*26,mercs_save_list_record(doc,MERCS_SAVE_SALVAGE_ITEMS,j),26));
        free(output.data);mercs_save_free(doc);
    }
    for(i=0;i<COUNT(mdv_money_cases);i++) {
        char text[24];mdv_money(mdv_money_cases[i].value,text);compare(text,mdv_money_cases[i].text);
    }
    puts("PASS: original DOS objective, financial, salvage-report and money text; saved selection bytes unchanged");
    return 0;
}
