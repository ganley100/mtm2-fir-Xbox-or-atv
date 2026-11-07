/*
 * compat.c - xbox_compat.h's POSIX on Win32, inside a UWP app container.
 *
 * Only calls a UWP app may make: SRW locks, condition variables, INIT_ONCE,
 * CreateThread, QueryPerformanceCounter, VirtualAllocFromApp and
 * VirtualProtectFromApp, FindFirstFileExW, GetDiskFreeSpaceExW, ReadFile
 * and WriteFile at an offset. Paths are explained in xbox_compat.h.
 */
#include "xbox_compat.h"
#include "pthread.h"
#include "dirent.h"
#include "sys/mman.h"
#include "sys/statvfs.h"

/* This file calls the real CRT functions underneath. */
#undef fopen
#undef open
#undef stat
#undef fstat
#undef lstat
#undef access
#undef mkdir
#undef rmdir
#undef unlink
#undef remove
#undef rename
#undef chdir
#undef getcwd
#undef popen
#undef pclose
#undef opendir
#undef statvfs
#undef __thread

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdarg.h>

/* ================================================================ paths */

static SRWLOCK g_cwd_lock = SRWLOCK_INIT;
static char g_cwd[1024];                 /* native, '/' separated, no trailing '/' */

static void slashes(char *s) { for (; *s; s++) if (*s == '\\') *s = '/'; }

void compat_set_start_dir(const char *native_dir) {
    AcquireSRWLockExclusive(&g_cwd_lock);
    snprintf(g_cwd, sizeof g_cwd, "%s", native_dir);
    slashes(g_cwd);
    size_t l = strlen(g_cwd);
    while (l > 3 && g_cwd[l - 1] == '/') g_cwd[--l] = 0;
    ReleaseSRWLockExclusive(&g_cwd_lock);
}

static int drive_abs(const char *p) {     /* "C:/..." or "C:\..." */
    return ((p[0] | 0x20) >= 'a' && (p[0] | 0x20) <= 'z') && p[1] == ':' && (p[2] == '/' || p[2] == '\\' || !p[2]);
}

const char *compat_native(const char *path, char *out, size_t n) {
    if (!path) return NULL;
    if (path[0] == '/' && drive_abs(path + 1)) {               /* "/C:/..." */
        snprintf(out, n, "%s", path + 1);
    } else if (drive_abs(path) || (path[0] == '\\' && path[1] == '\\')) {
        snprintf(out, n, "%s", path);
    } else {                                                   /* relative to the current folder */
        AcquireSRWLockShared(&g_cwd_lock);
        if (!g_cwd[0]) {
            ReleaseSRWLockShared(&g_cwd_lock);
            if (!_getcwd(g_cwd, sizeof g_cwd)) snprintf(g_cwd, sizeof g_cwd, ".");
            slashes(g_cwd);
            AcquireSRWLockShared(&g_cwd_lock);
        }
        if (!strcmp(path, ".")) snprintf(out, n, "%s", g_cwd);
        else snprintf(out, n, "%s/%s", g_cwd, path[0] == '.' && (path[1] == '/' || path[1] == '\\') ? path + 2 : path);
        ReleaseSRWLockShared(&g_cwd_lock);
    }
    slashes(out);
    return out;
}

#define NATIVE(p) char nat_[1100]; const char *np_ = compat_native((p), nat_, sizeof nat_)

