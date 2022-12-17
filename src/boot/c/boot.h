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

#define SYMBOL_MAX_LEN              4096

#define INIT_VALUES_BUFFER_LEN      10

#ifndef PATH_MAX
#define PATH_MAX    4096
#endif

// Globals

typedef struct intern_table intern_table;

typedef struct minim_globals {
    minim_thread *current_thread;
    intern_table *symbols;
} minim_globals;

extern minim_globals *globals;

#define current_thread()    (globals->current_thread)
#define intern(s)           (intern_symbol(globals->symbols, s))

// System

char* _get_current_dir();
int _set_current_dir(const char *path);
char *_get_file_path(const char *rel_path);

void set_current_dir(const char *str);
#define get_current_dir() _get_current_dir()
char *get_file_dir(const char *realpath);

// Interface

void minim_boot_init();
minim_object *load_file(const char *fname);

minim_object *make_env();
minim_object *eval_expr(minim_object *expr, minim_object *env);

minim_object *to_syntax(minim_object *o);
minim_object *strip_syntax(minim_object *o);

minim_object *read_object(FILE *in);
void write_object(FILE *out, minim_object *o);
void write_object2(FILE *out, minim_object *o, int quote, int display);

#endif  // _BOOT_H_
