#include <stdbool.h>

bool hwbp_hook(void *old[], void *new[], int count);
bool hwbp_unhook(void *old[], int count);
