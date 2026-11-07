/*
 * xbox_compat.h - the POSIX that pcrecomp's win32hle is written against, on
 * the Universal Windows Platform (Xbox Dev Mode), compiled with clang-cl.
 *
 * Force-included (/FI) ahead of every win32hle and host source, with this
 * folder first on the include path for <unistd.h>, <pthread.h>, <dirent.h>,
 * <sys/mman.h> and the rest. compat.c implements it on Win32 calls a UWP
 * app may make: SRW locks and condition variables, CreateThread,
 * VirtualAllocFromApp, FindFirstFileExW.
 *
 * Paths. win32hle keeps host paths in POSIX form and treats one as absolute
 * when it starts with '/'. Here a host path is the Windows path behind a
 * slash, "/C:/Users/.../LocalState/game": getcwd and realpath return that
 * form, every call below that takes a path takes it (or a plain Windows
 * path, or one relative to the current folder), and compat_native() gives
 * the Windows path for anything outside this layer (SDL's file calls). The
 * current folder is kept here: a UWP app cannot change its own.
 *
 * No <windows.h> here: win32hle defines Win32's names itself.
 */
#ifndef MTM_XBOX_COMPAT_H
#define MTM_XBOX_COMPAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <io.h>
#include <direct.h>
#include <process.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __thread __declspec(thread)
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

typedef intptr_t ssize_t;
typedef unsigned int useconds_t;
#ifndef __MINGW32__
typedef int mode_t;
#endif

/* ---- files ---- */
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#ifndef S_ISLNK
#define S_ISLNK(m) 0
#endif
#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#define S_IWUSR _S_IWRITE
#define S_IXUSR _S_IEXEC
#endif
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define FD_CLOEXEC 1

/* The current folder and the path forms (above). */
const char *compat_native(const char *path, char *out, size_t n);  /* the Windows path, '/' separated */
void        compat_set_start_dir(const char *native_dir);          /* the current folder at start */

FILE   *compat_fopen(const char *path, const char *mode);
int     compat_open(const char *path, int flags, ...);
/* struct stat as POSIX has it: the CRT's has no st_mtim/st_atim (with
 * nanoseconds), which win32hle reads. "struct stat" and stat() both mean
 * these below; a struct tag and a function may share a name. */
struct compat_stat {
    unsigned int st_dev, st_ino;
    unsigned short st_mode;
    short st_nlink, st_uid, st_gid;
    int64_t st_size;
    time_t st_atime, st_mtime, st_ctime;
    struct timespec st_atim, st_mtim, st_ctim;
};
int     compat_stat(const char *path, struct compat_stat *st);
int     compat_fstat(int fd, struct compat_stat *st);
int     compat_access(const char *path, int mode);
int     compat_mkdir(const char *path, mode_t mode);
int     compat_rmdir(const char *path);
int     compat_unlink(const char *path);
int     compat_remove(const char *path);
int     compat_rename(const char *from, const char *to);
int     compat_chdir(const char *path);
char   *compat_getcwd(char *buf, size_t n);
char   *realpath(const char *path, char *resolved);
ssize_t pread(int fd, void *buf, size_t n, int64_t off);
ssize_t pwrite(int fd, const void *buf, size_t n, int64_t off);
int     ftruncate(int fd, int64_t size);
#define UTIME_NOW  ((1l << 30) - 1l)
#define UTIME_OMIT ((1l << 30) - 2l)
int     futimens(int fd, const struct timespec ts[2]);   /* [0] access, [1] modification */
int     fcntl(int fd, int cmd, ...);
ssize_t readlink(const char *path, char *buf, size_t n);
int     symlink(const char *target, const char *path);
FILE   *compat_popen(const char *cmd, const char *mode);
int     compat_pclose(FILE *f);

#define fopen(p, m)        compat_fopen(p, m)
#define open               compat_open
#define stat               compat_stat
#define fstat              compat_fstat
#define lstat              compat_stat
#define access(p, m)       compat_access(p, m)
#define mkdir(p, m)        compat_mkdir(p, m)
#define rmdir(p)           compat_rmdir(p)
#define unlink(p)          compat_unlink(p)
#define remove(p)          compat_remove(p)
#define rename(a, b)       compat_rename(a, b)
#define chdir(p)           compat_chdir(p)
#define getcwd(b, n)       compat_getcwd(b, n)
#define popen(c, m)        compat_popen(c, m)
#define pclose(f)          compat_pclose(f)

/* ---- strings ---- */
#define strcasecmp  _stricmp
#define strncasecmp _strnicmp

/* ---- time ---- */
#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1
typedef int clockid_t;
int clock_gettime(clockid_t id, struct timespec *ts);
int nanosleep(const struct timespec *req, struct timespec *rem);
int usleep(useconds_t us);
int sched_yield(void);
time_t timegm(struct tm *tm);
struct tm *localtime_r(const time_t *t, struct tm *out);
struct tm *gmtime_r(const time_t *t, struct tm *out);

/* Local time minus UTC, in seconds: what glibc keeps in struct tm's
 * tm_gmtoff, which the CRT's struct tm lacks (CMakeLists.txt rewrites
 * kernel32_ext.c's one use of it to this). */
long compat_gmtoff(void);

/* ---- the system ---- */
int  compat_getpid(void);
#define getpid compat_getpid
#define _SC_NPROCESSORS_ONLN 84
#define _SC_PAGESIZE 30
long sysconf(int name);
int  getpagesize(void);

#ifdef __cplusplus
}
#endif
#endif
