/* Read the original PE tables independently of the generated native header. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "combat.h"

static uint32_t word(const unsigned char *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static float single(const unsigned char *p)
{ uint32_t bits=word(p);float value;memcpy(&value,&bits,4);return value; }
static const unsigned char *table(unsigned char *image,size_t length,uint32_t address)
{
    uint32_t pe=word(image+0x3c),opt=pe+24,va=address-word(image+opt+28),i;
    unsigned sections=image[pe+6]|image[pe+7]<<8,optional=image[pe+20]|image[pe+21]<<8;
    assert(pe+24+optional+40*sections<=length);
    for(i=0;i<sections;i++) {
        const unsigned char *s=image+opt+optional+i*40;
        uint32_t base=word(s+12),bytes=word(s+16),raw=word(s+20);
        if(va>=base && va-base+66*104<=bytes && raw+va-base+66*104<=length) return image+raw+va-base;
    }
    assert(0);return NULL;
}
static void profile(const char *path,int edition,uint32_t address)
{
    FILE *f=fopen(path,"rb");unsigned char *image;const unsigned char *p;
    long bytes;int w,ti=edition==COMBAT_PROFILE_TITANIUM;
    assert(f && !fseek(f,0,SEEK_END));bytes=ftell(f);assert(bytes>4096 && !fseek(f,0,SEEK_SET));
    image=malloc((size_t)bytes);assert(image&&fread(image,1,(size_t)bytes,f)==(size_t)bytes);fclose(f);
    p=table(image,(size_t)bytes,address);combat_set_weapon_profile(edition);
    for(w=0;w<66;w++,p+=104) {
        assert(combat_weapon_kind(w)==(int32_t)word(p+16));
        assert(combat_weapon_visual(w)==(int32_t)word(p+20));
        assert(combat_weapon_sound(w)==(int32_t)word(p+52));
        assert(combat_weapon_drop(w)==(int32_t)word(p+56));
        assert(combat_weapon_speed(w)==(ti?single(p+60):(float)word(p+60)));
        assert(combat_weapon_damage(w)==(ti?single(p+64):(float)word(p+64)));
        assert(combat_weapon_heat(w)==(ti?single(p+68):(float)word(p+68)/65536));
        assert(combat_weapon_range(w)==(ti?single(p+80):(float)word(p+80)));
        assert(combat_weapon_refire(w)==(int32_t)word(p+84));
        assert(combat_weapon_interval(w)==(int32_t)word(p+88));
        assert(combat_weapon_life(w)==(int32_t)word(p+92));
    }
    /* The actual native release path must use the profile, not a public
     * accessor alone. Both launcher families deliberately differ in Ti. */
    for(w=0;w<2;w++) {
        combat_unit u;int weapon=w?61:35;
        memset(&u,0,sizeof u);u.human=1;u.weapon_count=1;u.fire_edge=1;u.internal[0]=10;
        u.last_ticks=(float)combat_weapon_interval(weapon);
        u.weapons[0].weapon=weapon;u.weapons[0].ammo=100;u.weapons[0].state=CW_READY;
        combat_trigger_mask(&u,10000,1);
        assert(combat_next_projectile(&u)==weapon);
        assert(u.heat==combat_weapon_heat(weapon));
        if(ti) assert(u.heat==(w?4.0f:0.0f));
    }
    free(image);
}
int main(int argc,char **argv)
{
    assert(argc==4);
    profile(argv[1],COMBAT_PROFILE_RETAIL,0x100a7ba0);
    profile(argv[2],COMBAT_PROFILE_3DFX,0x1009b168);
    profile(argv[3],COMBAT_PROFILE_TITANIUM,0x1009a038);
    combat_set_weapon_timing(1);assert(combat_weapon_life(35)==910);
    puts("PASS:198 original retail/dedicated/Titanium weapon rows; native LRM/NARC heat release uses selected profile");
    return 0;
}
