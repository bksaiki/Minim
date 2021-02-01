#ifndef _MINIM_COMMON_H_
#define _MINIM_COMMON_H_

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "version.h"

#if defined (__WIN32) || defined (WIN32) || defined (__WIN64) || defined (WIN64)
  #define MINIM_WINDOWS 1
#else
  #define MINIM_LINUX   1
#endif

#endif