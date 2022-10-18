/*
    Header file for the bootstrap interpreter.
    Should not be referenced outside this directory
*/

#ifndef _BOOT_H_
#define _BOOT_H_

#include "../minim.h"

#define MINIM_BOOT_VERSION      "0.1.0"

// Limits

#define SYMBOL_MAX_LEN      4096

// Interface

void minim_boot_init();
minim_object *make_env();
minim_object *eval_expr(minim_object *expr, minim_object *env);
minim_object *read_object(FILE *in);
void write_object(FILE *out, minim_object *o);

extern minim_object *global_env;


#endif  // _BOOT_H_
