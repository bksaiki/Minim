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

static void not_environment_exn(const char *name, mobj *frame) {
    fprintf(stderr, "%s: not an environment frame: ", name);
    write_object(stderr, frame);
    fprintf(stderr, "\n");
    minim_shutdown(1);
}
void env_define_var_no_check(mobj env, mobj var, mobj val) {
    mobj frame, new_frame, bind;
    long frame_size, i;

    frame = minim_env_bindings(env);
    if (minim_vectorp(frame)) {
        // small namespace
        frame_size = minim_vector_len(frame);
        for (i = 0; i < frame_size; ++i) {
            if (minim_falsep(minim_vector_ref(frame, i))) {
                minim_vector_ref(frame, i) = Mcons(var, val);
                return;
            }
        }

        // too small: convert to a large namespace
        new_frame = Mhashtable(frame_size + 1);
        for (i = 0; i < frame_size; ++i) {
            bind = minim_vector_ref(frame, i);
            eq_hashtable_set(new_frame, minim_car(bind), minim_cdr(bind));
        }

        eq_hashtable_set(new_frame, var, val);
        minim_env_bindings(env) = new_frame;
    } else if (minim_hashtablep(frame)) {
        // large namespace
        eq_hashtable_set(frame, var, val);
    } else {
        not_environment_exn("env_define_var_no_check()", frame);
    }
}

mobj env_define_var(mobj env, mobj var, mobj val) {
    mobj frame, bind, old;
    long i;

    // check if it is defined, and overwrite the current value
    frame = minim_env_bindings(env);
    if (minim_vectorp(frame)) {
        // small namespace
        for (i = 0; i < minim_vector_len(frame); ++i) {
            bind = minim_vector_ref(frame, i);
            if (minim_falsep(bind))
                break;
            
            if (minim_car(bind) == var) {
                old = minim_cdr(bind);
                minim_cdr(bind) = val;
                return old;
            }
        }
    } else if (minim_hashtablep(frame)) {
        // large namespace
        bind = eq_hashtable_find(frame, var);
        if (!minim_falsep(bind)) {
            old = minim_cdr(bind);
            minim_cdr(bind) = val;
            return old;
        }
    } else {
        printf("%p %p %p %p %p %p\n", minim_true, minim_false, minim_eof, minim_values, minim_void, empty_env);
        printf("bad: %p %d %p %p\n", env, minim_type(frame), frame, minim_env_prev(env));
        not_environment_exn("env_define_var()", frame);
    }

    // else just add
    env_define_var_no_check(env, var, val);
    return NULL;
}

mobj env_set_var(mobj env, mobj var, mobj val) {
    mobj frame, bind, old;
    long i;

    while (minim_envp(env)) {
        frame = minim_env_bindings(env);
        if (minim_vectorp(frame)) {
            // small namespace
            for (i = 0; i < minim_vector_len(frame); ++i) {
                bind = minim_vector_ref(frame, i);
                if (minim_falsep(bind))
                    break;
                
                if (minim_car(bind) == var) {
                    old = minim_cdr(bind);
                    minim_cdr(bind) = val;
                    return old;
                }
            }
        } else if (minim_hashtablep(frame)) {
            // large namespace
            bind = eq_hashtable_find(frame, var);
            if (!minim_falsep(bind)) {
                old = minim_cdr(bind);
                minim_cdr(bind) = val;
                return old;
            }
        } else {
            not_environment_exn("env_set_var()", frame);
        }

        env = minim_env_prev(env);
    }

    fprintf(stderr, "unbound variable: %s\n", minim_symbol(var));
    minim_shutdown(1);
}

int env_var_is_defined(mobj env, mobj var, int recursive) {
    mobj frame, bind;
    long i;

    if (minim_envp(env)) {
        frame = minim_env_bindings(env);
        if (minim_vectorp(frame)) {
            // small namespace
            for (i = 0; i < minim_vector_len(frame); ++i) {
                bind = minim_vector_ref(frame, i);
                if (minim_falsep(bind))
                    break;
                
                if (minim_car(bind) == var)
                    return 1;
            }
        } else if (minim_hashtablep(frame)) {
            // large namespace
            bind = eq_hashtable_find(frame, var);
            if (!minim_falsep(bind))
                return 1;
        } else {
            not_environment_exn("env_var_is_defined()", frame);
        }

        if (!recursive)
            return 0;

        env = minim_env_prev(env);
    }

    return 0;
}

mobj env_lookup_var(mobj env, mobj var) {
    mobj frame, bind;
    long i;

    while (minim_envp(env)) {
        frame = minim_env_bindings(env);
        if (minim_vectorp(frame)) {
            // small namespace
            for (i = 0; i < minim_vector_len(frame); ++i) {
                bind = minim_vector_ref(frame, i);
                if (minim_falsep(bind))
                    break;
                
                if (minim_car(bind) == var)
                    return minim_cdr(bind);
            }
        } else if (minim_hashtablep(frame)) {
            // large namespace
            bind = eq_hashtable_find(frame, var);
            if (!minim_falsep(bind))
                return minim_cdr(bind);
        } else {
            not_environment_exn("env_lookup_var()", frame);
        }

        env = minim_env_prev(env);
    }

    fprintf(stderr, "unbound variable: %s\n", minim_symbol(var));
    minim_shutdown(1);
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
    return make_env();
}

mobj current_environment() {
    // (-> environment)
    fprintf(stderr, "current-environment: should not be called directly");
    minim_shutdown(1);
}

mobj extend_environment(mobj env) {
    // (-> environment environment)
    return Menv(env);
}

mobj environment_names(mobj env) {
    // (-> environment (listof symbol))
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
        } else if (minim_hashtablep(frame)) {
            // large namespace
            mobj keys = hashtable_keys(frame);
            for (; !minim_nullp(keys); keys = minim_cdr(keys))
                eq_hashtable_set(names, minim_car(keys), minim_null);
        } else {
            not_environment_exn("environment_names()", frame);
        }
    }

    return hashtable_keys(names);
}

mobj environment_variable_ref(mobj env, mobj k, mobj fail) {
    // (-> environment symbol any)
    if (env_var_is_defined(env, k, 0)) {
        // variable found
        return env_lookup_var(env, k);
    } else {
        // variable not found
        return fail;
    }
}

mobj environment_variable_set(mobj env, mobj k, mobj v) {
    // (-> environment symbol any void)
    env_define_var(env, k, v);
    return minim_void;
}
