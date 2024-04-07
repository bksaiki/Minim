/*
    Environments
*/

#include "../minim.h"

//F
//  Top-level environments
//  Just a special eq?-based hashtable
//  Each cell is: `((key . value) . (hash . mutable?))
//

#define start_size_ptr                      (&bucket_sizes[0])

mobj Mtop_env(size_t size_hint) {
    size_t *alloc_ptr;
    mobj env;
    
    alloc_ptr = start_size_ptr;
    for (; *alloc_ptr < 4 * size_hint; ++alloc_ptr);
    
    env = GC_alloc(minim_top_env_size);
    minim_type(env) = MINIM_OBJ_TOPENV;
    minim_top_env_alloc_ptr(env) = alloc_ptr;
    minim_top_env_buckets(env) = Mvector(minim_top_env_alloc(env), minim_null);
    minim_top_env_count(env) = 0;
    return env;
}

mobj top_env_copy(mobj env, int mutablep) {
    mobj env2, nb, b, cell;
    size_t i;

    // copy the underlying table
    nb = Mvector(minim_top_env_alloc(env), minim_null);
    for (i = 0; i < minim_top_env_alloc(env); i++) {
        b = minim_top_env_bucket(env, i);
        if (!minim_nullp(b)) {
            mobj hd, tl;

            cell = minim_car(b);
            hd = tl = Mlist1(Mcons(
                Mcons(minim_caar(cell), minim_cdar(cell)),
                Mcons(minim_cadr(cell), mutablep ? minim_true : minim_false)
            ));

            for (b = minim_cdr(b); !minim_nullp(b); b = minim_cdr(b)) {
                cell = minim_car(b);
                minim_cdr(tl) = Mlist1(Mcons(
                    Mcons(minim_caar(cell), minim_cdar(cell)),
                    Mcons(minim_cadr(cell), mutablep ? minim_true : minim_false)
                ));
                tl = minim_cdr(tl);
            }

            minim_vector_ref(nb, i) = hd;
        }
    }

    // create the environment object
    env2 = GC_alloc(minim_top_env_size);
    minim_type(env2) = MINIM_OBJ_TOPENV;
    minim_top_env_alloc_ptr(env2) = minim_top_env_alloc_ptr(env);
    minim_top_env_buckets(env2) = nb;
    minim_top_env_count(env2) = minim_top_env_count(env);
    return env2;
}

static void top_env_resize(mobj env) {
    mobj nb, b, cell;
    size_t *alloc_ptr;
    size_t i, j;

    // find the right size
    alloc_ptr = start_size_ptr;
    for (; *alloc_ptr < 4 * minim_top_env_count(env); ++alloc_ptr);

    nb = Mvector(*alloc_ptr, minim_null);
    for (i = 0; i < minim_top_env_alloc(env); i++) {
        for (b = minim_top_env_bucket(env, i); !minim_nullp(b); b = minim_cdr(b)) {
            cell = minim_car(b);
            j = minim_fixnum(minim_cadr(cell)) % *alloc_ptr;
            minim_vector_ref(nb, j) = Mcons(cell, minim_vector_ref(nb, j));
        }
    }

    minim_top_env_alloc_ptr(env) = alloc_ptr;
    minim_top_env_buckets(env) = nb;
}

mobj top_env_insert(mobj env, mobj k, mobj v) {
    mobj b, cell;
    size_t h, i;

    if (minim_top_env_count(env) > minim_top_env_alloc(env))
        top_env_resize(env);

    h = eq_hash(k);
    i = h % minim_top_env_alloc(env);
    b = minim_top_env_bucket(env, i);
    cell = Mcons(Mcons(k, v), Mcons(Mfixnum(h), minim_true));
    minim_top_env_bucket(env, i) = Mcons(cell, b);
    return cell;
}

mobj top_env_find(mobj env, mobj k) {
    mobj b, cell;
    size_t i;

    i = eq_hash(k) % minim_top_env_alloc(env);
    for (b = minim_top_env_bucket(env, i); !minim_nullp(b); b = minim_cdr(b)) {
        cell = minim_car(b);
        if (minim_caar(cell) == k)
            return cell;
    }

    return minim_false;
}

//
//  Environments
//
//  Ideally:
//    environments ::= (<frame0> <frame1> ...)
//    frames       ::= ((<var0> . <val1>) (<var1> . <val1>) ...)
//
//  Actually:
//    - each frame is a fixed size vector
//    - otherwise we allocate a hashtable
//

mobj Menv(mobj prev) {
    mobj env = GC_alloc(minim_env_size);
    minim_type(env) = MINIM_OBJ_ENV;
    minim_env_bindings(env) = Mvector(env_vector_max, minim_false);
    minim_env_prev(env) = prev;
    return env;
}

mobj Menv2(mobj prev, size_t size) {
    mobj env = GC_alloc(minim_env_size);
    minim_type(env) = MINIM_OBJ_ENV;
    minim_env_bindings(env) = Mvector(size, minim_false);
    minim_env_prev(env) = prev;
    return env;
}

