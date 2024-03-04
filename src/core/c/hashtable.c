/*
    Hashtables
*/

#include "../minim.h"

#define hash_init       5381L

#define MINIM_HASHTABLE_LOAD_FACTOR         0.75
#define start_size_ptr                      (&bucket_sizes[0])
#define load_factor(s, a)                   ((double)(s) / (double)(a))

mobj Mhashtable(size_t size_hint) {
    size_t *alloc_ptr;
    mobj o;

    alloc_ptr = start_size_ptr;
    for (; *alloc_ptr < size_hint; ++alloc_ptr);

    o = GC_alloc(minim_hashtable_size);
    minim_type(o) = MINIM_OBJ_HASHTABLE;
    minim_hashtable_alloc_ptr(o) = alloc_ptr;
    minim_hashtable_buckets(o) = Mvector(minim_hashtable_alloc(o), minim_null);
    minim_hashtable_count(o) = 0;
    return o;
}

static size_t hash_bytes(const void *data, size_t len, size_t hash0) {
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

static size_t eq_hash2(mobj o, size_t hash) {
    switch (minim_type(o))
    {
    case MINIM_OBJ_FIXNUM:
        return hash_bytes(&minim_fixnum(o), sizeof(minim_fixnum(o)), hash);
    case MINIM_OBJ_CHAR:
        return hash_bytes(&minim_char(o), sizeof(minim_char(o)), hash);
    default:
        return hash_bytes(&o, sizeof(mobj ), hash);
    }
}

size_t eq_hash(mobj o) {
    return eq_hash2(o, hash_init);
}

//
//  Primitives
//

mobj hashtablep_proc(mobj x) {
    // (-> any boolean)
    return minim_hashtablep(x) ? minim_true : minim_false;
}

mobj make_hashtable(mobj size) {
    // (-> integer hashtable)
    return Mhashtable(minim_fixnum(size));
}

mobj hashtable_size(mobj ht) {
    // (-> hashtable integer)
    return Mfixnum(minim_hashtable_count(ht));
}

mobj hashtable_size_set(mobj ht, mobj size) {
    // (-> hashtable integer void)
    minim_hashtable_count(ht) = minim_fixnum(size);
    return minim_void;
}

mobj hashtable_length(mobj ht) {
    // (-> hashtable integer)
    return Mfixnum(minim_hashtable_alloc(ht));
}

mobj hashtable_ref(mobj ht, mobj h) {
    // (-> hashtable integer (listof cons))
    size_t i = minim_fixnum(h) % minim_hashtable_alloc(ht);
    return minim_hashtable_bucket(ht, i);
}

mobj hashtable_set(mobj ht, mobj h, mobj cells) {
    // (-> hashtable integer (listof cons) void)
    size_t i = minim_fixnum(h) % minim_hashtable_alloc(ht);
    minim_hashtable_bucket(ht, i) = cells;
    return minim_void;
}

mobj hashtable_cells(mobj ht) {
    // (-> hashtable (listof cons)
    mobj cells = minim_null;
    for (long i = 0; i < minim_hashtable_alloc(ht); i++) {
        for (mobj b = minim_hashtable_bucket(ht, i); !minim_nullp(b); b = minim_cdr(b))
            cells = Mcons(minim_car(b), cells);
    }

    return cells;
}

mobj hashtable_keys(mobj ht) {
    // (-> hashtable (listof any))
    mobj keys = minim_null;
    for (long i = 0; i < minim_hashtable_alloc(ht); i++) {
        for (mobj b = minim_hashtable_bucket(ht, i); !minim_nullp(b); b = minim_cdr(b))
            keys = Mcons(minim_caar(b), keys);
    }

    return keys;
}

mobj hashtable_clear(mobj ht) {
    // (-> hashtable void)
    minim_hashtable_alloc_ptr(ht) = start_size_ptr;
    minim_hashtable_buckets(ht) = GC_alloc(minim_hashtable_alloc(ht) * sizeof(mobj));
    for (long i = 0; i < minim_hashtable_alloc(ht); i++)
        minim_hashtable_bucket(ht, i) = minim_null;

    return minim_void;
}

mobj eq_hash_proc(mobj x) {
    return Mfixnum(eq_hash(x));
}

mobj equal_hash_proc(mobj x) {
    return Mfixnum(eq_hash(x));
}
