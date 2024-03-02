/*
    Hashtables
*/

#include "../minim.h"

#define hash_init       5381L

#define MINIM_HASHTABLE_LOAD_FACTOR         0.75
#define start_size_ptr                      (&bucket_sizes[0])
#define load_factor(s, a)                   ((double)(s) / (double)(a))

mobj Mhashtable2(mobj hash_fn, mobj equiv_fn, size_t size_hint) {
    mobj o = GC_alloc(minim_hashtable_size);
    size_t *alloc_ptr = start_size_ptr;

    for (; *alloc_ptr < size_hint; ++alloc_ptr);
    minim_type(o) = MINIM_OBJ_HASHTABLE;
    minim_hashtable_alloc_ptr(o) = alloc_ptr;
    minim_hashtable_count(o) = 0;
    minim_hashtable_buckets(o) = GC_calloc(minim_hashtable_alloc(o), sizeof(mobj));
    minim_hashtable_hash(o) = hash_fn;
    minim_hashtable_equiv(o) = equiv_fn;
    return o;
}

mobj Mhashtable(mobj hash_fn, mobj equiv_fn) {
    return Mhashtable2(hash_fn, equiv_fn, 0);
}


static mobj hashtable_copy(const mobj src) {
    mobj dst, b, it, t;

    dst = GC_alloc(minim_hashtable_size);
    minim_type(dst) = MINIM_OBJ_HASHTABLE;
    minim_hashtable_alloc_ptr(dst) = minim_hashtable_alloc_ptr(src);
    minim_hashtable_count(dst) = minim_hashtable_count(src);
    minim_hashtable_buckets(dst) = GC_alloc(minim_hashtable_alloc(src) * sizeof(mobj));
    minim_hashtable_hash(dst) = minim_hashtable_hash(src);
    minim_hashtable_equiv(dst) = minim_hashtable_equiv(src);

    for (long i = 0; i < minim_hashtable_alloc(src); ++i) {
        b = minim_hashtable_bucket(src, i);
        if (b) {
            t = Mcons(Mcons(minim_caar(b), minim_cdar(b)), minim_null);
            minim_hashtable_bucket(dst, i) = t;
            for (it = minim_cdr(b); !minim_nullp(it); it = minim_cdr(it)) {
                minim_cdr(t) = Mcons(Mcons(minim_caar(it), minim_cdar(it)), minim_null);
                t = minim_cdr(t);
            }
        } else {
            minim_hashtable_bucket(dst, i) = NULL;
        }
    }

    return dst;
}

int hashtable_equalp(mobj h1, mobj h2) {
    mobj it, v;
    size_t i;

    if (minim_hashtable_count(h1) != minim_hashtable_count(h2))
        return 0;

    for (i = 0; i < minim_hashtable_alloc(h1); ++i) {
        it = minim_hashtable_bucket(h1, i);
        if (it) {
            for (; !minim_nullp(it); it = minim_cdr(it)) {
                v = hashtable_find(h2, minim_caar(it));
                if (minim_nullp(v) || !minim_equalp(minim_cdr(v), minim_cdar(it)))
                    return 0;
            }
        }
    }

    return 1;
}

