#include <stddef.h>
#include "util.h"

int all_of(void *arr, size_t num, size_t size, int (*pred)(const void*))
{
    char *it;
    for (size_t i = 0; i < num; ++i)
    {
        it = (char*)arr + i * size;
        if (!pred(it))  return 0;
    }

    return 1;
}

int any_of(void *arr, size_t num, size_t size, int (*pred)(const void*))
{
    char *it;
    for (size_t i = 0; i < num; ++i)
    {
        it = (char*)arr + i * size;
        if (pred(it))  return 1;
    }

    return 0;
}

void *for_first(void *arr, size_t num, size_t size, int (*pred)(const void*))
{
    char *it;
    for (size_t i = 0; i < num; ++i)
    {
        it = (char*)arr + i * size;
        if (pred(it))  return it;
    }

    return NULL;
}

void for_each(void *arr, size_t num, size_t size, void (*proc)(const void*))
{
    char *it, *base = (char*)arr;
    for (size_t i = 0; i < num; ++i)
    {
        it = (char*)(base + size * i);
        proc(it);
    }
}