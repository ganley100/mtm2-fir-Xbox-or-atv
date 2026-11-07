/* sys/statvfs.h on GetDiskFreeSpaceExW (compat.c). */
#ifndef MTM_XBOX_STATVFS_H
#define MTM_XBOX_STATVFS_H
#include "../xbox_compat.h"
typedef unsigned long long fsblkcnt_t;
struct statvfs {
    unsigned long f_bsize, f_frsize;
    fsblkcnt_t f_blocks, f_bfree, f_bavail;
};
int compat_statvfs(const char *path, struct statvfs *st);
#define statvfs(p, b) compat_statvfs(p, b)
#endif
