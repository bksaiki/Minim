#ifndef _MINIM_INTERN_H_
#define _MINIM_INTERN_H_

#include "object.h"

typedef struct InternTableBucket {
    MinimObject *sym;
    struct InternTableBucket *next;
} InternTableBucket;

typedef struct InternTable
{
    InternTableBucket **buckets;
    size_t *alloc_ptr;
    size_t alloc, size;
} InternTable;

InternTable *init_intern_table();
MinimObject *intern_symbol(InternTable *itab, const char *sym);

#endif
