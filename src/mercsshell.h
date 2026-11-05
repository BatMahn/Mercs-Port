#ifndef MERCS_SHELL_H
#define MERCS_SHELL_H

#include "portcfg.h"

/* Normal application entry after game-folder selection. The shell owns its
 * window and uses the registered pilot's authored campaign to launch missions.
 * Developer command-line entry points remain separate in tools/mercs.c. */
int mercs_shell_run(const char *exe, portcfg *cfg, int edition);

#endif
