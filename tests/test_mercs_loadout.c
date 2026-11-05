/* Focused regression against original retail MEK records and MERCSW.DLL
 * constants/consumers. Including combat.c exposes the critical dispatcher so
 * exact original equipment IDs can be exercised without random hit selection. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../src/combat.c"

static void nearf(float actual, float expected) { assert(fabsf(actual - expected) < 0.00001f); }
static void check_weapons(const combat_unit *u, const int *expected, int n)
{
    int i;
    assert(u->weapon_count == n);
    for (i = 0; i < n; i++) assert(u->weapons[i].weapon == expected[i]);
}
static int slot_of(const combat_unit *u, int loc, uint32_t id)
{
    int i;
    for (i = 0; i < u->slot_count[loc]; i++) if (u->slots[loc][i] == id) return i;
    assert(!"original equipment ID missing");
    return -1;
}
int main(int argc, char **argv)
{
    char error[256], label[80];
    prj_archive *a;
    combat_unit k, atl, h;
    const int kodiak[] = {19,22,7,7,23,23,23,23,23,23,23,23};
    const int atlas[] = {48,35,56,56,56,56,39};
    const int hunchback[] = {48,56,56,57};
    combat_planet planet = {0.7f,-101,50000,0};
    unsigned rng = 123;
    float health;
    int w;
    assert(argc == 2);
    a = prj_open(argv[1], error, sizeof error);
    if (!a) { fprintf(stderr, "%s\n", error); return 1; }
    combat_set_planet(&planet);
    assert(combat_init(&k,a,"KDK00STD")==0);
    assert(combat_init(&atl,a,"ATL00STD")==0);
    assert(combat_init(&h,a,"HCH00STD")==0);
    check_weapons(&k,kodiak,12); check_weapons(&atl,atlas,7); check_weapons(&h,hunchback,4);
    assert(k.bin_count==4 && k.weapons[0].ammo==10);
    assert(k.weapons[2].ammo==180 && k.weapons[3].ammo==180);
    assert(k.weapons[2].bin_first==k.weapons[3].bin_first);
    assert(k.weapons[2].bin_n==2 && k.weapons[3].bin_n==2);
    assert(combat_ammo_total(&k)==190 && combat_ammo_total(&atl)==340 && combat_ammo_total(&h)==10);
    assert(atl.weapons[0].ammo==10 && atl.weapons[1].ammo==240 && atl.weapons[6].ammo==90);
    assert(!strcmp(mek_item_name(2301,label,sizeof label),"ER MLaser (C)"));
    assert(!strcmp(mek_item_name(4801,label,sizeof label),"Autocannon/20"));
    assert(strstr(mek_item_name(7301,label,sizeof label),"Shoulder"));
    assert(!strcmp(mek_item_name(8001,label,sizeof label),"Heat Sink"));
    assert(!strcmp(mek_item_name(8501,label,sizeof label),"Jump Jet"));
    assert(!strcmp(mek_item_name(9550,label,sizeof label),"CASE"));
    nearf(k.sink_per_tick, 9500.0f/65536.0f);
    nearf(combat_weapon_damage(19),20); nearf(combat_weapon_damage(22),10); nearf(combat_weapon_damage(23),7);
    nearf(combat_weapon_range(19),36000); nearf(combat_weapon_range(22),75000); nearf(combat_weapon_range(23),45000);
    nearf(combat_weapon_damage(48),20); nearf(combat_weapon_range(48),27000);
    nearf(combat_weapon_damage(35),1); nearf(combat_weapon_range(35),63000);
    nearf(combat_weapon_damage(56),5); nearf(combat_weapon_range(56),27000);
    nearf(sim_weapons[48].heat,3.5f); nearf(sim_weapons[56].heat,1.5f);
    assert(combat_weapon_sound(19)==646 && combat_weapon_sound(48)==622);
    assert(sim_weapons[35].guided==1 && sim_weapons[42].guided==1);
    assert(sim_weapons[7].field3c==3000 && sim_weapons[42].field3c==25);
    /* Release from each real Kodiak launcher and verify the single pool. */
    k.sink_per_tick=0; k.last_ticks=45;
    combat_trigger_mask(&k,10000,1u<<2);
    assert(combat_next_projectile(&k)==7);
    assert(k.weapons[2].ammo==179 && k.weapons[3].ammo==179);
    nearf(k.heat,87425.0f/65536.0f);
    combat_trigger_mask(&k,10000,1u<<3);
    assert(combat_next_projectile(&k)==7);
    assert(k.weapons[2].ammo==178 && k.weapons[3].ammo==178);
    nearf(k.heat,174850.0f/65536.0f);
    /* The twelfth mount must be triggerable and release a real ER medium shot. */
    k.last_ticks=60;
    combat_trigger_mask(&k,10000,1u<<11);
    assert(combat_next_projectile(&k)==23);
    nearf(k.heat,174850.0f/65536.0f+2.5f);
    assert(combat_jettison(&k,2)==1);
    assert(k.weapons[2].ammo==0 && k.weapons[3].ammo==0 && k.weapons[2].state==-1 && k.weapons[3].state==-1);
    assert(combat_ammo_total(&k)==10);
    /* Actual IS projectile damage and original PPC target heat are live. */
    health=h.armor[MEK_RA];
    assert(combat_hit(&h,56,MEK_RA+1,180,&rng)==5);
    nearf(h.armor[MEK_RA],health-5);
    health=h.armor[MEK_LA];
    assert(combat_hit(&h,52,MEK_LA+1,180,&rng)==10);
    nearf(h.armor[MEK_LA],health-10); nearf(h.heat,3.5f);
    /* Mercs actuator IDs must no longer be mistaken for base-MW2 jump jets. */
    assert(combat_init(&k,a,"KDK00STD")==0); k.human=1;
    crit_item(&k,MEK_RA+1,slot_of(&k,MEK_RA,7301),0);
    assert(k.display_hits==1 && !k.destroyed);
    health=k.sink_per_tick;
    crit_item(&k,MEK_RA+1,slot_of(&k,MEK_RA,8001),0);
    nearf(k.sink_per_tick,health-50.0f/65536.0f);
    crit_item(&k,MEK_HEAD+1,slot_of(&k,MEK_HEAD,7750),0);
    assert(k.destroyed);
    /* Shared last-round exhaustion invalidates both mounts. */
    assert(combat_init(&k,a,"KDK00STD")==0); k.sink_per_tick=0; k.last_ticks=45;
    k.weapons[2].ammo=1; k.weapons[3].ammo=1;
    combat_trigger_mask(&k,10000,(1u<<2)|(1u<<3));
    w=combat_next_projectile(&k); assert(w==7 && combat_next_projectile(&k)==-1);
    assert(k.weapons[2].state==-1 && k.weapons[3].state==-1 && k.weapons[3].ammo==0);
    puts("PASS: real Kodiak 12 mounts/190 shared rounds, Atlas 7/340, Hunchback 4/10; retail stats, target heat, cold cooling, pooled firing/jettison/exhaustion, and equipment critical IDs");
    prj_close(a);
    return 0;
}
