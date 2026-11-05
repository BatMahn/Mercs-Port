/* Original Mercs Cockpit Controls tables0xb49ec/0xb228c and callbacks.
 * Metadata and map generation are separate from the SDL event adapter. */
#ifndef MERCSCONTROLS_VIEW_H
#define MERCSCONTROLS_VIEW_H
#include "mercsart.h"
#include "mercscontrols.h"
#include "datapath.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
enum { MCV_NONE, MCV_ACCEPT, MCV_CUSTOM, MCV_NOVICE, MCV_DEFAULTS,
       MCV_LOAD_1=10, MCV_SAVE_1=20 };
typedef struct {
    int active, editor, level, control, half, device, scroll, slot, novice;
    int selected[MC_DEVICES], present[MC_DEVICES], changed, editing_name, logo_restart;
    char old_name[64];
    mercs_control_config config;
} mercs_controls_view;
static int mcv_inside(int x,int y,int bx,int by,int w,int h)
{ return x>=bx && x<bx+w && y>=by && y<by+h; }
static int mcv_text_hit(mercs_art *art,int x,int y,int bx,int by,const char *text)
{ return mcv_inside(x,y,bx,by,mercs_art_width(art,10,text),mercs_art_height(art,10)); }
static int mercs_controls_open(mercs_controls_view *v,int novice)
{
    int i; char path[80]; unsigned char *data; size_t size;
    memset(v,0,sizeof *v); v->control=-1; v->novice=novice!=0; v->logo_restart=1;
    if (mercs_controls_load(&v->config,"CONFIG00.CPC") && mercs_controls_load(&v->config,"KEYBOARD.CPC")) return -1;
    mercs_controls_selected(&v->config,v->selected);
    for (i=0;i<MC_DEVICES;i++) {
        const mercs_control_device *d=mercs_control_device_at(i);
        if (!d) continue;
        snprintf(path,sizeof path,"GIDDI/%s.DLL",d->name);
        if (!dp_read_file(path,&data,&size)) { v->present[i]=size>0; free(data); }
    }
    /* SDL supplies the keyboard/mouse. Metadata is the original driver's;
     * legacy serial/HMD hardware transport is not executed by this editor. */
    v->present[0]=1; v->active=1; return 0;
}
static int mcv_selected_count(const mercs_controls_view *v)
{ int i,n=0; for (i=0;i<MC_DEVICES;i++) n+=v->selected[i]!=0; return n; }
static void mcv_binding_text(const mercs_control_binding *b,int half,char *out,size_t size)
{
    const mercs_control_device *d=mercs_control_device_at(b->device);
    const char *name=mercs_control_input_name(b,half);
    if (half) snprintf(out,size,"/%s",name?name:"----");
    else if (!d || !name) snprintf(out,size,"----");
    else snprintf(out,size,"%s %s",!strcmp(d->name,"keyboard")?"key":d->name,name);
}
static int mcv_second_x(mercs_art *art,const mercs_control_binding *b)
{ char text[128]; mcv_binding_text(b,0,text,sizeof text); return 286+mercs_art_width(art,10,text); }
static void mercs_controls_draw(mercs_art *art,mercs_controls_view *v)
{
    static const char *const pages[]={"Page One","Page Two","Page Three","Page Four"};
    static const char *const mods[]={"--","Ctrl","Alt","--","Shft","--","--","--"};
    static const char *const directions[7][2]={{"-/+","+/-"},{"L/R","R/L"},{"L/R","R/L"},
        {"D/U","U/D"},{"L/R","R/L"},{"D/U","U/D"},{"I/O","O/I"}};
    int i,count=mcv_selected_count(v); char text[160];
    const mercs_control_device *d=mercs_control_device_at(v->device);
    /* DOS0x2fcee passes resource28 to the one-based database loader. */
    mercs_art_background(art,27); mercs_art_rect(art,0,120,640,360,0);
    mercs_art_clip_flags(art,"amwlogo1",120,4,40u,v->logo_restart); v->logo_restart=0;
    if (!v->editor) {
        mercs_art_text(art,10,0,90,"Current Config:",17);
        mercs_art_text(art,10,80,90,v->config.name,17);
        mercs_art_text(art,10,0,112,"SELECT INPUT DEVICES",17);
        for (i=0;i<MC_DEVICES;i++) {
            const mercs_control_device *dev=mercs_control_device_at(i);
            if (dev && v->present[i]) mercs_art_text(art,10,0,134+11*i,dev->title,
                v->selected[i]?220:count>=4?22:17);
        }
        mercs_art_text(art,10,0,400,v->novice?"NOVICE CONTROLS: ON":"NOVICE CONTROLS: OFF",17);
        if (!v->novice) mercs_art_text(art,10,0,422,"CUSTOM CONFIGURATION",17);
        mercs_art_text(art,10,0,444,"ACCEPT",17); mercs_art_text(art,10,0,466,"ABORT",17);
        mercs_art_center(art,10,320,250,"WHITE = Selectable Option",17);
        mercs_art_center(art,10,320,272,"RED = Selected Option",220);
        mercs_art_center(art,10,320,294,"GREY = Unselectable",22);
        return;
    }
    mercs_art_line(art,172,100,172,470,16); mercs_art_line(art,477,100,477,470,16);
    mercs_art_text(art,10,175,90,"GAME CONTROLS",17);
    mercs_art_text(art,10,285,90,pages[v->level],17); mercs_art_text(art,10,375,90,"NEXT PAGE",17);
    for (i=0;i<MC_CONTROLS;i++) {
        const mercs_control_binding *b=&v->config.b[v->level][i]; int y=112+11*i;
        mercs_art_text(art,10,175,y,mercs_control_labels[i],22);
        if (i<7) mercs_art_center(art,10,248,y,directions[i][!!(b->flags&0x80000000u)],17);
        mercs_art_center(art,10,270,y,mods[b->flags&7],17);
        mcv_binding_text(b,0,text,sizeof text);
        mercs_art_text(art,10,285,y,text,v->control==i && !v->half?220:17);
        if (b->type==2) {
            int x=mcv_second_x(art,b); mcv_binding_text(b,1,text,sizeof text);
            mercs_art_text(art,10,x,y,text,v->control==i && v->half?220:17);
        }
    }
    mercs_art_text(art,10,0,90,"INPUT DEVICES",17);
    for (i=0;i<MC_DEVICES;i++) {
        const mercs_control_device *dev=mercs_control_device_at(i);
        if (dev && v->selected[i]) mercs_art_text(art,10,0,112+11*i,dev->title,i==v->device?220:17);
    }
    mercs_art_text(art,10,0,299,"Current Config:",17);
    if (v->slot) snprintf(text,sizeof text,"Custom %d",v->slot); else strcpy(text,"Default");
    mercs_art_text(art,10,90,299,text,17);
    mercs_art_text(art,10,20,310,v->config.name,v->editing_name?220:17);
    for (i=0;i<4;i++) {
        snprintf(text,sizeof text,"Load Custom %d",i+1); mercs_art_text(art,10,0,332+11*i,text,17);
        snprintf(text,sizeof text,"Save Custom %d",i+1); mercs_art_text(art,10,0,387+11*i,text,17);
    }
    mercs_art_text(art,10,0,442,"RESET DEFAULTS",17);
    mercs_art_text(art,10,0,464,"ACCEPT CONFIG AND EXIT",17);
    /* The authored 16-device table puts ABORT at y486, beyond a 480-line
     * screen. Keep its original font/label beside Accept so it is reachable. */
    mercs_art_text(art,10,135,464,"ABORT",17);
    if (!d) return;
    mercs_art_text(art,10,480,90,d->title,17);
    mercs_art_text(art,10,480,112,"---Directional---",17);
    for (i=0;i<d->naxis && i<7;i++) {
        int enabled=v->control>=0 && v->config.b[v->level][v->control].type!=1;
        mercs_art_text(art,10,480,134+11*i,d->axis[i].label,enabled?17:22);
    }
    mercs_art_text(art,10,480,222,"---Buttons---",17);
    mercs_art_text(art,10,600,244,"\1",v->scroll?17:22);
    mercs_art_text(art,10,600,431,"\2",v->scroll+18<d->nbutton?17:22);
    for (i=0;i<18 && i+v->scroll<d->nbutton;i++) {
        const mercs_control_input *in=d->button+i+v->scroll;
        if (in->label) mercs_art_text(art,10,480,244+11*i,in->label,
            v->control>=0 && in->map && in->map[0] && in->map[0]!='*'?17:22);
    }
}
static void mcv_cycle_input(mercs_control_binding *b,int half)
{
    const mercs_control_device *d=mercs_control_device_at(b->device);
    int count,index=half?b->b:b->a;
    if (!d) return;
    count=b->type?d->nbutton:d->naxis;
    do {
        const char *name;
        index++;
        if (index>=count) { b->device=-1; index=-1; break; }
        name=b->type?d->button[index].map:d->axis[index].map;
        if (name && name[0] && name[0]!='*') break;
    } while (index<count);
    if (half) b->b=index; else b->a=index;
}
/* DOS 0x2cbb0: selecting a discrete binding brings its current button to
 * the top of the device list, clamped to the last complete 18-row page. */
