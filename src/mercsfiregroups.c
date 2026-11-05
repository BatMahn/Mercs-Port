#include "mercsfiregroups.h"
#include "wtable.h"

static int count(const combat_unit *u)
{ return !u || u->weapon_count<0 ? 0 : u->weapon_count>14 ? 14 : u->weapon_count; }

static int loaded(const combat_unit *u,int i)
{ return i>=0 && i<count(u) && u->weapons[i].weapon>=0 && u->weapons[i].weapon<SIM_WEAPON_COUNT; }

static int selectable(const combat_unit *u,int i)
{
    return loaded(u,i) && u->weapons[i].state!=-1 &&
           sim_weapons[u->weapons[i].weapon].selectable;
}

int mercs_firegroup_assign(combat_unit *u,int selected,int group)
{
    if(!loaded(u,selected) || group<0 || group>4) return 0;
    u->weapons[selected].return_group=group;
    return 1;
}

int mercs_firegroup_next_group(const combat_unit *u,int selected)
{
    int step,i,group;
    if(!loaded(u,selected)) return selected;
    group=u->weapons[selected].return_group;
    if(group<0 || group>=14) return selected;
    for(step=1;step<14;step++) for(i=0;i<14;i++)
        if(selectable(u,i) && u->weapons[i].return_group==(group+step)%14)
            return i;
    return selected;
}

int mercs_firegroup_next_weapon(const combat_unit *u,int selected,int cycle_group)
{
    int step;
    if(!loaded(u,selected)) return selected;
    for(step=1;step<14;step++) {
        int i=(selected+step)%14;
        if(selectable(u,i) && u->weapons[i].return_group==u->weapons[selected].return_group)
            return i;
    }
    return cycle_group ? mercs_firegroup_next_group(u,selected) : selected;
}

int mercs_firegroup_select(const combat_unit *u,int selected,int group)
{
    int i;
    if(count(u)<1 || group<0 || group>2) return -1;
    /* Original same-group early success precedes its state/selectability
     * guards. A now-spent selection therefore stays selected here. */
    if(loaded(u,selected) && u->weapons[selected].return_group==group) return selected;
    for(i=0;i<14;i++) if(selectable(u,i) && u->weapons[i].return_group==group) return i;
    return -1;
}

uint32_t mercs_firegroup_mask(const combat_unit *u,int selected,int ready_only)
{
    uint32_t mask=0;
    int first,i,n=0;
    if(!loaded(u,selected)) return 0;
    first=i=mercs_firegroup_next_weapon(u,selected,0);
    do {
        if(loaded(u,i) && (ready_only ? u->weapons[i].state==CW_READY : u->weapons[i].state!=-1))
            mask|=1u<<i;
        i=mercs_firegroup_next_weapon(u,i,0);
    } while(i!=first && ++n<14);
    return mask;
}

uint32_t mercs_firegroup_alpha_mask(const combat_unit *u,int ready_only)
{
    uint32_t mask=0;
    int i;
    for(i=0;i<count(u);i++) if(loaded(u,i) &&
        (ready_only ? u->weapons[i].state==CW_READY : u->weapons[i].state!=-1)) mask|=1u<<i;
    return mask;
}
