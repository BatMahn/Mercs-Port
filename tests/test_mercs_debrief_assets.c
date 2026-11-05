#include "sdl3_test.h"
/* Original executed DOS report/layout/cursor goldens with user-supplied font
 * and PRJ assets. Arguments: MW2.PRJ DATABASE.MW2. No display server required. */
#include <assert.h>
#include "../tools/mercsdebrief_view.h"
#include "mercs_debrief_view_font_fixtures.h"
#include "mercs_debrief_view_layout_fixtures.h"
#include "datapath.h"

#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static void put16(uint8_t *p,unsigned n)
{ p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8); }
static uint64_t word_hash(uint64_t h,uint32_t value)
{
    int i;
    for(i=0;i<4;i++){h=(h^(value&255))*UINT64_C(1099511628211);value>>=8;}
    return h;
}
static void check_layout(mercs_art *art,const mercs_text_layout *layout,
                         const mdv_layout_golden *expected,const char *label)
{
    uint64_t h=UINT64_C(14695981039346656037);size_t i;int count=0;
    for(i=0;i<layout->count;i++) {
        const mercs_text_run *r=&layout->runs[i];const unsigned char *p=(const unsigned char *)r->text;
        int x=r->x;
        for(;*p;p++) {
            char ch[2]={(char)*p,0};int w=mercs_art_width(art,r->font,ch);
            if(w) {
                h=word_hash(h,(uint32_t)x);h=word_hash(h,(uint32_t)r->y);
                h=word_hash(h,(uint32_t)r->font);h=word_hash(h,*p);
                h=word_hash(h,(uint32_t)(r->ink<0?22:r->ink));count++;
            }
            x+=w;
        }
    }
    if(h!=expected->hash||count!=expected->glyphs||layout->rows!=expected->rows)
        fprintf(stderr,"%s: hash%016llx/%016llx glyphs%d/%d rows%d/%d\n",label,
            (unsigned long long)h,(unsigned long long)expected->hash,count,expected->glyphs,
            layout->rows,expected->rows);
    assert(h==expected->hash&&count==expected->glyphs&&layout->rows==expected->rows);
    assert(!layout->unsupported_controls&&layout->line_height==12);
}
static void result(const mdv_report_fixture *f,int outcome,mercs_car_record *car,
                   mercs_msn_record *msn,mercs_debrief_report *report)
{
    unsigned i;memset(car,0,sizeof *car);memset(msn,0,sizeof *msn);
    put16(car->bytes+0x1e,f->kills[0]);put16(car->bytes+0x4a,f->kills[1]);
    msn->outcome=(uint32_t)outcome;msn->row_count=f->count;
    for(i=0;i<f->count;i++) {
        msn->rows[i].succeeded=f->rows[i].state;msn->rows[i].priority=f->rows[i].priority;
        msn->rows[i].resolved_seconds=f->rows[i].time;
        strcpy((char *)msn->rows[i].text,f->rows[i].text);
    }
    assert(!mercs_debrief_build(&f->terms,car,msn,report));
}
int main(int argc,char **argv)
{
    shelldb database;prj_archive *archive;SDL_Surface *surface;SDL_Renderer *renderer;
    mercs_art *art;mercs_text_layout layout={0};mercs_debrief_view view={0};
    mercs_shell_state *shell_view=calloc(1,sizeof *shell_view);mercs_save_state *doc=mercs_save_create();
    char error[256],root[1024],*slash;size_t i;
    assert(argc==3&&shell_view&&doc);
    archive=prj_open(argv[1],error,sizeof error);assert(archive);
    assert(!shelldb_open(argv[2],&database));assert(SDL_Init(0));
    surface=SDL_CreateSurface(640, 480, SDL_PIXELFORMAT_ARGB8888);assert(surface);
    renderer=SDL_CreateSoftwareRenderer(surface);assert(renderer);
    art=mercs_art_create(&database,renderer);assert(art);
    snprintf(root,sizeof root,"%s",argv[1]);slash=strrchr(root,'/');assert(slash);*slash=0;
    assert(!dp_add_root(root));
    assert(mercs_art_height(art,10)==10&&mercs_art_height(art,6)==16);
    for(i=0;i<COUNT(mdv_section_goldens);i++) {
        const mdv_section_golden *g=&mdv_section_goldens[i];const mdv_report_fixture *f=&mdv_report_cases[g->source];
        mercs_car_record car;mercs_msn_record msn;mercs_debrief_report report;mdv_buffer output={0};
        const char *expected=g->finances?f->finances:f->objectives;
        result(f,2,&car,&msn,&report);
        assert(!(g->finances?mdv_finances(&output,art,&f->terms,&report):
                            mdv_objectives(&output,art,&f->terms,&report,&msn)));
        assert(!strcmp(output.data,expected));
        assert(!mercs_text_prepare(&layout,art,output.data,"",10,558));
        check_layout(art,&layout,&g->expected,g->finances?"finance":"objectives");free(output.data);
    }
    strcpy(shell_view->pilot,"HAWKES");strcpy(shell_view->unit,"HAWKES MERCS");
    for(i=0;i<COUNT(mdv_page_goldens);i++) {
        const mdv_page_golden *g=&mdv_page_goldens[i];const mdv_report_fixture *f=&mdv_report_cases[g->source];
        mercs_car_record car;mercs_debrief_report report;char scene[16];int page;
        result(f,g->outcome,&car,&shell_view->last_msn,&report);
        snprintf(scene,sizeof scene,"%.4sSCN1",g->resource);
        assert(!mercs_debrief_view_prepare(&view,art,archive,doc,shell_view,&f->terms,&report,scene,error,sizeof error));
        assert(!strcmp(view.resource,g->resource)&&view.ready&&view.content_mode==0);
        for(page=0;page<2;page++)check_layout(art,&view.text[page],&g->pages[page],g->resource);
        assert(mercs_debrief_view_click(&view,110,430)==MERCS_DEBRIEF_VIEW_TOGGLE&&view.content_mode==1);
        assert(mercs_debrief_view_click(&view,110,430)==MERCS_DEBRIEF_VIEW_TOGGLE&&view.content_mode==0);
        assert(mercs_debrief_view_click(&view,520,440)==MERCS_DEBRIEF_VIEW_NONE);
        mercs_debrief_view_scroll(&view,INT32_MAX);assert(view.scroll[0]>=0);
        mercs_debrief_view_scroll(&view,INT32_MIN);assert(view.scroll[0]==0);
        mercs_art_begin_page(art);mercs_debrief_view_draw(&view,art);assert(!mercs_art_present(art));
        /* A missing page must leave the last prepared view fully usable. */
        assert(mercs_debrief_view_prepare(&view,art,archive,doc,shell_view,&f->terms,&report,"ZZZZ",error,sizeof error)<0);
        assert(view.ready&&!strcmp(view.resource,g->resource));
    }
    /* The width-independent display fixtures remain covered by the standalone
     * text test; their definitions share this generated font fixture header. */
    (void)mdv_salvage_cases;(void)mdv_money_cases;
    mercs_debrief_view_free(&view);mercs_text_free(&layout);mercs_save_free(doc);free(shell_view);
    mercs_art_destroy(art);SDL_DestroyRenderer(renderer);SDL_DestroySurface(surface);SDL_Quit();
    shelldb_close(&database);prj_close(archive);dp_clear_roots();
    puts("PASS:9 original DOS report glyph goldens,16 authored debrief/analysis pages with actual fonts, original controls and atomic view preparation");
    return 0;
}
