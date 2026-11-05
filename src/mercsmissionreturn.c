#include "mercsmissionreturn.h"

#include <stdlib.h>
#include <string.h>

static const combat_unit *native_unit(const msim *sim, int actor)
{
    if (actor == -1) return &sim->player_unit;
    if (actor < 0 || actor >= sim->actor_count || !sim->units || !sim->armed || !sim->armed[actor]) return NULL;
    return &sim->units[actor];
}

static int same_path(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        unsigned x=(unsigned char)*a, y=(unsigned char)*b;
        if (x>='a' && x<='z') x-=32;
        if (y>='a' && y<='z') y-=32;
        if (x!=y) return 0;
    }
    return *a==*b;
}

static int owned_path(const char name[MERCS_MISSION_RETURN_NAME],
                       char path[MERCS_MISSION_RETURN_PATH])
{
    const char *end=memchr(name,0,MERCS_MISSION_RETURN_NAME);
    size_t length, i;
    int dot=0;
    if (!end || end==name) return -1;
    length=(size_t)(end-name);
    /* The verified deployment subset uses controller basenames. Reject paths
     * and device/wildcard syntax instead of escaping the launch directory. */
    if (!strcmp(name,".") || !strcmp(name,"..")) return -1;
    for (i=0; i<length; i++) {
        unsigned ch=(unsigned char)name[i];
        if (ch<33 || ch>126 || strchr("\\/:*?\"<>|",(int)ch)) return -1;
        if (ch=='.') dot=1;
    }
    if (4+length+(dot ? 0u : 4u)+1 > MERCS_MISSION_RETURN_PATH) return -1;
    memcpy(path,"mek\\",4);
    memcpy(path+4,name,length);
    if (!dot) { memcpy(path+4+length,".mek",4); length+=4; }
    path[4+length]=0;
    return 0;
}

static int validate_mapping(const msim *sim, const mercs_mission_return_context *context)
{
    size_t i,j,total;
    if (sim->actor_count<0) return -1;
    total=(size_t)sim->actor_count+1;
    if (context->binding_count>MERCS_MISSION_RETURN_MAX_UNITS ||
        context->binding_count>total || (context->binding_count && !context->bindings) ||
        (context->complete_unit_order && context->binding_count!=total)) return -1;
    for (i=0; i<context->binding_count; i++) {
        int actor=context->bindings[i].actor_index;
        if (actor < -1 || actor >= sim->actor_count) return -1;
        for (j=0; j<i; j++) if (context->bindings[j].actor_index==actor) return -1;
    }
    return 0;
}

void mercs_mission_return_free(mercs_mission_return_bundle *bundle)
{
    if (!bundle) return;
    free(bundle->mechs);
    memset(bundle,0,sizeof *bundle);
}

