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

        minim_env_bindings(env) = new_frame;
    } else if (minim_is_hashtable(frame)) {
        // large namespace
        hashtable_set(frame, var, val);
    } else {
        fprintf(stderr, "env_define_var_no_check(): not an environment frame: ");
        write_object(stderr, frame);
        fprintf(stderr, "\n");
        exit(1);
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
        exit(1);
    }

    // else just add
    env_define_var_no_check(env, var, val);
    return NULL;
}

minim_object *env_set_var(minim_object *env, minim_object *var, minim_object *val) {
    // while (!minim_is_null(env)) {
    //     minim_object *frame = minim_car(env);
    //     while (!minim_is_null(frame)) {
    //         minim_object *frame_var = minim_caar(frame);
    //         minim_object *frame_val = minim_cdar(frame);
    //         if (var == frame_var) {
    //             minim_cdar(frame) = val;
    //             return frame_val;
    //         }

    //         frame = minim_cdr(frame);
    //     }

    //     env = minim_cdr(env);
    // }

    // fprintf(stderr, "unbound variable: %s\n", minim_symbol(var));
    // exit(1);

    fprintf(stderr, "env_set_var(): unimplemented\n");
    exit(1);
}

int env_var_is_defined(minim_object *env, minim_object *var, int recursive) {
    // minim_object *frame, *frame_var;

    // if (minim_is_null(env))
    //     return 0;

    // for (frame = minim_car(env); !minim_is_null(frame); frame = minim_cdr(frame)) {
    //     frame_var = minim_caar(frame);
    //     if (var == frame_var)
    //         return 1; 
    // }

    // return recursive && env_var_is_defined(minim_cdr(env), var, 1);

    fprintf(stderr, "env_var_is_defined(): unimplemented\n");
    exit(1);
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
            exit(1);
        }

        env = minim_env_prev(env);
    }

    fprintf(stderr, "unbound variable: %s\n", minim_symbol(var));
    exit(1);
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
    // return make_pair(minim_null, minim_car(args));

    fprintf(stderr, "extend-environment: unimplemented\n");
    exit(1);
}

minim_object *environment_variable_value_proc(minim_object *args) {
    // (-> environment symbol -> any)
    // minim_object *env, *name, *exn;

    // env = minim_car(args);
    // name = minim_cadr(args);
    // if (!minim_is_symbol(name)) {
    //     bad_type_exn("environment-variable-value", "symbol?", name);
    // } else if (env_var_is_defined(env, name, 0)) {
    //     // variable found
    //     return env_lookup_var(env, name);
    // } else {
    //     // variable not found
    //     if (minim_is_null(minim_cddr(args))) {
    //         // default exception
    //         fprintf(stderr, "environment-variable-value: variable not bound");
    //         fprintf(stderr, " name: %s", minim_symbol(name));
    //     } else {
    //         // custom exception
    //         exn = minim_car(minim_cddr(args));
    //         if (!minim_is_proc(exn))
    //             bad_type_exn("environment-variable-value", "procedure?", exn);
    //         return call_with_args(exn, minim_null, env);
    //     }
    // }

    // fprintf(stderr, "unreachable");
    // return minim_void;

    fprintf(stderr, "environment-variable-value: unimplemented\n");
    exit(1);
}

minim_object *environment_set_variable_value_proc(minim_object *args) {
    // (-> environment symbol any void)
    // minim_object *env, *name, *val;

    // env = minim_car(args);
    // name = minim_cadr(args);
    // val = minim_car(minim_cddr(args));

    // if (!minim_is_symbol(name))
    //     bad_type_exn("environment-variable-value", "symbol?", name);

    // env_define_var(env, name, val);
    // return minim_void;

    fprintf(stderr, "environment-set-variable-value: unimplemented\n");
    exit(1);
}

minim_object *current_environment_proc(minim_object *args) {
    fprintf(stderr, "current-environment: should not be called directly");
    exit(1);
}
