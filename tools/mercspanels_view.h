#ifndef MERCS_PANELS_VIEW_H
#define MERCS_PANELS_VIEW_H

#include "mercspanels.h"
#include "mercsart.h"
#include <stdio.h>
#include <string.h>

/* Original DOS0x5abcc decimal grouping, including the sign and INT32_MIN. */
static inline void mercs_lords_money(int32_t value, char out[24])
{
    char raw[16];
    size_t i,n,first,at=0;
    snprintf(raw,sizeof raw,"%d",(int)value); n=strlen(raw); first=raw[0]=='-' ? 1u : 0u;
    for (i=0;i<n;i++) {
        if (i>first && (n-i)%3==0) out[at++]=',';
        out[at++]=raw[i];
    }
    out[at]=0;
}

/* DOS0x469d0. A key or fresh left/right press returns to the shared menu.
 * Host consumes the input that opened this panel, and does not display modern
 * added buttons. This is a read-only view of original/default mw2high.cfg. */
static inline int mercs_lords_draw(mercs_art *a, const mercs_panels *data,
                                    const mercs_lords_table *table)
{
    unsigned i;
    if (!a || !data || !table || table->count>MERCS_LORDS_ROWS ||
        mercs_art_background(a,25)) return -1;
    mercs_art_text(a,6,91,150,data->headings[0],17);
    mercs_art_text(a,6,270,150,data->headings[1],17);
    mercs_art_text(a,6,570-mercs_art_width(a,6,data->headings[2]),150,data->headings[2],17);
    for (i=0;i<table->count;i++) {
        char money[24];
        int y=182+16*(int)i,ink=table->rows[i].highlighted==1 ? 220 : 17;
        mercs_lords_money(table->rows[i].wealth,money);
        mercs_art_text(a,10,91,y,table->rows[i].pilot,ink);
        mercs_art_text(a,10,270,y,table->rows[i].group,ink);
        mercs_art_text(a,10,570-mercs_art_width(a,10,money),y,money,ink);
    }
    return 0;
}

/* DOS0x30150. Original loop moves one pixel after two driver polls; its exact
 * wall-clock speed depends on the DOS driver. The host supplies scroll in
 * authored pixels (a native timing policy), never scaled display pixels.
 * Initial first-row y460; spacing20. Original draws rows y105..459 then
 * restores background strips105..124 and460..479, giving this exact clip.
 * The original waits for input even after all rows have passed. */
static inline int mercs_circuit_draw(mercs_art *a, const mercs_panels *data,
                                      unsigned pixels_scrolled)
{
    unsigned i;
    int base;
    if (!a || !data || mercs_art_background(a,28)) return -1;
    if (pixels_scrolled>460u+20u*MERCS_CIRCUIT_ROWS) return 0;
    base=460-(int)pixels_scrolled;
    for (i=0;i<MERCS_CIRCUIT_ROWS;i++) {
        const char *s=data->credits[i];
        int y=base+20*(int)i,font=10,ink=17;
        if (y<105 || y>=460 || !*s) continue;
        if (*s=='<') { font=5; s++; }
        else if (*s=='>') { font=6; s++; }
        else if (*s=='~') { font=5; ink=239; s++; }
        mercs_art_text_clipped(a,font,320-mercs_art_width(a,font,s)/2,y,s,ink,125,460);
    }
    return 0;
}

#endif
