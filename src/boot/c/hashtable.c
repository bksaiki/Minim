/*
    Hashtables
*/

#include "../minim.h"

#define hash_init       5381L

#define MINIM_HASHTABLE_LOAD_FACTOR         0.75
#define start_size_ptr                      (&bucket_sizes[0])
#define load_factor(s, a)                   ((double)(s) / (double)(a))

minim_object *make_hashtable(minim_object *hash_fn, minim_object *equiv_fn) {
    minim_hashtable_object *o;
    
    o = GC_alloc(sizeof(minim_hashtable_object));
    o->type = MINIM_HASHTABLE_TYPE;
    o->alloc_ptr = start_size_ptr;
    o->alloc = *o->alloc_ptr;
    o->size = 0;
    o->buckets = GC_calloc(o->alloc, sizeof(minim_object*));
    o->hash = hash_fn;
    o->equiv = equiv_fn;

    return ((minim_object *) o);
}

static uint64_t hash_bytes(const void *data, size_t len, uint64_t hash0) {
    const char *str;
    uint64_t hash;
    size_t i;
    
    hash = hash0;
    str = (const char*) data;
    for (i = 0; i < len; ++i)
        hash = ((hash << 6) + hash) + str[i]; /* hash * 65 + c */

    // preserve only 62 bits
    return hash >> 2;
}


uint64_t eq_hash2(minim_object *o, uint64_t hash) {
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

static uint64_t equal_hash2(minim_object *o, uint64_t hash) {
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
            it = minim_hashtable_bucket(o, i);
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

static uint64_t eq_hash(minim_object *o) {
    return eq_hash2(o, hash_init);
}

static uint64_t equal_hash(minim_object *o) {
    return equal_hash2(o, hash_init);
}

static uint64_t hash_key(minim_object *ht, minim_object *k) {
    minim_object *i, *env;

    env = global_env(current_thread());   // TODO: this seems problematic
    i = call_with_args(minim_hashtable_hash(ht), make_pair(k, minim_null), env);
    if (!minim_is_fixnum(i)) {
        fprintf(stderr, "hash function associated with hash table ");
        write_object(stderr, ht);
        fprintf(stderr, " did not return a fixnum");
        exit(1);
    }

    return minim_fixnum(i);
}

static int key_equiv(minim_object *ht, minim_object *k1, minim_object *k2) {
    minim_object *eq, *env;

    env = global_env(current_thread());   // TODO: this seems problematic
    eq = call_with_args(minim_hashtable_equiv(ht), make_pair(k1, make_pair(k2, minim_null)), env);
    return !minim_is_false(eq);
}

static void hashtable_opt_resize(minim_object *ht) {
    minim_object **buckets, *it;
    uint64_t *alloc_ptr;
    uint64_t alloc, sz, i, idx;

    sz = minim_hashtable_size(ht);
    alloc = minim_hashtable_alloc(ht);
    if (load_factor(sz, alloc) > MINIM_HASHTABLE_LOAD_FACTOR) {
        alloc_ptr = minim_hashtable_alloc_ptr(ht);
        for (; load_factor(sz, *alloc_ptr) > MINIM_HASHTABLE_LOAD_FACTOR; ++alloc_ptr);

        buckets = GC_calloc(*alloc_ptr, sizeof(minim_object *));
        for (i = 0; i < minim_hashtable_alloc(ht); ++i) {
            it = minim_hashtable_bucket(ht, i);
            if (it) {
                for (; !minim_is_null(it); it = minim_cdr(it)) {
                    idx = hash_key(ht, minim_caar(it)) % *alloc_ptr;
                    buckets[idx] = make_pair(make_pair(minim_caar(it), minim_cdar(it)),
                                             buckets[idx] ? buckets[idx] : minim_null);
                }
            }
        }

        minim_hashtable_alloc_ptr(ht) = alloc_ptr;
        minim_hashtable_alloc(ht) = *alloc_ptr;
        minim_hashtable_buckets(ht) = buckets;
    }
}

static void hashtable_set(minim_object *ht, minim_object *k, minim_object *v) {
    minim_object *b, *bi;
    uint64_t i;

    i = hash_key(ht, k) % minim_hashtable_alloc(ht);
    b = minim_hashtable_bucket(ht, i);
    if (b) {
        for (bi = b; !minim_is_null(bi); bi = minim_cdr(bi)) {
            if (key_equiv(ht, minim_caar(bi), k)) {
                minim_cdar(bi) = v;
                return;
            }
        }
    } else {
        b = minim_null;
    }

    hashtable_opt_resize(ht);
    minim_hashtable_bucket(ht, i) = make_pair(make_pair(k, v), b);
    ++minim_hashtable_size(ht);
}

static minim_object *hashtable_find(minim_object *ht, minim_object *k) {
    minim_object *b;
    uint64_t i;

    i = hash_key(ht, k) % minim_hashtable_alloc(ht);
    b = minim_hashtable_bucket(ht, i);
    if (b) {
        for (; !minim_is_null(b); b = minim_cdr(b)) {
            if (key_equiv(ht, minim_caar(b), k))
                return minim_car(b);
        }
    }

    return minim_null;
}

//
//  Primitives
//

minim_object *is_hashtable_proc(minim_object *args) {
    // (-> any boolean)
    return (minim_is_hashtable(minim_car(args)) ? minim_true : minim_false);
}

minim_object *make_eq_hashtable_proc(minim_object *args) {
    // (-> hashtable)
    return make_hashtable(make_prim_proc(eq_hash_proc, "eq-hash", 1, 1),
                          make_prim_proc(eq_proc, "eq?", 2, 2));
}

minim_object *hashtable_size_proc(minim_object *args) {
    // (-> hashtable any any void)
    minim_object *ht;

    ht = minim_car(args);
    if (!minim_is_hashtable(ht))
        bad_type_exn("hashtable-set!", "hashtable?", ht);
    return make_fixnum(minim_hashtable_size(ht));
}

minim_object *hashtable_set_proc(minim_object *args) {
    // (-> hashtable any any void)
    minim_object *ht, *k, *v;

    ht = minim_car(args);
    if (!minim_is_hashtable(ht))
        bad_type_exn("hashtable-set!", "hashtable?", ht);

    k = minim_cadr(args);
    v = minim_car(minim_cddr(args));
    hashtable_set(ht, k, v);
    return minim_void;
}

minim_object *hashtable_ref_proc(minim_object *args) {
    // (-> hashtable any any)
    minim_object *ht, *k, *b;

    ht = minim_car(args);
    if (!minim_is_hashtable(ht))
        bad_type_exn("hashtable-ref", "hashtable?", ht);

    k = minim_cadr(args);
    b = hashtable_find(ht, k);
    if (minim_is_null(b)) {
        fprintf(stderr, "hashtable-ref: could not find key ");
        write_object(stderr, k);
        fprintf(stderr, "\n");
        exit(1);
    }

    return minim_cdr(b);
}

minim_object *eq_hash_proc(minim_object *args) {
    return make_fixnum(eq_hash(minim_car(args)));
}

minim_object *equal_hash_proc(minim_object *args) {
    return make_fixnum(equal_hash(minim_car(args)));
}
