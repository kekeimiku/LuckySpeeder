#include <stdbool.h>

#if !defined(HWBPHOOK_EXPORT)
#define HWBPHOOK_VISIBILITY __attribute__((visibility("hidden")))
#else
#define HWBPHOOK_VISIBILITY __attribute__((visibility("default")))
#endif

HWBPHOOK_VISIBILITY bool hwbp_rebind(void *old, void *new);
