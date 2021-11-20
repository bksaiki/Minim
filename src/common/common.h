#ifndef _MINIM_COMMON_H_
#define _MINIM_COMMON_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "version.h"

#if defined (_WIN32) || defined (_WIN64)
#define MINIM_WINDOWS 1
#else
#define MINIM_LINUX   1
#endif

#if defined (__GNUC__)
#define MINIM_GCC     1
#define NORETURN    __attribute__ ((noreturn))
#else
#error "compiler not supported"
#endif

#if defined(ENABLE_STATS)
#define MINIM_TRACK_STATS     1
#else
#define MINIM_TRACK_STATS     0
#endif

#endif
