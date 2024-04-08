/*
    Hashtables
*/

#include "../minim.h"

//
//  Hashing
//

#define hash_init       5381L

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
        return hash_bytes(&o, sizeof(mobj), hash);
    }
}

size_t eq_hash(mobj o) {
    return eq_hash2(o, hash_init);
}

//
//  Hashtables
//

#define start_size_ptr                      (&bucket_sizes[0])

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

static mobj hashtable_copy2(mobj ht) {
    mobj o, hd, tl, b;
    
    o = GC_alloc(minim_hashtable_size);
    minim_type(o) = MINIM_OBJ_HASHTABLE;
    minim_hashtable_alloc_ptr(o) = minim_hashtable_alloc_ptr(ht);
    minim_hashtable_buckets(o) = Mvector(minim_hashtable_alloc(o), NULL);
    minim_hashtable_count(o) = minim_hashtable_count(ht);

    for (long i = 0; i < minim_hashtable_alloc(o); i++) {
        b = minim_hashtable_bucket(ht, i);
        if (minim_nullp(b)) {
            minim_hashtable_bucket(o, i) = minim_null;
        } else {
            hd = tl = Mcons(Mcons(minim_caar(b), minim_cdar(b)), NULL);
            for (b = minim_cdr(b); !minim_nullp(b); b = minim_cdr(b)) {
                minim_cdr(tl) = Mcons(Mcons(minim_caar(b), minim_cdar(b)), NULL);
                tl = minim_cdr(tl);
            }

            minim_cdr(tl) = minim_null;
            minim_hashtable_bucket(o, i) = hd;
        }
    }

    return o;
}

mobj eq_hashtable_find(mobj ht, mobj k) {
    return eq_hashtable_find2(ht, k, eq_hash(k));
}

mobj eq_hashtable_find2(mobj ht, mobj k, size_t h) {
    mobj b, bi;
    size_t i;

    i = h % minim_hashtable_alloc(ht);
    b = minim_hashtable_bucket(ht, i);
    for (bi = b; !minim_nullp(bi); bi = minim_cdr(bi)) {
        if (minim_eqp(minim_caar(bi), k)) {
            return minim_car(bi);
        }
    }
    
    return minim_false;
}

static void eq_hashtable_resize(mobj ht) {
    mobj nb, b;
    size_t *alloc_ptr;
    size_t idx;
    long i;
    
    alloc_ptr = start_size_ptr;
    for (; *alloc_ptr < minim_hashtable_count(ht); ++alloc_ptr);

    nb = Mvector(*alloc_ptr, minim_null);
    for (i = 0; i < minim_hashtable_alloc(ht); i++) {
        for (b = minim_hashtable_bucket(ht, i); !minim_nullp(b); b = minim_cdr(b)) {
            idx = eq_hash(minim_caar(b)) % *alloc_ptr;
            minim_vector_ref(nb, idx) = Mcons(minim_car(b), minim_vector_ref(nb, idx));
        }
    }

    minim_hashtable_alloc_ptr(ht) = alloc_ptr;
    minim_hashtable_buckets(ht) = nb;
}

void eq_hashtable_set(mobj ht, mobj k, mobj v) {
    mobj b, bi;
    size_t i;

    i = eq_hash(k) % minim_hashtable_alloc(ht);
    b = minim_hashtable_bucket(ht, i);
    for (bi = b; !minim_nullp(bi); bi = minim_cdr(bi)) {
        if (minim_eqp(minim_caar(bi), k)) {
            minim_cdar(bi) = v;
            return;
        }
    }

    minim_hashtable_bucket(ht, i) = Mcons(Mcons(k, v), b);
    minim_hashtable_count(ht)++;
    if (minim_hashtable_count(ht) > minim_hashtable_alloc(ht)) {
        eq_hashtable_resize(ht);
    }
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

mobj hashtable_copy(mobj ht) {
    // (-> hashtable hashtable)
    return hashtable_copy2(ht);
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
    minim_hashtable_buckets(ht) = Mvector(minim_hashtable_alloc(ht), minim_null);
    minim_hashtable_count(ht) = 0;
    return minim_void;
}

mobj eq_hash_proc(mobj x) {
    // (-> any fixnum)
    return Mfixnum(eq_hash(x));
}

mobj equal_hash_proc(mobj x) {
    // (-> any fixnum)
    size_t hc;
    if (minim_symbolp(x)) {
        hc = hash_bytes(minim_symbol(x), strlen(minim_symbol(x)), hash_init);
    } else if (minim_string(x)) {
        hc = hash_bytes(minim_string(x), strlen(minim_string(x)), hash_init);
    } else {
        hc = eq_hash(x);
    }

    return Mfixnum(hc);
}
