#ifndef MERCS_REPAIR_VIEW_H
#define MERCS_REPAIR_VIEW_H

/* Original retail repair screen, DOS0x65b2c / Windows0x1006c210.
 * This helper composes original assets and translates their original input
 * rectangles. The caller owns repair actions, confirmation and persistence.
 * See MERCS_REPAIR_AUDIT.md and career-return/original-repair-layout.json.
 *
 * init() selects the record's authored location; set year to the career year.
 * refresh() follows original redraw0x63be0 and must run after All, Selected or
 * Clear. Its quote side effects belong to the staged transaction. draw() never
 * reruns a quote or changes financial/inventory state on animation frames.
 */
#include "mercsart.h"
#include "mercsrepair.h"
#include "mercsdebrief.h"
#include "mek.h"
#include <stdio.h>
#include <string.h>

enum mercs_repair_view_action {
    MERCS_REPAIR_VIEW_NONE = 0,
    MERCS_REPAIR_VIEW_ACCEPT,
    MERCS_REPAIR_VIEW_ALL,
    MERCS_REPAIR_VIEW_CLEAR,
    MERCS_REPAIR_VIEW_SELECTED,
    MERCS_REPAIR_VIEW_PREV,
    MERCS_REPAIR_VIEW_NEXT,
    MERCS_REPAIR_VIEW_LOCATION,
    MERCS_REPAIR_VIEW_ROW
};
typedef struct mercs_repair_view {
    int location, part, slot, bay_style;
    int32_t year, owed;
    unsigned selected_slots;
} mercs_repair_view;

static inline uint32_t mercs_repair_view_u32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static inline int32_t mercs_repair_view_signed(uint32_t v)
{ return v <= INT32_MAX ? (int32_t)v : (int32_t)(-1-(int64_t)(UINT32_MAX-v)); }
static inline int32_t mercs_repair_view_i32(const uint8_t *p)
{ return mercs_repair_view_signed(mercs_repair_view_u32(p)); }

/* Original DOS0x5c288. The location flag byte holds the original quantized
 * front/rear baseline. Do not replace this with a current/reference ratio.
 * All arithmetic before signed division retains the original DWORD wrap. */
static inline int mercs_repair_view_condition(const mercs_career_mech *m, int location)
{
    const uint8_t *p = m->bytes+0x760+18*location;
    int32_t internal = mercs_repair_view_i32(p);
    int32_t front = mercs_repair_view_i32(p+8), rear = mercs_repair_view_i32(p+12);
    int damage[2] = {0,0}, i;
    int baselines[2] = {p[16]&15,p[16]>>4};
    int32_t armor[2] = {front,rear};
    if (internal <= 0 && front <= 0 && rear <= 0) return 3;
    if (internal < 0) internal = 0;
    for (i = 0; i < 2; i++) if (baselines[i]) {
        uint32_t total;
        if (armor[i] < 0) armor[i] = 0;
        total = (((uint32_t)internal<<16)+((uint32_t)armor[i]<<16))*3u;
        damage[i] = 15-mercs_repair_view_signed(total)/(baselines[i]<<16);
        if (damage[i] < 0) damage[i] = 0;
        if (damage[i] > 15) damage[i] = 15;
    }
    i = damage[0] > damage[1] ? damage[0] : damage[1];
    return i > 11 ? 2 : i > 0 ? 1 : 0;
}

static inline void mercs_repair_view_refresh(mercs_repair_view *v, mercs_repair_session *s)
{
    const mercs_career_mech *m = mercs_repair_current(s);
    int location;
    int32_t owed;
    if (!v || !m) return;
    location = mercs_repair_view_i32(m->bytes+0x364);
    if (location < 0 || location >= 8) location = 0;
    if (location != v->location) { v->part = v->slot = -1; v->selected_slots = 0; }
    v->location = location;
    if (!mercs_repair_owed(s,&owed)) v->owed = owed;
}

