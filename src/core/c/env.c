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

static void hashtable_set2(mobj ht, mobj k, mobj v) {
    mobj h = eq_hash_proc(k);
    mobj b = hashtable_ref(ht, h);
    for (mobj bi = b; !minim_nullp(bi); bi = minim_cdr(bi)) {
        if (minim_caar(bi) == k) {
            minim_cdar(bi) = v;
            return;
        }
    }

    hashtable_set(ht, h, Mcons(Mcons(k, v), b));
    minim_hashtable_count(ht)++;
}

static mobj hashtable_find(mobj ht, mobj k) {
    mobj h = eq_hash_proc(k);
    for (mobj b = hashtable_ref(ht, h); !minim_nullp(b); b = minim_cdr(b)) {
        if (minim_caar(b) == k)
            return minim_car(b);
    }

    return minim_false;
}

static mobj environment_names(mobj env) {
    mobj frame, names;

    names = Mhashtable(0);
    for (; minim_envp(env); env = minim_env_prev(env)) {
        frame = minim_env_bindings(env);
        if (minim_vectorp(frame)) {
            // small namespace
            for (int i = 0; i < minim_vector_len(frame); ++i) {
                mobj bind = minim_vector_ref(frame, i);
                if (minim_falsep(bind))
                    break;
                
                hashtable_set2(names, minim_car(bind), minim_null);
            }
        } else if (minim_hashtablep(frame)) {
            // large namespace
            for (mobj keys = hashtable_keys(frame); !minim_nullp(keys); keys = minim_cdr(keys))
                hashtable_set2(names, minim_car(keys), minim_null);
        } else {
            not_environment_exn("environment_names()", frame);
        }
    }

    return hashtable_keys(names);
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
            hashtable_set2(new_frame, minim_car(bind), minim_cdr(bind));
        }

        hashtable_set2(new_frame, var, val);
        minim_env_bindings(env) = new_frame;
    } else if (minim_hashtablep(frame)) {
        // large namespace
        hashtable_set2(frame, var, val);
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
        bind = hashtable_find(frame, var);
        if (!minim_falsep(bind)) {
            old = minim_cdr(bind);
            minim_cdr(bind) = val;
            return old;
        }
    } else {
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
            bind = hashtable_find(frame, var);
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
            bind = hashtable_find(frame, var);
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
            bind = hashtable_find(frame, var);
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

mobj setup_env() {
    return Menv(empty_env);
}

//
//  Primitives
//

mobj interaction_environment_proc(int argc, mobj *args) {
    // (-> environment)
    return global_env(current_thread());
}

mobj empty_environment_proc(int argc, mobj *args) {
    // (-> environment)
    return setup_env();
}

mobj environment_proc(int argc, mobj *args) {
    // (-> environment)
    return make_env();
}

mobj extend_environment_proc(int argc, mobj *args) {
    // (-> environment environment)
    if (!minim_envp(args[0]))
        bad_type_exn("extend-environment", "environment?", args[0]);
    return Menv(args[0]);
}

mobj environment_names_proc(int argc, mobj *args) {
    // (-> environment list)
    if (!minim_envp(args[0]))
        bad_type_exn("extend-environment", "environment?", args[0]);
    return environment_names(args[0]);
}

mobj environment_variable_value_proc(int argc, mobj *args) {
    // (-> environment symbol -> any)
    mobj env, name, exn, res;
    long stashc;

    env = args[0];
    if (!minim_envp(env))
        bad_type_exn("environment-variable-value", "environment?", env);

    name = args[1];
    if (!minim_symbolp(name)) {
        bad_type_exn("environment-variable-value", "symbol?", name);
    } else if (env_var_is_defined(env, name, 0)) {
        // variable found
        return env_lookup_var(env, name);
    } else {
        // variable not found
        if (argc == 2) {
            // default exception
            fprintf(stderr, "environment-variable-value: variable not bound");
            fprintf(stderr, " name: %s", minim_symbol(name));
        } else {
            // custom exception
            exn = args[2];
            if (!minim_procp(exn))
                bad_type_exn("environment-variable-value", "procedure?", exn);

            stashc = stash_call_args();
            res = call_with_args(exn, env);
            prepare_call_args(stashc);
            return res;
        }
    }

    fprintf(stderr, "unreachable");
    return minim_void;
}

mobj environment_set_variable_value_proc(int argc, mobj *args) {
    // (-> environment symbol any void)
    mobj env, name, val;

    env = args[0];
    if (!minim_envp(env))
        bad_type_exn("environment-set-variable-value!", "environment?", env);

    name = args[1];
    if (!minim_symbolp(name))
        bad_type_exn("environment-set-variable-value!", "symbol?", name);

    val = args[2];
    env_define_var(env, name, val);
    return minim_void;
}

mobj current_environment_proc(int argc, mobj *args) {
    fprintf(stderr, "current-environment: should not be called directly");
    minim_shutdown(1);
}
