#ifndef MERCS_HUD_LAYOUT_H
#define MERCS_HUD_LAYOUT_H
#include "combat.h"

/* Mercs CPIT/MW2MECH has nineteen rectangles: seven weapon rows per column,
 * followed by the target picture and its text. MERCSW.DLL 0x1005c34a..0x1005c94d
 * assigns all fourteen mounts by location and reorders them left/right by row. */
enum {
    HUD_WEAPON_FIRST = 3, HUD_WEAPON_ROWS = 7, HUD_WEAPON_RIGHT = 10,
    HUD_WEAPON_LAST = 16, HUD_WEAPON_COUNT = 14,
    HUD_TARGET_VIEW = 17, HUD_TARGET_TEXT = 18
};

int mercs_weapon_window(const combat_unit *pu, int wi);
int mercs_weapon_next(const combat_unit *pu, int wi, int include_self, int ready_only);

typedef struct {
    char shape[PRJ_NAME_LEN + 1];
    int regions[15][4];
} mercs_hud_damage;

/* Mercs uses one common HUD/MW2HUD record, not HUD/<mech skeleton>.
 * set: 0 = 320x200, 1 = 640x480, 2 = 1024x768 resource set. */
int mercs_hud_damage_load(prj_archive *a, int set, mercs_hud_damage *out);

/* Extra Mercs instrument windows come from MERCSW.DLL 0x100b2cf0;
 * only the first nineteen windows are supplied by CPIT/MW2MECH. */
enum { MERCS_HUD_SPEED, MERCS_HUD_ALTITUDE, MERCS_HUD_HEAT,
       MERCS_HUD_DHDT, MERCS_HUD_INSTRUMENT_COUNT };
typedef struct { int x0, y0, x1, y1, refx, refy; } mercs_hud_window;
const mercs_hud_window *mercs_hud_instrument(int instrument);

/* Mercs compass sprite hotspots, in the selected sprite set's pixels.
 * DOS 0x345b0 / 0x343c4, independently checked against MERCSW 0x10034a3b /
 * 0x1003481a. Both use whole degrees and rounded 16.16 pixels per degree.
 * The tape moves left as heading increases; target markers move right for
 * a positive relative bearing. The caller floors its floating angle first. */
int mercs_hud_compass_tape_x(int center, int sprite_width, int heading_degrees);
int mercs_hud_compass_marker_x(int center, int sprite_width, int relative_degrees);

/* Original mercs.hud order: main, A, F, E, B, C, D. DOS stores the final
 * radar byte in 37 bytes; Windows stores that field as a 32-bit integer. */
enum { MERCS_MFD_MAIN, MERCS_MFD_A, MERCS_MFD_F, MERCS_MFD_E,
       MERCS_MFD_B, MERCS_MFD_C, MERCS_MFD_D, MERCS_MFD_COUNT };
enum { MERCS_MFD_OFF, MERCS_MFD_DAMAGE, MERCS_MFD_HTAL, MERCS_MFD_REAR,
       MERCS_MFD_FORWARD, MERCS_MFD_DOWN, MERCS_MFD_SATELLITE,
       MERCS_MFD_WEAPON, MERCS_MFD_TARGET, MERCS_MFD_SPEED,
       MERCS_MFD_ALTITUDE, MERCS_MFD_HEAT_BAR, MERCS_MFD_HEAT_DIAL,
       MERCS_MFD_HEAT_RATE, MERCS_MFD_MODES };
typedef struct {
    int32_t mode[MERCS_MFD_COUNT];
    int32_t weapons, compass, radar;
} mercs_hud_settings;
void mercs_hud_defaults(mercs_hud_settings *settings);
int mercs_hud_command(mercs_hud_settings *settings, unsigned command);
int mercs_hud_decode(mercs_hud_settings *out, const uint8_t *data, size_t size);
size_t mercs_hud_encode(const mercs_hud_settings *settings, int windows, uint8_t data[40]);
int mercs_hud_load_settings(mercs_hud_settings *settings);
int mercs_hud_save_settings(const mercs_hud_settings *settings, const char *directory,
                           int windows, char *error, size_t error_size);
#endif
