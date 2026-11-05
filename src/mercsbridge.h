#ifndef MERCS_BRIDGE_H
#define MERCS_BRIDGE_H

#include "msim.h"
#include "mercsexchange.h"

/* Resolve an explicit native transport identity. -1 is the player; -2/-3
 * bind USERSTAR deployment slots1/2 in the selected player's actual table.
 * Returns native actor index (or-1 player), -2 on missing/ambiguous input.
 * Death does not remove a deployed identity. Filename/armed validation is
 * performed by publish before any return packet is committed. */
int mercs_bridge_resolve_actor(const msim *simulation, int manifest_key);

/* Shared by the real mission frontend and the headless round-trip checks.
 * Builds only explicitly deployed owned units, publishes one complete packet,
 * then commits the serializer's critical-slot side effect. Returns0 on success;
 * any failure prevents shell import. The simulation must be frozen here. */
int mercs_bridge_publish(msim *simulation, const char *overlay,
                          const char *token, const char *edition);

#endif
