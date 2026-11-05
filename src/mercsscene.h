#ifndef MERCS_SCENE_H
#define MERCS_SCENE_H

#include "bwd.h"

/* Validate the authored scene graph without consulting loose launch files.
 * Return 0 for one external USERSTAR and no embedded player, 1 for one
 * validated archive STD player and no USERSTAR, or -1 for an invalid graph.
 * On 1, optionally return that player; otherwise leave stock_player unchanged.
 * Fixed-player missions still deploy the owned roster as the original shell
 * does, but the simulator does not bind those unused files to the stock player. */
int mercs_shell_scene_player(prj_archive *archive,const char *scene,
                              bwd_gps *stock_player,char *error,size_t error_size);


#endif
