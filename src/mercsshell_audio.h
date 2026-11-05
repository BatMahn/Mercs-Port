/* Original Mercenaries DOS shell DATABASE audio. See
 * tests/fixtures/MERCS_SHELL_AUDIO_AUDIT.md. No proprietary audio is embedded. */
#ifndef MERCS_SHELL_AUDIO_H
#define MERCS_SHELL_AUDIO_H

#include <stddef.h>
#include <stdint.h>
#include "shelldb.h"
#include "sfx.h"

typedef struct mercs_shell_audio mercs_shell_audio;

typedef enum {
    MSA_PAGE_NONE = -1,
    MSA_PAGE_MENU, MSA_PAGE_HALL, MSA_PAGE_REGISTER, MSA_PAGE_NEWS,
    MSA_PAGE_OFFICE, MSA_PAGE_COMPUTER, MSA_PAGE_CONTRACTS,
    MSA_PAGE_DROPSHIP, MSA_PAGE_COMMAND, MSA_PAGE_ROSTER,
    MSA_PAGE_BRIEFING, MSA_PAGE_SETTINGS, MSA_PAGE_DEBRIEF,
    MSA_PAGE_SAVE, MSA_PAGE_REPAIR,
    MSA_PAGE_FINANCE, MSA_PAGE_INVENTORY, MSA_PAGE_PERSONNEL, MSA_PAGE_TRADING,
    MSA_PAGE_MECHBAY, MSA_PAGE_MECHOPS, MSA_PAGE_DROP_FINANCE, MSA_PAGE_DROP_INVENTORY, MSA_PAGE_FACTORY,
    MSA_PAGE_GROUPS, MSA_PAGE_MECHLAB, MSA_PAGE_INSTANT, MSA_PAGE_INSTANT_PARAMS,
    MSA_PAGE_CREW, MSA_PAGE_CREW_BIO
} mercs_shell_audio_page;

typedef enum {
    MSA_CUE_MENU_COMMANDER, MSA_CUE_MENU_MERCENARY,
    MSA_CUE_MENU_MERCNET, MSA_CUE_MENU_INSTANT, MSA_CUE_MENU_OPTIONS,
    MSA_CUE_REGISTER_ACCEPT,
    MSA_CUE_HALL_TERMINAL, MSA_CUE_HALL_OFFICE, MSA_CUE_HALL_BOARD,
    MSA_CUE_HALL_UNREGISTERED, MSA_CUE_HALL_NEED_CONTRACT, MSA_CUE_HALL_NO_MECH,
    MSA_CUE_NEWS_EXIT,
    MSA_CUE_OFFICE_COMPUTER, MSA_CUE_OFFICE_STARPORT, MSA_CUE_OFFICE_MECHBAY,
    MSA_CUE_COMPUTER_SELECT, MSA_CUE_COMPUTER_EXIT,
    MSA_CUE_CONTRACT_SELECT, MSA_CUE_CONTRACT_ACCEPT, MSA_CUE_CONTRACT_EXIT,
    MSA_CUE_CABIN_COMMAND,
    MSA_CUE_COMMAND_SELECT, MSA_CUE_COMMAND_EXIT,
    MSA_CUE_ROSTER_SELECT, MSA_CUE_ROSTER_EXIT, MSA_CUE_ROSTER_ROW,
    MSA_CUE_BRIEFING_LAUNCH, MSA_CUE_BRIEFING_EXIT,
    MSA_CUE_DEBRIEF_SELECT,
    MSA_CUE_SAVE_SELECT, MSA_CUE_SAVE_LOAD, MSA_CUE_SAVE_WRITE, MSA_CUE_SAVE_EXIT,
    MSA_CUE_REPAIR_SELECT, MSA_CUE_REPAIR_CHANGE, MSA_CUE_REPAIR_EXIT,
    MSA_CUE_REPAIR_ROW, MSA_CUE_REPAIR_APPLY,
    MSA_CUE_ROSTER_REJECT,
    MSA_CUE_LEDGER_EXIT, MSA_CUE_PERSONNEL_SELECT, MSA_CUE_PERSONNEL_EXIT,
    MSA_CUE_INVENTORY_CATEGORY, MSA_CUE_INVENTORY_MECH, MSA_CUE_INVENTORY_EXIT,
    MSA_CUE_TRADING_SELECT, MSA_CUE_TRADING_EXIT,
    MSA_CUE_MECHBAY_OPS, MSA_CUE_MECHBAY_OFFICE,
    MSA_CUE_MECHOPS_SELECT, MSA_CUE_MECHOPS_EXIT,
    MSA_CUE_FACTORY_SELECT, MSA_CUE_FACTORY_MECH, MSA_CUE_FACTORY_EXIT,
    MSA_CUE_GROUPS_SELECT, MSA_CUE_GROUPS_DEFAULT, MSA_CUE_GROUPS_EXIT,
    MSA_CUE_MECHLAB_SELECT, MSA_CUE_MECHLAB_ROW, MSA_CUE_MECHLAB_CHANGE,
    MSA_CUE_MECHLAB_APPLY, MSA_CUE_MECHLAB_EXIT,
    MSA_CUE_INSTANT_SELECT, MSA_CUE_INSTANT_EXIT, MSA_CUE_INSTANT_PARAMS,
    MSA_CUE_INSTANT_PARAM_SELECT, MSA_CUE_INSTANT_PARAM_EXIT,
    MSA_CUE_CREW_SELECT, MSA_CUE_CREW_BIO_EXIT,
    MSA_CUE_COUNT
} mercs_shell_audio_cue;

