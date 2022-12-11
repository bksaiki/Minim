/*
    Header file for the bootstrap interpreter.
    Should not be referenced outside this directory
*/

#ifndef _BOOT_H_
#define _BOOT_H_

#include "../minim.h"
#include "intern.h"

#define MINIM_BOOT_VERSION      "0.4.0"

// Limits

#define SYMBOL_MAX_LEN      4096

#ifndef PATH_MAX
#define PATH_MAX    4096
#endif

// System

char* get_current_dir();
int set_current_dir(const char *path);
char *get_file_path(const char *rel_path);

// Interface

void minim_boot_init();

minim_object *make_env();
minim_object *eval_expr(minim_object *expr, minim_object *env);

minim_object *to_syntax(minim_object *o);
minim_object *strip_syntax(minim_object *o);

minim_object *read_object(FILE *in);
void write_object(FILE *out, minim_object *o);
void write_object2(FILE *out, minim_object *o, int quote, int display);

extern minim_object *global_env;
extern intern_table *symbols;

#endif  // _BOOT_H_
