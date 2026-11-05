#ifndef MERCS_FIREGROUPS_H
#define MERCS_FIREGROUPS_H

#include "combat.h"
#include <stdint.h>

/* Mercs DOS66d8c/6687c/66a1c/66ae4/66e9c/66f80. Native weapon indices
 * retain the original loader's fourteen physical-mount ordering. HUD window
 * positions are presentation and must not reorder these command scans.
 * Group assignments are stored in return_group and survive returned MEKs. */
int mercs_firegroup_assign(combat_unit *unit, int selected, int group);
int mercs_firegroup_next_weapon(const combat_unit *unit, int selected,
                                 int cycle_group);
int mercs_firegroup_next_group(const combat_unit *unit, int selected);
/* Direct group-fire inputs select only0..2, as the original consumer does.
 * Return selected weapon index, or-1 if that requested group is unavailable. */
int mercs_firegroup_select(const combat_unit *unit, int selected, int group);
/* ready_only=0 keeps the group trigger held through repeat/recycle states;
 * ready_only=1 is the exact original ready-state set at trigger time. Group
 * fire preserves selection. Alpha includes all physical loaded mounts. The
 * combat AMS callback still owns AMS, even when an alpha mask contains it. */
uint32_t mercs_firegroup_mask(const combat_unit *unit, int selected, int ready_only);
uint32_t mercs_firegroup_alpha_mask(const combat_unit *unit, int ready_only);

#endif