FILE *compat_fopen(const char *path, const char *mode) {
    NATIVE(path);
    char m[16];
    snprintf(m, sizeof m, "%s%s", mode, strchr(mode, 'b') || strchr(mode, 't') ? "" : "b");
    return fopen(np_, m);
}
int compat_open(const char *path, int flags, ...) {
    NATIVE(path);
    int mode = 0;
    if (flags & _O_CREAT) { va_list ap; va_start(ap, flags); mode = va_arg(ap, int); va_end(ap); }
    if (!(flags & (_O_TEXT | _O_BINARY))) flags |= _O_BINARY;
    return _open(np_, flags & ~O_CLOEXEC, mode ? (mode & 0200 ? _S_IREAD | _S_IWRITE : _S_IREAD) : _S_IREAD | _S_IWRITE);
}
static void from_crt(const struct _stat64 *c, struct compat_stat *st) {
    memset(st, 0, sizeof *st);
    st->st_dev = c->st_dev, st->st_ino = c->st_ino, st->st_mode = c->st_mode;
    st->st_nlink = c->st_nlink, st->st_uid = c->st_uid, st->st_gid = c->st_gid;
    st->st_size = c->st_size;
    st->st_atime = st->st_atim.tv_sec = (time_t)c->st_atime;
    st->st_mtime = st->st_mtim.tv_sec = (time_t)c->st_mtime;
    st->st_ctime = st->st_ctim.tv_sec = (time_t)c->st_ctime;
}
int compat_stat(const char *path, struct compat_stat *st) {
    NATIVE(path);
    char t[1100];
    snprintf(t, sizeof t, "%s", np_);
    size_t l = strlen(t);
    while (l > 3 && t[l - 1] == '/') t[--l] = 0;          /* the CRT's stat refuses "dir/" */
    struct _stat64 c;
    if (_stat64(t, &c) != 0) return -1;
    from_crt(&c, st);
    return 0;
}
int compat_fstat(int fd, struct compat_stat *st) {
    struct _stat64 c;
    if (_fstat64(fd, &c) != 0) return -1;
    from_crt(&c, st);
    return 0;
}
int compat_access(const char *path, int mode) {
    NATIVE(path);
    return _access(np_, mode & (R_OK | W_OK));             /* X_OK is an invalid parameter to the CRT */
}
int compat_mkdir(const char *path, mode_t mode) { (void)mode; NATIVE(path); return _mkdir(np_); }
int compat_rmdir(const char *path) { NATIVE(path); return _rmdir(np_); }
int compat_unlink(const char *path) { NATIVE(path); return _unlink(np_); }
int compat_remove(const char *path) { NATIVE(path); return remove(np_); }
int compat_rename(const char *from, const char *to) {
    char a[1100], b[1100];
    compat_native(from, a, sizeof a);
    compat_native(to, b, sizeof b);
    _unlink(b);                                             /* POSIX rename replaces */
    return rename(a, b);
}

int compat_chdir(const char *path) {
    NATIVE(path);
    struct _stat64 st;
    if (_stat64(np_, &st) != 0 || !S_ISDIR(st.st_mode)) { errno = ENOENT; return -1; }
    char full[1100];
    if (!_fullpath(full, np_, sizeof full)) return -1;
    compat_set_start_dir(full);
    return 0;
}

char *compat_getcwd(char *buf, size_t n) {
    char t[1100];
    compat_native(".", t, sizeof t);
    if (!buf) { n = strlen(t) + 2; buf = (char *)malloc(n); if (!buf) return NULL; }
    if (strlen(t) + 2 > n) { errno = ERANGE; return NULL; }
    snprintf(buf, n, "/%s", t);
    return buf;
}

char *realpath(const char *path, char *resolved) {
    NATIVE(path);
    char full[1100];
    struct _stat64 st;
    if (!_fullpath(full, np_, sizeof full) || _stat64(full, &st) != 0) { errno = ENOENT; return NULL; }
    slashes(full);
    size_t l = strlen(full);
    while (l > 3 && full[l - 1] == '/') full[--l] = 0;
    if (!resolved) { resolved = (char *)malloc(l + 2); if (!resolved) return NULL; }
    snprintf(resolved, l + 2, "/%s", full);                /* callers give PATH_MAX */
    return resolved;
}

ssize_t readlink(const char *path, char *buf, size_t n) { (void)path; (void)buf; (void)n; errno = EINVAL; return -1; }
int symlink(const char *target, const char *path) { (void)target; (void)path; errno = EPERM; return -1; }
/* No processes in an app container (gdidc.c asks fontconfig this way). */
FILE *compat_popen(const char *cmd, const char *mode) { (void)cmd; (void)mode; errno = ENOSYS; return NULL; }
int compat_pclose(FILE *f) { (void)f; return -1; }

/* ================================================================ fd I/O at an offset */