static mobj env_find(mobj env, mobj k, int rec) {
    mobj frame, cell;
    size_t h;
    int hp = 0;

    for (; minim_envp(env); env = minim_env_prev(env)) {
        frame = minim_env_bindings(env);
        if (minim_vectorp(frame)) {
            // small namespace
            for (long i = 0; i < minim_vector_len(frame); i++) {
                cell = minim_vector_ref(frame, i);
                if (minim_falsep(cell))
                    break;

                if (minim_car(cell) == k)
                    return cell;
            }
        } else {
            // large namespace
            frame = minim_env_bindings(env);

            // hash key as needed
            if (!hp) {
                h = eq_hash(k);
                hp = 1;
            }

            cell = eq_hashtable_find2(frame, k, h);
            if (!minim_falsep(cell))
                return cell;
        }

        if (!rec)
            break;
    }

    cell = top_env_find(env, k);
    return minim_falsep(cell) ? minim_false : minim_car(cell);
}

static mobj env_names(mobj env) {
    mobj names = Mhashtable(0);
    for (; minim_envp(env); env = minim_env_prev(env)) {
        mobj frame = minim_env_bindings(env);
        if (minim_vectorp(frame)) {
            // small namespace
            for (long i = 0; i < minim_vector_len(frame); ++i) {
                mobj bind = minim_vector_ref(frame, i);
                if (minim_falsep(bind))
                    break;
                
                eq_hashtable_set(names, minim_car(bind), minim_null);
            }
        } else{
            // large namespace
            mobj keys = hashtable_keys(frame);
            for (; !minim_nullp(keys); keys = minim_cdr(keys))
                eq_hashtable_set(names, minim_car(keys), minim_null);
        }
    }

    return hashtable_keys(names);
}

void env_define_var_no_check(mobj env, mobj var, mobj val) {
    mobj frame, nframe, cell;
    size_t size;

    if (minim_top_envp(env)) {
        // TODO: splice this out
        top_env_insert(env, var, val);
    } else {
        frame = minim_env_bindings(env);
        if (minim_vectorp(frame)) {
            // small namespace
            size = minim_vector_len(frame);
            for (long i = 0; i < size; ++i) {
                cell = minim_vector_ref(frame, i);
                if (minim_falsep(cell)) {
                    minim_vector_ref(frame, i) = Mcons(var, val);
                    return;
                }
            }

            // too small: convert to a large namespace
            nframe = Mhashtable(size + 1);
            for (long i = 0; i < size; ++i) {
                cell = minim_vector_ref(frame, i);
                eq_hashtable_set(nframe, minim_car(cell), minim_cdr(cell));
            }

            eq_hashtable_set(nframe, var, val);
            minim_env_bindings(env) = nframe;
        } else {
            // large namespace
            eq_hashtable_set(frame, var, val);
        }
    }
}

mobj env_define_var(mobj env, mobj var, mobj val) {
    mobj cell, old;
    
    cell = env_find(env, var, 0);
    if (minim_falsep(cell)) {
        // insert new cell
        env_define_var_no_check(env, var, val);
        return NULL;
    } else {
        // overwrite cell
        old = minim_cdr(cell);
        minim_cdr(cell) = val;
        return old;
    }
}

mobj env_set_var(mobj env, mobj var, mobj val) {
    mobj cell, old;
    
    cell = env_find(env, var, 1);
    if (minim_falsep(cell)) {
        // panic, cannot find cell
        minim_error1("env_set_var", "unbound variable", var);
    } else {
        // overwrite cell
        old = minim_cdr(cell);
        minim_cdr(cell) = val;
        return old;
    }
}

mobj env_lookup_var(mobj env, mobj var) {
    mobj cell = env_find(env, var, 1);
    if (minim_falsep(cell)) {
        // panic, cannot find cell
        minim_error1("env_lookup_var", "unbound variable", var);
    } else {
        // found cell
        return minim_cdr(cell);
    }
}

#define add_value(env, name, c_val)  \
    env_define_var(env, intern(name), c_val);

mobj base_env;

void init_envs() {
    // base environment
    base_env = Mtop_env(1024);
    add_value(base_env, "null", minim_null);
    add_value(base_env, "true", minim_true);
    add_value(base_env, "false", minim_false);
    add_value(base_env, "eof", minim_eof);
    init_prims(base_env);
}

mobj make_empty_env() {
    return Mtop_env(0);
}

mobj make_base_env() {
    return top_env_copy(base_env, 0);
}

//
//  Primitives
//

mobj environmentp_proc(mobj x) {
    // (-> any boolean)
    return minim_envp(x) ? minim_true : minim_false;
}

mobj interaction_environment() {
    // (-> environment)
    return tc_env(current_tc());
}

mobj empty_environment() {
    // (-> environment)
    return make_empty_env();
}

mobj environment_proc() {
    // (-> environment)
    return make_base_env();
}

mobj extend_environment(mobj env) {
    // (-> environment environment)
    return Menv(env);
}

mobj environment_names(mobj env) {
    // (-> environment (listof symbol))
    return env_names(env);
}

mobj environment_variable_ref(mobj env, mobj k, mobj fail) {
    // (-> environment symbol any)
    mobj cell = env_find(env, k, 1);
    return minim_falsep(cell) ? fail : minim_cdr(cell);
}

mobj environment_variable_set(mobj env, mobj k, mobj v) {
    // (-> environment symbol any void)
    env_define_var(env, k, v);
    return minim_void;
}
