/* The Xbox build leaves out win32hle's wsock32.c (BSD sockets; MTM2 imports
 * no Winsock), so what the rest of win32hle asks of it is here: no shims
 * (a Winsock call would bind to a self-naming stub, like any unanswered
 * import) and nothing for the message pump to poll. */
#include "win32hle.h"

const win32hle_shim win32hle_wsock32[] = { { NULL, NULL } };
void hle_ws_poll(void) { }