int mercs_mission_return_build(const msim *sim,
                               const mercs_mission_return_context *context,
                               mercs_mission_return_bundle *out)
{
    mercs_mission_return_bundle next;
    size_t i,j;
    int failure=MERCS_MISSION_RETURN_INVALID;
    if (!sim || !context || !out || !context->career || !context->stars ||
        validate_mapping(sim,context)) return MERCS_MISSION_RETURN_INVALID;
    memset(&next,0,sizeof next);
    next.binding_count=context->binding_count;
    next.salvage_complete=context->complete_unit_order!=0;
    if (context->binding_count) {
        next.mechs=calloc(context->binding_count,sizeof *next.mechs);
        if (!next.mechs) return MERCS_MISSION_RETURN_NO_MEMORY;
    }
    /* The explicit complete CAR comes first, matching the reached exit tail.
     * No outcome/status or unknown214-byte tail is synthesized here. */
    if (mercs_car_encode(context->career,next.career,sizeof next.career,
                         &next.career_size)!=MERCS_RESULT_OK) goto fail;
    for (i=0; i<context->binding_count; i++) {
        const mercs_mission_return_binding *binding=&context->bindings[i];
        const combat_unit *live;
        combat_unit snapshot;
        mercs_mission_return_mek *mek;
        char path[MERCS_MISSION_RETURN_PATH];
        int existing;
        if (!context->owned_returns_enabled) {
            next.decisions[i]=MERCS_MISSION_UNIT_DISABLED;
            continue;
        }
        if (!(binding->controller_flags & 2)) {
            next.decisions[i]=MERCS_MISSION_UNIT_NOT_OWNED;
            continue;
        }
        if (owned_path(binding->controller_name,path) || !context->existing_file) goto fail;
        existing=context->existing_file(context->existing_user,path);
        if (existing<0) { failure=MERCS_MISSION_RETURN_EXISTING_ERROR; goto fail; }
        if (!existing) { next.decisions[i]=MERCS_MISSION_UNIT_FILE_MISSING; continue; }
        /* A duplicated case-insensitive destination cannot be represented as
         * distinct staged files. Reject the unsupported deployment rather
         * than silently changing the original sequential overwrite order. */
        for (j=0; j<next.mech_count; j++) if (same_path(path,next.mechs[j].path)) goto fail;
        live=native_unit(sim,binding->actor_index);
        if (!live) { failure=MERCS_MISSION_RETURN_CAPTURE_ERROR; goto fail; }
        snapshot=*live;
        mek=&next.mechs[next.mech_count];
        mek->original_unit_index=context->complete_unit_order ? i : SIZE_MAX;
        mek->actor_index=binding->actor_index;
        memcpy(mek->path,path,strlen(path)+1);
        memcpy(mek->slots_before,live->slots,sizeof mek->slots_before);
        if (mercs_return_unit_bytes(&snapshot,1,sim->difficulty,
                binding->actor_index==-1,binding->alliance,mek->bytes,sizeof mek->bytes,
                &mek->size)!=MERCS_RETURN_OK) {
            failure=MERCS_MISSION_RETURN_CAPTURE_ERROR;
            goto fail;
        }
        memcpy(mek->slots_after,snapshot.slots,sizeof mek->slots_after);
        next.decisions[i]=MERCS_MISSION_UNIT_STAGED;
        next.mech_count++;
    }
    /* DOS0x63c80: only the first100 ORIGINAL indices, exclude the player and
     * mask2, then use literal state&5. A partial deployment manifest does not
     * establish the omitted actors' flags or original array order. */
    if (context->complete_unit_order) {
        for (i=0; i<context->binding_count && i<MERCS_MISSION_RETURN_SALVAGE_LIMIT; i++) {
            const mercs_mission_return_binding *binding=&context->bindings[i];
            if (binding->actor_index==-1 || (binding->controller_flags&2)) continue;
            if (!binding->controller_state_known) { next.salvage_complete=0; continue; }
            if (binding->controller_state&5) next.salvage_indices[next.salvage_count++]=i;
        }
    }
    if (mercs_msn_encode_selected(context->stars,context->star_count,context->selected_star,
                                  next.mission,sizeof next.mission,&next.mission_size)!=MERCS_RESULT_OK) goto fail;
    *out=next;
    return MERCS_MISSION_RETURN_OK;
fail:
    mercs_mission_return_free(&next);
    return failure;
}

int mercs_mission_return_commit_slots(msim *sim,
                                      const mercs_mission_return_bundle *bundle)
{
    size_t i,j;
    if (!sim || !bundle || bundle->mech_count>MERCS_MISSION_RETURN_MAX_UNITS ||
        (bundle->mech_count && !bundle->mechs)) return MERCS_MISSION_RETURN_INVALID;
    for (i=0; i<bundle->mech_count; i++) {
        const mercs_mission_return_mek *mek=&bundle->mechs[i];
        const combat_unit *unit=native_unit(sim,mek->actor_index);
        if (!unit || memcmp(unit->slots,mek->slots_before,sizeof mek->slots_before))
            return MERCS_MISSION_RETURN_STALE;
        for (j=0; j<i; j++) if (bundle->mechs[j].actor_index==mek->actor_index)
            return MERCS_MISSION_RETURN_INVALID;
    }
    for (i=0; i<bundle->mech_count; i++) {
        const mercs_mission_return_mek *mek=&bundle->mechs[i];
        combat_unit *unit=mek->actor_index==-1 ? &sim->player_unit : &sim->units[mek->actor_index];
        memcpy(unit->slots,mek->slots_after,sizeof mek->slots_after);
    }
    return MERCS_MISSION_RETURN_OK;
}