static inline void mercs_repair_view_init(mercs_repair_view *v, mercs_repair_session *s, int bay_style)
{
    const mercs_career_mech *m = mercs_repair_current(s);
    int location = m ? mercs_repair_view_i32(m->bytes+0x364) : 0;
    memset(v,0,sizeof *v);
    v->part = v->slot = -1; v->bay_style = !!bay_style;
    if (location < 0 || location >= 8) location = 0;
    v->location = location;
    (void)mercs_repair_select_location(s,location);
    mercs_repair_view_refresh(v,s);
}

static inline void mercs_repair_view_number(char out[24], int32_t value)
{
    char raw[16];
    size_t i, j = 0, n;
    uint32_t magnitude = value < 0 ? 0u-(uint32_t)value : (uint32_t)value;
    snprintf(raw,sizeof raw,"%lu",(unsigned long)magnitude); n = strlen(raw);
    if (value < 0) out[j++] = '-';
    for (i = 0; i < n; i++) {
        if (i && (n-i)%3 == 0) out[j++] = ',';
        out[j++] = raw[i];
    }
    out[j] = 0;
}

/* DOS row+4 encodes0x80000011 as previousY+height+1, not seventeen
 * pixels;0x80000022 is previousY+2*height+2. Fonts are DATABASE5/6/10.
 * A missing critical returns no text object and inherits the prior height. */
static inline void mercs_repair_view_rows(mercs_art *a, const mercs_career_mech *reference,
                                         int location, int y[25], int height[25])
{
    int body = mercs_art_height(a,10), title = mercs_art_height(a,6), i;
    y[0] = 84; height[0] = mercs_art_height(a,5);
    y[1] = 74; height[1] = body;
    for (i = 2; i < 25; i++) {
        int missing = i >= 13 && mercs_repair_view_i32(reference->bytes+0x368+48*location+4*(i-13)) < 0;
        y[i] = y[i-1]+(i == 13 ? 2 : 1)*height[i-1]+(i == 13 ? 2 : 1);
        height[i] = i == 12 ? title : missing ? height[i-1] : body;
    }
}

static inline void mercs_repair_view_critical_name(int32_t id, char out[96])
{
    static const struct { int id; const char *name; } equipment[] = {
        {7000,"MASC"},{7050,"Targeting Computer"},{7100,"ECM"},{7150,"Artemis IV"},
        {7200,"Beagle Active Probe"},{7250,"TAG"},{7300,"Shoulder"},{7350,"Upper Arm Actuator"},
        {7400,"Lower Arm Actuator"},{7450,"Hand Actuator"},{7500,"Hip"},{7550,"Upper Leg Actuator"},
        {7600,"Lower Leg Actuator"},{7650,"Foot Actuator"},{7700,"Sensors"},{7750,"Cockpit"},
        {7800,"Gyro"},{7850,"Engine"},{7900,"Life Support"},{8000,"Heat Sink"},
        {8500,"Jump Jet"},{9000,"Endo Steel"},{9500,"Ferro-Fibrous"}
    };
    int i;
    if (id <= 0) { snprintf(out,96,"-"); return; }
    if (id < 7000 && id/100 < mek_weapon_count) {
        snprintf(out,96,"%s #%ld",mek_weapons[id/100].name,(long)(id%100)); return;
    }
    if (id < 10000) {
        for (i = 22; i >= 0; i--) if (equipment[i].id/50 == id/50) {
            snprintf(out,96,"%s",equipment[i].name); return;
        }
    } else if ((id-10000)/10000 < mek_weapon_count) {
        snprintf(out,96,"Ammo (%s)",mek_weapons[(id-10000)/10000].name); return;
    }
    snprintf(out,96,"BAD CRITICAL %ld",(long)id);
}

