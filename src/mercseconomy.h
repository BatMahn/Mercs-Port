#ifndef MERCS_ECONOMY_H
#define MERCS_ECONOMY_H

#include "mercssave.h"

/* Retail DOS shell transactions, operating on the complete owned save.
 * Caller stages/persists its campaign and rehydrates its shell view after a
 * successful mutation. Every failure leaves the supplied document intact. */
enum mercs_economy_status {
    MERCS_ECONOMY_OK = 0,
    MERCS_ECONOMY_INVALID = -1,
    MERCS_ECONOMY_NOMEM = -2,
    MERCS_ECONOMY_FUNDS = -3,
    MERCS_ECONOMY_CAPACITY = -4,
    MERCS_ECONOMY_PLAYER = -5,
    MERCS_ECONOMY_ASSIGNED = -6,
    MERCS_ECONOMY_EMPTY = -7,
    MERCS_ECONOMY_LIMIT = -8
};
enum mercs_personnel_kind { MERCS_PERSONNEL_MECHWARRIOR, MERCS_PERSONNEL_AEROTECH };

/* DOS0x29312/0x29471: buy at trunc(value*1.1), sell at stored value. One
 * actual row moves, retaining all its bytes except a sold weapon's owner.
 * ammo selects the separate ammunition list; index is a source-list index. */
int mercs_economy_item_price(const uint8_t item[MERCS_SAVE_ITEM_BYTES], int buy,
                            int32_t *price);
int mercs_economy_trade_item(mercs_save_state *state, int ammo, int buy, size_t index);

/* DOS0x4e3c1/0x4e673 whole-mech factory. Prices include current damage,
 * equipment and original depreciation. Buying allocates the next career ID
 * and first free two-digit deployment filename. Current/reference records
 * move together; selling unassigns the pilot. Native storage also removes
 * the purchased reference from shop stock (the DOS caller retained a shared
 * pointer there, corrupting the paired list on later serialization). */
int mercs_economy_mech_price(const mercs_save_state *state, int shop, size_t index,
                            int32_t *price);
int mercs_economy_trade_mech(mercs_save_state *state, int buy, size_t index);

/* DOS0x5c050 after an equipment/mech sale. An owned mech keeps the unit
 * viable; otherwise cash plus liquidation value must cover the cheapest
 * shop mech's truncated marked price. The original empty-shop ceiling is
 * 100,000,000 C-Bills. This is read-only and uses original DWORD arithmetic. */
int mercs_economy_viable(const mercs_save_state *state,int *viable);

enum mercs_item_category { MERCS_ITEM_MISSILE, MERCS_ITEM_ENERGY, MERCS_ITEM_PROJECTILE,
                           MERCS_ITEM_EQUIPMENT, MERCS_ITEM_AMMO };
typedef struct mercs_item_group {
    int32_t id, price;
    size_t first_index, count;
    unsigned type;
} mercs_item_group;
/* DOS0x5aed8 category jump table, DOS0x27ab8 grouped trading rows. Group
 * preparation is read-only: unlike the old pointer-list scratch flags it
 * does not change saved item bytes. Free the returned array with free(). */
int mercs_economy_item_category(const uint8_t item[MERCS_SAVE_ITEM_BYTES]);
int mercs_economy_item_groups(const mercs_save_state *state, int shop,
                              enum mercs_item_category category,
                              mercs_item_group **groups, size_t *count);

/* DOS0x46225/0x4638e/0x464d9/0x4662b. Hiring has no upfront fee; the
 * original monthly ledger charges salaries. Original limits are eight
 * MechWarriors including the player, and one AeroTech pilot. */
int mercs_economy_hire(mercs_save_state *state, enum mercs_personnel_kind kind, size_t index);
int mercs_economy_dismiss(mercs_save_state *state, enum mercs_personnel_kind kind, size_t index);

/* Original Duty Roster0x3ec84/0x3edcc. -1 denotes the virtual unassigned
 * entry. Cycling skips mechs assigned to any other pilot and never steals
 * their assignment. Current-mech and pilot links change together; reference
 * condition, instance IDs, filenames and equipment remain byte-exact. */
int mercs_economy_assign_pilot(mercs_save_state *state,size_t pilot_index,int mech_index);
int mercs_economy_cycle_pilot(mercs_save_state *state,size_t pilot_index,int direction);
/* Original0x3e318 support toggle and0x3ef10 mission roster gate. A mission
 * allowing three starmates may include one AeroTech; at most two ordinary
 * MechWarriors deploy. The caller presents original in-game notices. */
int mercs_economy_toggle_aerotech(mercs_save_state *state,size_t index);
int mercs_economy_roster_ready(const mercs_save_state *state,int *mechmates,int *aerotech);

/* Original Weapon Groups0x43734/0x444e1. A weapon click cycles0..4;
 * Default clears just bits8..11 of all14 weapon link fields. The remaining
 * link bits, every reference record, ammunition and identities are retained.
 * slot is the physical owned-mech weapon slot, or-1 for Default. */
int mercs_economy_weapon_group(mercs_save_state *state,size_t mech_index,int slot);

typedef struct mercs_finance {
    int32_t maintenance, technicians, technicians_display;
    int32_t travel, broker, guild, pilot_salaries, aero_salaries;
    int32_t monthly_total, funds;
} mercs_finance;
/* Read-only original finance ledger. DOS0x427db/0x42857 adds one C-Bill
 * to the technician row and displayed monthly total. The mission-expense
 * routine is separate and retains its own original formula. */
int mercs_economy_finance(const mercs_save_state *state, mercs_finance *out);

#endif
