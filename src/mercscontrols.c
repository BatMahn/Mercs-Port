#include "mercscontrols.h"
#include "datapath.h"
#include "mercsfile.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "../tests/fixtures/controls-completion/original-driver-data.h"

const char *const mercs_control_labels[MC_CONTROLS] = {
    "Throttle ","Chassis ","Turret Turn","Turret Tilt","Eye Point ","Eye Point ","Eye Point ",
    "Recenter Torso","Glance Left","Glance Right","Glance Up","Glance Down","Jump Jets On",
    "Jump Jets Front","Jump Jets Back","Jump Jets Left","Jump Jets Right","Fire Weapon",
    "Cycle Weapon","Cycle Group","Fire Group 1","Fire Group 2","Fire Group 3","Group Toggle",
    "Target Next","Target Prev","Target Reticle","Target Friendly","Nearest Enemy","Inspect Target",
    "Next Nav Point","Novice Forward","Novice Backward"
};
const char *const mercs_control_names[MC_CONTROLS] = {
    "throttle","legs_pan_delta","torso_pan","torso_tilt","pilot_pan","pilot_tilt","zoom_factor",
    "torso_tilt_reset","glance_left","glance_right","glance_up","glance_down","jumpjet_enabled",
    "jumpjet_fire_forward","jumpjet_fire_backward","jumpjet_fire_left","jumpjet_fire_right","weapon_fire",
    "weapon_cycle","weapon_cycle_group","weapon_fire_group_1","weapon_fire_group_2","weapon_fire_group_3",
    "toggle_group_fire","advance_target","previous_target","target_reticle","target_friendly",
    "nearest_enemy","inspect_target","advance_nav","novice_forward","novice_backward"
};

