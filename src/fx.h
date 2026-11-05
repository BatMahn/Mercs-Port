/*
 * fx.h - weapon effects (impacts, explosions, muzzle flashes), as the 3D editions' engine
 * builds them.
 *
 * Game data:
 *   MW2_SHT1 (BWD)  OBJ + XPLO pairs, one per pool slot: the OBJ's model is a flat card
 *                   (LAS0_0, XPL0_0, PPC0_0, MZL0_0, DEATH0_0...); XPLO payload
 *                   { i16 slot index, i16 texture slot, i16 effect type, i16 0 }.
 *   mw2_xpl1 (BWD)  texture slots: a run of BMPJ { u16 cel id, name } lists the frames, then
 *                   BMID { u16 slot } + BSEC { u16 slot, u16 rate } per slot.
 * Original retail Mercs engine:
 *   Windows 0x1004ab46 / DOS 0x64ad8: impact remapping and pool selection;
 *     eligible nearby effects permit three at high detail, one at low.
 *   Windows 0x100a9670 / DOS 0xbe6dc: 36 records of 44 bytes, including
 *     duration, sound/probability, light, damage/heat coefficients and flags.
 * Texture-slot clocks, reset/stop controls and per-pool polygon bindings have
 * separate original-instruction/data checks in tests/fixtures/fidelity-followup.
 * Model rasterization retains native GL behavior; clock parity is not a claim
 * of original-driver pixel identity.
 */
#ifndef MW2_FX_H
#define MW2_FX_H

#include <stdint.h>
#include "prj.h"
#include "mercstexclock.h"
#include "mercstexmap.h"

#define FX_TYPES   36   /* Mercs XPLO loader accepts 0..35 (DOS 0x66854 / Win 0x1006df3d). */
#define FX_LIVE    256
#define FX_FRAMES  32
#define FX_CARD_SLOTS 32   /* Mercs MZL pool has 32 authored objects. */
#define FX_PALETTE_SLOTS 20
#define FX_PALETTE_COMPONENTS 768

/* hit flags (engine projectile flags) */
#define FX_HIT_UNIT    0x100
#define FX_HIT_STRUCT  0x200
#define FX_HIT_GROUND  0x400
#define FX_HIT_GAUSS   0x1000
#define FX_HIT_PLAYER  0x2000

typedef mercs_texbinding fx_anim;

typedef struct {
    int     slots[FX_TYPES];          /* pool slots of each type (XPLO count) */
    int     anim[FX_TYPES];           /* texture slot of the type's first pool object, -1 none */
    int     control[FX_TYPES][FX_CARD_SLOTS]; /* XPLO slot reset/start/stop target */
    int     render[FX_TYPES][FX_CARD_SLOTS];  /* each object's actual polygon slot */
    float   half_w[FX_TYPES], half_h[FX_TYPES];   /* card size (cm) */
    /* a colour type 3 card (0x3xxx, drawn by the billboard 0x1002a8e0): its foot (texel u 0, v > 0) and top (u 0, v 0)
     * corners, cm from the effect's position (x 0..2: game x, y, z; the pool objects are unrotated, 0x10045c30) */
    int     bb[FX_TYPES];
    float   bb_foot[FX_TYPES][3], bb_top[FX_TYPES][3];
    /* a card drawn as a 3D polygon (colour 0x8xxx: MZL / JET, 0xbxxx: DEATH - 0x1002a4b0 -> 0x10026e80 -> 0x1002c880, not
     * the billboard): each pool object's polygons (up to 2, 3-4 corners) x its OBJ scale, in the object's frame (game x,
     * y, z), texel u / v as 0-1 of the bitmap, and each polygon's normal as the engine holds it (the file's negated,
     * 0x10042140) - drawn when (corner 0 - eye) . normal < 0 (0x1002b9b0). Per pool slot (the type's XPLO entries in
     * MW2_SHT1 order, 0x1003ed70): the slots' models differ (MZL0_2 / MZL0_3: one polygon, the others two) */
    int     card_n[FX_TYPES][FX_CARD_SLOTS], card_nv[FX_TYPES][FX_CARD_SLOTS][2];
    float   card_v[FX_TYPES][FX_CARD_SLOTS][2][4][3], card_uv[FX_TYPES][FX_CARD_SLOTS][2][4][2], card_nrm[FX_TYPES][FX_CARD_SLOTS][2][3];
    fx_anim tex[512];                 /* texture slots (u16, engine 0x1002d060: < 0x200) */
    int     scene_bound;              /* modes already replayed in original include order */
} fx_defs;