static void hashtable_clear(mobj o) {
    minim_hashtable_alloc_ptr(o) = start_size_ptr;
    minim_hashtable_count(o) = 0;
    minim_hashtable_buckets(o) = GC_calloc(minim_hashtable_alloc(o), sizeof(mobj));
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

static size_t equal_hash2(mobj o, size_t hash) {
    mobj it, res;
    minim_thread *th;
    long stashc, i;

    switch (minim_type(o))
    {
    case MINIM_OBJ_SYMBOL:
        return hash_bytes(minim_symbol(o), strlen(minim_symbol(o)), hash);
    case MINIM_OBJ_STRING:
        return hash_bytes(minim_string(o), strlen(minim_string(o)), hash);
    case MINIM_OBJ_PAIR:
        return equal_hash2(minim_cdr(o), equal_hash2(minim_car(o), hash));
    case MINIM_OBJ_VECTOR:
        for (i = 0; i < minim_vector_len(o); ++i)
            hash = equal_hash2(minim_vector_ref(o, i), hash);
        return hash;
    case MINIM_OBJ_HASHTABLE:
        for (i = 0; i < minim_hashtable_alloc(o); ++i) {
            it = minim_hashtable_bucket(o, i);
            if (it) {
                for (; !minim_nullp(it); it = minim_cdr(it)) {
                    hash = equal_hash2(minim_caar(it), hash);
                    hash = equal_hash2(minim_cdar(it), hash);
                }
            }
        }
        return hash;
    case MINIM_OBJ_BOX:
        return equal_hash2(minim_unbox(o), hash);
    case MINIM_OBJ_RECORD:
        // Hashing records using `equal?` recursively
        // descends through the record
        if (record_valuep(o)) {
        // Unsafe code to follow
            th = current_thread();
            stashc = stash_call_args();

            push_call_arg(o);
            push_call_arg(env_lookup_var(global_env(th), intern("equal-hash")));
            res = call_with_args(record_hash_proc(th), global_env(th));

            prepare_call_args(stashc);
            if (!minim_fixnump(res))
                bad_type_exn("record hash procedure result", "number?", res);
            return hash + minim_fixnum(res);
        } else {
            return eq_hash2(o, hash);
        }
    default:
        return eq_hash2(o, hash);
    }
}

static size_t eq_hash(mobj o) {
    return eq_hash2(o, hash_init);
}

static size_t equal_hash(mobj o) {
    return equal_hash2(o, hash_init);
}

static size_t hash_key(mobj ht, mobj k) {
    mobj i, proc;
    minim_thread *th;
    long stashc;

    proc = minim_hashtable_hash(ht);
    if (minim_primp(proc) && minim_prim(proc) == eq_hash_proc) {
        return eq_hash(k);
    } else if (minim_primp(proc) && minim_prim(proc) == equal_hash_proc) {
        return equal_hash(k);
    } else {
        th = current_thread();
        stashc = stash_call_args();
        i = call_with_args(minim_hashtable_hash(ht), global_env(th));
        prepare_call_args(stashc);

        if (!minim_fixnump(i)) {
            fprintf(stderr, "hash function associated with hash table ");
            write_object(stderr, ht);
            fprintf(stderr, " did not return a fixnum");
            minim_shutdown(1);
        }

        return minim_fixnum(i);
    }
}

static int key_equiv(mobj ht, mobj k1, mobj k2) {
    minim_thread *th;
    mobj proc, res;
    long stashc;

    proc = minim_hashtable_equiv(ht);
    if (minim_primp(proc) && minim_prim(proc) == eq_proc) {
        return minim_eqp(k1, k2);
    } else if (minim_primp(proc) && minim_prim(proc) == equal_proc) {
        return minim_equalp(k1, k2);
    } else {
        th = current_thread();
        stashc = stash_call_args();

        push_call_arg(k1);
        push_call_arg(k2);
        res = call_with_args(minim_hashtable_equiv(ht), global_env(th));

        prepare_call_args(stashc);
        return !minim_falsep(res);
    }
}

static void hashtable_resize(mobj ht, size_t *alloc_ptr) {
    mobj *buckets, it;
    size_t i, idx;

    buckets = GC_calloc(*alloc_ptr, sizeof(mobj ));
    for (i = 0; i < minim_hashtable_alloc(ht); ++i) {
        it = minim_hashtable_bucket(ht, i);
        if (it) {
            for (; !minim_nullp(it); it = minim_cdr(it)) {
                idx = hash_key(ht, minim_caar(it)) % *alloc_ptr;
                buckets[idx] = Mcons(minim_car(it), (buckets[idx] ? buckets[idx] : minim_null));
            }
        }
    }

    minim_hashtable_alloc_ptr(ht) = alloc_ptr;
    minim_hashtable_alloc(ht) = *alloc_ptr;
    minim_hashtable_buckets(ht) = buckets;
}

static void hashtable_opt_resize(mobj ht) {
    size_t *alloc_ptr;
    size_t alloc, count;

    count = minim_hashtable_count(ht);
    alloc = minim_hashtable_alloc(ht);
    if (load_factor(count, alloc) > MINIM_HASHTABLE_LOAD_FACTOR) {
        alloc_ptr = minim_hashtable_alloc_ptr(ht);
        for (; load_factor(count, *alloc_ptr) > MINIM_HASHTABLE_LOAD_FACTOR; ++alloc_ptr);
        hashtable_resize(ht, alloc_ptr);
    }
}

int hashtable_set(mobj ht, mobj k, mobj v) {
    mobj b, bi;
    size_t i;

    hashtable_opt_resize(ht);
    i = hash_key(ht, k) % minim_hashtable_alloc(ht);
    b = minim_hashtable_bucket(ht, i);
    if (b) {
        for (bi = b; !minim_nullp(bi); bi = minim_cdr(bi)) {
            if (key_equiv(ht, minim_caar(bi), k)) {
                minim_cdar(bi) = v;
                return 1;
            }
        }
    } else {
        b = minim_null;
    }

    minim_hashtable_bucket(ht, i) = Mcons(Mcons(k, v), b);
    ++minim_hashtable_count(ht);
    return 0;
}

static int hashtable_delete(mobj ht, mobj k) {
    mobj b, bi, bp;
    size_t i;

    i = hash_key(ht, k) % minim_hashtable_alloc(ht);
    b = minim_hashtable_bucket(ht, i);
    if (b) {
        bp = NULL;
        for (bi = b; !minim_nullp(bi); bp = bi, bi = minim_cdr(bi)) {
            if (key_equiv(ht, minim_caar(bi), k)) {
                if (bp == NULL) {
                    // start of the bucket
                    if (minim_nullp(minim_cdr(bi))) {
                        minim_hashtable_bucket(ht, i) = NULL;
                    } else {
                        minim_hashtable_bucket(ht, i) = minim_cdr(bi);
                    }
                } else {
                    // somewhere in the middle
                    minim_cdr(bp) = minim_cdr(bi);
                }
                
                --minim_hashtable_count(ht);
                return 1;
            }
        }
    }

    return 0;
}

mobj hashtable_find(mobj ht, mobj k) {
    mobj b;
    size_t i;

    i = hash_key(ht, k) % minim_hashtable_alloc(ht);
    b = minim_hashtable_bucket(ht, i);
    if (b) {
        for (; !minim_nullp(b); b = minim_cdr(b)) {
            if (key_equiv(ht, minim_caar(b), k))
                return minim_car(b);
        }
    }

    return minim_null;
}

mobj hashtable_keys(mobj ht) {
    mobj b, ks;
    size_t i;

    ks = minim_null;
    for (i = 0; i < minim_hashtable_alloc(ht); ++i) {
        b = minim_hashtable_bucket(ht, i);
        if (b) {
            for (; !minim_nullp(b); b = minim_cdr(b))
                ks = Mcons(minim_caar(b), ks);
        }
    }

    return ks;
}

static void key_not_found_exn(const char *name, mobj k) {
    fprintf(stderr, "%s: could not find key ", name);
    write_object(stderr, k);
    fprintf(stderr, "\n");
    minim_shutdown(1);
}

//
//  Primitives
//

mobj is_hashtable_proc(int argc, mobj *args) {
    // (-> any boolean)
    return (minim_hashtablep(args[0]) ? minim_true : minim_false);
}

mobj make_eq_hashtable_proc(int argc, mobj *args) {
    // (-> hashtable)
    return Mhashtable(eq_hash_proc_obj, eq_proc_obj);
}

mobj Mhashtable_proc(int argc, mobj *args) {
    // (-> hashtable)
    return Mhashtable(equal_hash_proc_obj, equal_proc_obj);
}

mobj hashtable_size_proc(int argc, mobj *args) {
    // (-> hashtable any any void)
    mobj ht = args[0];
    if (!minim_hashtablep(ht))
        bad_type_exn("hashtable-set!", "hashtable?", ht);
    return Mfixnum(minim_hashtable_count(ht));
}

mobj hashtable_contains_proc(int argc, mobj *args) {
    // (-> hashtable any boolean)
    mobj ht, v;

    ht = args[0];
    if (!minim_hashtablep(ht))
        bad_type_exn("hashtable-contains?", "hashtable?", ht);

    v = hashtable_find(ht, args[1]);
    return minim_nullp(v) ? minim_false : minim_true;
}

mobj hashtable_set_proc(int argc, mobj *args) {
    // (-> hashtable any any void)
    mobj ht;

    ht = args[0];
    if (!minim_hashtablep(ht))
        bad_type_exn("hashtable-set!", "hashtable?", ht);

    hashtable_set(ht, args[1], args[2]);
    return minim_void;
}

mobj hashtable_delete_proc(int argc, mobj *args) {
    // (-> hashtable any void)
    mobj ht, k;

    ht = args[0];
    if (!minim_hashtablep(ht))
        bad_type_exn("hashtable-delete!", "hashtable?", ht);

    k = args[1];
    if (!hashtable_delete(ht, k))
        key_not_found_exn("hashtable-delete!", k);

    return minim_void;
}

mobj hashtable_update_proc(int argc, mobj *args) {
    // (-> hashtable any (-> any any) void)
    // (-> hashtable any (-> any any) any void)
    mobj ht, k, proc, b, env;
    long stashc;

    ht = args[0];
    if (!minim_hashtablep(ht))
        bad_type_exn("hashtable-update!", "hashtable?", ht);

    k = args[1];
    proc = args[2];
    if (!minim_procp(proc))
        bad_type_exn("hashtable-update!", "procedure?", proc);

    stashc = stash_call_args();
    b = hashtable_find(ht, k);
    if (minim_nullp(b)) {
        if (argc == 3) {
            // no failure result provided
            key_not_found_exn("hashtable-update!", k);
        } else {
            // user-provided failure
            mobj fail, v;

            fail = args[3];
            env = global_env(current_thread());     // TODO: this seems problematic
            if (!minim_procp(fail)) {
                b = fail;
            } else {
                b = call_with_args(fail, env);
            }

            push_call_arg(b);
            v = call_with_args(proc, env);
            hashtable_set(ht, k, v);
        }
    } else {
        push_call_arg(minim_cdr(b));
        env = global_env(current_thread());     // TODO: this seems problematic
        minim_cdr(b) = call_with_args(proc, env);
    }

    prepare_call_args(stashc);
    return minim_void;
}

mobj hashtable_ref_proc(int argc, mobj *args) {
    // (-> hashtable any any)
    // (-> hashtable any any any)
    mobj ht, k, b;

    ht = args[0];
    if (!minim_hashtablep(ht))
        bad_type_exn("hashtable-ref", "hashtable?", ht);

    k = args[1];
    b = hashtable_find(ht, k);
    if (minim_nullp(b)) {
        if (argc == 2) {
            // no failure result provided
            key_not_found_exn("hashtable-ref", k);
            write_object(stderr, k);
            fprintf(stderr, "\n");
            minim_shutdown(1);
        } else {
            // user-provided failure
            mobj fail, env, res;
            long stashc;

            fail = args[2];
            if (!minim_procp(fail))
                return fail;
            
            stashc = stash_call_args();
            env = global_env(current_thread());   // TODO: this seems problematic
            res = call_with_args(fail, env);
            prepare_call_args(stashc);
            return res;
        }
    }

    return minim_cdr(b);
}

mobj hashtable_keys_proc(int argc, mobj *args) {
    // (-> hashtable (listof any))
    mobj ht;

    ht = args[0];
    if (!minim_hashtablep(ht))
        bad_type_exn("hashtable-keys", "hashtable?", ht);
    return hashtable_keys(ht);
}

mobj hashtable_copy_proc(int argc, mobj *args) {
    // (-> hashtable void)
    mobj ht;

    ht = args[0];
    if (!minim_hashtablep(ht))
        bad_type_exn("hashtable-copy", "hashtable?", ht);
    return hashtable_copy(ht);
}

mobj hashtable_clear_proc(int argc, mobj *args) {
    // (-> hashtable void)
    mobj ht;

    ht = args[0];
    if (!minim_hashtablep(ht))
        bad_type_exn("hashtable-clear!", "hashtable?", ht);
    hashtable_clear(ht);
    return minim_void;
}

mobj eq_hash_proc(int argc, mobj *args) {
    return Mfixnum(eq_hash(args[0]));
}

mobj equal_hash_proc(int argc, mobj *args) {
    return Mfixnum(equal_hash(args[0]));
}
