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

#endif  // _BOOT_H_
