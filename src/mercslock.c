#include "mercslock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#endif

struct mercs_launch_lock {
#ifdef _WIN32
    HANDLE handle;
#else
    int fd;
#endif
};

static int lock_message(int result, char *error, size_t error_size, const char *text)
{
    if (error && error_size) snprintf(error, error_size, "%s", text);
    return result;
}

static int lock_busy(char *error, size_t error_size)
{
    return lock_message(MERCS_LOCK_BUSY, error, error_size,
        "Another Mercenaries window is using this edition's mission files. Finish its mission, then try again.");
}

int mercs_launch_lock_acquire(const char *overlay, mercs_launch_lock **out,
                              char *error, size_t error_size)
{
    char path[4096], message[256];
    int length;
    mercs_launch_lock *lock;
    if (error && error_size) error[0] = 0;
    if (!out || *out || !overlay || !overlay[0])
        return lock_message(MERCS_LOCK_ERROR, error, error_size, "The mission installation folder is not ready.");
    length = snprintf(path, sizeof path, "%s/MERCS.LCK", overlay);
    if (length < 0 || (size_t)length >= sizeof path)
        return lock_message(MERCS_LOCK_ERROR, error, error_size, "The mission installation path is too long.");
    lock = malloc(sizeof *lock);
    if (!lock)
        return lock_message(MERCS_LOCK_ERROR, error, error_size, "Not enough memory to reserve the mission files.");
#ifdef _WIN32
    {
        DWORD code;
        BY_HANDLE_FILE_INFORMATION info;
        /* No sharing, no inheritable security attributes. OPEN_ALWAYS preserves
         * the same lock file between launches instead of replacing its identity. */
        lock->handle = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                  OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
        if (lock->handle == INVALID_HANDLE_VALUE) {
            code = GetLastError(); free(lock);
            if (code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION)
                return lock_busy(error, error_size);
            snprintf(message, sizeof message, "Cannot reserve the mission files (Windows error %lu).", (unsigned long)code);
            return lock_message(MERCS_LOCK_ERROR, error, error_size, message);
        }
        if (!GetFileInformationByHandle(lock->handle, &info) ||
            (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
            CloseHandle(lock->handle); free(lock);
            return lock_message(MERCS_LOCK_ERROR, error, error_size, "The mission lock must be a regular file in the installation folder.");
        }
    }
#else
    {
        struct stat info;
        int flags = O_RDWR | O_CREAT | O_NONBLOCK, code;
#ifdef O_CLOEXEC
        flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
        flags |= O_NOFOLLOW;
#endif
        lock->fd = open(path, flags, 0600);
        if (lock->fd < 0) {
            code = errno; free(lock);
            snprintf(message, sizeof message, "Cannot reserve the mission files: %s", strerror(code));
            return lock_message(MERCS_LOCK_ERROR, error, error_size, message);
        }
        if (fstat(lock->fd, &info) != 0 || !S_ISREG(info.st_mode)) {
            close(lock->fd); free(lock);
            return lock_message(MERCS_LOCK_ERROR, error, error_size, "The mission lock must be a regular file in the installation folder.");
        }
        if (fcntl(lock->fd, F_SETFD, FD_CLOEXEC) == -1) {
            code = errno; close(lock->fd); free(lock);
            snprintf(message, sizeof message, "Cannot reserve the mission files: %s", strerror(code));
            return lock_message(MERCS_LOCK_ERROR, error, error_size, message);
        }
        /* flock ties ownership to this open file description, so even a second
         * acquisition in the same process cannot accidentally share/release it. */
        if (flock(lock->fd, LOCK_EX | LOCK_NB) != 0) {
            code = errno; close(lock->fd); free(lock);
            if (code == EWOULDBLOCK || code == EAGAIN) return lock_busy(error, error_size);
            snprintf(message, sizeof message, "Cannot reserve the mission files: %s", strerror(code));
            return lock_message(MERCS_LOCK_ERROR, error, error_size, message);
        }
    }
#endif
    *out = lock;
    return MERCS_LOCK_OK;
}

void mercs_launch_lock_release(mercs_launch_lock *lock)
{
    if (!lock) return;
#ifdef _WIN32
    CloseHandle(lock->handle);
#else
    close(lock->fd);
#endif
    free(lock);
}