static ssize_t rw_at(int fd, void *buf, size_t n, int64_t off, int write) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
    OVERLAPPED ov = { 0 };
    ov.Offset = (DWORD)off;
    ov.OffsetHigh = (DWORD)(off >> 32);
    DWORD done = 0;
    BOOL ok = write ? WriteFile(h, buf, (DWORD)n, &done, &ov) : ReadFile(h, buf, (DWORD)n, &done, &ov);
    if (!ok) {
        if (!write && GetLastError() == ERROR_HANDLE_EOF) return 0;
        errno = EIO;
        return -1;
    }
    return (ssize_t)done;
}
ssize_t pread(int fd, void *buf, size_t n, int64_t off) { return rw_at(fd, buf, n, off, 0); }
ssize_t pwrite(int fd, const void *buf, size_t n, int64_t off) { return rw_at(fd, (void *)buf, n, off, 1); }
int ftruncate(int fd, int64_t size) { return _chsize_s(fd, size) == 0 ? 0 : -1; }
int futimens(int fd, const struct timespec ts[2]) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
    FILETIME ft[2], *p[2] = { NULL, NULL };
    for (int i = 0; i < 2; i++) {
        if (ts && ts[i].tv_nsec == UTIME_OMIT) continue;
        uint64_t t;
        if (!ts || ts[i].tv_nsec == UTIME_NOW) {
            GetSystemTimePreciseAsFileTime(&ft[i]);
        } else {
            t = (uint64_t)ts[i].tv_sec * 10000000ULL + (uint64_t)ts[i].tv_nsec / 100 + 116444736000000000ULL;
            ft[i].dwLowDateTime = (DWORD)t, ft[i].dwHighDateTime = (DWORD)(t >> 32);
        }
        p[i] = &ft[i];
    }
    if (!SetFileTime(h, NULL, p[0], p[1])) { errno = EIO; return -1; }
    return 0;
}
int compat_getpid(void) { return (int)GetCurrentProcessId(); }
int fcntl(int fd, int cmd, ...) { (void)fd; (void)cmd; return 0; }   /* flags and close-on-exec: nothing to do */

/* ================================================================ directories */

struct compat_dir {
    HANDLE h;
    WIN32_FIND_DATAW fd;
    int first, done;
    struct dirent ent;
};

DIR *compat_opendir(const char *path) {
    char n[1100];
    compat_native(path, n, sizeof n);
    size_t l = strlen(n);
    snprintf(n + l, sizeof n - l, "%s*", l && n[l - 1] == '/' ? "" : "/");
    WCHAR w[1100];
    if (!MultiByteToWideChar(CP_UTF8, 0, n, -1, w, 1100)) { errno = ENOENT; return NULL; }
    for (WCHAR *c = w; *c; c++) if (*c == L'/') *c = L'\\';
    DIR *d = (DIR *)calloc(1, sizeof *d);
    if (!d) return NULL;
    d->h = FindFirstFileExW(w, FindExInfoBasic, &d->fd, FindExSearchNameMatch, NULL, 0);
    if (d->h == INVALID_HANDLE_VALUE) { free(d); errno = ENOENT; return NULL; }
    d->first = 1;
    return d;
}
struct dirent *readdir(DIR *d) {
    if (!d || d->done) return NULL;
    if (!d->first && !FindNextFileW(d->h, &d->fd)) { d->done = 1; return NULL; }
    d->first = 0;
    WideCharToMultiByte(CP_UTF8, 0, d->fd.cFileName, -1, d->ent.d_name, sizeof d->ent.d_name, NULL, NULL);
    d->ent.d_type = d->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? DT_DIR : DT_REG;
    return &d->ent;
}
int closedir(DIR *d) {
    if (!d) return -1;
    FindClose(d->h);
    free(d);
    return 0;
}

int compat_statvfs(const char *path, struct statvfs *st) {
    char n[1100];
    compat_native(path, n, sizeof n);
    WCHAR w[1100];
    MultiByteToWideChar(CP_UTF8, 0, n, -1, w, 1100);
    ULARGE_INTEGER avail, total, freeb;
    if (!GetDiskFreeSpaceExW(w, &avail, &total, &freeb)) { errno = ENOENT; return -1; }
    st->f_bsize = st->f_frsize = 4096;
    st->f_blocks = total.QuadPart / 4096;
    st->f_bfree = freeb.QuadPart / 4096;
    st->f_bavail = avail.QuadPart / 4096;
    return 0;
}

/* ================================================================ memory */

static ULONG page_prot(int prot) {
    if (prot & PROT_WRITE) return PAGE_READWRITE;
    if (prot & PROT_READ) return PAGE_READONLY;
    return PAGE_NOACCESS;
}

