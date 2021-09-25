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

/* GC block type */
typedef struct gc_block_t {
    void *ptr;
    gc_dtor_t dtor;
    gc_mark_t mrk;
    size_t size, hash;
    uint8_t flags;
} gc_block_t;

/* Main GC type */
typedef struct gc_t {
    gc_block_t *young, *old;
    void *stack_bottom;
    size_t youngc, nyoung;
    size_t oldc, nold;
    size_t allocs, dirty;
    size_t cycles;
    uint8_t flags;
} gc_t;

#endif
