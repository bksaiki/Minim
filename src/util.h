#ifndef _MINIM_UTIL_H_
#define _MINIM_UTIL_H_

// C version of std::all_of in <algorithm>
int all_of(void *arr, size_t num, size_t size, int (*pred)(const void*));

// Calls 'proc' on each elements in the array.
void for_each(void *arr, size_t num, size_t size, void (*proc)(const void*));

#endif