/* Resources are the ORIGINAL ONE-BASED database IDs. frames is per channel.
 * Decode accepts original PCM WAV8/WAV16, mono/stereo; samples are native S16.
 * Decode owns only *out->samples. Call mercs_shell_wave_free() after use. */
typedef struct {
    int16_t *samples;
    int frames, channels, rate, original_bits;
} mercs_shell_wave;
int mercs_shell_wave_decode(const uint8_t *data, size_t size, mercs_shell_wave *out);
void mercs_shell_wave_free(mercs_shell_wave *wave);

/* Borrow the existing mixer and DATABASE. Destroy this object BEFORE either.
 * This module never opens/closes/pauses SDL devices or takes the music callback. */
mercs_shell_audio *mercs_shell_audio_create(sfx *mixer, const shelldb *database);
void mercs_shell_audio_destroy(mercs_shell_audio *audio);

/* Enter destroys the old page's sounds as the original page destructor does.
 * registered is original career byte+1. previous chooses original door entry
 * cues (hall from office, office from hall/Mech Bay). No typing beep is added. */
void mercs_shell_audio_enter(mercs_shell_audio *audio, mercs_shell_audio_page page,
                            int registered, mercs_shell_audio_page previous);
void mercs_shell_audio_leave(mercs_shell_audio *audio);

/* The original 0x6c614 caller waited until completion. trigger() starts the
 * exact cue and returns nonzero while such a wait is required. Keep pumping
 * SDL and drawing the current page; defer its action/transition until busy()==0.
 * Nonblocking door sounds finish during the existing door animation. A disabled
 * or unavailable audio device never makes a transition wait indefinitely. */
int mercs_shell_audio_trigger(mercs_shell_audio *audio, mercs_shell_audio_cue cue);
void mercs_shell_audio_tick(mercs_shell_audio *audio);
int mercs_shell_audio_busy(mercs_shell_audio *audio);

/* Around full FMV or the simulation child: suspend before global mixer pause,
 * resume after it is unpaused. Only this module's sounds stop; resume restores
 * the current room loop, never repeats entry speech or duplicates music. */
void mercs_shell_audio_suspend(mercs_shell_audio *audio);
void mercs_shell_audio_resume(mercs_shell_audio *audio);

/* Original text display loops resource49 at volume10 only WHILE glyphs are
 * being revealed. Stop on completion/skip/page leave. Static text is silent. */
void mercs_shell_audio_text_reveal(mercs_shell_audio *audio, int active);

/* Original hall/registration/NEWS idle table and Watcom LCG, with a distinct
 * native UI seed. Seed from elapsed UI ticks at career/menu selection; this is
 * not the campaign RNG and does not alter salvage. Original NEWS suppresses
 * idle sounds while its MLETTER object exists, including before it starts. */
void mercs_shell_audio_seed(mercs_shell_audio *audio, uint32_t seed);
void mercs_shell_audio_idle_enabled(mercs_shell_audio *audio, int enabled);

/* Read-only metadata/debug checks. Loading never starts a sound. */
const mercs_shell_wave *mercs_shell_audio_wave(mercs_shell_audio *audio, int resource);
int mercs_shell_audio_resource_playing(mercs_shell_audio *audio, int resource);

#endif
