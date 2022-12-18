/*
    Environments
*/

#include "../minim.h"

minim_globals *globals;
minim_object *empty_env;

//
//  Environments
//
//  environments ::= (<frame0> <frame1> ...)
//  frames       ::= ((<var0> . <val1>) (<var1> . <val1>) ...)
//

static minim_object *make_frame(minim_object *vars, minim_object *vals) {
    return make_assoc(vars, vals);
}

void env_define_var_no_check(minim_object *env, minim_object *var, minim_object *val) {
    minim_object *frame = minim_car(env);
    minim_car(env) = make_pair(make_pair(var, val), frame);
}

minim_object *env_define_var(minim_object *env, minim_object *var, minim_object *val) {
    minim_object *frame = minim_car(env);
    for (minim_object *it = frame; !minim_is_null(it); it = minim_cdr(it)) {
        minim_object *frame_var = minim_caar(frame);
        minim_object *frame_val = minim_cdar(frame);
        if (var == frame_var) {
            minim_cdar(frame) = val;
            return frame_val;
        }
    }

    minim_car(env) = make_pair(make_pair(var, val), frame);
    return NULL;
}

minim_object *env_set_var(minim_object *env, minim_object *var, minim_object *val) {
    while (!minim_is_null(env)) {
        minim_object *frame = minim_car(env);
        while (!minim_is_null(frame)) {
            minim_object *frame_var = minim_caar(frame);
            minim_object *frame_val = minim_cdar(frame);
            if (var == frame_var) {
                minim_cdar(frame) = val;
                return frame_val;
            }

            frame = minim_cdr(frame);
        }

        env = minim_cdr(env);
    }

    fprintf(stderr, "unbound variable: %s\n", minim_symbol(var));
    exit(1);
}

int env_var_is_defined(minim_object *env, minim_object *var, int recursive) {
    minim_object *frame, *frame_var;

    if (minim_is_null(env))
        return 0;

    for (frame = minim_car(env); !minim_is_null(frame); frame = minim_cdr(frame)) {
        frame_var = minim_caar(frame);
        if (var == frame_var)
            return 1; 
    }

    return recursive && env_var_is_defined(minim_cdr(env), var, 1);
}

minim_object *env_lookup_var(minim_object *env, minim_object *var) {
    while (!minim_is_null(env)) {
        minim_object *frame = minim_car(env);
        while (!minim_is_null(frame)) {
            minim_object *frame_var = minim_caar(frame);
            minim_object *frame_val = minim_cdar(frame);
            if (var == frame_var)
                return frame_val;

            frame = minim_cdr(frame);
        }

        env = minim_cdr(env);
    }

    fprintf(stderr, "unbound variable: %s\n", minim_symbol(var));
    exit(1);
}

minim_object *extend_env(minim_object *vars,
                         minim_object *vals,
                         minim_object *base_env) {
    return make_pair(make_frame(vars, vals), base_env);
}

minim_object *setup_env() {
    return extend_env(minim_null, minim_null, empty_env);
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
    return make_pair(minim_null, minim_car(args));
}

minim_object *environment_variable_value_proc(minim_object *args) {
    // (-> environment symbol -> any)
    minim_object *env, *name, *exn;

    env = minim_car(args);
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
    name = minim_cadr(args);
    val = minim_car(minim_cddr(args));

    if (!minim_is_symbol(name))
        bad_type_exn("environment-variable-value", "symbol?", name);

    env_define_var(env, name, val);
    return minim_void;
}

minim_object *current_environment_proc(minim_object *args) {
    fprintf(stderr, "current-environment: should not be called directly");
    exit(1);
}
