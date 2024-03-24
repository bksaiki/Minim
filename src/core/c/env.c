/*
    Environments
*/

#include "../minim.h"

minim_globals *globals;
mobj empty_env;

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

//
//  Objects
//

mobj Menv(mobj prev) {
    return Menv2(prev, ENVIRONMENT_VECTOR_MAX);
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
    
    return minim_false;
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

static mobj base_env;

mobj make_env() {
    mobj env  = setup_env();
    if (base_env == NULL) {
        add_value(env, "null", minim_null);
        add_value(env, "true", minim_true);
        add_value(env, "false", minim_false);
        add_value(env, "eof", minim_eof);
        init_prims(env);
        base_env = env;
        return env;
    } else {
        minim_env_bindings(env) = hashtable_copy(minim_env_bindings(base_env));
        return env;
    }
}

mobj setup_env() {
    return Menv(empty_env);
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
    return global_env(current_thread());
}

mobj empty_environment() {
    // (-> environment)
    return setup_env();
}

mobj environment_proc() {
    // (-> environment)
    return base_env;
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
