#ifndef _MINIM_COMMON_SYSTEM_H_
#define _MINIM_COMMON_SYSTEM_H_

#include "common.h"
#include "types.h"

// Limits

#ifndef PATH_MAX
#define PATH_MAX    4096
#endif

// System queries

pid_t get_current_pid();
char* get_current_dir();

#endif
