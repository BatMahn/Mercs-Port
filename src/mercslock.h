/* Serialize one edition overlay's launch files while its mission is running. */
#ifndef MERCS_LOCK_H
#define MERCS_LOCK_H

#include <stddef.h>

typedef struct mercs_launch_lock mercs_launch_lock;
enum { MERCS_LOCK_ERROR = -1, MERCS_LOCK_OK = 0, MERCS_LOCK_BUSY = 1 };

/* overlay must already be validated/created by portcfg_overlay_dir. Nonblocking:
 * OK owns the lock, BUSY means another launch owns it, ERROR is an I/O failure.
 * *out must initially be NULL and remains NULL on failure. The error buffer is
 * optional and receives a user-facing message for BUSY/ERROR.
 *
 * Hold the returned object from before prepare_launch until the mission child
 * exits (also release on preparation/spawn failure). The descriptor/handle is
 * not inherited across exec/CreateProcess. The kernel releases it on process
 * exit. The persistent MERCS.LCK file is deliberately never unlinked: deleting
 * it could let competing instances lock different inodes for the same overlay. */
int mercs_launch_lock_acquire(const char *overlay, mercs_launch_lock **out,
                              char *error, size_t error_size);
void mercs_launch_lock_release(mercs_launch_lock *lock);

#endif