typedef struct {
    int     type, alive;
    unsigned serial;                /* generation: a recycled pool slot must not retain an older area effect */
    int     source_visual;            /* projectile visual before impact remapping; controls local lights */
    int     slot;                     /* the pool slot of its type: the first free one (0x10045d00) */
    float   pos[3];
    int     texture_slot, control_slot; /* shared texture state lives in fx_world */
    int32_t remaining;                /* original remaining-life countdown, raw integer ticks */
    int     has_rot;                  /* rot: the object's orientation (row-major, game coordinates) - a muzzle flash's or
                                       * jet flame's, its mount node's (0x10044a70); otherwise unrotated (0x10045c30) */
    float   rot[9];
    int     attach_actor, attach_node; /* actor -2 = none; stable skeleton node, across LOD changes */
    int     light_marked;             /* original slot+0x20: was accepted as a scene-light owner */
} fx_live;

/* Optional indexed-palette service. The immutable bank and callback owner
 * outlive the binding. RGB components stay in their original six-bit domain;
 * display conversion belongs to the frontend. The existing world counters
 * below remain the single admission/retention state for every fade caller. */
typedef struct {
    const uint8_t (*bank)[FX_PALETTE_COMPONENTS];
    const int *valid;
    void (*submit)(void *user, const uint8_t *rgb6);
    void *user;
    int current, dirty, base, queued, want, back_slot;
    int frames, step, buffer_present;
    uint8_t rgb[FX_PALETTE_COMPONENTS];
    int8_t delta[FX_PALETTE_COMPONENTS];
    int16_t remainder[FX_PALETTE_COMPONENTS];
} fx_palette;

typedef struct {
    fx_live e[FX_LIVE];
    mercs_texclock clocks[512];
    int clocks_ready;
    const fx_defs *definitions;       /* immutable definitions outlive this world */
    unsigned next_serial;
    int light_slot;                   /* slot+1; zero means the original scene light is restored */
    float light_pos[3];               /* position latched at creation, independent of a following burn */
    int palette_left, palette_back;   /* original palette frame counters, not elapsed ticks */
    int external_fade;               /* shared damage/vision/time-of-day palette service is busy */
    fx_palette palette;              /* bank==NULL retains the counter-only legacy frontend */
} fx_world;

/* Load MW2_SHT1 and mw2_xpl1 from the models archive. 0 on success. */
int  fx_load(prj_archive *models, fx_defs *d);
/* Replay the mission's texture/control stream, including repeated includes.
 * Pool geometry is unchanged; failure leaves the prior definitions intact. */
int  fx_bind_scene(prj_archive *models, const char *scene, fx_defs *d);

/* engine 0x10045d00: effect type for a projectile of weapon visual `visual` ending with `flags`;
 * -1 for none. */
int  fx_impact_type(int visual, int flags);
/* Mercs visual11 dispatches chunks directly (no effect-pool allocation). */
int  fx_impact_chunks(int visual, int flags);

/* Muzzle effect types of weapon w (engine weapon table 0x1025a6c0 +0x08 / +0x0c), k = 0 or 1;
 * -1 none. Only types with pool objects show (0x17 ballistic, 0x1a missile, 0x1e PPC). */
int  fx_muzzle(int weapon, int k);
#define FX_JET 0x19   /* jump-jet flame (engine 0x1001bf90, at the two jet mounts while jetting) */

/* Original retail Mercs tables: DOS 0xbe6dc; Windows 0x100a9670.
 * The hardware profiles currently use retail Windows timing pending their
 * own full engine audit. This does not certify their special callbacks. */
