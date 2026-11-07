/* pthread.h: the part win32hle uses, on Win32 (compat.c). Mutexes are SRW
 * locks, conditions are condition variables, a once is an INIT_ONCE: each
 * pointer-sized and zero to start, as the static initialisers below. */
#ifndef MTM_XBOX_PTHREAD_H
#define MTM_XBOX_PTHREAD_H
#include "xbox_compat.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef uintptr_t pthread_t;                      /* the thread id */
typedef struct { void *srw; } pthread_mutex_t;
typedef struct { void *cv; } pthread_cond_t;
typedef struct { void *once; } pthread_once_t;
typedef int pthread_attr_t;
typedef int pthread_mutexattr_t;
typedef int pthread_condattr_t;
#define PTHREAD_MUTEX_INITIALIZER { 0 }
#define PTHREAD_COND_INITIALIZER  { 0 }
#define PTHREAD_ONCE_INIT         { 0 }

int  pthread_create(pthread_t *t, const pthread_attr_t *attr, void *(*fn)(void *), void *arg);
int  pthread_detach(pthread_t t);
void pthread_exit(void *ret);
pthread_t pthread_self(void);
int  pthread_equal(pthread_t a, pthread_t b);
int  pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a);
int  pthread_mutex_destroy(pthread_mutex_t *m);
int  pthread_mutex_lock(pthread_mutex_t *m);
int  pthread_mutex_trylock(pthread_mutex_t *m);
int  pthread_mutex_unlock(pthread_mutex_t *m);
int  pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a);
int  pthread_cond_destroy(pthread_cond_t *c);
int  pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
int  pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *abstime);
int  pthread_cond_signal(pthread_cond_t *c);
int  pthread_cond_broadcast(pthread_cond_t *c);
int  pthread_once(pthread_once_t *o, void (*fn)(void));
#ifdef __cplusplus
}
#endif
#endif