const mercs_control_device *mercs_control_device_at(int i)
{
    return i >= 0 && (size_t)i < sizeof mercs_original_devices / sizeof mercs_original_devices[0]
        ? mercs_original_devices + i : NULL;
}
static uint32_t mc_u32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static void mc_w32(uint8_t *p, uint32_t v)
{ p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static void mc_devices(mercs_control_config *c)
{
    int i;
    for (i=0;i<MC_DEVICES;i++) {
        const mercs_control_device *d=mercs_control_device_at(i);
        c->dev_index[i]=d?i:-1; memset(c->dev_name[i],0,12);
        if (d) snprintf(c->dev_name[i],12,"%s",d->name);
    }
}
void mercs_controls_clear(mercs_control_config *c)
{
    int l,s;
    memset(c,0,sizeof *c); mc_devices(c); strcpy(c->name,"Default Config");
    for (l=0;l<MC_LEVELS;l++) for (s=0;s<MC_SLOTS;s++) {
        mercs_control_binding *b=&c->b[l][s];
        b->type=s<7?0:1; b->device=-1; b->a=b->b=-1;
    }
}
int mercs_controls_decode(mercs_control_config *c, const uint8_t *data, size_t size)
{
    mercs_control_config n;
    int i,l,s;
    if (!c || !data || size!=MC_CPC_BYTES || !memchr(data,0,64)) return -1;
    memcpy(n.name,data,64);
    for (i=0;i<MC_DEVICES;i++) {
        n.dev_index[i]=(int32_t)mc_u32(data+64+16*i);
        memcpy(n.dev_name[i],data+68+16*i,12);
        if (!memchr(n.dev_name[i],0,12)) return -1;
    }
    for (l=0;l<MC_LEVELS;l++) for (s=0;s<MC_SLOTS;s++) {
        const uint8_t *p=data+320+24*(l*MC_SLOTS+s);
        mercs_control_binding *b=&n.b[l][s];
        b->type=(int32_t)mc_u32(p); b->device=(int32_t)mc_u32(p+4);
        b->flags=mc_u32(p+8); b->pad=(int32_t)mc_u32(p+12);
        b->a=(int32_t)mc_u32(p+16); b->b=(int32_t)mc_u32(p+20);
        if (b->device>=MC_DEVICES || (b->device>=0 && (b->type<0 || b->type>2))) return -1;
    }
    *c=n; return 0;
}
void mercs_controls_encode(const mercs_control_config *c, uint8_t data[MC_CPC_BYTES])
{
    int i,l,s;
    memcpy(data,c->name,64);
    for (i=0;i<MC_DEVICES;i++) {
        mc_w32(data+64+16*i,(uint32_t)c->dev_index[i]); memcpy(data+68+16*i,c->dev_name[i],12);
    }
    for (l=0;l<MC_LEVELS;l++) for (s=0;s<MC_SLOTS;s++) {
        uint8_t *p=data+320+24*(l*MC_SLOTS+s);
        const mercs_control_binding *b=&c->b[l][s];
        mc_w32(p,(uint32_t)b->type); mc_w32(p+4,(uint32_t)b->device); mc_w32(p+8,b->flags);
        mc_w32(p+12,(uint32_t)b->pad); mc_w32(p+16,(uint32_t)b->a); mc_w32(p+20,(uint32_t)b->b);
    }
}
int mercs_controls_remap(mercs_control_config *c)
{
    int map[MC_DEVICES],i,j,l,s;
    for (i=0;i<MC_DEVICES;i++) {
        map[i]=-1;
        for (j=0;j<MC_DEVICES;j++) {
            const mercs_control_device *d=mercs_control_device_at(j);
            if (d && !strcasecmp(c->dev_name[i],d->name)) { map[i]=j; break; }
        }
    }
    for (l=0;l<MC_LEVELS;l++) for (s=0;s<MC_SLOTS;s++)
        if (c->b[l][s].device>=MC_DEVICES) return -1;
    for (l=0;l<MC_LEVELS;l++) for (s=0;s<MC_SLOTS;s++)
        if (c->b[l][s].device>=0) c->b[l][s].device=map[c->b[l][s].device];
    mc_devices(c); return 0;
}
void mercs_controls_selected(const mercs_control_config *c, int selected[MC_DEVICES])
{
    int i,l,s,count=1;
    memset(selected,0,MC_DEVICES*sizeof *selected); selected[0]=1;
    for (l=0;l<MC_LEVELS;l++) for (s=0;s<MC_SLOTS;s++)
        if (c->b[l][s].device>=0 && c->b[l][s].device<MC_DEVICES) selected[c->b[l][s].device]=1;
    for (i=1;i<MC_DEVICES;i++) if (selected[i] && ++count>4) selected[i]=0;
}
int mercs_controls_load(mercs_control_config *c, const char *name)
{
    unsigned char *data=NULL; size_t n=0; char path[128]; int rc;
    if (!name || strchr(name,'/') || strchr(name,'\\')) return -1;
    snprintf(path,sizeof path,"GIDDI/%s",name);
    if (dp_read_file(path,&data,&n)) return -1;
    rc=mercs_controls_decode(c,data,n); free(data);
    return rc?rc:mercs_controls_remap(c);
}
int mercs_controls_defaults(mercs_control_config *c, const int selected[MC_DEVICES])
{
    mercs_control_config n,d;
    int device,l,s,k,loaded=0;
    char name[40];
    mercs_controls_clear(&n);
    for (device=0;device<MC_DEVICES;device++) {
        const mercs_control_device *dev=mercs_control_device_at(device);
        if (!dev || !selected[device]) continue;
        snprintf(name,sizeof name,"%s.CPC",dev->name);
        if (mercs_controls_load(&d,name)) continue;
        loaded++;
        for (l=0;l<MC_LEVELS;l++) for (s=0;s<MC_SLOTS;s++) {
            const mercs_control_binding *b=&d.b[l][s];
            if (b->device!=device) continue;
            /* Mercs0x2ee64 searches all pages from page zero, unlike the
             * inherited Mech2 merge. An exact duplicate ends that search. */
            for (k=0;k<MC_LEVELS;k++) {
                mercs_control_binding *target=&n.b[k][s];
                if (!memcmp(target,b,sizeof *b)) break;
                if (target->device<0) { *target=*b; break; }
            }
        }
    }
    if (!loaded) return -1;
    *c=n; return 0;
}
const char *mercs_control_input_name(const mercs_control_binding *b, int half)
{
    const mercs_control_device *d=mercs_control_device_at(b->device);
    int index=half?b->b:b->a;
    if (!d || index<0 || b->type<0 || b->type>2) return NULL;
    if (!b->type) return index<d->naxis?d->axis[index].map:NULL;
    return index<d->nbutton?d->button[index].map:NULL;
}
void mercs_control_cycle_modifier(mercs_control_binding *b)
{ b->flags=(b->flags&0x80000000u) | ((b->flags&7)?((b->flags<<1)&7):1); }

typedef struct { char *p; size_t n,cap; int failed; mercs_control_counts counts; } mc_writer;
static void mc_put(mc_writer *w, const char *format, ...)
{
    va_list ap; int n;
    if (w->failed) return;
    va_start(ap,format); n=vsnprintf(w->p+w->n,w->cap-w->n,format,ap); va_end(ap);
    if (n<0 || (size_t)n>=w->cap-w->n) { w->failed=1; return; }
    w->n+=(size_t)n;
}
static void mc_emit_one(mc_writer *w,const char *name,const mercs_control_binding *b,int axis)
{
    static const char *const mod[3]={"Control","Alt","Shift"};
    const mercs_control_device *d=mercs_control_device_at(b->device);
    const char *input=mercs_control_input_name(b,0); int m;
    if (!d || !input) return;
    mc_put(w,"%s {\r\n\t%c %s\t%s\r\n",name,axis && (b->flags&0x80000000u)?'-':'+',d->name,input);
    for (m=0;m<3;m++) {
        if (b->flags&(1u<<m)) mc_put(w,"\t+ keyboard\t%s\r\n",mod[m]);
        else if (b->flags&(0x10u<<m)) mc_put(w,"\t- keyboard\t%s\r\n",mod[m]);
    }
    mc_put(w,"}\r\n");
    if (axis) w->counts.analog++; else w->counts.discrete++;
    if (!axis && !strncmp(name,"jumpjet_fire_",13)) mc_emit_one(w,"jumpjet_enabled",b,0);
}
static void mc_emit(mc_writer *w,const char *name,const mercs_control_binding *b)
{
    mercs_control_binding copy=*b;
    if (b->a<0 || !mercs_control_device_at(b->device)) return;
    if (b->type==0) mc_emit_one(w,name,b,1);
    else if (b->type==1) mc_emit_one(w,name,b,0);
    else if (b->type==2) {
        char base[64],name_a[80],name_b[80]; char *delta;
        snprintf(base,sizeof base,"%s",name); delta=strstr(base,"_delta"); if (delta) *delta=0;
        snprintf(name_a,sizeof name_a,"%s_%s",base,(b->flags&0x80000000u)?"plus":"minus");
        snprintf(name_b,sizeof name_b,"%s_%s",base,(b->flags&0x80000000u)?"minus":"plus");
        mc_emit_one(w,name_a,b,0); copy.a=b->b; copy.flags=(uint32_t)b->pad;
        mc_emit_one(w,name_b,&copy,0);
    }
    if (!strcmp(name,"pilot_tilt")) mc_emit(w,"track_height_delta",b);
    else if (!strcmp(name,"pilot_pan")) mc_emit(w,"eyepoint_pan_delta",b);
    else if (!strcmp(name,"zoom_factor")) { copy=*b; copy.flags^=0x80000000u; mc_emit(w,"track_distance_delta",&copy); }
    else if (!strcmp(name,"torso_tilt_reset")) {
        mc_emit(w,"pilot_tilt_reset",b); mc_emit(w,"torso_pan_reset",b); mc_emit(w,"pilot_pan_reset",b);
    } else if (!strcmp(name,"glance_up")) mc_emit(w,"track_height_minus",b);
    else if (!strcmp(name,"glance_down")) mc_emit(w,"track_height_plus",b);
    else if (!strcmp(name,"glance_left")) mc_emit(w,"eyepoint_pan_minus",b);
    else if (!strcmp(name,"glance_right")) mc_emit(w,"eyepoint_pan_plus",b);
}
size_t mercs_controls_generate(const mercs_control_config *c, char *out,size_t size,mercs_control_counts *counts)
{
    mercs_control_config copy=*c;
    mc_writer w={out,0,size,0,{0,0}};
    mercs_control_binding *all=&copy.b[0][0]; int i,j;
    if (!out || !size) return 0;
    for (i=0;i<MC_LEVELS*MC_SLOTS;i++) {
        mercs_control_binding *b=all+i;
        b->pad=b->type==2?(b->a==b->b?(int32_t)((b->flags<<4)&0x70):(int32_t)b->flags):0;
    }
    for (i=0;i<MC_LEVELS*MC_SLOTS;i++) {
        mercs_control_binding *b=all+i;
        uint32_t neg=(b->flags<<4)&0x70;
        if (b->device<0 || !neg) continue;
        for (j=0;j<MC_LEVELS*MC_SLOTS;j++) {
            mercs_control_binding *other=all+j;
            if (i==j || other->device<0) continue;
            if (b->type==0) { if (!other->type && other->a==b->a) other->flags|=neg; }
            else if (b->type<=2 && other->type) {
                if (other->a==b->a) other->flags|=neg;
                if (other->type==2) {
                    if (other->b==b->a) other->pad|=(int32_t)neg;
                    if (other->a==b->b) other->flags|=((uint32_t)b->pad<<4)&0x70;
                    if (other->b==b->b) other->pad|=(int32_t)(((uint32_t)b->pad<<4)&0x70);
                }
            }
        }
    }
    mc_put(&w,"# mw2shell CockPit Config generated map file\r\n");
    for (i=0;i<MC_LEVELS*MC_SLOTS;i++)
        if (i%MC_SLOTS<MC_CONTROLS && all[i].device>=0)
            mc_emit(&w,mercs_control_names[i%MC_SLOTS],all+i);
    {
        mercs_control_binding b={1,0,0,-1,102,-1};
        mc_emit(&w,"legs_pan_minus",&b); mc_emit(&w,"jumpjet_enabled",&b);
        b.a=100; mc_emit(&w,"legs_pan_plus",&b); mc_emit(&w,"jumpjet_enabled",&b);
    }
    mc_put(&w,"# analog count = %d\r\n# discrete count = %d\r\n",w.counts.analog,w.counts.discrete);
    if (counts) *counts=w.counts;
    return w.failed?0:w.n;
}

static int mc_directory(const char *overlay,char *out,size_t size,char *error,size_t esize)
{
    return mercs_file_directory(overlay,"GIDDI",out,size,error,esize);
}
int mercs_controls_save_slot(const mercs_control_config *c,const char *overlay,int slot,char *error,size_t size)
{
    uint8_t data[MC_CPC_BYTES]; char dir[2048],name[24];
    if (slot<0 || slot>4) { errno=EINVAL; return mercs_file_failure(error,size,"control slot"); }
    if (mc_directory(overlay,dir,sizeof dir,error,size)) return -1;
    mercs_controls_encode(c,data); snprintf(name,sizeof name,"CONFIG%02d.CPC",slot);
    return mercs_atomic_write(dir,name,data,sizeof data,error,size);
}
int mercs_controls_apply(const mercs_control_config *c,const char *overlay,char *error,size_t size)
{
    char map[65536]; mercs_control_counts counts; size_t n=mercs_controls_generate(c,map,sizeof map,&counts);
    if (!n || counts.analog>40 || counts.discrete>90) {
        snprintf(error,size,"Too many control bindings. The original limit is 40 directional and 90 discrete bindings."); return -1;
    }
    /* An interrupted write cannot truncate either existing record. Writing
     * CONFIG00 first leaves the active map intact if the final commit fails;
     * the editor retains the staged configuration for retry. */
    if (mercs_controls_save_slot(c,overlay,0,error,size)) return -1;
    return mercs_atomic_write(overlay,"INPUT.MAP",(const uint8_t *)map,n,error,size);
}
