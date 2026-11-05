/* Verify every selected timer against the original binaries, then exercise
 * the boundary where the exact values alter live weapon behavior. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "combat.h"

static uint32_t original_u32(FILE *f,long offset)
{
    unsigned char b[4];
    assert(fseek(f,offset,SEEK_SET)==0 && fread(b,1,4,f)==4);
    return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;
}

int main(int argc,char **argv)
{
    FILE *original[2];
    const long base[2]={0xa5da0,0xaac0c};
    const char *lrm_loadout[2]={"KTF04STD","CAT00STD"};
    char error[256];
    prj_archive *a;
    combat_unit u;
    int dos,w,j;
    assert(argc==4);
    original[0]=fopen(argv[2],"rb"); original[1]=fopen(argv[3],"rb");
    assert(original[0] && original[1]);
    a=prj_open(argv[1],error,sizeof error);
    if (!a) { fprintf(stderr,"%s\n",error); return 1; }
    assert(combat_weapon_refire(23)==91); /* retail DOS default */
    for (dos=0;dos<2;dos++) {
        combat_set_weapon_timing(dos);
        for (w=0;w<66;w++) {
            long offset=base[dos]+104*w+0x54;
            assert(combat_weapon_refire(w)==(int)original_u32(original[dos],offset));
            assert(combat_weapon_interval(w)==(int)original_u32(original[dos],offset+4));
            assert(combat_weapon_life(w)==(int)original_u32(original[dos],offset+8));
        }
        assert(combat_weapon_refire(-1)==0 && combat_weapon_interval(66)==0 && combat_weapon_life(66)==0);
        /* Real Kodiak ERML: recycle at90 Windows ticks,91 DOS ticks. */
        assert(combat_init(&u,a,"KDK00STD")==0 && u.weapons[4].weapon==23);
        u.no_heat=1;
        u.weapons[4].state=CW_RECYCLE; u.weapons[4].t=89;
        combat_tick(&u,1.0f/COMBAT_TICKS_PER_S);
        assert(u.weapons[4].state==(dos?CW_RECYCLE:CW_READY));
        combat_tick(&u,1.0f/COMBAT_TICKS_PER_S);
        assert(u.weapons[4].state==CW_READY);
        assert(combat_weapon_life(23)==(dos?182:181));
        /* The non-scalable exception: Clan and IS LRM15 release their first
         * missile at18 DOS ticks,22 Windows ticks. Both real loadouts. */
        for (j=0;j<2;j++) {
            int type=j?36:1;
            assert(combat_init(&u,a,lrm_loadout[j])==0 && u.weapons[0].weapon==type);
            u.no_heat=1; u.last_ticks=18;
            combat_trigger_mask(&u,10000,1u);
            assert(combat_next_projectile(&u)==(dos?type:-1));
            if (!dos) {
                combat_tick(&u,4.0f/COMBAT_TICKS_PER_S);
                assert(combat_next_projectile(&u)==type);
            }
            assert(combat_next_projectile(&u)==-1);
        }
    }
    fclose(original[0]); fclose(original[1]); prj_close(a);
    puts("PASS: all66 original timer triplets in each edition; live ERML refire90/91, lifetime181/182, Clan/IS LRM15 interval22/18; DOS default and explicit Windows selection.");
    return 0;
}
