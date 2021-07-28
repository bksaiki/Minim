# Minim-GC
Garbage collector developed for the [Minim](https://github.com/bksaiki/Minim) project.
The Minim-GC is a conservative, generational, thread-local, Mark&Sweep garbage collector.
It only works for 64-bit programs and is less efficient since it calls the underlying `malloc`.
Much of this work is based on the [Tiny Garbage Collector](https://github.com/orangeduck/tgc) which
  in turn, borrows from the garbage collector from [Cello](https://github.com/orangeduck/Cello).

## Usage
```c
void GC_init(void *stack);
```
Starts the garbage collector in the current thread, marking the beginning of the stack with `stack`.
The garbage collector is guaranteed to scan the stack from the stack pointer to this variable.

```c
void GC_finalize();
```
Stops the garbage collector and frees any remaining memory.

### Allocation
```c
void *GC_malloc(size_t size);
```
Allocates at least `size` bytes of memory that will be collected when
  no pointer references it.
  
```c
void *GC_calloc(size_t nmem, size_t size);
```
Allocates an array of `nmem` elements, each of which is `size` bytes.
Initializes all bits to 0.
Like `GC_malloc(size)`, this memory will be garbage collected.
  
```c
void *GC_realloc(void *ptr, size_t size);
```
Changes the size of the memory block pointed to by `ptr`.
If `ptr` is not a valid memory block, a new block will be allocated.

```c
void *GC_malloc_atomic(size_t size);
void *GC_calloc_atomic(size_t nmem, size_t size);
void *GC_realloc_atomic(void *ptr, size_t size);
```
Like `GC_malloc`, `GC_calloc`, and `GC_realloc`, but the user promises the memory
  block contains no pointers.
 Useful for strings and pointerless structs.

```c
void *GC_malloc_opt(size_t size, void (*dtor)(void*), void (*mrk)(void (void*,void*), void*, void*));
void *GC_calloc_opt(size_t nmem, size_t size, void (*dtor)(void*), void (*mrk)(void (void*,void*), void*, void*));
void *GC_realloc_opt(void *ptr, size_t size, void (*dtor)(void*), void (*mrk)(void (void*,void*), void*, void*));
```
Like `GC_malloc`, `GC_calloc`, and `GC_realloc`, but destructors and markers can be
  explicitly registered.
Destructors frees any internal memory not allocated by the garbage collector.
Markers explicitly mark internal pointers rather than marking every 8-byte value
  within the memory block (see `GC_register_mrk`).

### Deallocation
```c
void GC_free(void *ptr);
```
Manually frees the memory block at `ptr`. Runs the destructor by the memory block if registered.

```c
void GC_collect();
void GC_collect_minor();
```
Manually runs a full and minor cycle of garbage collection.

### Destructors and Markers
```c
GC_register_dtor(void *ptr, void(*func)(void*));
```
Registers a destructor that will be called on the memory block at `ptr` when
  the block becomes unreachable.
  
```c
GC_register_mrk(void *ptr, void(*func)(void(*mrk)(void*, void*),void *gc,void *ptr));
```
Registers a marker that will be called on the memory block during a cycle of garabage collection.
The marker calls `mrk(gc, internal_ptr)` on each internal pointer within the memory block
  stored at `ptr`.
  
```c
GC_atomic_mrk(void(*func)(void*,void*),void*,void*);
```
Signals to the garbage collector that the memory block contains no internal pointers.

### Miscellaneous
```c
size_t GC_get_allocated();
size_t GC_get_reachable();
size_t GC_get_collectable();
```
Memory usage statistics.
