#ifndef MERCS_INSTANT_H
#define MERCS_INSTANT_H

#include "mercsrmg.h"
#include "mek.h"

/* Original DOS Instant Action0x5f02c/0x5fdc8 and mission parameters0x61150.
 * Six physical slots: player, two mates, then three enemies. These are never
 * references into the player's owned career inventory. */
enum { MERCS_INSTANT_SLOTS=6,MERCS_INSTANT_VARIANTS=100,MERCS_INSTANT_CHASSIS=62 };
typedef struct {
    int chassis;                 /* friendly:-1 none; enemy:-2 random,-1 none */
    char loadout[9];
    uint8_t custom[MEK_MAX_SERIALIZED_BYTES];
    size_t custom_size;
    int variant_active,variant_index,variant_count;
    char variants[MERCS_INSTANT_VARIANTS][9];
} mercs_instant_slot;
typedef struct {
    mercs_rmg_config parameters,request;
    mercs_rmg_bundle world;
    mercs_instant_slot slots[MERCS_INSTANT_SLOTS];
    int selected,restart,ready;
} mercs_instant;
typedef struct {
    char token[33],edition[12];
    mercs_rmg_config request;
    int difficulty;              /* original MW2DIF+5, separate from RMG skill */
    struct {
        int chassis;
        char loadout[9];
        size_t size;
        uint8_t bytes[MEK_MAX_SERIALIZED_BYTES];
    } slots[MERCS_INSTANT_SLOTS];
} mercs_instant_launch;

void mercs_instant_init(mercs_instant *state);
void mercs_instant_free(mercs_instant *state);
const char *mercs_instant_chassis_name(int chassis);
const char *mercs_instant_prefix(int chassis);
const char *mercs_instant_planet(int planet);
int mercs_instant_planet_index(const char *name);
int mercs_instant_cycle_chassis(int slot,int chassis,int step);
void mercs_instant_cycle_variant(int *active,int count,int *index,int step);
void mercs_instant_choose(mercs_instant *state,int slot,int step);
int mercs_instant_generate(prj_archive *archive,mercs_instant *state,uint32_t seed,
                           char *error,size_t error_size);
int mercs_instant_variant(mercs_instant *state,int step,const char *overlay,
                          char *error,size_t error_size);
int mercs_instant_loadout(prj_archive *archive,const mercs_instant *state,int slot,
                          uint8_t *bytes,size_t capacity,size_t *size);
/* Save an accepted MechLab design as an original PREFIXnnUSR.MEK. Existing
 * designs and career PREFIXnnMER.MEK files are never replaced. */
int mercs_instant_design(mercs_instant *state,const uint8_t *bytes,size_t size,
                        const char *overlay,char *error,size_t error_size);
int mercs_instant_prepare(const mercs_instant *state,const char *edition,int difficulty,
                         mercs_instant_launch *launch,char *error,size_t error_size);
/* Generate and atomically install ephemeral BWD/MEK records in this archive.
 * No loose USERSTAR, owned loadout, campaign, or return file is changed. */
int mercs_instant_install(prj_archive *archive,const mercs_instant_launch *launch,
                         char *error,size_t error_size);
/* Native process transport with exact lengths, checksum, edition and token.
 * Call begin while holding the selected edition's launch lock. */
int mercs_instant_begin(const char *overlay,mercs_instant_launch *launch,
                       char *error,size_t error_size);
int mercs_instant_read(const char *overlay,const char *token,const char *edition,
                      mercs_instant_launch *launch,char *error,size_t error_size);
#endif