static void mcv_reveal_binding(mercs_controls_view *v)
{
    const mercs_control_device *d=mercs_control_device_at(v->device);
    const mercs_control_binding *b;
    int max;
    if (!d || v->control<0) return;
    b=&v->config.b[v->level][v->control];
    if (b->device!=v->device || !b->type) return;
    v->scroll=v->half?b->b:b->a;
    max=d->nbutton>18?d->nbutton-18:0;
    if (v->scroll<0) v->scroll=0;
    if (v->scroll>max) v->scroll=max;
}
static void mcv_select_binding(mercs_controls_view *v,int row,int half,int clear)
{
    mercs_control_binding *b=&v->config.b[v->level][row];
    const mercs_control_device *d=mercs_control_device_at(v->device);
    if (!d) return;
    if (clear) {
        if (b->type==2) b->type=0;
        b->device=-1; b->flags=0; b->pad=0; b->a=b->b=-1;
        v->control=row; v->half=0; return;
    }
    if (v->control!=row || v->half!=half) {
        if (!half || b->type==2) { v->control=row; v->half=half; }
        mcv_reveal_binding(v);
        return;
    }
    if (!half) {
        if (!b->type) {
            if (!d->naxis) return;
            if (b->device!=v->device) { b->device=v->device; b->a=0; }
            else mcv_cycle_input(b,0);
        } else if (b->type==1) {
            if (!d->nbutton) return;
            if (b->device!=v->device) { b->device=v->device; b->a=-1; }
            mcv_cycle_input(b,0);
        } else {
            if (b->device!=v->device) {
                if (!d->nbutton && !d->naxis) return;
                if (!d->nbutton) b->type=0;
                b->device=v->device; b->a=b->b=-1;
            }
            mcv_cycle_input(b,0);
            if (b->a<0) { if (b->b<0) b->type=0; else b->device=v->device; }
        }
    } else {
        if (b->device!=v->device) {
            if (d->nbutton) { b->device=v->device; b->a=-1; b->b=0; }
            else if (d->naxis) { b->device=v->device; b->type=0; b->a=0; b->b=-1; v->half=0; }
        } else {
            mcv_cycle_input(b,1);
            if (b->b<0) { if (b->a<0) { b->type=0; v->half=0; } else b->device=v->device; }
        }
    }
    mcv_reveal_binding(v);
}
static void mcv_assign(mercs_controls_view *v,int axis,int index)
{
    const mercs_control_device *d=mercs_control_device_at(v->device);
    mercs_control_binding *b;
    const char *name;
    if (!d || v->control<0 || index<0 || index>=(axis?d->naxis:d->nbutton)) return;
    name=axis?d->axis[index].map:d->button[index].map;
    if (!name || !name[0] || name[0]=='*') return;
    b=&v->config.b[v->level][v->control];
    if (axis) {
        if (b->type==1) return;
        b->type=0; b->device=v->device; b->a=index; b->b=-1; v->half=0;
    } else if (v->half) {
        if (b->device!=v->device) { b->device=v->device; b->a=-1; }
        b->b=index; if (b->a<0) v->half=0;
    } else {
        if (!b->type) { b->type=2; b->b=-1; }
        if (b->device!=v->device) b->b=-1;
        b->device=v->device; b->a=index;
        if (b->type==2 && b->b<0) v->half=1;
    }
}
static void mercs_controls_scroll(mercs_controls_view *v,int delta)
{
    const mercs_control_device *d=mercs_control_device_at(v->device);
    int max=d && d->nbutton>18?d->nbutton-18:0;
    v->scroll+=delta; if (v->scroll<0) v->scroll=0; if (v->scroll>max) v->scroll=max;
}
static int mercs_controls_click(mercs_art *art,mercs_controls_view *v,int x,int y,int right)
{
    int i,h=mercs_art_height(art,10); char text[40];
    if (!v->editor) {
        for (i=1;i<MC_DEVICES;i++) if (v->present[i] && mcv_inside(x,y,0,134+11*i,100,h)) {
            if (v->selected[i]) v->selected[i]=0;
            else if (mcv_selected_count(v)<4) v->selected[i]=1;
            else return MCV_NONE;
            v->changed=1; return MCV_NONE;
        }
        if (mcv_text_hit(art,x,y,0,400,v->novice?"NOVICE CONTROLS: ON":"NOVICE CONTROLS: OFF")) return MCV_NOVICE;
        if (!v->novice && mcv_text_hit(art,x,y,0,422,"CUSTOM CONFIGURATION")) return MCV_CUSTOM;
        if (mcv_text_hit(art,x,y,0,444,"ACCEPT")) return MCV_ACCEPT;
        if (mcv_text_hit(art,x,y,0,466,"ABORT")) v->active=0;
        return MCV_NONE;
    }
    if (mcv_text_hit(art,x,y,375,90,"NEXT PAGE")) { v->level=(v->level+1)%4; return MCV_NONE; }
    for (i=0;i<MC_CONTROLS;i++) if (y>=112+11*i && y<112+11*i+h) {
        mercs_control_binding *b=&v->config.b[v->level][i];
        if (i<7 && x>=238 && x<258) b->flags^=0x80000000u;
        else if (x>=260 && x<280) mercs_control_cycle_modifier(b);
        else if (b->type==2 && x>=mcv_second_x(art,b) && x<477) mcv_select_binding(v,i,1,right);
        else if (x>=285 && x<385) mcv_select_binding(v,i,0,right);
    }
    for (i=0;i<MC_DEVICES;i++) if (v->selected[i] && mcv_inside(x,y,0,112+11*i,100,h)) {
        v->device=i; v->scroll=0; return MCV_NONE;
    }
    if (mcv_inside(x,y,20,310,150,h)) { v->editing_name=1; memcpy(v->old_name,v->config.name,64); return MCV_NONE; }
    for (i=0;i<4;i++) {
        snprintf(text,sizeof text,"Load Custom %d",i+1); if (mcv_text_hit(art,x,y,0,332+11*i,text)) return MCV_LOAD_1+i;
        snprintf(text,sizeof text,"Save Custom %d",i+1); if (mcv_text_hit(art,x,y,0,387+11*i,text)) return MCV_SAVE_1+i;
    }
    if (mcv_text_hit(art,x,y,0,442,"RESET DEFAULTS")) return MCV_DEFAULTS;
    if (mcv_text_hit(art,x,y,0,464,"ACCEPT CONFIG AND EXIT")) return MCV_ACCEPT;
    if (mcv_text_hit(art,x,y,135,464,"ABORT")) { v->active=0; return MCV_NONE; }
    for (i=0;i<7;i++) if (mcv_inside(x,y,480,134+11*i,100,h)) mcv_assign(v,1,i);
    for (i=0;i<18;i++) if (mcv_inside(x,y,480,244+11*i,100,h)) mcv_assign(v,0,i+v->scroll);
    if (mcv_inside(x,y,600,244,10,h)) mercs_controls_scroll(v,-1);
    if (mcv_inside(x,y,600,431,10,h)) mercs_controls_scroll(v,1);
    return MCV_NONE;
}
#endif