static inline void mercs_repair_view_draw(mercs_art *a, mercs_repair_view *v, mercs_repair_session *s)
{
    static const char *const locations[8] = {"Head","Right Torso","Center Torso","Left Torso",
        "Right Arm","Left Arm","Right Leg","Left Leg"};
    static const char *const body[8] = {"head","rtor","ctor","ltor","rarm","larm","rleg","lleg"};
    static const int bx[8] = {105,75,88,120,26,154,11,129}, by[8] = {122,108,154,107,126,126,173,173};
    static const char *const buttons[6] = {"accept","repaira","cleara","repairi","nextr","prevr"};
    static const int x[6] = {509,54,54,292,546,452}, yb[6] = {440,389,424,384,330,326};
    const mercs_career_mech *m = mercs_repair_current(s), *reference = mercs_repair_reference(s);
    char text[320], number[24], name[257], clip[20];
    const char *display;
    const uint8_t *p, *r;
    int y[25], height[25], i, state, ink;
    int32_t value = 0;
    size_t n;
    if (!a || !v || !m || !reference) return;
    mercs_art_background(a,v->bay_style ? 38 : 12);
    for (i = 0; i < 6; i++) {
        snprintf(clip,sizeof clip,"%c%s",v->bay_style ? 'b' : 'd',buttons[i]);
        mercs_art_button(a,clip,x[i],yb[i],0);
    }
    for (i = 0; i < 8; i++) {
        state = mercs_repair_view_condition(m,i);
        if (state == 3 && i != v->location) continue;
        mercs_art_named_shape(a,body[i],state+(i == v->location ? 3 : 0),bx[i],by[i]);
    }
    /* DOS0x649ac: the original rotating chassis movie is a<prefix>. */
    memcpy(name,m->bytes+0x81c,7); name[7] = 0;
    snprintf(clip,sizeof clip,"a%.7s",name);
    mercs_art_clip_flags(a,clip,454,142,0x68u,0);

    mercs_repair_view_rows(a,reference,v->location,y,height);
    memcpy(name,m->bytes+(m->bytes[0x100] ? 0x100 : 0),256); name[256] = 0;
    display = name;
    /* Original fit-name0x5bea4 discards only the first word when too wide. */
    if (mercs_art_width(a,5,name) > 166) {
        n = 0; while (name[n] && name[n] != ' ') n++;
        while (name[n] == ' ') n++;
        if (name[n]) display = name+n;
    }
    mercs_art_center(a,5,535,y[0],display,17);
    mercs_art_text(a,10,253,y[1],locations[v->location],17);
    snprintf(text,sizeof text,"Tech Year: %ld",(long)mercs_repair_view_i32(m->bytes+0x843));
    mercs_art_text(a,10,253,y[3],text,22);
    if (!mercs_debrief_mech_value(m,v->year,&value)) {
        mercs_repair_view_number(number,value);
        snprintf(text,sizeof text,"Value: %s",number); mercs_art_text(a,10,253,y[4],text,22);
    }
    p = m->bytes+0x760+18*v->location; r = reference->bytes+0x760+18*v->location;
    state = mercs_repair_view_condition(m,v->location);
    ink = state == 0 ? 22 : state == 1 ? 34 : 58;
    if (mercs_repair_view_i32(r+12) < 0)
        snprintf(text,sizeof text,"ARMOR: front: %ld/%ld  rear: None",
                 (long)mercs_repair_view_i32(p+8),(long)mercs_repair_view_i32(r+8));
    else snprintf(text,sizeof text,"ARMOR: front: %ld/%ld  rear: %ld/%ld",
                 (long)mercs_repair_view_i32(p+8),(long)mercs_repair_view_i32(r+8),
                 (long)mercs_repair_view_i32(p+12),(long)mercs_repair_view_i32(r+12));
    mercs_art_text(a,10,253,y[7],text,v->part == MERCS_REPAIR_ARMOR ? 17 : ink);
    snprintf(text,sizeof text,"INTERNALS: %ld/%ld",(long)mercs_repair_view_i32(p),(long)mercs_repair_view_i32(r));
    mercs_art_text(a,10,253,y[8],text,v->part == MERCS_REPAIR_INTERNAL ? 17 : ink);
    mercs_art_text(a,6,270,y[12],"CRITICAL STATUS",22);
    for (i = 0; i < 12; i++) {
        size_t offset = 0x368u+48u*(size_t)v->location+4u*(size_t)i;
        int32_t id = mercs_repair_view_i32(reference->bytes+offset), cost;
        char critical_name[96];
        if (id < 0) continue;
        ink = 22;
        if (id > 0 && mercs_repair_view_i32(m->bytes+offset) <= 0)
            ink = mercs_repair_has_replacement(s,id,&cost) == 1 ? 34 : 58;
        if (id > 0 && v->part == MERCS_REPAIR_CRITICAL && (v->selected_slots & (1u<<(unsigned)i))) ink = 17;
        mercs_repair_view_critical_name(id,critical_name);
        mercs_art_text(a,10,253,y[13+i],critical_name,ink);
    }
    mercs_art_text(a,6,490,10,"Funds:",17);
    mercs_art_text(a,6,490,23,"Price:",17);
    mercs_art_text(a,6,490,36,"Bal:",17);
    for (i = 0; i < 3; i++) {
        int32_t funds = mercs_repair_funds(s);
        int32_t amount = i == 0 ? funds : i == 1 ? v->owed :
                         mercs_repair_view_signed((uint32_t)funds-(uint32_t)v->owed);
        mercs_repair_view_number(number,amount);
        mercs_art_text(a,6,620-mercs_art_width(a,6,number),10+13*i,number,17);
    }
}

