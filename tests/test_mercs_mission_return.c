#include "mercsmissionreturn.h"
#include "mercsshell_data.h"
#include "mercsbridge.h"
#include "datapath.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../tests/fixtures/result-audit/mission_return_payload_fixtures.h"
#include "../tests/fixtures/result-audit/mission_return_selection_fixtures.h"

/* Explicit native materialization of declared original INPUT. These five
 * fixture controllers have two type39 mounts sharing one contiguous pool.
 * Output bytes come from executing both originals, never from this setup. */
static void native_fixture(const mercs_return_state *in, combat_unit *unit)
{
    int i,l,k;
    assert(in->weapon_count==2 && in->bin_count==2 && in->pool_count==1);
    memset(unit,0,sizeof *unit);
    unit->return_valid=1; unit->tons=in->tonnage;
    unit->return_walk_speed=in->walk_speed_raw;unit->jets=in->jump_mp;
    unit->armor_mult=combat_armor_mult(in->player,in->alliance,in->difficulty);
    memcpy(unit->return_config,in->config,sizeof unit->return_config);
    unit->weapon_count=2;unit->bin_count=2;unit->ammo_pool_count=1;
    unit->ammo_pools[0].weapon=39;unit->ammo_pools[0].rounds=in->pools[0].rounds;
    unit->ammo_pools[0].bin_n=2;unit->ammo_pools[0].bin_cur=in->pools[0].current_bin;
    for (l=0;l<8;l++) {
        unit->armor[l]=(float)in->locations[l].armor_q16/65536.0f;
        unit->rear[l]=(float)in->locations[l].rear_q16/65536.0f;
        unit->internal[l]=(float)in->locations[l].internal_q16/65536.0f;
        unit->slot_count[l]=in->locations[l].slot_count;
        unit->return_flags[l]=in->locations[l].flags;
        for (k=0;k<12;k++) unit->slots[l][k]=in->locations[l].slots[k];
    }
    for (i=0;i<2;i++) {
        combat_weapon *weapon=&unit->weapons[i];
        assert(in->mounts[i].weapon==39 && in->pools[0].bin_indices[i]==i);
        weapon->weapon=39;weapon->return_group=in->mounts[i].group;
        weapon->return_location=in->mounts[i].location;weapon->item=in->mounts[i].item;
        weapon->destroyed=in->mounts[i].destroyed;
        weapon->ammo=in->pools[0].rounds;weapon->bin_n=2;weapon->bin_cur=in->pools[0].current_bin;
        weapon->state=weapon->ammo ? CW_READY : -1;
        unit->return_bin_order[i]=i;unit->bins[i].weapon=39;
        unit->bins[i].id=(uint32_t)(int32_t)in->bins[i].id;unit->bins[i].rounds=in->bins[i].rounds;
    }
}

typedef struct { const char *expected_path; int exists, calls, fail_at; } file_guard;
static int existing(void *user,const char *path)
{
    file_guard *guard=user;
    guard->calls++;
    if (guard->expected_path) assert(!strcmp(path,guard->expected_path));
    if (guard->fail_at && guard->calls==guard->fail_at) return -1;
    return guard->exists;
}

static void explicit_results(mercs_car_record *career,mercs_msn_star stars[2])
{
    int i;
    for (i=0;i<MERCS_CAR_BYTES;i++) career->bytes[i]=(uint8_t)(17+i*31);
    memset(stars,0,2*sizeof *stars);
    stars[0].outcome=2;stars[0].resolved_seconds=999;
    stars[1].started_seconds=17;stars[1].resolved_seconds=-1;stars[1].outcome=0;
    stars[1].node_count=2;
    stars[1].nodes[0].visible=1;stars[1].nodes[0].runtime_state=5;
    stars[1].nodes[0].priority=8;stars[1].nodes[0].started_seconds=23;
    stars[1].nodes[0].resolved_seconds=29;stars[1].nodes[0].objective_value=0x12345678;
    strcpy(stars[1].nodes[0].text,"Explicit selected star");
    strcpy(stars[1].nodes[1].text,"Hidden row");
}

