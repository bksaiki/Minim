#ifndef _MINIM_READ_H_
#define _MINIM_READ_H_

#include "env.h"
#include "module.h"

// Reads a file given by `str` and returns a new module whose previous module is `prev`.
// If a failure occurs, NULL is returned and `perr` will point to the error.
MinimModule *minim_load_file_as_module(MinimModule *prev, const char *fname, MinimObject **perr);

// Runs a file in a new environment where `env` contains the builtin environment
// at its lowest level. On success, a non-zero result is returned.
// On failure, `perr` will point to the error.
int minim_load_file(MinimEnv *env, const char *fname, MinimObject **perr);

// Runs a file in the environment `env` and returns a non-zero result on success.
// On failure, `perr` will point to the error
int minim_run_file(MinimEnv *env, const char *fname, MinimObject **perr);

#endif
