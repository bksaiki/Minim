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

minim_object *make_hashtable2(minim_object *hash_fn, minim_object *equiv_fn, size_t size_hint) {
    minim_hashtable_object *o;
    size_t *alloc_ptr;

    for (alloc_ptr = start_size_ptr; *alloc_ptr < size_hint; ++alloc_ptr);

    o = GC_alloc(sizeof(minim_hashtable_object));
    o->type = MINIM_HASHTABLE_TYPE;
    o->alloc_ptr =  alloc_ptr;
    o->alloc = *o->alloc_ptr;
    o->size = 0;
    o->buckets = GC_calloc(o->alloc, sizeof(minim_object*));
    o->hash = hash_fn;
    o->equiv = equiv_fn;

    return ((minim_object *) o);
}

minim_object *copy_hashtable(minim_object *src) {
    minim_hashtable_object *o, *ht;
    minim_object *b, *it, *t;
    uint64_t i;

    ht = ((minim_hashtable_object *) src);
    
    o = GC_alloc(sizeof(minim_hashtable_object));
    o->type = MINIM_HASHTABLE_TYPE;
    o->alloc_ptr = ht->alloc_ptr;
    o->alloc = ht->alloc;
    o->size = ht->size;
    o->buckets = GC_alloc(o->alloc * sizeof(minim_object*));
    o->hash = ht->hash;
    o->equiv = ht->equiv;

    for (i = 0; i < ht->alloc; ++i) {
        b = minim_hashtable_bucket(ht, i);
        if (b) {
            t = make_pair(make_pair(minim_caar(b), minim_cdar(b)), minim_null);
            minim_hashtable_bucket(o, i) = t;
            for (it = minim_cdr(b); !minim_is_null(it); it = minim_cdr(it)) {
                minim_cdr(t) = make_pair(make_pair(minim_caar(it), minim_cdar(it)), minim_null);
                t = minim_cdr(t);
            }
        } else {
            minim_hashtable_bucket(o, i) = NULL;
        }
    }

    return ((minim_object *) o);
}

void hashtable_clear(minim_object *o) {
    minim_hashtable_object *ht;

    ht = ((minim_hashtable_object *) o);
    ht->alloc_ptr = start_size_ptr;
    ht->alloc = *ht->alloc_ptr;
    ht->size = 0;
    ht->buckets = GC_calloc(ht->alloc, sizeof(minim_object *));
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
    minim_object *i, *proc, *env;

    proc = minim_hashtable_hash(ht);
    if (minim_is_prim_proc(proc) && minim_prim_proc(proc) == eq_hash_proc) {
        return eq_hash(k);
    } else if (minim_is_prim_proc(proc) && minim_prim_proc(proc) == equal_hash_proc) {
        return equal_hash(k);
    } else {
        assert_no_call_args();
        env = global_env(current_thread());   // TODO: this seems problematic
        i = call_with_args(minim_hashtable_hash(ht), env);
        if (!minim_is_fixnum(i)) {
            fprintf(stderr, "hash function associated with hash table ");
            write_object(stderr, ht);
            fprintf(stderr, " did not return a fixnum");
            minim_shutdown(1);
        }

        return minim_fixnum(i);
    }
}

static int key_equiv(minim_object *ht, minim_object *k1, minim_object *k2) {
    minim_object *env, *proc;

    proc = minim_hashtable_equiv(ht);
    if (minim_is_prim_proc(proc) && minim_prim_proc(proc) == eq_proc) {
        return minim_is_eq(k1, k2);
    } else if (minim_is_prim_proc(proc) && minim_prim_proc(proc) == equal_proc) {
        return minim_is_equal(k1, k2);
    } else {
        assert_no_call_args();
        push_call_arg(k1);
        push_call_arg(k2);

        env = global_env(current_thread());   // TODO: this seems problematic
        return !minim_is_false(call_with_args(minim_hashtable_equiv(ht), env));
    }
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
                    buckets[idx] = make_pair(minim_car(it), (buckets[idx] ? buckets[idx] : minim_null));
                }
            }
        }

        minim_hashtable_alloc_ptr(ht) = alloc_ptr;
        minim_hashtable_alloc(ht) = *alloc_ptr;
        minim_hashtable_buckets(ht) = buckets;
    }
}

