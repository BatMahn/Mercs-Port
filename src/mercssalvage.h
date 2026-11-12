#ifndef MERCS_SALVAGE_H
#define MERCS_SALVAGE_H

/* Dynamic salvage, as Mercenaries Titanium does it (MW2SHELL.DLL Salvage.cpp,
 * mercsw.dll 0x10059a10 / 0x10059d0f). The mission records every mech of the
 * player's star that is lost and every enemy mech the player kills; after a
 * won contract the debrief opens the Salvage Yard, where components are taken
 * from those wrecks into the dropship's inventory, inventory goes on the
 * discard pile, and whole chassis are recovered. The DOS and retail Windows
 * shells never read the wreck file, so this is an option in the native port.
 *
 * SALVAGE.MW2 is written in the Titanium engine's layout (0x10059d0f): u32
 * count, then per wreck: name 0x19, title 0x32, two u32 zeros, eight 0x28
 * location records {i32 front, i32 rear, i32 internal, 0x18 bytes, u16, u16},
 * a 0x140-byte block and 96 rand() values (0x180 bytes). */

#include <stddef.h>
#include <stdint.h>
#include "mercscareer.h"
#include "mercssave.h"
#include "prj.h"

#define MERCS_SALVAGE_MAX_WRECKS 100
#define MERCS_SALVAGE_NAME_BYTES 0x19
#define MERCS_SALVAGE_TITLE_BYTES 0x32
#define MERCS_SALVAGE_RECORD_BYTES (0x19 + 0x32 + 8 + 8 * 0x28 + 0x140 + 0x180)
#define MERCS_SALVAGE_FILE_MAX (4 + MERCS_SALVAGE_MAX_WRECKS * MERCS_SALVAGE_RECORD_BYTES)
/* Titanium's dropship hold (0x100652b8 / 0x100633ff): 200 critical slots, 18
 * distinct items a category, 12 mechs. */
#define MERCS_SALVAGE_HOLD_SLOTS 200
#define MERCS_SALVAGE_CATEGORY_LIMIT 18
#define MERCS_SALVAGE_MECH_LIMIT 12

typedef struct {
    char name[MERCS_SALVAGE_NAME_BYTES + 1];    /* stock design prefix: <name>STD.MEK */
    char title[MERCS_SALVAGE_TITLE_BYTES + 1];  /* display name */
    int32_t front[8], rear[8], internal[8];     /* per location, armour in MEK points */
    int32_t rnd[96];                            /* the engine's rand() draws for the shell's rolls */
} mercs_salvage_wreck;

enum { MERCS_SALVAGE_OK = 0, MERCS_SALVAGE_INVALID = -1, MERCS_SALVAGE_NOMEM = -2,
       MERCS_SALVAGE_FULL = -3, MERCS_SALVAGE_REFUSED = -4, MERCS_SALVAGE_MISSING = -5,
       MERCS_SALVAGE_CATEGORY_FULL = -6, MERCS_SALVAGE_LAST_MECH = -7, MERCS_SALVAGE_MECHS_FULL = -8 };

/* The Microsoft C runtime rand() the Titanium engine draws with. */
int32_t mercs_salvage_rand(uint32_t *state);
int mercs_salvage_encode(const mercs_salvage_wreck *wrecks, size_t count,
                         uint8_t *out, size_t capacity, size_t *size);
int mercs_salvage_decode(const uint8_t *bytes, size_t size,
                         mercs_salvage_wreck *wrecks, size_t capacity, size_t *count);

/* The debrief's salvage gate (0x10020000 area): a campaign mission's scene name
 * (e.g. "gra1scn1"), whether a generated contract granted salvage rights, and
 * whether this is OUT3 (which always opens, with a free Kodiak). 1 = opens. */
int mercs_salvage_allowed(const char *scene, int contract_rights);
int mercs_salvage_is_out3(const char *scene);

/* 0x10061b13: 0 missile, 1 energy, 2 projectile, 3 equipment (heat sink, jump
 * jet, CASE), 4 ammunition, -1 not salvageable (actuators, engine, armour ...). */
int mercs_salvage_category(int32_t code);
/* 0x1004ae6f on a 0x850-byte career mech: 0 intact .. 2 heavily damaged,
 * 3 destroyed. */
int mercs_salvage_condition(const uint8_t *mech, int location);
/* 0x1006729d: whether the whole chassis can be recovered. */
int mercs_salvage_whole(const uint8_t *mech, const int32_t rnd[96], int index);

/* The yard: the wrecks as Titanium builds them (0x100675e8), a working copy of
 * the career document, and the discard pile. */
typedef struct {
    mercs_career_mech current;    /* damaged, items lost or taken set to 0 */
    mercs_career_mech listing;    /* the location lists' names (Titanium's mirror copy) */
    int32_t status[8][12];        /* -1 unset, 0 intact, 1 damaged, 2 destroyed */
    int whole, visited;
    int owned;                    /* 1: an owned mech put back from the Chassis page */
    mercs_career_mech_pair pair;  /* its owned identity, kept if taken again */
} mercs_salvage_entry;

typedef struct {
    mercs_salvage_entry *entries;
    size_t count;
    mercs_save_state *document;   /* working copy; committed by accept */
    uint8_t (*discards)[MERCS_SAVE_ITEM_BYTES];
    size_t discard_count;
} mercs_salvage_yard;

int mercs_salvage_yard_open(mercs_salvage_yard *yard, prj_archive *archive,
                            const mercs_salvage_wreck *wrecks, size_t count,
                            int out3, const mercs_save_state *document);
void mercs_salvage_yard_free(mercs_salvage_yard *yard);

/* Take the component in a wreck's location slot into the hold (0x100652b8). */
int mercs_salvage_take_component(mercs_salvage_yard *yard, size_t entry, int location, int slot);
/* Move one inventory item of the given kind/id to the discards (0x1006581f:
 * a damaged copy first) or back (0x1006565a: an intact copy first).
 * kind: 0 weapon, 1 ammo, 2 equipment. */
int mercs_salvage_leave_item(mercs_salvage_yard *yard, unsigned kind, int32_t id);
int mercs_salvage_take_discard(mercs_salvage_yard *yard, unsigned kind, int32_t id);
/* Titanium's damaged flag on an inventory row (+0x16; never on ammunition). */
int mercs_salvage_row_damaged(const uint8_t *row);
/* Recover a whole chassis into the owned mechs (0x10065c2e). */
int mercs_salvage_recover(mercs_salvage_yard *yard, prj_archive *archive, size_t entry);
/* Put an owned mech back among the wrecks (0x100659c6). */
int mercs_salvage_discard_owned(mercs_salvage_yard *yard, size_t owned_index);
/* Hold totals for the footer: used critical slots (0x100632fd). */
int mercs_salvage_hold_used(const mercs_salvage_yard *yard);

/* The item code a location slot shows, its display name and critical slots. */
int mercs_salvage_item_slots(int32_t code);
void mercs_salvage_item_name(int32_t code, char *out, size_t capacity);
/* An inventory row's display name (kind 0 weapon, 1 ammo, 2 equipment). */
void mercs_salvage_row_name(unsigned kind, int32_t id, char *out, size_t capacity);
/* Inventory category of a row (Titanium 0x1006383c): 0..4, -1 none. */
int mercs_salvage_row_category(unsigned kind, int32_t id);

#endif
