/* Same-directory atomic replacement for native overlay records. */
#ifndef MERCSFILE_H
#define MERCSFILE_H
#include "datapath.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif


static inline int mercs_file_failure(char *error, size_t size, const char *name)
{
    if (error && size) snprintf(error, size, "Could not use %s: %s", name, strerror(errno));
    return -1;
}

static inline int mercs_file_directory(const char *parent, const char *name,
                                      char *out, size_t size, char *error, size_t error_size)
{
    int n;
    if (!parent || !parent[0] || !name || !name[0] || strchr(name,'/') ||
        strchr(name,'\\') || !strcmp(name,".") || !strcmp(name,"..")) {
        errno=EINVAL; return mercs_file_failure(error,error_size,"directory");
    }
    if (dp_resolve_in(parent,name,out,size)) {
        n=snprintf(out,size,"%s/%s",parent,name);
        if (n<0 || (size_t)n>=size) { errno=ENAMETOOLONG; return mercs_file_failure(error,error_size,name); }
#ifdef _WIN32
        if (!CreateDirectoryA(out,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS) {
            errno=EIO; return mercs_file_failure(error,error_size,name);
        }
#else
        if (mkdir(out,0755) && errno!=EEXIST) return mercs_file_failure(error,error_size,name);
#endif
    }
#ifdef _WIN32
    {
        DWORD attributes=GetFileAttributesA(out);
        if (attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY) ||
            (attributes&FILE_ATTRIBUTE_REPARSE_POINT)) { errno=EINVAL; return mercs_file_failure(error,error_size,name); }
    }
#else
    {
        struct stat st;
        if (lstat(out,&st) || !S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode)) {
            errno=EINVAL; return mercs_file_failure(error,error_size,name);
        }
    }
#endif
    return 0;
}

static inline int mercs_atomic_write(const char *dir, const char *name, const uint8_t *bytes,
                        size_t count, char *error, size_t error_size)
{
    char path[2048], temporary[2100];
    int n, bad = 0, saved_errno;
    FILE *f;
    if (!name || !name[0] || strchr(name, '/') || strchr(name, '\\') ||
        !strcmp(name, ".") || !strcmp(name, "..") || (!bytes && count)) {
        errno = EINVAL; return mercs_file_failure(error, error_size, "file name");
    }
#ifdef _WIN32
    HANDLE h;
    int fd, attempt;
    DWORD attrs = dir ? GetFileAttributesA(dir) : INVALID_FILE_ATTRIBUTES;
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY) ||
        (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) { errno = EINVAL; return mercs_file_failure(error, error_size, name); }
#else
    struct stat st;
    int fd;
    if (!dir || lstat(dir, &st) || !S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode)) {
        errno = EINVAL; return mercs_file_failure(error, error_size, name);
    }
#endif
    if (dp_resolve_in(dir, name, path, sizeof path)) {
        n = snprintf(path, sizeof path, "%s/%s", dir, name);
        if (n < 0 || (size_t)n >= sizeof path) { errno = ENAMETOOLONG; return mercs_file_failure(error, error_size, name); }
    }
#ifdef _WIN32
    for (attempt = 0; attempt < 100; attempt++) {
        snprintf(temporary, sizeof temporary, "%s.tmp-%lu-%d", path, (unsigned long)GetCurrentProcessId(), attempt);
        h = CreateFileA(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) break;
        if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) break;
    }
    if (h == INVALID_HANDLE_VALUE) { errno = EIO; return mercs_file_failure(error, error_size, name); }
    fd = _open_osfhandle((intptr_t)h, _O_WRONLY | _O_BINARY);
    if (fd < 0) { CloseHandle(h); remove(temporary); return mercs_file_failure(error, error_size, name); }
    f = _fdopen(fd, "wb");
#else
    snprintf(temporary, sizeof temporary, "%s.tmp-XXXXXX", path);
    fd = mkstemp(temporary);
    if (fd < 0) return mercs_file_failure(error, error_size, name);
    f = fdopen(fd, "wb");
#endif
    if (!f) {
#ifdef _WIN32
        _close(fd);
#else
        close(fd);
#endif
        remove(temporary); return mercs_file_failure(error, error_size, name);
    }
    if (fwrite(bytes, 1, count, f) != count || fflush(f)) bad = 1;
#ifdef _WIN32
    if (!bad && _commit(fd)) bad = 1;
#else
    if (!bad && fsync(fd)) bad = 1;
#endif
    saved_errno = errno;
    if (fclose(f)) { bad = 1; saved_errno = errno; }
    if (!bad) {
#ifdef _WIN32
        if (!MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { bad = 1; saved_errno = EIO; }
#else
        if (rename(temporary, path)) { bad = 1; saved_errno = errno; }
#endif
    }
    if (bad) { remove(temporary); errno = saved_errno ? saved_errno : EIO; return mercs_file_failure(error, error_size, name); }
    return 0;
}

#endif
