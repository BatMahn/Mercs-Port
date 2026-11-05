#include "sdl3_test.h"
#include "mercshud.h"
#include "inputmap.h"
#include "mercstarget_math.h"
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include "../tests/fixtures/controls-completion/original-runtime.h"
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
int main(void)
{
    size_t i;
    int j;
    mercs_hud_settings s, decoded;
    uint8_t bytes[40];
    for (i=0;i<COUNT(original_hud_commands);i++) {
        mercs_hud_defaults(&s);
        for(j=0;j<7;j++)s.mode[j]=original_hud_commands[i].before;
        assert(mercs_hud_command(&s,(unsigned)original_hud_commands[i].command));
        for(j=0;j<7;j++)assert(s.mode[j]==original_hud_commands[i].mode[j]);
        assert(s.weapons==original_hud_commands[i].weapons && s.compass==original_hud_commands[i].compass);
    }
    for(i=0;i<COUNT(original_hud_files);i++) {
        assert(!mercs_hud_decode(&s,original_hud_files[i],37));
        assert(mercs_hud_encode(&s,0,bytes)==37 && !memcmp(bytes,original_hud_files[i],37));
        assert(mercs_hud_encode(&s,1,bytes)==40 && bytes[37]==0 && bytes[38]==0 && bytes[39]==0);
        assert(!mercs_hud_decode(&decoded,bytes,40) && !memcmp(&s,&decoded,sizeof s));
    }
    assert(mercs_hud_decode(&decoded,bytes,36)<0);
    bytes[0]=14;assert(mercs_hud_decode(&decoded,bytes,40)<0);
    assert(!mercs_hud_command(&s,0xff));
    for(i=0;i<COUNT(original_novice);i++) {
        int reverse=original_novice[i].reverse;
        float servo=222.0f;
        int changed=im_novice_drive(original_novice[i].forward,original_novice[i].backward,&reverse,&servo);
        assert(reverse==original_novice[i].after_reverse);
        assert(changed==(original_novice[i].forward || original_novice[i].backward));
        assert(changed ? servo==1 && original_novice[i].servo==2048 : servo==222);
    }
    for(i=0;i<COUNT(original_novice_boost);i++)
        assert(im_boost_speed_q16(original_novice_boost[i].before,original_novice_boost[i].masc,
                   original_novice_boost[i].novice,original_novice_boost[i].player)==original_novice_boost[i].after);
    for(i=0;i<COUNT(original_novice_aim);i++)
        assert(im_novice_tilt_q16(original_novice_aim[i].before,original_novice_aim[i].player,
                   original_novice_aim[i].novice,(unsigned)original_novice_aim[i].flags,
                   original_novice_aim[i].elevation)==original_novice_aim[i].after);
    for(i=0;i<COUNT(original_target_bearings);i++) {
        int32_t a,h,e;
        mercs_target_bearing(original_target_bearings[i].point,&a,&h,&e);
        assert((uint32_t)a==original_target_bearings[i].heading);
        assert((uint32_t)h==original_target_bearings[i].horizontal);
        assert((uint32_t)e==original_target_bearings[i].elevation);
    }
    {
        inputmap m;
        FILE *f=tmpfile();
        bool keys[SDL_SCANCODE_COUNT]={0};
        int mods=0;
        assert(f); memset(&m,0,sizeof m);
        fputs("novice_forward {\n+ keyboard GreyUpArrow\n}\nnovice_backward {\n+ keyboard GreyDownArrow\n}\n",f);
        rewind(f);im_parse_input(&m,f);fclose(f);
        assert(im_has_control(&m,"novice_forward") && im_has_control(&m,"novice_backward"));
        assert(!im_has_control(&m,"throttle"));
        keys[SDL_SCANCODE_UP]=1;assert(im_held(&m,"novice_forward",keys));
        keys[SDL_SCANCODE_DOWN]=1;assert(im_held(&m,"novice_backward",keys));
        assert(im_keytoken("POUND",&mods)== '#');
    }
    printf("PASS: %zu original HUD commands, %zu DOS/Windows-width HUD files, %zu novice drive, %zu speed, %zu aim and %zu target-bearing cases; live bindings and malformed HUD rejection\n",
           COUNT(original_hud_commands),COUNT(original_hud_files),COUNT(original_novice),
           COUNT(original_novice_boost),COUNT(original_novice_aim),COUNT(original_target_bearings));
    return 0;
}
