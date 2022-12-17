/*
    Header file for the bootstrap interpreter.
    Should not be referenced outside this directory

    Defines the symbol/string interner.
*/

#ifndef _BOOT_INTERN_H_
#define _BOOT_INTERN_H_

#include "../minim.h"

// intern table bucket
typedef struct intern_table_bucket {
    minim_object *sym;
    struct intern_table_bucket *next;
} intern_table_bucket;

// intern table
typedef struct intern_table {
    intern_table_bucket **buckets;
    size_t *alloc_ptr;
    size_t alloc, size;
} intern_table;

// Interface

intern_table *make_intern_table();
minim_object *intern_symbol(intern_table *itab, const char *sym);

#endif  // _BOOT_INTERN_H_
