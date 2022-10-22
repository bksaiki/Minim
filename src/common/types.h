#ifndef _MINIM_COMMON_TYPES_H_
#define _MINIM_COMMON_TYPES_H_

#include "common.h"

// character type
typedef unsigned int    mchar_t;

// OS-dependent types
#if defined(MINIM_LINUX)
#include <sys/types.h>
typedef pid_t       pid_t;
#else
typedef int         pid_t;
#endif

#endif
