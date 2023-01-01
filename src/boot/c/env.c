/*
    Environments
*/

#include "../minim.h"

minim_globals *globals;
minim_object *empty_env;

//
//  Environments
//
//  Ideally:
//    environments ::= (<frame0> <frame1> ...)
//    frames       ::= ((<var0> . <val1>) (<var1> . <val1>) ...)
//
//  Actually:
//    - each frame is a vector if # of <var>s is less than 10
//    - otherwise we allocate a hashtable
//

minim_object *make_environment(minim_object *prev) {
    minim_env *env = GC_alloc(sizeof(minim_env));
    env->type = MINIM_ENVIRONMENT_TYPE;
    env->bindings = make_vector(ENVIRONMENT_VECTOR_MAX, minim_false);
    env->prev = prev;

    
    return ((minim_object *) env);
}

void env_define_var_no_check(minim_object *env, minim_object *var, minim_object *val) {
    minim_object *frame;
    long i;

    frame = minim_env_bindings(env);
    if (minim_is_vector(frame)) {
        minim_object *new_frame, *bind, *hash, *eq;

        // small namespace
        for (i = 0; i < ENVIRONMENT_VECTOR_MAX; ++i) {
            if (minim_is_false(minim_vector_ref(frame, i))) {
                minim_vector_ref(frame, i) = make_pair(var, val);
                return;
            }
        }

        // too small: convert to a large namespace
        hash = make_prim_proc(eq_hash_proc, "eq-hash", 1, 1);
        eq = make_prim_proc(eq_proc, "eq?", 2, 2);
        new_frame = make_hashtable2(hash, eq, ENVIRONMENT_VECTOR_MAX + 1);
        for (i = 0; i < ENVIRONMENT_VECTOR_MAX; ++i) {
            bind = minim_vector_ref(frame, i);
            hashtable_set(new_frame, minim_car(bind), minim_cdr(bind));
        }

        hashtable_set(new_frame, var, val);
        minim_env_bindings(env) = new_frame;
    } else if (minim_is_hashtable(frame)) {
        // large namespace
        hashtable_set(frame, var, val);
    } else {
        fprintf(stderr, "env_define_var_no_check(): not an environment frame: ");
        write_object(stderr, frame);
        fprintf(stderr, "\n");
        minim_shutdown(1);
    }
}

minim_object *env_define_var(minim_object *env, minim_object *var, minim_object *val) {
    minim_object *frame, *bind, *old;
    long i;

    // check if it is defined, and overwrite the current value
    frame = minim_env_bindings(env);
    if (minim_is_vector(frame)) {
        // small namespace
        for (i = 0; i < ENVIRONMENT_VECTOR_MAX; ++i) {
            bind = minim_vector_ref(frame, i);
            if (minim_is_false(bind))
                break;
            
            if (minim_car(bind) == var) {
                old = minim_cdr(bind);
                minim_cdr(bind) = val;
                return old;
            }
        }
    } else if (minim_is_hashtable(frame)) {
        // large namespace
        bind = hashtable_find(frame, var);
        if (!minim_is_null(bind)) {
            old = minim_cdr(bind);
            minim_cdr(bind) = val;
            return old;
        }
    } else {
        fprintf(stderr, "env_define_var(): not an environment frame: ");
        write_object(stderr, frame);
        fprintf(stderr, "\n");
        minim_shutdown(1);
    }

    // else just add
    env_define_var_no_check(env, var, val);
    return NULL;
}

minim_object *env_set_var(minim_object *env, minim_object *var, minim_object *val) {
    minim_object *frame, *bind, *old;
    long i;

    while (minim_is_env(env)) {
        frame = minim_env_bindings(env);
        if (minim_is_vector(frame)) {
            // small namespace
            for (i = 0; i < ENVIRONMENT_VECTOR_MAX; ++i) {
                bind = minim_vector_ref(frame, i);
                if (minim_is_false(bind))
                    break;
                
                if (minim_car(bind) == var) {
                    old = minim_cdr(bind);
                    minim_cdr(bind) = val;
                    return old;
                }
            }
        } else if (minim_is_hashtable(frame)) {
            // large namespace
            bind = hashtable_find(frame, var);
            if (!minim_is_null(bind)) {
                old = minim_cdr(bind);
                minim_cdr(bind) = val;
                return old;
            }
        } else {
            fprintf(stderr, "env_lookup_var(): not an environment frame: ");
            write_object(stderr, frame);
            fprintf(stderr, "\n");
            minim_shutdown(1);
        }

        env = minim_env_prev(env);
    }

    fprintf(stderr, "unbound variable: %s\n", minim_symbol(var));
    minim_shutdown(1);
}