static inline int mercs_repair_view_click(mercs_art *a, mercs_repair_view *v,
                                         mercs_repair_session *s, int x, int y)
{
    /* DOS0xc1458 and Windows0x100aaa38 agree on all112 DWORDs. */
    static const int bounds[16][4] = {
        {500,427,602,475},{400,427,499,475},{537,321,631,365},{443,321,536,365},
        {47,389,190,423},{47,424,190,461},{283,381,383,414},{283,415,383,446},
        {104,122,135,157},{164,121,218,228},{18,121,67,228},{99,156,140,229},
        {135,101,163,156},{74,101,104,156},{143,228,231,381},{7,228,107,381}
    };
    static const int actions[8] = {MERCS_REPAIR_VIEW_ACCEPT,MERCS_REPAIR_VIEW_NONE,
        MERCS_REPAIR_VIEW_NEXT,MERCS_REPAIR_VIEW_PREV,MERCS_REPAIR_VIEW_ALL,
        MERCS_REPAIR_VIEW_CLEAR,MERCS_REPAIR_VIEW_SELECTED,MERCS_REPAIR_VIEW_NONE};
    static const int body_location[8] = {0,5,4,2,3,1,7,6};
    const mercs_career_mech *reference = mercs_repair_reference(s);
    int rows[25], heights[25], i, location = -1;
    if (!a || !v || !reference) return MERCS_REPAIR_VIEW_NONE;
    mercs_repair_view_rows(a,reference,v->location,rows,heights);
    if (x >= 253 && x < 353) {
        if (y >= rows[1] && y < rows[1]+heights[1]) location = (v->location+1)%8;
        for (i = 7; i <= 8; i++) if (y >= rows[i] && y < rows[i]+heights[i]) {
            v->part = i-7; v->slot = -1; v->selected_slots = 0; return MERCS_REPAIR_VIEW_ROW;
        }
        for (i = 0; i < 12; i++) if (y >= rows[13+i] && y < rows[13+i]+heights[13+i]) {
            int32_t id = mercs_repair_view_i32(reference->bytes+0x368+48*v->location+4*i);
            int k;
            if (id <= 0) return MERCS_REPAIR_VIEW_NONE;
            v->part = MERCS_REPAIR_CRITICAL; v->slot = i; v->selected_slots = 1u<<(unsigned)i;
            if (id != 7800 && id != 7850) {
                v->selected_slots = 0; v->slot = -1;
                for (k = 0; k < 12; k++)
                    if (mercs_repair_view_i32(reference->bytes+0x368+48*v->location+4*k) == id) {
                        v->selected_slots |= 1u<<(unsigned)k;
                        if (v->slot < 0) v->slot = k;
                    }
            }
            return MERCS_REPAIR_VIEW_ROW;
        }
    }
    if (location < 0) for (i = 0; i < 16; i++)
        if (x >= bounds[i][0] && x <= bounds[i][2] && y >= bounds[i][1] && y <= bounds[i][3]) {
            if (i < 8) return actions[i];
            location = body_location[i-8]; break;
        }
    if (location >= 0 && location != v->location && !mercs_repair_select_location(s,location)) {
        v->location = location; v->part = v->slot = -1; v->selected_slots = 0;
        return MERCS_REPAIR_VIEW_LOCATION;
    }
    return MERCS_REPAIR_VIEW_NONE;
}

#endif
