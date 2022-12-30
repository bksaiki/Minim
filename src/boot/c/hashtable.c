/*
    Hashtables
*/

#include "../minim.h"

#define hash_init       5381

#define MINIM_INTERN_TABLE_LOAD_FACTOR     0.75
#define start_size_ptr                     (&bucket_sizes[0])
#define load_factor(s, a)                  ((double)(s) / (double)(a))

minim_object *make_hashtable(void *hash_fn) {
    minim_hashtable_object *o = GC_alloc(sizeof(minim_hashtable_object));
    o->type = MINIM_HASHTABLE_TYPE;
    o->alloc_ptr = start_size_ptr;
    o->alloc = *o->alloc_ptr;
    o->size = 0;
    o->buckets = GC_calloc(o->alloc, sizeof(minim_object*));
    o->hash_fn = hash_fn;
    return ((minim_object *) o);
}

static uint32_t hash_bytes(const void *data, size_t len, uint32_t hash0) {
    const char *str;
    size_t i;
    uint32_t hash;
    
    hash = hash0;
    str = (const char*) data;
    for (i = 0; i < len; ++i)
        hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */

    return hash;
}


uint32_t eq_hash2(minim_object *o, uint32_t hash) {
    switch (o->type)
    {
    case MINIM_FIXNUM_TYPE:
        return hash_bytes(&minim_fixnum(o), sizeof(minim_fixnum(o)), hash);
    case MINIM_CHAR_TYPE:
        return hash_bytes(&minim_char(o), sizeof(minim_char(o)), hash);
    default:
        return hash_bytes(&o, sizeof(minim_object *), hash);
    }
}

static uint32_t equal_hash2(minim_object *o, uint32_t hash) {
    minim_object *it;
    long i;

    switch (o->type)
    {
    case MINIM_SYMBOL_TYPE:
        return hash_bytes(&minim_symbol(o), strlen(minim_symbol(o)), hash);
    case MINIM_STRING_TYPE:
        return hash_bytes(&minim_string(o), strlen(minim_string(o)), hash);
    case MINIM_PAIR_TYPE:
        return equal_hash2(minim_cdr(o), equal_hash2(minim_car(o), hash));
    case MINIM_VECTOR_TYPE:
        for (i = 0; i < minim_vector_len(o); ++i)
            hash = equal_hash2(minim_vector_ref(o, i), hash);
        return hash;
    case MINIM_HASHTABLE_TYPE:
        for (i = 0; i < minim_hashtable_alloc(o); ++i) {
            it = minim_hashtable_buckets(o)[i];
            if (it) {
                for (; !minim_is_null(it); it = minim_cdr(it)) {
                    hash = equal_hash2(minim_caar(it), hash);
                    hash = equal_hash2(minim_cdar(it), hash);
                }
            }
        }
        return hash;
    default:
        return eq_hash2(o, hash);
    }
}

uint32_t eq_hash(minim_object *o) {
    return eq_hash2(o, hash_init);
}

uint32_t equal_hash(minim_object *o) {
    return equal_hash2(o, hash_init);
}

//
//  Primitives
//

minim_object *is_hashtable_proc(minim_object *args) {
    return (minim_is_hashtable(minim_car(args)) ? minim_true : minim_false);
}

minim_object *make_eq_hashtable_proc(minim_object *args) {
    return make_hashtable(eq_hash);
}
