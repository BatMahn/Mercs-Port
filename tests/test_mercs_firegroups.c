#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mercsfiregroups.h"
#include "../tests/fixtures/combat-completion/original-firegroups.h"

static void original_case(const original_firegroup_case *c)
{
    combat_unit u;
    int i,selected=c->selected,choice;
    uint32_t mask=0;
    memset(&u,0,sizeof u); u.weapon_count=c->count;
    for(i=0;i<14;i++) {
        u.weapons[i].weapon=c->mounts[i][0];
        u.weapons[i].state=c->mounts[i][1];
        u.weapons[i].return_group=c->mounts[i][2];
        u.weapons[i].destroyed=u.weapons[i].state==-1;
        /* HUD location order deliberately opposes mount order. */
        u.weapons[i].location=i&1 ? MEK_LA : MEK_RA;
    }
    switch(c->operation) {
    case 0: selected=mercs_firegroup_next_weapon(&u,selected,c->argument);break;
    case 1: selected=mercs_firegroup_next_group(&u,selected);break;
    case 2: choice=mercs_firegroup_select(&u,selected,c->argument);if(choice>=0)selected=choice;break;
    case 3: mercs_firegroup_assign(&u,selected,c->argument);break;
    case 4: mask=mercs_firegroup_mask(&u,selected,1);break;
    case 5: mask=mercs_firegroup_alpha_mask(&u,1);break;
    default: assert(0);
    }
    for(i=0;i<14;i++) if(mask&(1u<<i)) u.weapons[i].state=CW_FIRING;
    if(selected!=c->want_selected) {
        fprintf(stderr,"firegroup op%d selected%d arg%d expected%d got%d\n",
                c->operation,c->selected,c->argument,c->want_selected,selected);
        assert(selected==c->want_selected);
    }
    for(i=0;i<c->count;i++) {
        assert(u.weapons[i].state==c->want_states[i]);
        /* Original assignment also writes an empty selected mount. Native
         * arrays do not create a loadout item for that invalid input. */
        if(c->mounts[i][0]>=0) assert(u.weapons[i].return_group==c->want_groups[i]);
    }
}

int main(void)
{
    size_t i;
    combat_unit u;
    for(i=0;i<sizeof original_firegroups/sizeof original_firegroups[0];i++) original_case(&original_firegroups[i]);
    memset(&u,0,sizeof u);u.weapon_count=3;
    u.weapons[0].weapon=48;u.weapons[0].state=CW_REPEAT;
    u.weapons[1].weapon=48;u.weapons[1].state=CW_RECYCLE;
    u.weapons[2].weapon=48;u.weapons[2].state=-1;
    assert(mercs_firegroup_mask(&u,0,0)==3 && !mercs_firegroup_mask(&u,0,1));
    assert(!mercs_firegroup_assign(&u,0,5));
    assert(mercs_firegroup_select(&u,0,3)==-1);
    printf("PASS: %zu original DOS weapon group/selection/alpha/assignment cases; repeat hold and bounds\n",i);
    return 0;
}
