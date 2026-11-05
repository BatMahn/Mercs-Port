#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mercsshell_data.h"
#include "datapath.h"
#include "msim.h"

int main(int argc, char **argv)
{
    char error[512];
    prj_archive *a;
    mercs_shell_state *state = calloc(1, sizeof *state);
    msim *sim = calloc(1, sizeof *sim);
    int mission;
    assert(argc == 3 && state && sim);
    a = prj_open(argv[1], error, sizeof error); assert(a);
    dp_clear_roots(); assert(dp_add_root(argv[2]) == 0);
    assert(mercs_shell_register(a, state, 1, "Contract Pilot", "Test Unit", error, sizeof error) == 0);
    combat_set_weapon_timing(1);
    for (mission = 0; mission < 2; mission++) {
        world_actor *actors = NULL;
        int n, i;
        assert(mercs_shell_prepare_launch(a, state, mission, argv[2], error, sizeof error) == 0);
        assert(world3d_actors(a, state->missions[mission].scene, &actors, &n) == 0);
        assert(n == (mission ? 6 : 4));
        msim_init(sim, a, state->missions[mission].scene, actors, n);
        assert(!strcmp(sim->player_loadout, "PLY00MER") && !strcmp(sim->player_skel, "COMMANDO"));
        assert(sim->pl_mech_ok && sim->player_unit.weapon_count == 4);
        for (i = 0; i < 4; i++) assert(sim->player_unit.weapons[i].weapon == (i == 3 ? 39 : 56));
        assert(combat_ammo_total(&sim->player_unit) == 180);
        printf("%s: player %s/%s,4 weapons,180 missiles; %d actors\n",
               state->missions[mission].scene, sim->player_skel, sim->player_loadout, n);
        for (i = 0; i < n; i++) {
            assert(sim->armed[i] && sim->units[i].weapon_count > 0);
            printf("  %s: %d weapons, %d rounds\n", sim->units[i].loadout,
                   sim->units[i].weapon_count, combat_ammo_total(&sim->units[i]));
        }
        for (i = 0; i < 40; i++) msim_step(sim, 50);
        assert(!sim->player_lost && !sim->over);
        msim_free(sim);
        for (i = 0; i < n; i++) mech3d_free(&actors[i].mech);
        free(actors);
    }
    assert(mercs_shell_register(a, state, 0, "Training Pilot", "Academy", error, sizeof error) == 0);
    assert(state->assigned_index == -1 && !state->missions[0].launchable);
    for (mission = 0; mission < 3; mission++) {
        static const int expected_weapons[] = {4,5,2};
        static const int expected_ammo[] = {180,100,100};
        world_actor *actors = NULL;
        int n, i;
        assert(mercs_shell_assign(a, state, 0, mission, error, sizeof error) == 0);
        assert(mercs_shell_prepare_launch(a, state, 0, argv[2], error, sizeof error) == 0);
        assert(world3d_actors(a, state->missions[0].scene, &actors, &n) == 0 && n == 4);
        msim_init(sim, a, state->missions[0].scene, actors, n);
        assert(!strcmp(sim->player_loadout, state->training_mechs[mission].loadout));
        assert(!strcmp(sim->player_skel, state->training_mechs[mission].skeleton));
        assert(sim->pl_mech_ok && sim->player_unit.weapon_count == expected_weapons[mission]);
        assert(combat_ammo_total(&sim->player_unit) == expected_ammo[mission]);
        for (i = 0; i < n; i++) assert(sim->armed[i] && sim->units[i].weapon_count > 0);
        for (i = 0; i < 20; i++) msim_step(sim, 50);
        assert(!sim->player_lost && !sim->over);
        printf("NOVICE1 choice %d: %s/%s, %d weapons, %d rounds,4 armed actors,1s step\n", mission,
               sim->player_skel, sim->player_loadout, sim->player_unit.weapon_count,
               combat_ammo_total(&sim->player_unit));
        msim_free(sim);
        for (i = 0; i < n; i++) mech3d_free(&actors[i].mech);
        free(actors);
    }
    prj_close(a); free(sim); free(state);
    puts("PASS: both authored first contracts load every actor and step2s with registered Commando; no fallback player or silently unarmed actors");
    return 0;
}
