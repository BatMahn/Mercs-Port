#ifndef MERCS_CREW_VIEW_H
#define MERCS_CREW_VIEW_H

/* DOS crew-photo screen0x400e4/0x402c0 and biography0x3fc20/0x3ff38.
 * Geometry/resource names are original table0xb6dd8/0xb6fb4. Biography prose
 * remains in the user's BWD ORDR resources; no art or prose is embedded. */
#include "mercsart.h"
#include "mercstext.h"
#include "mercsui_layout.h"
#include "bwd.h"
#include <stdlib.h>
#include <string.h>

static const mercs_ui_hotspot mercs_crew_hotspots[] = {
    {0,50,300,70,320,274,125,"DUSTIN BROWDER"},
    {1,110,270,130,290,274,125,"DAN KEGEL"},
    {2,155,267,170,280,274,125,"SMITH SURASMITH"},
    {3,175,260,190,275,274,125,"MATT CANDLER"},
    {4,195,250,206,260,274,125,"HENRY YU"},
    {5,212,235,225,250,274,125,"KEN HULLETT"},
    {6,236,230,255,250,274,125,"MASON DEMING"},
    {7,262,230,283,250,274,125,"TIM MORTEN"},
    {8,305,220,330,250,274,125,"JERRETT FARMER"},
    {9,375,215,400,240,274,125,"JOHN KEATING"},
    {10,410,215,430,240,274,125,"JACK MAMAIS"},
    {11,480,250,500,275,274,125,"DAIVA VENCKUS"},
    {12,580,200,610,230,274,125,"JOHN PECK"},
    {13,200,335,225,360,274,125,"BILL FERRER"},
    {14,240,365,270,400,274,125,"SEAN KINNEAR"},
    {15,190,400,210,425,274,125,"JJ FRANZEN"},
    {16,0,0,640,30,315,20,"EXIT"}
};
static const char *const mercs_crew_resources[16]={
    "DUSTBBIO","KEGELBIO","SMITHBIO","MATTBIO","HENRYBIO","KENBIO",
    "MASONBIO","TIMBIO","JERETBIO","KEATBIO","JACKMBIO","DAIVABIO",
    "PECKBIO","BILLFBIO","KBIO","JJBIO"
};
static inline int mercs_crew_hit(int x,int y)
{
    int i;
    for(i=0;i<17;i++){
        const mercs_ui_hotspot *h=&mercs_crew_hotspots[i];
        if(x>=h->x1&&x<=h->x2&&y>=h->y1&&y<=h->y2)return i;
    }
    return -1;
}
static inline char *mercs_crew_text(prj_archive *archive,int selected)
{
    prj_record record={0};bwd_chunk *chunks=NULL;char *raw=NULL;int count,i;
    if(selected<0||selected>=16||prj_read_named(archive,"BWD",mercs_crew_resources[selected],&record)!=PRJ_OK)return NULL;
    count=bwd_chunks(record.data,record.size,NULL,0);
    if(count<1||count>64)goto done;
    chunks=calloc((size_t)count,sizeof *chunks);if(!chunks)goto done;
    if(bwd_chunks(record.data,record.size,chunks,count)!=count)goto done;
    for(i=0;i<count;i++)if(!strcmp(chunks[i].tag,"ORDR")){
        size_t size=chunks[i].size;
        if(!size||size>65536)goto done;
        raw=malloc(size+1);if(!raw)goto done;
        memcpy(raw,chunks[i].data,size);raw[size]=0;break;
    }
done:free(chunks);prj_record_free(&record);return raw;
}
static inline void mercs_crew_draw(mercs_art *art,int x,int y,int subtitles)
{
    int index=mercs_crew_hit(x,y);
    mercs_art_background(art,46);
    if(subtitles&&index>=0){const mercs_ui_hotspot *h=&mercs_crew_hotspots[index];
        mercs_art_text(art,MERCS_UI_FONT_MENU,h->tx,h->ty,h->label,17);}
}
static inline int mercs_crew_bio_exit(int x,int y)
{return x>=501&&x<=602&&y>=431&&y<=473;}
static inline void mercs_crew_bio_draw(mercs_art *art,const mercs_text_layout *body,
                                       int scroll,int visible_rows,int x,int y)
{
    mercs_art_background(art,32);
    mercs_text_draw_rows(body,art,47,65,286,scroll,22,visible_rows);
    mercs_art_button(art,"ocbce",510,440,mercs_crew_bio_exit(x,y));
}
#endif
