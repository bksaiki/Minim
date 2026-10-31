// hash.c: hashing

#include "minim.h"

#define hash_init       5381L

static size_t hash_bytes2(const void *data, size_t len, size_t hash0) {
    const char *str;
    size_t hash;
    size_t i;
    
    hash = hash0;
    str = (const char*) data;
    for (i = 0; i < len; ++i)
        hash = ((hash << 6) + hash) + str[i]; /* hash * 65 + c */

    // preserve only 62 bits
    return hash >> 2;
}

size_t hash_bytes(const void *data, size_t len) {
    return hash_bytes2(data, len, hash_init);
}
