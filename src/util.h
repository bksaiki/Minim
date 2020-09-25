#ifndef _MINIM_UTIL_H_
#define _MINIM_UTIL_H_

#include "base.h"

// C version of std::all_of in <algorithm>
int all_of(void *arr, size_t num, size_t size, int (*pred)(const void*));

// C version of std::any_of in <algorithm>
int any_of(void *arr, size_t num, size_t size, int (*pred)(const void*));

void *for_first(void *arr, size_t num, size_t size, int (*pred)(const void*));

// Calls 'proc' on each elements in the array.
void for_each(void *arr, size_t num, size_t size, void (*proc)(const void*));

#endif