static void check_results(const mercs_mission_return_bundle *bundle,const mercs_car_record *career)
{
    mercs_msn_record mission;
    assert(bundle->career_size==MERCS_CAR_BYTES && !memcmp(bundle->career,career->bytes,MERCS_CAR_BYTES));
    assert(bundle->mission_size==MERCS_MSN_BYTES);
    assert(mercs_msn_parse(bundle->mission,bundle->mission_size,&mission)==MERCS_RESULT_OK);
    assert(mission.row_count==1 && mission.started_seconds==17 && mission.resolved_seconds==-1 && mission.outcome==0);
    assert(mission.rows[0].succeeded==1 && mission.rows[0].priority==8);
    assert(mission.rows[0].started_seconds==23 && mission.rows[0].resolved_seconds==29);
    assert(mission.rows[0].objective_value==0x12345678);
    assert(!strcmp((const char *)mission.rows[0].text,"Explicit selected star"));
}

static void compare_mek(const mercs_mission_return_mek *mek,const mercs_return_original_fixture *golden)
{
    int l,k;
    assert(mek->size==golden->expected_size && !memcmp(mek->bytes,golden->expected,mek->size));
    for (l=0;l<8;l++) for (k=0;k<12;k++) assert(mek->slots_after[l][k]==golden->post_slots[l][k]);
}

static void original_selection_cases(void)
{
    size_t i,checked=0;
    for (i=0;i<sizeof mission_return_selection_fixtures/sizeof mission_return_selection_fixtures[0];i++) {
        const mission_return_selection_fixture *f=&mission_return_selection_fixtures[i];
        const mercs_return_original_fixture *golden=&mercs_return_original_fixtures[
            f->difficulty==0 ? 4 : f->player ? 0 : 2];
        mercs_car_record career;
        mercs_msn_star stars[2];
        mercs_mission_return_binding bindings[2]={{0}};
        mercs_mission_return_context context={0};
        mercs_mission_return_bundle bundle={0};
        combat_unit actor,before;
        int armed=1,target=f->player ? 0 : 1;
        msim *sim=calloc(1,sizeof *sim);
        file_guard guard={f->path,f->exists,0,0};
        assert(sim);
        if (f->write_fails) { free(sim); continue; } /* file commit belongs to the outer transaction */
        native_fixture(&mercs_return_original_fixtures[0].input,&sim->player_unit);
        native_fixture(&mercs_return_original_fixtures[f->difficulty==0 ? 4 : 2].input,&actor);
        sim->actor_count=!f->player;sim->units=&actor;sim->armed=&armed;sim->difficulty=f->difficulty;
        sim->outcome=2;sim->now=123456; /* must not replace explicit selected-star fields */
        bindings[0].actor_index=-1;bindings[0].controller_state=2;bindings[0].controller_state_known=1;
        bindings[1].actor_index=0;
        bindings[target].controller_flags=(uint16_t)f->flags;
        bindings[target].controller_state=(uint8_t)f->state;
        bindings[target].controller_state_known=1;bindings[target].alliance=f->alliance;
        strcpy(bindings[target].controller_name,f->name);
        if (f->player) {
            sim->player_unit.destroyed=f->state==4 || f->state==5;
            sim->player_unit.intact_out=f->state==5;
            sim->player_unit.ejected=f->state==5;
            before=sim->player_unit;
        } else { actor.destroyed=f->state==4 || f->state==5;actor.intact_out=f->state==5;before=actor; }
        explicit_results(&career,stars);
        context.owned_returns_enabled=f->enabled;context.bindings=bindings;
        context.binding_count=(size_t)target+1;context.complete_unit_order=1;
        context.existing_file=existing;context.existing_user=&guard;
        context.career=&career;context.stars=stars;context.star_count=2;context.selected_star=1;
        assert(mercs_mission_return_build(sim,&context,&bundle)==MERCS_MISSION_RETURN_OK);
        assert(bundle.mech_count==(size_t)f->owned_count);
        assert(bundle.salvage_complete && bundle.salvage_count==(size_t)f->salvage_count);
        if (bundle.salvage_count) assert(bundle.salvage_indices[0]==(size_t)target);
        assert(guard.calls==(f->enabled && (f->flags&2) ? 1 : 0));
        assert(!memcmp(f->player ? &sim->player_unit : &actor,&before,sizeof before));
        check_results(&bundle,&career);
        if (bundle.mech_count) {
            assert(bundle.mechs[0].actor_index==(f->player ? -1 : 0));
            assert(bundle.mechs[0].original_unit_index==(size_t)target);
            assert(!strcmp(bundle.mechs[0].path,f->path));compare_mek(&bundle.mechs[0],golden);
        }
        mercs_mission_return_free(&bundle);free(sim);checked++;
    }
    printf("PASS: %zu original DOS/Windows eligibility, identity, filename, state and salvage-selection cases; exact staged MEK bytes\n",checked);
}

