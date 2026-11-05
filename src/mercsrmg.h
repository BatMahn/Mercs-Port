#ifndef MERCS_RMG_H
#define MERCS_RMG_H
#include "prj.h"

/* Native transcription of the retail DOS shell 0x201b0 generator. Static
 * names, planet/mech tables and briefing prose remain in the user's original
 * MW2SHELL.EXE. The executable is read as data and is never executed. */
typedef struct {
    uint32_t seed;
    int number, year, success_pay, failure_pay; /* year=-1: original Instant Action draw */
    int kind;                       /* 0 random, 1 attack, 2 defend, 3 patrol */
    int skill;                      /* original option byte: 0 defaults to 2 */
    char planet[17];                /* empty lets the original generator choose */
} mercs_rmg_config;

#define MERCS_RMG_MAX_FILES 32
/* An original rand()==RAND_MAX draw would index beyond an original table.
 * A new preview can choose another seed; a saved accepted seed must not change. */
#define MERCS_RMG_BAD_SEED (-2)
typedef struct {
    mercs_rmg_config config;
    int template_index, enemy_count, employer, opponent;
    char scene[17], companion[17], world[17];
    prj_record files[MERCS_RMG_MAX_FILES];
    size_t count;
    uint32_t writer_max_chunk;       /* original 0xaf2dc carries between output files */
} mercs_rmg_bundle;

int mercs_rmg_available(char *error,size_t error_size);
int mercs_rmg_generate(prj_archive *archive,const mercs_rmg_config *config,
                       mercs_rmg_bundle *out,char *error,size_t error_size);
void mercs_rmg_free(mercs_rmg_bundle *bundle);
int mercs_rmg_install(prj_archive *archive,const mercs_rmg_bundle *bundle);
/* Resolve campaign terms and original name[3] atoi numbering. The supplied
 * seed controls the entire graph and can be stored at SHELLMER +0x162. */
int mercs_rmg_campaign(prj_archive *archive,const char *campaign,int year,
                       uint32_t seed,mercs_rmg_bundle *out,char *error,size_t error_size);
#endif
