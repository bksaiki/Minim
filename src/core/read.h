#ifndef _MINIM_READ_H_
#define _MINIM_READ_H_

#include "env.h"
#include "module.h"

// Reads a file given by `str` and returns a new module whose previous module is `prev`.
MinimModule *minim_load_file_as_module(MinimModule *prev, const char *fname);

// Runs a file in a new environment where `env` contains the builtin environment
// at its lowest level.
void minim_load_file(MinimEnv *env, const char *fname);

// Runs a file in the environment `env`.
void minim_run_file(MinimEnv *env, const char *fname);

// Loads minim file from cache. Returns a port object
// of the file or NULL on failure.
MinimObject *load_processed_file(MinimObject *fport);

// Outputs a module into a minim source code.
void emit_processed_file(MinimObject *fport, MinimModule *module);

#endif