int hashtable_set(minim_object *ht, minim_object *k, minim_object *v) {
    minim_object *b, *bi;
    uint64_t i;

    hashtable_opt_resize(ht);
    i = hash_key(ht, k) % minim_hashtable_alloc(ht);
    b = minim_hashtable_bucket(ht, i);
    if (b) {
        for (bi = b; !minim_is_null(bi); bi = minim_cdr(bi)) {
            if (key_equiv(ht, minim_caar(bi), k)) {
                minim_cdar(bi) = v;
                return 1;
            }
        }
    } else {
        b = minim_null;
    }

    minim_hashtable_bucket(ht, i) = make_pair(make_pair(k, v), b);
    ++minim_hashtable_size(ht);
    return 0;
}

static int hashtable_delete(minim_object *ht, minim_object *k) {
    minim_object *b, *bi;
    uint64_t i;

    i = hash_key(ht, k) % minim_hashtable_alloc(ht);
    b = minim_hashtable_bucket(ht, i);
    if (b) {
        for (bi = b; !minim_is_null(bi); bi = minim_cdr(bi)) {
            if (key_equiv(ht, minim_caar(bi), k)) {
                if (bi == b) {
                    minim_hashtable_bucket(ht, i) = NULL;
                } else {
                    for (; minim_cdr(b) != b; b = minim_cdr(b));
                    minim_cdr(b) = minim_cdr(bi);
                }
                
                --minim_hashtable_size(ht);
                return 1;
            }
        }
    }

    return 0;
}

