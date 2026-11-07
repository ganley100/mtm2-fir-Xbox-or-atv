/* sys/mman.h on VirtualAllocFromApp (compat.c), for what win32hle's guest
 * memory does with it on a 64-bit host: reserve the guest's 4 GB without
 * committing it, then put fresh zeroed pages (or a file's bytes) over parts
 * of it with MAP_FIXED. A file mapping is a copy: MAP_SHARED writes do not
 * reach the file. */
#ifndef MTM_XBOX_MMAN_H
#define MTM_XBOX_MMAN_H
#include "../xbox_compat.h"
#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4
#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON      MAP_ANONYMOUS
#define MAP_NORESERVE 0x4000
#define MAP_FAILED ((void *)-1)
#define MADV_DONTNEED 4
void *mmap(void *addr, size_t len, int prot, int flags, int fd, int64_t off);
int   munmap(void *addr, size_t len);
int   madvise(void *addr, size_t len, int advice);
int   mprotect(void *addr, size_t len, int prot);
#endif
