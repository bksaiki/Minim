#ifndef _MINIM_GC_TYPES_H_
#define _MINIM_GC_TYPES_H_

#include <stdint.h>

/* Block flag */
#define GC_RECORD_MARK       0x1
#define GC_RECORD_ROOT       0x2

/* GC flag */
#define GC_COLLECT          0x1

/* Destructor signature */
typedef void (*gc_dtor_t)(void*);

/* Marking signature */
typedef void (*gc_mark_t)(void*,void*,void*);

/* GC bucket type */
typedef struct gc_record_t {
    struct gc_record_t *next;
    size_t size;
    gc_dtor_t dtor;
    gc_mark_t mrk;
} gc_record_t;

/* Main GC type */
typedef struct gc_t {
    gc_record_t **buckets;
    void *stack_bottom;
    size_t *alloc_ptr;
    size_t alloc, size;
    size_t allocs, dirty;
    uint8_t flags;
} gc_t;

/* Accessors */

#define gc_record_ptr(r)            ((void *) (((uintptr_t) (r)) + sizeof(gc_record_t)))
#define gc_record_next(r)           ((void *) (((uintptr_t) (r)->next) & ~0x3))
#define gc_record_flags(r)          (((uintptr_t) (r)->next) & 0x3)
#define gc_record_alloc_size(r)     ((r)->size + sizeof(gc_record_t))

#define gc_load(gc)     (((double) (gc)->size) / ((double) (gc)->alloc))
#define gc_hash(p)      (((size_t) (p)) >> 3)

/* Predicates */

#define gc_record_markp(r)      (gc_record_flags(r) & GC_RECORD_MARK)
#define gc_record_rootp(r)      (gc_record_flags(r) & GC_RECORD_ROOT)

/* Setters */

#define gc_record_mark_set(r)       (*((uintptr_t*) &((r)->next)) |= GC_RECORD_MARK)
#define gc_record_mark_unset(r)     (*((uintptr_t*) &((r)->next)) &= ~GC_RECORD_MARK)

#define gc_record_root_set(r)       (*((uintptr_t*) &((r)->next)) |= GC_RECORD_ROOT)
#define gc_record_root_unset(r)     (*((uintptr_t*) &((r)->next)) &= ~GC_RECORD_ROOT)

#define gc_record_next_set(r, n)    \
    ((r)->next = (void *) (((uintptr_t) (n)) | ((uintptr_t) gc_record_flags(r))))

#endif
