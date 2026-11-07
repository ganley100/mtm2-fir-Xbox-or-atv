/*
 * recomp_types.h, as the lift sees it on 32-bit ARM (armeabi-v7a).
 *
 * x86 reads and writes any width at any address. 32-bit ARM does a 32-bit
 * LDR/STR unaligned, but a double, a float or a 64-bit value goes through
 * VLDR/VSTR or LDRD/STRD, which fault (SIGBUS) off a 4-byte boundary. The
 * lifted game does such accesses wherever its own data put them. This header
 * sits ahead of pcrecomp's in the include path (CMakeLists.txt, Android
 * armeabi-v7a only), takes pcrecomp's, and redefines those three widths as
 * members of packed structs: still lvalues, read and written with
 * instructions that take any address.
 */
#ifndef MTM_ARM32_RECOMP_TYPES_H
#define MTM_ARM32_RECOMP_TYPES_H
#include_next "recomp_types.h"

typedef struct __attribute__((packed)) { uint64_t v; } mtm_u64_unaligned;
typedef struct __attribute__((packed)) { float    v; } mtm_f32_unaligned;
typedef struct __attribute__((packed)) { double   v; } mtm_f64_unaligned;
#undef MEM64
#undef MEMF
#undef MEMD
#define MEM64(addr) (((volatile mtm_u64_unaligned *)ADDR(addr))->v)
#define MEMF(addr)  (((volatile mtm_f32_unaligned *)ADDR(addr))->v)
#define MEMD(addr)  (((volatile mtm_f64_unaligned *)ADDR(addr))->v)
#endif