static void deployment_identity_cases(void)
{
    msim *s=calloc(1,sizeof *s);
    world_actor actors[5];
    assert(s);memset(actors,0,sizeof actors);
    s->actors=actors;s->actor_count=5;s->player_table=7;
    /* Native encounter order interleaves an enemy, another friendly star,
     * and USERSTAR slots. None of those array indices equals its STAR slot. */
    strcpy(actors[0].group,"ENEMY");actors[0].table=1;actors[0].gps_slot=1;
    strcpy(actors[1].group,"USERSTAR");actors[1].table=7;actors[1].gps_slot=2;
    strcpy(actors[2].group,"OTHER");actors[2].table=7;actors[2].gps_slot=1;actors[2].friendly=1;
    strcpy(actors[3].group,"USERSTAR");actors[3].table=7;actors[3].gps_slot=1;
    strcpy(actors[4].group,"USERSTAR");actors[4].table=0;actors[4].gps_slot=1;
    assert(mercs_bridge_resolve_actor(s,-1)==-1);
    assert(mercs_bridge_resolve_actor(s,-2)==3);
    assert(mercs_bridge_resolve_actor(s,-3)==1);
    assert(mercs_bridge_resolve_actor(s,-4)==-2);
    assert(mercs_bridge_resolve_actor(s,5)==-2);
    actors[3].gps_slot=3;assert(mercs_bridge_resolve_actor(s,-2)==-2);
    actors[3].gps_slot=1;actors[4].table=7;
    assert(mercs_bridge_resolve_actor(s,-2)==-2); /* duplicate explicit identity */
    free(s);
}

static void salvage_order_cases(void)
{
    int player, i;
    msim *sim=calloc(1,sizeof *sim);
    mercs_car_record career;
    mercs_msn_star stars[2];
    mercs_mission_return_binding bindings[102];
    mercs_mission_return_context context={0};
    mercs_mission_return_bundle bundle={0};
    assert(sim);sim->actor_count=101;
    explicit_results(&career,stars);
    context.bindings=bindings;context.binding_count=102;context.complete_unit_order=1;
    context.career=&career;context.stars=stars;context.star_count=2;context.selected_star=1;
    for (player=0;player<=100;player+=100) {
        memset(bindings,0,sizeof bindings);
        for (i=0;i<102;i++) {
            bindings[i].actor_index=i==player ? -1 : i<player ? i : i-1;
            bindings[i].controller_state=4;bindings[i].controller_state_known=1;
        }
        assert(mercs_mission_return_build(sim,&context,&bundle)==MERCS_MISSION_RETURN_OK);
        assert(bundle.salvage_complete && bundle.salvage_count==(size_t)(player ? 100 : 99));
        for (i=0;i<(int)bundle.salvage_count;i++) assert(bundle.salvage_indices[i]==(size_t)(player ? i : i+1));
        mercs_mission_return_free(&bundle);
    }
    bindings[0].controller_state_known=0;
    assert(mercs_mission_return_build(sim,&context,&bundle)==MERCS_MISSION_RETURN_OK);
    assert(!bundle.salvage_complete && bundle.salvage_count==99);
    mercs_mission_return_free(&bundle);
    context.complete_unit_order=0;context.binding_count=1;bindings[0].actor_index=-1;
    assert(mercs_mission_return_build(sim,&context,&bundle)==MERCS_MISSION_RETURN_OK);
    assert(!bundle.salvage_complete && bundle.salvage_count==0);
    mercs_mission_return_free(&bundle);free(sim);
    puts("PASS: original first100-index salvage bound, nonzero player index, and explicit incomplete/subset context");
}

