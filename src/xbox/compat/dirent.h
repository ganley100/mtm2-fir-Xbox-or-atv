/* dirent.h on FindFirstFileExW (compat.c). Names are UTF-8. */
#ifndef MTM_XBOX_DIRENT_H
#define MTM_XBOX_DIRENT_H
#include "xbox_compat.h"
#define DT_UNKNOWN 0
#define DT_DIR     4
#define DT_REG     8
struct dirent { unsigned char d_type; char d_name[260 * 3]; };
typedef struct compat_dir DIR;
DIR *compat_opendir(const char *path);
struct dirent *readdir(DIR *d);
int closedir(DIR *d);
#define opendir(p) compat_opendir(p)
#endif