static int fill_from(void *p, size_t len, int fd, int64_t off) {
    for (size_t done = 0; done < len;) {
        ssize_t r = pread(fd, (uint8_t *)p + done, len - done, off + (int64_t)done);
        if (r < 0) return -1;
        if (r == 0) break;                                  /* past the end: zeros, as mmap */
        done += (size_t)r;
    }
    return 0;
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, int64_t off) {
    if (!len) { errno = EINVAL; return MAP_FAILED; }
    if (flags & MAP_SHARED && fd >= 0 && prot & PROT_WRITE) {
        static LONG said;
        if (!InterlockedExchange(&said, 1))
            fprintf(stderr, "[compat] a shared file mapping is a copy here: writes to it do not reach the file\n");
    }
    void *p;
    if (!(flags & MAP_FIXED)) {
        /* A reservation (the guest's 4 GB: PROT_NONE, NORESERVE) or new pages. */
        ULONG type = prot == PROT_NONE ? MEM_RESERVE : MEM_RESERVE | MEM_COMMIT;
        p = VirtualAllocFromApp(addr, len, type, prot == PROT_NONE ? PAGE_NOACCESS : PAGE_READWRITE);
        if (!p && addr) p = VirtualAllocFromApp(NULL, len, type, prot == PROT_NONE ? PAGE_NOACCESS : PAGE_READWRITE);
        if (!p) { errno = ENOMEM; return MAP_FAILED; }
    } else {
        /* Over part of a reservation: fresh zeroed pages, or none. */
        p = addr;
        VirtualFree(p, len, MEM_DECOMMIT);
        if (prot == PROT_NONE) return p;
        if (!VirtualAllocFromApp(p, len, MEM_COMMIT, PAGE_READWRITE)) { errno = ENOMEM; return MAP_FAILED; }
    }
    if (fd >= 0 && fill_from(p, len, fd, off) != 0) { errno = EIO; return MAP_FAILED; }
    if (prot != PROT_NONE && page_prot(prot) != PAGE_READWRITE) {
        ULONG old;
        VirtualProtectFromApp(p, len, page_prot(prot), &old);
    }
    return p;
}
int munmap(void *addr, size_t len) { return VirtualFree(addr, len, MEM_DECOMMIT) ? 0 : -1; }
int madvise(void *addr, size_t len, int advice) {
    if (advice != MADV_DONTNEED) return 0;
    VirtualFree(addr, len, MEM_DECOMMIT);                   /* reads as zeros again once recommitted */
    return VirtualAllocFromApp(addr, len, MEM_COMMIT, PAGE_READWRITE) ? 0 : -1;
}
int mprotect(void *addr, size_t len, int prot) {
    ULONG old;
    return VirtualProtectFromApp(addr, len, page_prot(prot), &old) ? 0 : -1;
}

/* ================================================================ time */

int clock_gettime(clockid_t id, struct timespec *ts) {
    if (id == CLOCK_MONOTONIC) {
        static LARGE_INTEGER freq;
        LARGE_INTEGER c;
        if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&c);
        ts->tv_sec = (time_t)(c.QuadPart / freq.QuadPart);
        ts->tv_nsec = (long)((c.QuadPart % freq.QuadPart) * 1000000000LL / freq.QuadPart);
        return 0;
    }
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime) - 116444736000000000ULL;   /* 1601 -> 1970, 100 ns */
    ts->tv_sec = (time_t)(t / 10000000);
    ts->tv_nsec = (long)(t % 10000000 * 100);
    return 0;
}

/* Sleep() counts in milliseconds: a shorter wait yields, a longer one is
 * rounded to the nearest, never to nothing. */
static void sleep_us(uint64_t us) {
    if (us < 500) { Sleep(0); return; }
    Sleep((DWORD)((us + 500) / 1000));
}
int usleep(useconds_t us) { sleep_us(us); return 0; }
int nanosleep(const struct timespec *req, struct timespec *rem) {
    if (rem) rem->tv_sec = 0, rem->tv_nsec = 0;
    sleep_us((uint64_t)req->tv_sec * 1000000 + (uint64_t)req->tv_nsec / 1000);
    return 0;
}
int sched_yield(void) { Sleep(0); return 0; }

