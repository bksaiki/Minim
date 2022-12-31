#ifndef _MINIM_GC_H_
#define _MINIM_GC_H_

#include <stddef.h>
#include <stdlib.h>

/*************** Interface ******************/

/* Initialize GC */
void GC_init(void *stack);

/* Shutdown GC and clean up */
void GC_finalize();

/* Pauses and resumes automated collection */
void GC_pause();
void GC_resume();

/* GC equivalent of malloc(), calloc(), and realloc() */
void *GC_alloc(size_t size);
void *GC_calloc(size_t nmem, size_t size);
void *GC_realloc(void *ptr, size_t size);

/* GC equivalent of malloc(), calloc(), and realloc() except the data
   contains no pointer data. */
void *GC_alloc_atomic(size_t size);
void *GC_calloc_atomic(size_t nmem, size_t size);
void *GC_realloc_atomic(void *ptr, size_t size);

/* GC equivalent of malloc(), calloc(), and realloc() except with explicit
   destructor and marker functions */
void *GC_alloc_opt(size_t size, void (*dtor)(void*), void (*mrk)(void (void*, void*), void*, void*));
void *GC_calloc_opt(size_t nmem, size_t size, void (*dtor)(void*), void (*mrk)(void (void*, void*), void*, void*));
void *GC_realloc_opt(void *ptr, size_t size, void (*dtor)(void*), void (*mrk)(void (void*, void*), void*, void*));

/* Manually free pointer */
void GC_free(void *ptr);

/* Manually run garbage collection */
void GC_collect();

/* Register destructor and marker functions for objects.  */
void GC_register_dtor(void *ptr, void (*func)(void*, void*));
void GC_register_mrk(void *ptr, void (*func)(void (void*,void*),void*,void*));

/* Register object as a root (never garbage collected) */
void GC_register_root(void *ptr);

/* GC info */
size_t GC_get_allocated();
size_t GC_get_reachable();
size_t GC_get_collectable();

/* Signals to the GC that the object contains no pointers */
void GC_atomic_mrk(void (*func)(void*, void*), void* gc, void* ptr);

/* Forces compiler to store start of array on stack or in registers 
   Compiling with optimization flags may purposely lose track of array.
   Calls GC_atomic_mrk() on start of array which is essentially a no-op.
   This macro should only be used for arrays that will never be used outside
   the caller and the first element is only referenced once.
   In this case, invoke after the array is no longer needed. */
#define GC_REGISTER_LOCAL_ARRAY(arr)     GC_atomic_mrk(NULL, NULL, arr);

#endif
