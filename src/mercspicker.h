/* Native startup helpers. Choosing a folder is read-only: the caller validates
 * its edition/archive before creating an installation overlay or saving config. */
#ifndef MERCS_PICKER_H
#define MERCS_PICKER_H

#include <stddef.h>
#include "portcfg.h"

/* Absolute path to this executable, suitable for relaunching a mission child.
 * Returns 0 on success, -1 if it cannot be determined without truncation. */
int mercs_executable_path(const char *argv0, char *out, size_t n);

/* Native chooser, following the mature MW2 startup flow. Invalid selections
 * show an explanation and retry. 1 selected/resolved, 0 cancelled, -1 error or
 * unavailable native picker. cfg is unchanged unless a valid folder is chosen. */
int mercs_pick_game_folder(portcfg *cfg, const char *exe_dir);

/* Also reports to stderr for launches from a terminal. Returns SDL's result. */
int mercs_gui_error(const char *message);

#endif
