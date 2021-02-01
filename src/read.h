#ifndef _MINIM_TOP_READ_H_
#define _MINIM_TOP_READ_H_

#include "minim.h"

/* Runs a file in a Minim instance */
int minim_load_file(MinimEnv *env, const char *str);

/* Runs a file in a new Minim instance */
int minim_run_file(const char *str);

/* Loads every file from the base library */
int minim_load_library(MinimEnv *env);

#endif