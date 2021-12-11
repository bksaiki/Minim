#ifndef _MINIM_GC_TYPES_H_
#define _MINIM_GC_TYPES_H_

/* Block flag */
#define GC_BLOCK_MARK       0x1
#define GC_BLOCK_ROOT       0x2

/* GC flag */
#define GC_COLLECT          0x1

/* Destructor signature */
typedef void (*gc_dtor_t)(void*);

/* Marking signature */
typedef void (*gc_mark_t)(void*,void*,void*);

/* GC record type */
typedef struct gc_record_t {
    void *ptr;
    gc_dtor_t dtor;
    gc_mark_t mrk;
    size_t size, hash;
    uint8_t flags;
} gc_record_t;

/* GC bucket type */
typedef struct gc_bucket_t {
    gc_record_t record;
    struct gc_bucket_t *next;
} gc_bucket_t;

/* Main GC type */
typedef struct gc_t {
    gc_bucket_t **buckets;
    void *stack_bottom;
    size_t *alloc_ptr;
    size_t alloc, size;
    size_t allocs, dirty;
    uint8_t flags;
} gc_t;

#endif
