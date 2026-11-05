#ifndef MERCS_MOTION_AUDIO_H
#define MERCS_MOTION_AUDIO_H

/* Mercenaries movement sounds. These are Mercs SNDS identifiers, not the
 * similarly named MechWarrior 2 records. Original DOS initializer 0x26930
 * and gait selector 0x26af4; retail Windows 0x1007074a / 0x100709b2.
 * DOS landing 0x33748 and world-impact branch 0x30c58. */
enum {
    MERCS_SOUND_FOOTSTEP_OTHER = 282,    /* NONFOOT */
    MERCS_SOUND_FOOTSTEP_MEDIUM = 231,   /* MECFSMED; fallback only, prefer ASND */
    MERCS_SOUND_IMPACT_BUILDING = 217,   /* MECBLDC1 */
    MERCS_SOUND_LANDING_HARD = 250,      /* MECMTNHD */
    MERCS_SOUND_LANDING_SOFT = 251,      /* MECMTNSF */
    MERCS_SOUND_TORSO_SERVO = 341        /* TORSLOOP; Mech2's316 is Mercs STAR3A1 speech */
};

/* Mercs SNDS names used by the native in-mission menu. */
enum {
    MERCS_SOUND_MENU_MOVE = 237,         /* MECGCLX1 */
    MERCS_SOUND_MENU_OPEN = 245          /* MECMENU1 */
};

/* Mercs gameplay events, verified against the original DOS / retail-Windows
 * callers and their SNDS inventory. Never translate numeric IDs globally:
 * e.g. 223 really is SYSTEMS_READY, but Mech2 used 223 for jump jets.
 * See tests/fixtures/gameplay-audio/README.md for caller/asset provenance. */
enum {
    MERCS_SOUND_ENEMY_FIRE = 92,         /* ENEMYFIR */
    MERCS_SOUND_INFRARED = 149,          /* IREDMODE */
    MERCS_SOUND_JETTISON = 167,          /* JETTISON */
    MERCS_SOUND_EJECT = 214,             /* MECBEJXX */
    MERCS_SOUND_MASC_FAILED = 218,       /* MECBMAMA */
    MERCS_SOUND_MASC_OFF = 219,          /* MECBMAOF */
    MERCS_SOUND_MASC_ON = 220,           /* MECBMAON */
    MERCS_SOUND_SHUTDOWN_OVERRIDE = 222, /* MECBSOXX */
    MERCS_SOUND_SYSTEMS_READY = 223,     /* MECBSYRX */
    MERCS_SOUND_TRACK_ACQUIRED = 224,    /* MECBYHLX */
    MERCS_SOUND_CRITICAL = 225,          /* MECCHIN1 */
    MERCS_SOUND_DISPLAY = 240,           /* MECHUDON; inspection also uses this */
    MERCS_SOUND_JET_FUEL = 242,          /* MECJFUEL */
    MERCS_SOUND_JET = 243,               /* MECJJRXX */
    MERCS_SOUND_NAV_ARRIVAL = 252,       /* MECNAVPT */
    MERCS_SOUND_MAP = 253,               /* MECOMBEP */
    MERCS_SOUND_OVERHEAT = 254,          /* MECOVHW1 */
    MERCS_SOUND_ARMOR_BREACH = 257,      /* MECRDWA1 */
    MERCS_SOUND_SHUTDOWN = 263,          /* MECSHTD1 */
    MERCS_SOUND_STARTUP = 268,           /* MECTURX2 */
    MERCS_SOUND_FIRE_GROUP = 274,        /* MECWPTG1 */
    MERCS_SOUND_TARGET_LOCK = 275,       /* MECXLOCK */
    MERCS_SOUND_INCOMING_TRACK = 278,    /* MISLTRCK */
    MERCS_SOUND_OTHER_STARTUP = 283,     /* NONPSTRT */
    MERCS_SOUND_VIEW_ZOOM = 347,         /* VIEWZOOM */
    MERCS_SOUND_WEAPON_BUSY = 349        /* WEPNBUSY */
};

/* Windows 0x10026c76 copies field4 from the original behavior table selected
 * by GPS attribute6, not pilot level. 0x100234a9 compares rand(100) to it.
 * Undefined out-of-table authored input does not invent an audible event. */
static inline int mercs_motion_enemy_fire_chance(unsigned behavior)
{
    static const unsigned char chances[7] = {25, 25, 15, 25, 60, 100, 85};
    return behavior < 7 ? chances[behavior] : 0;
}

/* Mercs Windows0x100639ad retains a successful forced-lock roll only for
 * AI control. Other control kinds run the normal acquisition calculation. */
static inline int mercs_motion_forced_lock(int control, int chance, unsigned lock)
{ return control == 2 && chance != 0 && (lock & 0x80u) != 0; }

/* Mercs Windows 0x1006b47b / DOS 0x33630. A live exhaust node is required.
 * The player's low-fuel warning replaces this update's jet effect, and both
 * sides of the crossing are strict: 455 ticks in DOS, 452 in Windows.
 * Native fuel can be fractional; retain the integer boundary and ordering. */
static inline int mercs_motion_jet_sound(int dos, int player, int firing,
                                          float fuel, float elapsed_ticks)
{
    float threshold = dos ? 455.0f : 452.0f;
    if (!firing) return -1;
    if (player && fuel < threshold && fuel + elapsed_ticks > threshold)
        return MERCS_SOUND_JET_FUEL;
    return MERCS_SOUND_JET;
}

/* Original Mercs DOS0x32d6a and retail Windows0x100222c4: only the
 * player's cockpit hears TORSLOOP, and only while the torso trails its
 * demand by more than two degrees. Leg turning alone is silent. */
static inline int mercs_motion_torso_active(float demand, float actual, int cockpit)
{
    return cockpit && (demand - actual > 2.0f || actual - demand > 2.0f);
}

/* Once per gait-state transition, including the reverse-to-walk special
 * case. State starts at -2. The unchanged current/requested sequence keeps
 * playing its animation without repeating the transition sound. Returns -1
 * when the original selector is silent. Sound playback remains the caller's
 * responsibility, with its existing camera-relative position and volume. */
static inline int mercs_motion_gait_sound(int *state, int current, int requested)
{
    static const short sounds[16] = {
        -1, 339, 339, -1, 352, -1, -1, 352,
        294, 294, 348, 294, 293, 293, 293, 338
    };
    int old, sound;
    if (!state || current < -1 || current > 2 || requested < -1 || requested > 2)
        return -1;
    old = *state;
    if (current != requested ? old == current : old == -2) return -1;
    sound = current == 0 && old == 2 ? sounds[1] :
            sounds[5 + requested + 4 * current];
    *state = current == requested ? -2 : current;
    return sound;
}

#endif