int env_var_is_defined(minim_object *env, minim_object *var, int recursive) {
    minim_object *frame, *bind;
    long i;

    if (minim_is_env(env)) {
        frame = minim_env_bindings(env);
        if (minim_is_vector(frame)) {
            // small namespace
            for (i = 0; i < ENVIRONMENT_VECTOR_MAX; ++i) {
                bind = minim_vector_ref(frame, i);
                if (minim_is_false(bind))
                    break;
                
                if (minim_car(bind) == var)
                    return 1;
            }
        } else if (minim_is_hashtable(frame)) {
            // large namespace
            bind = hashtable_find(frame, var);
            if (!minim_is_null(bind))
                return 1;
        } else {
            fprintf(stderr, "env_var_is_defined(): not an environment frame: ");
            write_object(stderr, frame);
            fprintf(stderr, "\n");
            minim_shutdown(1);
        }

        if (!recursive)
            return 0;

        env = minim_env_prev(env);
    }

    return 0;
}

minim_object *env_lookup_var(minim_object *env, minim_object *var) {
    minim_object *frame, *bind;
    long i;

    while (minim_is_env(env)) {
        frame = minim_env_bindings(env);
        if (minim_is_vector(frame)) {
            // small namespace
            for (i = 0; i < ENVIRONMENT_VECTOR_MAX; ++i) {
                bind = minim_vector_ref(frame, i);
                if (minim_is_false(bind))
                    break;
                
                if (minim_car(bind) == var)
                    return minim_cdr(bind);
            }
        } else if (minim_is_hashtable(frame)) {
            // large namespace
            bind = hashtable_find(frame, var);
            if (!minim_is_null(bind))
                return minim_cdr(bind);
        } else {
            fprintf(stderr, "env_lookup_var(): not an environment frame: ");
            write_object(stderr, frame);
            fprintf(stderr, "\n");
            minim_shutdown(1);
        }

        env = minim_env_prev(env);
    }

    fprintf(stderr, "unbound variable: %s\n", minim_symbol(var));
    minim_shutdown(1);
}

minim_object *extend_env(minim_object *vars,
                         minim_object *vals,
                         minim_object *base_env) {
    minim_object *env, *var_it, *val_it;

    // TODO: optimize
    env = make_environment(base_env);
    var_it = vars; val_it = vals;
    while (!minim_is_null(var_it)) {
        env_define_var_no_check(env, minim_car(var_it), minim_car(val_it));
        var_it = minim_cdr(var_it);
        val_it = minim_cdr(val_it);   
    }

    return env;
}

minim_object *setup_env() {
    return make_environment(empty_env);
}

//
//  Primitives
//

minim_object *interaction_environment_proc(minim_object *args) {
    // (-> environment)
    return global_env(current_thread());
}

minim_object *empty_environment_proc(minim_object *args) {
    // (-> environment)
    return setup_env();
}

minim_object *environment_proc(minim_object *args) {
    // (-> environment)
    return make_env();
}

minim_object *extend_environment_proc(minim_object *args) {
    // (-> environment environment)
    if (!minim_is_env(minim_car(args)))
        bad_type_exn("extend-environment", "environment?", minim_car(args));
    return make_environment(minim_car(args));
}

minim_object *environment_variable_value_proc(minim_object *args) {
    // (-> environment symbol -> any)
    minim_object *env, *name, *exn;

    env = minim_car(args);
    if (!minim_is_env(env))
        bad_type_exn("environment-variable-value", "environment?", env);

    name = minim_cadr(args);
    if (!minim_is_symbol(name)) {
        bad_type_exn("environment-variable-value", "symbol?", name);
    } else if (env_var_is_defined(env, name, 0)) {
        // variable found
        return env_lookup_var(env, name);
    } else {
        // variable not found
        if (minim_is_null(minim_cddr(args))) {
            // default exception
            fprintf(stderr, "environment-variable-value: variable not bound");
            fprintf(stderr, " name: %s", minim_symbol(name));
        } else {
            // custom exception
            exn = minim_car(minim_cddr(args));
            if (!minim_is_proc(exn))
                bad_type_exn("environment-variable-value", "procedure?", exn);
            return call_with_args(exn, minim_null, env);
        }
    }

    fprintf(stderr, "unreachable");
    return minim_void;
}

minim_object *environment_set_variable_value_proc(minim_object *args) {
    // (-> environment symbol any void)
    minim_object *env, *name, *val;

    env = minim_car(args);
    if (!minim_is_env(env))
        bad_type_exn("environment-set-variable-value!", "environment?", env);

    name = minim_cadr(args);
    val = minim_car(minim_cddr(args));

    if (!minim_is_symbol(name))
        bad_type_exn("environment-set-variable-value!", "symbol?", name);

    env_define_var(env, name, val);
    return minim_void;
}

minim_object *current_environment_proc(minim_object *args) {
    fprintf(stderr, "current-environment: should not be called directly");
    minim_shutdown(1);
}
