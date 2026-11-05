/*
 * datapath.h - where the game's files come from.
 *
 * The original reads some files from the install folder and the rest from the
 * CD (it refuses to start without the disc). The port has no CD check: it
 * searches an ordered list of folders instead, typically
 *
 *   1. the install folder (MW2.PRJ, configs, saves)
 *   2. a folder holding a copy of the CD's files
 *
 * DOS filenames are case-insensitive and use backslashes; Linux and macOS
 * paths are not. dp_resolve() maps "MEK\\MDG00STD.MEK" onto whatever case the
 * files actually have on disk (e.g. mek/mdg00std.mek).
 */
#ifndef MW2_DATAPATH_H
#define MW2_DATAPATH_H

#include <stddef.h>
#include <stdio.h>

#define DP_MAX_ROOTS 8

/* Add a search folder; earlier roots take priority. Returns 0 on success. */
int   dp_add_root(const char *dir);
void  dp_clear_roots(void);
/* Explicit writable overlay. Added read roots remain read-only for dp_fopen.
 * NULL disables writes. Environment setup uses MERCS_INSTALL_DIR. */
int   dp_set_write_root(const char *dir);

/* Add an ISO 9660 image (2048-byte sectors, e.g. cdrip's mw2cd.iso) as a
 * read-only root. subdir, if not NULL, is a folder inside the image to treat
 * as the root (e.g. "MERCS"). Returns 0 on success. */
int   dp_add_iso_root(const char *iso_path, const char *subdir);

/* Adds roots from the environment, in this order:
 *   MERCS_INSTALL_DIR   writable install overlay
 *   MERCS_CD_DIR        folder holding a copy of the disc
 *   MERCS_CD_IMAGE      the disc's data track as an .iso
 * Disc roots are searched in their MERCS folder first, then at the top. */
void  dp_add_roots_from_env(void);

/* Read a whole file from any root (folders or ISO images) into a malloc'd
 * buffer. Returns 0 on success; free(*data) when done. */
int   dp_read_file(const char *dos_path, unsigned char **data, size_t *len);

/* Resolve a DOS-style relative path to a real file in a folder root (ISO roots
 * are skipped). Returns 0 and fills out on success. */
int   dp_resolve(const char *dos_path, char *out, size_t outlen);

/* dp_resolve() in one folder only (any case of each component); 0 and out filled if found. */
int   dp_resolve_in(const char *root, const char *dos_path, char *out, size_t outlen);

/* Reads use dp_resolve(). All writes use the explicit writable overlay;
 * an existing fallback file is copied there for append/update modes. Parent
 * traversal and (on POSIX) symlinks in overlay paths are refused. */
FILE *dp_fopen(const char *dos_path, const char *mode);

#endif