minim_object *hashtable_find(minim_object *ht, minim_object *k) {
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

minim_object *hashtable_keys(minim_object *ht) {
    minim_object *b, *ks;
    uint64_t i;

    ks = minim_null;
    for (i = 0; i < minim_hashtable_alloc(ht); ++i) {
        b = minim_hashtable_bucket(ht, i);
        if (b) {
            for (; !minim_is_null(b); b = minim_cdr(b))
                ks = make_pair(minim_caar(b), ks);
        }
    }

    return ks;
}

static void key_not_found_exn(const char *name, minim_object *k) {
    fprintf(stderr, "%s: could not find key ", name);
    write_object(stderr, k);
    fprintf(stderr, "\n");
    minim_shutdown(1);
}

//
//  Primitives
//

minim_object *is_hashtable_proc(int argc, minim_object **args) {
    // (-> any boolean)
    return (minim_is_hashtable(args[0]) ? minim_true : minim_false);
}

minim_object *make_eq_hashtable_proc(int argc, minim_object **args) {
    // (-> hashtable)
    return make_hashtable(make_prim_proc(eq_hash_proc, "eq-hash", 1, 1),
                          make_prim_proc(eq_proc, "eq?", 2, 2));
}

minim_object *make_hashtable_proc(int argc, minim_object **args) {
    // (-> hashtable)
    return make_hashtable(make_prim_proc(equal_hash_proc, "equal-hash", 1, 1),
                          make_prim_proc(equal_proc, "equal?", 2, 2));
}

minim_object *hashtable_size_proc(int argc, minim_object **args) {
    // (-> hashtable any any void)
    minim_object *ht;

    ht = args[0];
    if (!minim_is_hashtable(ht))
        bad_type_exn("hashtable-set!", "hashtable?", ht);
    return make_fixnum(minim_hashtable_size(ht));
}

minim_object *hashtable_contains_proc(int argc, minim_object **args) {
    // (-> hashtable any boolean)
    minim_object *ht, *k;

    ht = args[0];
    if (!minim_is_hashtable(ht))
        bad_type_exn("hashtable-contains?", "hashtable?", ht);

    k = args[1];
    return (minim_is_null(hashtable_find(ht, k)) ? minim_false : minim_true);
}

minim_object *hashtable_set_proc(int argc, minim_object **args) {
    // (-> hashtable any any void)
    minim_object *ht, *k, *v;

    ht = args[0];
    if (!minim_is_hashtable(ht))
        bad_type_exn("hashtable-set!", "hashtable?", ht);

    k = args[1];
    v = args[2];
    hashtable_set(ht, k, v);
    return minim_void;
}

minim_object *hashtable_delete_proc(int argc, minim_object **args) {
    // (-> hashtable any void)
    minim_object *ht, *k;

    ht = args[0];
    if (!minim_is_hashtable(ht))
        bad_type_exn("hashtable-delete!", "hashtable?", ht);

    k = args[1];
    if (!hashtable_delete(ht, k))
        key_not_found_exn("hashtable-delete!", k);

    return minim_void;
}

minim_object *hashtable_update_proc(int argc, minim_object **args) {
    // (-> hashtable any (-> any any) void)
    // (-> hashtable any (-> any any) any void)
    minim_object *ht, *k, *proc, *b, *env;

    ht = args[0];
    if (!minim_is_hashtable(ht))
        bad_type_exn("hashtable-update!", "hashtable?", ht);

    k = args[1];
    proc = args[2];
    if (!minim_is_proc(proc))
        bad_type_exn("hashtable-update!", "procedure?", proc);

    b = hashtable_find(ht, k);
    if (minim_is_null(b)) {
        if (argc == 3) {
            // no failure result provided
            key_not_found_exn("hashtable-update!", k);
        } else {
            // user-provided failure
            minim_object *fail, *v;

            fail = args[3];
            env = global_env(current_thread());     // TODO: this seems problematic
            if (!minim_is_proc(fail)) {
                b = fail;
            } else {
                assert_no_call_args();
                b = call_with_args(fail, env);
            }

            assert_no_call_args();
            push_call_arg(b);
            v = call_with_args(proc, env);
            hashtable_set(ht, k, v);
        }
    } else {
        assert_no_call_args();
        push_call_arg(minim_cdr(b));
        env = global_env(current_thread());     // TODO: this seems problematic
        minim_cdr(b) = call_with_args(proc, env);
    }

    return minim_void;
}

minim_object *hashtable_ref_proc(int argc, minim_object **args) {
    // (-> hashtable any any)
    // (-> hashtable any any any)
    minim_object *ht, *k, *b;

    ht = args[0];
    if (!minim_is_hashtable(ht))
        bad_type_exn("hashtable-ref", "hashtable?", ht);

    k = args[1];
    b = hashtable_find(ht, k);
    if (minim_is_null(b)) {
        if (argc == 2) {
            // no failure result provided
            key_not_found_exn("hashtable-ref", k);
            write_object(stderr, k);
            fprintf(stderr, "\n");
            minim_shutdown(1);
        } else {
            // user-provided failure
            minim_object *fail, *env;

            fail = args[2];
            if (!minim_is_proc(fail))
                return fail;
            
            assert_no_call_args();
            env = global_env(current_thread());   // TODO: this seems problematic
            return call_with_args(fail, env);
        }
    }

    return minim_cdr(b);
}

minim_object *hashtable_keys_proc(int argc, minim_object **args) {
    // (-> hashtable (listof any))
    minim_object *ht;

    ht = args[0];
    if (!minim_is_hashtable(ht))
        bad_type_exn("hashtable-keys", "hashtable?", ht);
    return hashtable_keys(ht);
}

minim_object *hashtable_copy_proc(int argc, minim_object **args) {
    // (-> hashtable void)
    minim_object *ht;

    ht = args[0];
    if (!minim_is_hashtable(ht))
        bad_type_exn("hashtable-copy", "hashtable?", ht);
    return copy_hashtable(ht);
}

minim_object *hashtable_clear_proc(int argc, minim_object **args) {
    // (-> hashtable void)
    minim_object *ht;

    ht = args[0];
    if (!minim_is_hashtable(ht))
        bad_type_exn("hashtable-clear!", "hashtable?", ht);
    hashtable_clear(ht);
    return minim_void;
}

minim_object *eq_hash_proc(int argc, minim_object **args) {
    return make_fixnum(eq_hash(args[0]));
}

minim_object *equal_hash_proc(int argc, minim_object **args) {
    return make_fixnum(equal_hash(args[0]));
}
