/* Original OUT3 DOS screenshot: seven available rows per column, six used.
 * Exercise the same layout/selection functions linked into the native viewer. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mercshud.h"
#include "mercs_compass_original_fixtures.h"

int main(int argc, char **argv)
{
    char error[256];
    prj_archive *a;
    combat_unit u;
    /* Raw criticals: RT=1901, CT=2201, LT=701/702, RA=2301..2304,
     * LA=2305..2308. Screenshot rows: SSRM6/UAC20, SSRM6/ERML,
     * three ERML/ERML rows, then ERML/ERLL. */
    const int sequence[12] = {2,0,3,4,8,5,9,6,10,7,11,1};
    const int windows[12] = {10,15,3,4,11,12,13,14,5,6,7,8};
    unsigned seen=0;
    int i,w;
    assert(argc==2 || (argc==3 && strcmp(argv[2],"--resources-only")==0));
    {
        size_t k;
        /* Captured calls from the supplied DOS and Windows compass routines,
         * including wrapping, both sprite widths and signed half-pixel ties. */
        for (k=0;k<sizeof mercs_compass_tape_cases/sizeof mercs_compass_tape_cases[0];k++) {
            assert(mercs_hud_compass_tape_x(mercs_compass_tape_cases[k].center,
                       mercs_compass_tape_cases[k].width, mercs_compass_tape_cases[k].heading)
                   == mercs_compass_tape_cases[k].x);
        }
        for (k=0;k<sizeof mercs_compass_marker_cases/sizeof mercs_compass_marker_cases[0];k++) {
            assert(mercs_hud_compass_marker_x(mercs_compass_marker_cases[k].center,
                       mercs_compass_marker_cases[k].width, mercs_compass_marker_cases[k].relative)
                   == mercs_compass_marker_cases[k].x);
        }
    }
    a=prj_open(argv[1],error,sizeof error);
    if (!a) { fprintf(stderr,"%s\n",error); return 1; }
    if (argc==2) {
    assert(combat_init(&u,a,"KDK00STD")==0 && u.weapon_count==12);
    setvbuf(stdout,NULL,_IONBF,0);
    for (i=0;i<12;i++) {
        w=mercs_weapon_window(&u,i);
        printf("mount %d type%d loc%d window%d\n",i,u.weapons[i].weapon,u.weapons[i].location,w);
        assert(w==windows[i]);
        assert(!(seen & (1u<<w)));
        seen|=1u<<w;
    }
    w=mercs_weapon_next(&u,-1,0,0);
    for (i=0;i<12;i++) {
        assert(w==sequence[i]);
        w=mercs_weapon_next(&u,w,0,0);
    }
    assert(w==sequence[0]);
    u.weapons[2].state=-1;
    assert(mercs_weapon_next(&u,-1,0,0)==0);
    }
    {
        /* The supplied Mercs archives have no HUD/KODIAK or HUD/PANTHER;
         * their common HUD record selects these three original shapes. */
        static const char *const shape[] = {"WIREDM", "WIREDM6", "WIREDMK"};
        mercs_hud_damage d;
        prj_record r;
        int set;
        assert(prj_find_id(a,prj_find_type(a,"HUD"),"KODIAK") < 0);
        for (set=0;set<3;set++) {
            assert(mercs_hud_damage_load(a,set,&d)==0);
            assert(strcmp(d.shape,shape[set])==0);
            assert(prj_read_named(a,"SHP",d.shape,&r)==PRJ_OK && r.size>16);
            prj_record_free(&r);
            /* Original MW2HUD.HDI head and left-leg rectangles. */
            assert(d.regions[0][0]==16 && d.regions[0][1]==6 && d.regions[0][2]==2 && d.regions[0][3]==7);
            assert(d.regions[14][0]==0 && d.regions[14][1]==23 && d.regions[14][2]==16 && d.regions[14][3]==14);
        }
        assert(mercs_hud_damage_load(a,3,&d)==-1 && !d.shape[0]);
        assert(mercs_hud_damage_load(NULL,0,&d)==-1 && !d.shape[0]);
    }
    {
        /* Literal corners/reference recovered from all three MERCSW DLL
         * tables; these sit above the viewers, not Mech2's bottom bar. */
        const mercs_hud_window *speed=mercs_hud_instrument(MERCS_HUD_SPEED);
        const mercs_hud_window *alt=mercs_hud_instrument(MERCS_HUD_ALTITUDE);
        const mercs_hud_window *heat=mercs_hud_instrument(MERCS_HUD_HEAT);
        const mercs_hud_window *rate=mercs_hud_instrument(MERCS_HUD_DHDT);
        assert(speed->x0==258 && speed->y0==95 && speed->x1==309 && speed->y1==129 && speed->refy==0x4000);
        assert(alt->x0==10 && alt->y0==95 && alt->x1==61 && alt->y1==129 && alt->refx==0x3333);
        assert(heat->x0==132 && heat->y0==145 && heat->x1==183 && heat->y1==179);
        assert(rate->x0==193 && rate->y0==145 && rate->x1==244 && rate->y1==179);
        assert(!mercs_hud_instrument(-1) && !mercs_hud_instrument(MERCS_HUD_INSTRUMENT_COUNT));
    }
    prj_close(a);
    puts(argc==2 ? "PASS: original Mercs compass placement, Kodiak weapon windows/selection and common HUD resources/layout." :
                   "PASS: original Mercs compass placement, damage resources and instrument layout (edition-specific loadout preserved).");
    return 0;
}
