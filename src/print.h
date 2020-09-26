#ifndef _MINIM_PRINT_H_
#define _MINIM_PRINT_H_

#include <stdio.h>
#include "env.h"

// Writes a string representation of the object to stdout
int print_minim_object(MinimObject *obj, MinimEnv *env, size_t maxlen);

// Writes a string representation of the object to stream.
int print_to_port(MinimObject *obj, MinimEnv *env, size_t maxlen, FILE *stream);

// Returns a string representation of the object.
char *print_to_string(MinimObject *obj, MinimEnv *env, size_t maxlen);

#endif