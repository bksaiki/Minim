#ifndef _MINIM_TOP_READ_H_
#define _MINIM_TOP_READ_H_

#include "minim.h"

/* Runs a file in a new Minim instance */
int minim_run(const char *str, uint32_t flags);

/* Loads every file from the base library */
int minim_load_library(MinimEnv *env);

#endif
