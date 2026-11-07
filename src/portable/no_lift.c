/* A build without the lift (-DMTM_NO_LIFT=ON): an empty dispatch, so the
 * host links and the toolchain and packaging can be checked before the long
 * compile of src/recomp/gen. mtm_hle.c refuses to run on it. */
#include "recomp_types.h"

static void no_lift(void) { }
const recomp_dispatch_entry_t recomp_dispatch_table[] = { { 0u, no_lift } };
const uint32_t recomp_dispatch_count = 0;
const uint32_t mtm_entry_va = 0;
