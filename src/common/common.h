#ifndef _MINIM_COMMON_H_
#define _MINIM_COMMON_H_

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "version.h"
#include "../gc/gc.h"

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

// Configuration options

#define MINIM_USE_CACHE         1

#endif
