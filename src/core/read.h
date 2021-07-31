#ifndef _MINIM_READ_H_
#define _MINIM_READ_H_

#include "env.h"
#include "module.h"

// Reads a file given by `str` and returns a new module whose previous module is `prev`.
// If a failure occurs, NULL is returned and `perr` will point to the error.
MinimModule *minim_load_file_as_module(MinimModule *prev, const char *fname, MinimObject **perr);

// Runs a file under the current environment 'env'
// `perr` will point to an error if `perr` is NULL when
// called and an error occured
int minim_load_file(MinimEnv *env, const char *fname, MinimObject **perr);

#endif