void fx_set_timing(int dos);
int  fx_duration(int type);   /* ticks */
int  fx_debris_duration(void); /* registered detached objects: DOS3640 / Windows3620 */
int  fx_sound(int type);      /* SNDS index, -1 none */
int  fx_light(int type);      /* lights its surroundings */
int  fx_palette_index(int type); /* palette relative to current time-of-day group; -1 none */
int  fx_local_light(int source_visual); /* original separate local-light producer before hit remapping */
int  fx_sound_roll(int type, int roll); /* original 0..99 roll; -1 if suppressed */
int  fx_damage_q16(int type); /* per tick, zero for non-damaging types */
int  fx_heat_q16(int type);   /* per tick */
int  fx_always_splash(int type);
int  fx_attaches(int type);
int  fx_initial_life(int type, int attached, int frame_ticks);
/* Non-unit area consumer rounds Q16 coefficient*ticks to whole hit points. */
int  fx_thing_damage(int coefficient, int frame_ticks);
float fx_area_radius(int type, float radius, int elapsed_ticks);

/* Start an effect at pos (cm) at time `now` (animation ticks). frame_ticks is
 * this simulation update's nonnegative integer raw delta: original creation
 * initializes remaining life to duration + frame_ticks before that update.
 * Returns its index or -1 when the pool/near limit refuses it. */
extern int fx_detail_low;   /* DISPLAY DETAIL low: no neighbouring effects of a type within 5 m */
/* Original integer approximate distance used by the five-meter pool gate. */
int32_t fx_near_distance(int32_t x, int32_t y, int32_t z);
int  fx_spawn(fx_world *w, const fx_defs *d, int type, const float pos[3], int32_t now, int frame_ticks);
/* Original palette counter control: mode1 out/back, mode2 immediate/back,
 * mode0 replacement. The supplied palette service must have valid records. */
int  fx_palette_begin(fx_world *w, int duration, int mode, int frame_ticks);
void fx_palette_step(fx_world *w, int frame_ticks);
/* Retail DOS4dab8/Windows1006065d: shared mode0 replacement, mode1 out/back,
 * mode2 one-step/full-target return. Binding does not submit or advance. */
int  fx_palette_bind(fx_world *w, const uint8_t bank[FX_PALETTE_SLOTS][FX_PALETTE_COMPONENTS],
                     const int valid[FX_PALETTE_SLOTS], int initial_slot,
                     void (*submit)(void *user, const uint8_t *rgb6), void *user);
int  fx_palette_request(fx_world *w, int slot, int duration, int mode, int frame_ticks);
/* Queue/base changes do not mark dirty. Flush submits only after an active
 * transition has finished; call immediately after the presentation step. */
void fx_palette_queue(fx_world *w, int slot);
void fx_palette_flush(fx_world *w);
/* Offer a newly created effect to the scene-light latch. Only a nearer new
 * blast can replace a live owner; signed32 distance wrapping is original. */
int  fx_light_offer(fx_world *w, int slot, const float camera[3], int allowed, int palette_available, int frame_ticks);
/* Compare the admitted effect's ordinary point against the prior stored
 * light point, then store light_point on acceptance. NULL preserves the
 * ordinary point. Titanium uses float-stored distances; other profiles keep
 * signed32 integer wrapping. The legacy wrapper retains integer semantics. */
int  fx_light_offer_point(fx_world *w, int slot, const float camera[3], int allowed,
                          int palette_available, int frame_ticks,
                          const float light_point[3], int float_coordinates);
/* True only for the marked current light owner during an active palette
 * transition. serial protects simulation area effects from slot recycling. */
int  fx_retains(const fx_world *w, int slot, unsigned serial);
/* Call once per simulation step after spawning/area damage; never on redraw.
 * Subtract this same integer delta, then clear effects whose life is <= 0. */
void fx_update(fx_world *w, int frame_ticks);
/* Original DOS0x452d8 / Windows0x1005f2f6, after effect countdown in the
 * original frame. Rendering is an observer and never advances animation. */
void fx_animate(fx_world *w, const fx_defs *d, int32_t now);
/* Initialize all bindings and XPLO stop modes once at mission creation. */
void fx_clocks_init(fx_world *w, const fx_defs *d);
/* Observer only: the object's polygon reads its shared slot, -1 if hidden. */
int  fx_frame(const fx_world *w, const fx_defs *d, const fx_live *e);

#endif
