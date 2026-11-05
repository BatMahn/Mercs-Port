#include "mercsbridge.h"
#include "mercsmissionreturn.h"
#include "mercsscene.h"
#include "mek.h"
#include "datapath.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int user_design(const char *name)
{ return name && strlen(name) >= 8 && !strncasecmp(name + 5, "USR", 3); }

/* Manifest -2/-3 identify the shell's explicit USERSTAR deployment slots.
 * Their native actor indices depend on the complete mission include order.
 * Death does not erase this identity: defeated owned units must be returned. */
int mercs_bridge_resolve_actor(const msim *s, int key)
{
    int i, found=-1, slot;
    if(!s || s->actor_count<0 || (s->actor_count && !s->actors)) return -2;
    if (key >= -1) return key<s->actor_count ? key : -2;
    if (key < -3) return -2;
    slot = -key-1;
    for(i=0;i<s->actor_count;i++) {
        const world_actor *a=&s->actors[i];
        if(a->table!=s->player_table || a->gps_slot!=slot ||
           strcasecmp(a->group,"USERSTAR")) continue;
        if(found>=0) return -2; /* a duplicate identity cannot authorize a return */
        found=i;
    }
    return found>=0 ? found : -2;
}

static int readable_owned(void *user, const char *logical_path)
{
    char path[4096];
    FILE *f;
    /* This deliberately does not use dp_fopen's archive/disc fallback. DOS
     *0x63b50 opens the existing deployment file rb before it can return it. */
    if (dp_resolve_in((const char *)user, logical_path, path, sizeof path)) return 0;
    f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* The original shell deploys its owned roster even when the scene embeds a
 * stock player. DOS0x5f26d leaves that stock controller's owned flag clear,
 * and0x63b50 therefore never rewrites these external deployment files. Carry
 * their unchanged bytes through the existing selective import, which still
 * recalculates derived values and clears the deployed marker as the original
 * shell does. Do not substitute the borrowed vehicle's damage or equipment. */
static int retained_owned(const char *overlay, const mercs_exchange_unit *unit,
                            mercs_exchange_mek *out)
{
    char logical[40], path[4096];
    size_t i, length = strlen(unit->name), size;
    int extra, bad;
    FILE *f;
    mek_def loadout;
    if (!length || length > 8) return -1;
    for (i=0;i<length;i++) {
        unsigned char c=(unsigned char)unit->name[i];
        if (!((c>='A'&&c<='Z') || (c>='a'&&c<='z') ||
              (c>='0'&&c<='9') || c=='_' || c=='-')) return -1;
    }
    snprintf(logical,sizeof logical,"MEK\\%s.MEK",unit->name);
    if (dp_resolve_in(overlay,logical,path,sizeof path) || !(f=fopen(path,"rb"))) return -1;
    size=fread(out->bytes,1,sizeof out->bytes,f);
    extra=fgetc(f);bad=ferror(f);fclose(f);
    if (bad || extra!=EOF || mek_parse(out->bytes,size,&loadout)) return -1;
    out->actor_index=unit->actor_index;
    strcpy(out->name,unit->name);out->size=size;
    return 0;
}

int mercs_bridge_publish(msim *s, const char *overlay,
                          const char *token, const char *edition)
{
    mercs_exchange_launch launch;
    mercs_exchange_result *packet = NULL;
    mercs_mission_return_binding bindings[MERCS_EXCHANGE_UNITS];
    mercs_mission_return_context context;
    mercs_mission_return_bundle bundle = {0};
    mercs_car_record career;
    mercs_msn_star star;
    size_t i,j;
    int rc = -1, detail = 0, stock_scene = 0;
    const char *stage = "validate deployed unit bindings";
    if (!s || s->actor_count < 0 || (s->actor_count && !s->actors) || !edition) return -1;
    stage = "read launch manifest";
    if ((detail = mercs_exchange_load(overlay, token, &launch))) goto done;
    stage = "match mission and edition";
    if (strcasecmp(launch.scene, s->scene) || strcmp(launch.edition, edition)) goto done;
    stage = "capture career statistics";
    if ((detail = msim_capture_career(s, &career))) goto done;
    stage = "capture objective results";
    if ((detail = msim_capture_result_star(s, &star))) goto done;
    /* DOS0x5f267 clears the original global on any USR loadout, including an
     * actor outside our deployed subset. Until native loading retains that
     * global, do not claim this MER/STD-only bridge handles user designs. */
    stage = "validate original loadout return support";
    if (user_design(s->player_loadout)) goto done;
    for (i = 0; i < (size_t)s->actor_count; i++)
        if (user_design(s->actors[i].loadout)) goto done;
    /* An unused owned deployment cannot be bound to a different player by
     * name alone. Establish the exact authored stock-player scene first. */
    if (launch.unit_count && (strcasecmp(s->player_loadout,launch.units[0].name) ||
        (strlen(s->player_loadout)==8 && !strcasecmp(s->player_loadout+5,"STD")))) {
        bwd_gps authored;
        char error[192];
        stage = "validate authored stock player";
        if (mercs_shell_scene_player(s->arch,s->scene,&authored,error,sizeof error)!=1 ||
            strcasecmp(authored.loadout,s->player_loadout) ||
            strcasecmp(authored.skeleton,s->player_skel) || authored.table!=s->player_table ||
            launch.unit_count>3) goto done;
        stock_scene=1;
        for (i=0;i<launch.unit_count;i++) {
            const mercs_exchange_unit *u=&launch.units[i];
            if (u->actor_index!=-(int32_t)i-1 || !(u->controller_flags&2) || u->alliance!=0) goto done;
        }
    }
    stage = "validate deployed unit bindings";
    memset(bindings, 0, sizeof bindings);
    for (i = 0; !stock_scene && i < launch.unit_count; i++) {
        const mercs_exchange_unit *u = &launch.units[i];
        const char *loaded;
        int actor=mercs_bridge_resolve_actor(s,u->actor_index);
        if (actor == -1) loaded = s->player_loadout;
        else if (actor >= 0 && actor < s->actor_count &&
                 s->armed && s->armed[actor]) loaded = s->actors[actor].loadout;
        else goto done;
        if (strcasecmp(loaded, u->name)) goto done;
        for(j=0;j<i;j++) if(bindings[j].actor_index==actor) goto done;
        bindings[i].actor_index = actor;
        bindings[i].controller_flags = u->controller_flags;
        bindings[i].alliance = u->alliance;
        memcpy(bindings[i].controller_name, u->name, sizeof u->name);
    }
    memset(&context, 0, sizeof context);
    context.owned_returns_enabled = 1;
    context.bindings = bindings;
    context.binding_count = stock_scene ? 0 : launch.unit_count;
    context.existing_file = readable_owned;
    context.existing_user = (void *)overlay;
    context.career = &career;
    /* Capture already selected s->player_table. This one-element view does
     * not select table0 from the live mission or assume s->outcome is final. */
    context.stars = &star;
    context.star_count = 1;
    context.selected_star = 0;
    stage = "serialize owned mechs and mission results";
    if ((detail = mercs_mission_return_build(s, &context, &bundle))) goto done;
    stage = "bind serialized return files";
    if (bundle.mech_count > MERCS_EXCHANGE_UNITS) goto done;
    packet = calloc(1, sizeof *packet);
    if (!packet) goto done;
    memcpy(packet->career, bundle.career, sizeof packet->career);
    memcpy(packet->mission, bundle.mission, sizeof packet->mission);
    packet->mech_count = bundle.mech_count;
    for (i = 0; i < bundle.mech_count; i++) {
        const mercs_mission_return_mek *m = &bundle.mechs[i];
        for (j = 0; j < launch.unit_count && bindings[j].actor_index != m->actor_index; j++);
        if (j == launch.unit_count || m->size > sizeof packet->mechs[i].bytes) goto done;
        packet->mechs[i].actor_index = launch.units[j].actor_index;
        strcpy(packet->mechs[i].name, launch.units[j].name);
        packet->mechs[i].size = m->size;
        memcpy(packet->mechs[i].bytes, m->bytes, m->size);
    }
    if (stock_scene) {
        stage = "retain unused owned deployment files";
        for (i=0;i<launch.unit_count;i++)
            if (retained_owned(overlay,&launch.units[i],&packet->mechs[i])) goto done;
        packet->mech_count=launch.unit_count;
    }
    stage = "publish complete return packet";
    if ((detail = mercs_exchange_publish(overlay, &launch, packet))) goto done;
    stage = "finalize original critical-slot return edits";
    if ((detail = mercs_mission_return_commit_slots(s, &bundle))) goto done;
    rc = 0;
done:
    if (rc) fprintf(stderr, "mercs mission return: could not %s (code %d)\n", stage, detail);
    free(packet);
    mercs_mission_return_free(&bundle);
    return rc;
}