time_t timegm(struct tm *tm) { return _mkgmtime(tm); }
long compat_gmtoff(void) {
    time_t now = time(NULL);
    struct tm l;
    if (localtime_s(&l, &now)) return 0;
    return (long)(_mkgmtime(&l) - now);
}
struct tm *localtime_r(const time_t *t, struct tm *out) { return localtime_s(out, t) ? NULL : out; }
struct tm *gmtime_r(const time_t *t, struct tm *out) { return gmtime_s(out, t) ? NULL : out; }

long sysconf(int name) {
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    if (name == _SC_NPROCESSORS_ONLN) return (long)si.dwNumberOfProcessors;
    if (name == _SC_PAGESIZE) return (long)si.dwPageSize;
    return -1;
}
int getpagesize(void) { return (int)sysconf(_SC_PAGESIZE); }

/* ================================================================ threads */

struct start { void *(*fn)(void *); void *arg; };
static DWORD WINAPI trampoline(LPVOID p) {
    struct start s = *(struct start *)p;
    free(p);
    s.fn(s.arg);
    return 0;
}
int pthread_create(pthread_t *t, const pthread_attr_t *attr, void *(*fn)(void *), void *arg) {
    (void)attr;
    struct start *s = (struct start *)malloc(sizeof *s);
    if (!s) return ENOMEM;
    s->fn = fn, s->arg = arg;
    DWORD id;
    /* 8 MB, a Linux thread's default: win32hle runs the guest's threads on
     * these, and lifted code is deep. */
    HANDLE h = CreateThread(NULL, 8u << 20, trampoline, s, STACK_SIZE_PARAM_IS_A_RESERVATION, &id);
    if (!h) { free(s); return EAGAIN; }
    CloseHandle(h);                                         /* nothing joins: every thread is detached */
    if (t) *t = id;
    return 0;
}
int pthread_detach(pthread_t t) { (void)t; return 0; }
void pthread_exit(void *ret) { (void)ret; ExitThread(0); }
pthread_t pthread_self(void) { return GetCurrentThreadId(); }
int pthread_equal(pthread_t a, pthread_t b) { return a == b; }

int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) { (void)a; InitializeSRWLock((PSRWLOCK)&m->srw); return 0; }
int pthread_mutex_destroy(pthread_mutex_t *m) { (void)m; return 0; }
int pthread_mutex_lock(pthread_mutex_t *m) { AcquireSRWLockExclusive((PSRWLOCK)&m->srw); return 0; }
int pthread_mutex_trylock(pthread_mutex_t *m) { return TryAcquireSRWLockExclusive((PSRWLOCK)&m->srw) ? 0 : EBUSY; }
int pthread_mutex_unlock(pthread_mutex_t *m) { ReleaseSRWLockExclusive((PSRWLOCK)&m->srw); return 0; }

int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a) { (void)a; InitializeConditionVariable((PCONDITION_VARIABLE)&c->cv); return 0; }
int pthread_cond_destroy(pthread_cond_t *c) { (void)c; return 0; }
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) {
    return SleepConditionVariableSRW((PCONDITION_VARIABLE)&c->cv, (PSRWLOCK)&m->srw, INFINITE, 0) ? 0 : EINVAL;
}
int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *abstime) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    int64_t ms = ((int64_t)abstime->tv_sec - now.tv_sec) * 1000 + (abstime->tv_nsec - now.tv_nsec) / 1000000;
    if (ms < 0) ms = 0;
    if (SleepConditionVariableSRW((PCONDITION_VARIABLE)&c->cv, (PSRWLOCK)&m->srw, (DWORD)ms, 0)) return 0;
    return GetLastError() == ERROR_TIMEOUT ? ETIMEDOUT : EINVAL;
}
int pthread_cond_signal(pthread_cond_t *c) { WakeConditionVariable((PCONDITION_VARIABLE)&c->cv); return 0; }
int pthread_cond_broadcast(pthread_cond_t *c) { WakeAllConditionVariable((PCONDITION_VARIABLE)&c->cv); return 0; }

static BOOL CALLBACK once_cb(PINIT_ONCE o, PVOID fn, PVOID *ctx) { (void)o; (void)ctx; ((void (*)(void))fn)(); return TRUE; }
int pthread_once(pthread_once_t *o, void (*fn)(void)) {
    InitOnceExecuteOnce((PINIT_ONCE)&o->once, once_cb, (PVOID)fn, NULL);
    return 0;
}
