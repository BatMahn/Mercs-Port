#include "sdl3_test.h"
/* Fixture regression for the supplied retail Mercs GAMEKEY.MAP / INPUT.MAP.
 * This checks recognition and original command IDs, not frontend behavior. */
#include <math.h>
#include <stdlib.h>
#include "inputmap.h"

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); failures++; } } while (0)

static unsigned command(const inputmap *m, const char *name)
{
    int i;
    for (i = 0; i < m->nbinds; i++)
        if (!strcmp(m->binds[i].action, name)) return m->binds[i].command_id;
    return 0;
}

int main(int argc, char **argv)
{
    inputmap m, bad;
    char gk[2048], in[2048];
    FILE *f;
    bool ks[SDL_SCANCODE_COUNT] = {0};
    if (argc != 2) {
        fprintf(stderr, "usage: %s <retail MERCS directory>\n", argv[0]);
        return 2;
    }
    snprintf(gk, sizeof gk, "%s/GAMEKEY.MAP", argv[1]);
    snprintf(in, sizeof in, "%s/INPUT.MAP", argv[1]);
    CHECK(im_load(&m, gk, in) == 0);
    CHECK(m.have_gamekey && m.have_input);
    CHECK(m.nbinds == 98 && m.nrules == 61);   /* 97 explicit + Ctrl+Q; before GIDDI .STD additions */

    /* The former parser stopped after MAIN_MENU at line 11. Check new names,
     * nonsequential IDs, modifiers, and bindings near the end of the file. */
    CHECK(command(&m, "MAIN_MENU") == 0x4a);
    CHECK(command(&m, "STARMATE_MENU") == 0x59);
    CHECK(command(&m, "STARMATE_SEVEN_MENU") == 0x60);
    CHECK(command(&m, "AEROTECHSTRIKE") == 0x23);
    CHECK(command(&m, "MFD_CYCLE_A") == 0x03);
    CHECK(command(&m, "INFRARED") == 0xbf);
    CHECK(command(&m, "TOGGLE_AMS") == 0xc1);
    CHECK(command(&m, "ADD_WEAPON_TO_GROUP_4") == 0xc2);
    CHECK(command(&m, "ADD_WEAPON_TO_GROUP_5") == 0xc4);
    CHECK(command(&m, "FIRE_CYCLE_WEAPON_GROUP") == 0xc6);
    CHECK(command(&m, "FIRE_ALL_WEAPONS") == 0x63);
    CHECK(command(&m, "PAUSE_GAME") == 0x70);
    CHECK(command(&m, "EXIT_SIM") == 0x71);
    CHECK(im_action(&m, "EXIT_SIM", SDLK_Q, SDL_KMOD_RCTRL));
    CHECK(!im_action(&m, "EXIT_SIM", SDLK_Q, 0));
    CHECK(!im_action(&m, "EXIT_SIM", SDLK_Q, SDL_KMOD_RCTRL | SDL_KMOD_LSHIFT));
    CHECK(im_action(&m, "STARMATE_MENU", SDLK_B, 0));
    CHECK(im_action(&m, "STARMATE_MENU", SDLK_F1, SDL_KMOD_RCTRL));
    CHECK(!im_action(&m, "ALL_PT_MENU", SDLK_F1, SDL_KMOD_RCTRL));
    CHECK(im_action(&m, "STARMATE_SEVEN_MENU", SDLK_F8, SDL_KMOD_LCTRL));
    CHECK(im_action(&m, "NEXT_TARGET", SDLK_T, 0));
    CHECK(!im_action(&m, "NEXT_TARGET", SDLK_T, SDL_KMOD_LCTRL));
    CHECK(im_action(&m, "RESET_TARGETTING", SDLK_T, SDL_KMOD_LCTRL));
    CHECK(im_action(&m, "TOGGLE_AMS", SDLK_A, SDL_KMOD_LCTRL));
    CHECK(im_action(&m, "FIRE_CYCLE_WEAPON_GROUP", SDLK_P, 0));
    CHECK(im_action(&m, "FIRE_ALL_WEAPONS", SDLK_P, SDL_KMOD_LSHIFT));
    CHECK(im_action(&m, "PAUSE_GAME", SDLK_P, SDL_KMOD_LALT));
    CHECK(im_action(&m, "PAUSE_GAME", SDLK_PAUSE, 0));
    ks[SDL_SCANCODE_SEMICOLON] = 1;
    CHECK(im_action_held(&m, "FIRE_WEAPON_GROUP", ks));

    /* Recognizing Mercs names must not make truly unknown names acceptable
     * or silently turn separate commands into the inherited MW2 menu names. */
    memset(&bad, 0, sizeof bad);
    f = tmpfile();
    CHECK(f != NULL);
    if (f) {
        fputs("STARMATE_MENU CTRL+Q\nstarmate_menu ctrl+f1\nNOT_A_MERCS_COMMAND z\nPAUSE_GAME Pause\n", f);
        rewind(f);
        im_parse_gamekey(&bad, f);
        fclose(f);
        CHECK(bad.nbinds == 2 && command(&bad, "STARMATE_MENU") == 0x59);
        CHECK(im_action(&bad, "EXIT_SIM", SDLK_Q, SDL_KMOD_LCTRL));
        CHECK(!im_action(&bad, "STARMATE_MENU", SDLK_Q, SDL_KMOD_LCTRL));
        CHECK(!im_action(&bad, "PAUSE_GAME", SDLK_PAUSE, 0));
    }
    printf("Mercs input: %d bindings, %d control rules; %s\n",
           m.nbinds, m.nrules, failures ? "FAILED" : "passed");
    return failures != 0;
}