static void transaction_cases(void)
{
    msim *sim=calloc(1,sizeof *sim);
    combat_unit actor,before_player,before_actor;
    int armed=1;
    mercs_car_record career;
    mercs_msn_star stars[2];
    mercs_mission_return_binding bindings[2]={{0}};
    mercs_mission_return_context context={0};
    mercs_mission_return_bundle bundle={0},untouched={0};
    file_guard guard={NULL,1,0,0};
    assert(sim);native_fixture(&mercs_return_original_fixtures[1].input,&sim->player_unit);
    native_fixture(&mercs_return_original_fixtures[3].input,&actor);
    before_player=sim->player_unit;before_actor=actor;
    sim->actor_count=1;sim->units=&actor;sim->armed=&armed;sim->difficulty=1;
    bindings[0].actor_index=-1;bindings[1].actor_index=0;
    bindings[0].controller_flags=bindings[1].controller_flags=2;
    strcpy(bindings[0].controller_name,"PLY00MER");strcpy(bindings[1].controller_name,"AI100MER");
    explicit_results(&career,stars);
    context.owned_returns_enabled=1;context.bindings=bindings;context.binding_count=2;
    context.complete_unit_order=1;context.existing_file=existing;context.existing_user=&guard;
    context.career=&career;context.stars=stars;context.star_count=2;context.selected_star=1;
    assert(mercs_mission_return_build(sim,&context,&bundle)==MERCS_MISSION_RETURN_OK && bundle.mech_count==2);
    compare_mek(&bundle.mechs[0],&mercs_return_original_fixtures[1]);
    compare_mek(&bundle.mechs[1],&mercs_return_original_fixtures[3]);
    assert(!memcmp(&sim->player_unit,&before_player,sizeof before_player) && !memcmp(&actor,&before_actor,sizeof actor));
    actor.slots[0][0]^=1;
    assert(mercs_mission_return_commit_slots(sim,&bundle)==MERCS_MISSION_RETURN_STALE);
    assert(!memcmp(&sim->player_unit,&before_player,sizeof before_player));
    actor=before_actor;
    assert(mercs_mission_return_commit_slots(sim,&bundle)==MERCS_MISSION_RETURN_OK);
    assert(!memcmp(sim->player_unit.slots,bundle.mechs[0].slots_after,sizeof sim->player_unit.slots));
    assert(!memcmp(actor.slots,bundle.mechs[1].slots_after,sizeof actor.slots));
    mercs_mission_return_free(&bundle);
    sim->player_unit=before_player;actor=before_actor;actor.armor[0]=NAN;
    assert(mercs_mission_return_build(sim,&context,&bundle)==MERCS_MISSION_RETURN_CAPTURE_ERROR);
    assert(!memcmp(&bundle,&untouched,sizeof bundle) && !memcmp(&sim->player_unit,&before_player,sizeof before_player));
    actor=before_actor;context.selected_star=2;
    assert(mercs_mission_return_build(sim,&context,&bundle)==MERCS_MISSION_RETURN_INVALID);
    assert(!memcmp(&bundle,&untouched,sizeof bundle) && !memcmp(&actor,&before_actor,sizeof actor));
    context.selected_star=1;guard.calls=0;guard.fail_at=2;
    assert(mercs_mission_return_build(sim,&context,&bundle)==MERCS_MISSION_RETURN_EXISTING_ERROR);
    assert(!memcmp(&bundle,&untouched,sizeof bundle) && !memcmp(&sim->player_unit,&before_player,sizeof before_player));
    guard.fail_at=0;strcpy(bindings[1].controller_name,"../escape");
    assert(mercs_mission_return_build(sim,&context,&bundle)==MERCS_MISSION_RETURN_INVALID);
    strcpy(bindings[1].controller_name,"ply00mer");
    assert(mercs_mission_return_build(sim,&context,&bundle)==MERCS_MISSION_RETURN_INVALID);
    strcpy(bindings[1].controller_name,"AI100MER");bindings[1].actor_index=-1;
    assert(mercs_mission_return_build(sim,&context,&bundle)==MERCS_MISSION_RETURN_INVALID);
    bindings[1].actor_index=0;
    assert(!memcmp(&sim->player_unit,&before_player,sizeof before_player) && !memcmp(&actor,&before_actor,sizeof actor));
    context.complete_unit_order=0;context.binding_count=1;
    assert(mercs_mission_return_build(sim,&context,&bundle)==MERCS_MISSION_RETURN_OK);
    assert(bundle.mech_count==1 && bundle.mechs[0].original_unit_index==SIZE_MAX && !bundle.salvage_complete);
    mercs_mission_return_free(&bundle);free(sim);
    puts("PASS: staged depleted-slot edits, all-or-none commit, failed later capture/result/guard, path and identity rejection");
}

typedef struct { const char *root; int calls; } overlay_guard;
static int existing_overlay(void *user,const char *path)
{
    overlay_guard *guard=user;
    char resolved[1024];FILE *f;
    guard->calls++;
    if (dp_resolve_in(guard->root,path,resolved,sizeof resolved)) return 0;
    f=fopen(resolved,"rb");if (!f) return 0;
    fclose(f);return 1;
}

static void authored_launch_case(const char *archive_path)
{
    char root[]="/tmp/mercs-mission-return-XXXXXX",error[512],path[1024];
    mercs_shell_state *shell=calloc(1,sizeof *shell);
    msim *sim=calloc(1,sizeof *sim);
    prj_archive *archive;
    world_actor *actors=NULL;
    int count=0,i,launch_result;
    unsigned char *launched=NULL,*after=NULL;size_t launch_size=0,after_size=0;
    combat_unit before;
    mercs_car_record career;mercs_msn_star stars[2];
    mercs_mission_return_binding binding={0};mercs_mission_return_context context={0};
    mercs_mission_return_bundle bundle={0};mek_def returned;
    overlay_guard guard={root,0};
    assert(shell && sim && mkdtemp(root));
    archive=prj_open(archive_path,error,sizeof error);assert(archive);
    dp_clear_roots();assert(dp_add_root(root)==0);
    assert(mercs_shell_register(archive,shell,1,"Return Pilot","Return Unit",error,sizeof error)==0);
    launch_result=mercs_shell_prepare_launch(archive,shell,1,root,error,sizeof error);
    if (launch_result) fprintf(stderr,"Authentic shell launch failed: %s\n",error);
    assert(launch_result==0);
    assert(!strcmp(shell->starter_loadout,"PLY00MER"));
    assert(dp_read_file("MEK\\PLY00MER.MEK",&launched,&launch_size)==0);
    assert(world3d_actors(archive,shell->missions[1].scene,&actors,&count)==0);
    assert(msim_init(sim,archive,shell->missions[1].scene,actors,count)==0);
    assert(!strcmp(sim->player_loadout,"PLY00MER") && sim->player_unit.return_valid);
    /* Current native battle damage, with a destroyed arm. The surrounding
     * battle and selected-star inputs are declared, not a full playthrough. */
    sim->player_unit.armor[MEK_HEAD]=8.0f;sim->player_unit.internal[MEK_LA]=0;
    before=sim->player_unit;
    explicit_results(&career,stars);
    binding.actor_index=-1;binding.controller_flags=2;strcpy(binding.controller_name,shell->starter_loadout);
    context.owned_returns_enabled=1;context.bindings=&binding;context.binding_count=1;
    context.existing_file=existing_overlay;context.existing_user=&guard;
    context.career=&career;context.stars=stars;context.star_count=2;context.selected_star=1;
    assert(mercs_mission_return_build(sim,&context,&bundle)==MERCS_MISSION_RETURN_OK);
    assert(bundle.mech_count==1 && !strcmp(bundle.mechs[0].path,"mek\\PLY00MER.mek"));
    assert(!bundle.salvage_complete && !memcmp(&sim->player_unit,&before,sizeof before));
    assert(mek_parse(bundle.mechs[0].bytes,bundle.mechs[0].size,&returned)==0);
    assert(returned.loc[MEK_HEAD].armor==2 && returned.loc[MEK_LA].internal==0);
    assert(!returned.has_config_extension);check_results(&bundle,&career);
    assert(dp_read_file("MEK\\PLY00MER.MEK",&after,&after_size)==0);
    assert(launch_size==after_size && !memcmp(launched,after,launch_size));
    free(launched);free(after);mercs_mission_return_free(&bundle);
    assert(dp_resolve_in(root,"MEK\\PLY00MER.MEK",path,sizeof path)==0 && unlink(path)==0);
    assert(mercs_mission_return_build(sim,&context,&bundle)==MERCS_MISSION_RETURN_OK);
    assert(bundle.mech_count==0 && bundle.decisions[0]==MERCS_MISSION_UNIT_FILE_MISSING);
    assert(!memcmp(&sim->player_unit,&before,sizeof before));mercs_mission_return_free(&bundle);
    msim_free(sim);for(i=0;i<count;i++)mech3d_free(&actors[i].mech);free(actors);
    assert(dp_resolve_in(root,"USERSTAR.BWD",path,sizeof path)==0 && unlink(path)==0);
    snprintf(path,sizeof path,"%s/MEK",root);assert(rmdir(path)==0);
    assert(rmdir(root)==0);dp_set_write_root(NULL);dp_clear_roots();prj_close(archive);free(sim);free(shell);
    puts("PASS: actual Mercs shell launch -> owned PLY00MER -> native msim damage -> staged bundle; source transient unchanged and missing-file guard respected");
}

int main(int argc,char **argv)
{
    assert(argc==1 || argc==2);
    deployment_identity_cases();original_selection_cases();salvage_order_cases();transaction_cases();
    if (argc==2) authored_launch_case(argv[1]);
    return 0;
}
