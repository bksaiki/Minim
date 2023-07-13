/*
    Interpreter
*/

#include "../minim.h"

#define SET_NAME_IF_CLOSURE(name, val) {                    \
    if (minim_is_closure(val)) {                       \
        if (minim_closure_name(val) == NULL)                \
            minim_closure_name(val) = minim_symbol(name);   \
    }                                                       \
}

//
//  Interpreter argument stack
//

static void resize_call_args(short req) {
    while (irt_call_args_size < req)  irt_call_args_size *= 2;
    irt_call_args = GC_realloc(irt_call_args, irt_call_args_size * sizeof(minim_object*));
}

static void resize_saved_args(short req) {
    while (irt_saved_args_size < req)  irt_saved_args_size *= 2;
    irt_saved_args = GC_realloc(irt_saved_args, irt_saved_args_size * sizeof(minim_object*));
}

void assert_no_call_args() {
    if (irt_call_args_count > 0) {
        fprintf(stderr, "[assert failed] assert_no_call_args(): args on stack %ld\n", irt_call_args_count);
        exit(1);
    }
}

void push_saved_arg(minim_object *arg) {
    if (irt_saved_args_count >= irt_saved_args_size)
        resize_saved_args(irt_saved_args_count + 2);
    irt_saved_args[irt_saved_args_count++] = arg;
}

void push_call_arg(minim_object *arg) {
    if (irt_call_args_count >= irt_call_args_size)
        resize_call_args(irt_call_args_count + 1);
    irt_call_args[irt_call_args_count++] = arg;
    irt_call_args[irt_call_args_count] = NULL;
}

void prepare_call_args(long count) {
    if (count > irt_saved_args_count) {
        fprintf(stderr, "prepare_call_args(): trying to restore %ld values, only found %ld\n",
                        count, irt_saved_args_count);
        minim_shutdown(1);
    }

    if (irt_call_args_count > 0) {
        fprintf(stderr, "prepare_call_args(): already prepared, found %ld arguments\n", irt_call_args_count);
        minim_shutdown(1);
    }

    if (count < irt_call_args_size)
        resize_call_args(count + 1);
    
    memcpy(irt_call_args, &irt_saved_args[irt_saved_args_count - count], count * sizeof(minim_object*));
    irt_call_args[count] = NULL;
    irt_call_args_count = count;
    irt_saved_args_count -= count;
}

long stash_call_args() {
    long stash_count, i;
    
    stash_count = irt_call_args_count;
    for (i = 0; i < stash_count; i++)
        push_saved_arg(irt_call_args[i]);

    irt_call_args_count = 0;
    return stash_count;
}

void clear_call_args() {
    irt_call_args_count = 0;
}

//
//  Runtime check
//

void bad_syntax_exn(minim_object *expr) {
    fprintf(stderr, "%s: bad syntax\n", minim_symbol(minim_car(expr)));
    fprintf(stderr, " at: ");
    write_object2(stderr, expr, 1, 0);
    fputc('\n', stderr);
    minim_shutdown(1);
}

void bad_type_exn(const char *name, const char *type, minim_object *x) {
    fprintf(stderr, "%s: type violation\n", name);
    fprintf(stderr, " expected: %s\n", type);
    fprintf(stderr, " received: ");
    write_object(stderr, x);
    fputc('\n', stderr);
    minim_shutdown(1);
}

void arity_mismatch_exn(const char *name, proc_arity *arity, short actual) {
    if (name != NULL)
        fprintf(stderr, "%s: ", name);
    fprintf(stderr, "arity mismatch\n");

    if (proc_arity_is_fixed(arity)) {
        switch (proc_arity_min(arity)) {
        case 0:
            fprintf(stderr, " expected 0 arguments, received %d\n", actual);
            break;
        
        case 1:
            fprintf(stderr, " expected 1 argument, received %d\n", actual);
            break;

        default:
            fprintf(stderr, " expected %d arguments, received %d\n",
                            proc_arity_min(arity), actual);
        }
    } else if (proc_arity_is_unbounded(arity)) {
        switch (proc_arity_min(arity)) {
        case 0:
            fprintf(stderr, " expected at least 0 arguments, received %d\n", actual);
            break;
        
        case 1:
            fprintf(stderr, " expected at least 1 argument, received %d\n", actual);
            break;

        default:
            fprintf(stderr, " expected at least %d arguments, received %d\n",
                            proc_arity_min(arity), actual);
        }
    } else {
        fprintf(stderr, " expected between %d and %d arguments, received %d\n",
                        proc_arity_min(arity), proc_arity_max(arity), actual);
    }

    minim_shutdown(1);
}

void result_arity_exn(short expected, short actual) {
    fprintf(stderr, "result arity mismatch\n");
    fprintf(stderr, "  expected: %d, received: %d\n", expected, actual);
    minim_shutdown(1);
}

void uncallable_prim_exn(const char *name) {
    fprintf(stderr, "%s: should not be called directly", name);
    minim_shutdown(1);
}

static int check_proc_arity(proc_arity *arity, int argc, const char *name) {
    int min_arity, max_arity;

    min_arity = proc_arity_min(arity);
    max_arity = proc_arity_max(arity);

    if (argc > max_arity)
        arity_mismatch_exn(name, arity, argc);

    if (argc < min_arity)
        arity_mismatch_exn(name, arity, argc);

    return argc;
}

#define check_prim_proc_arity(prim, argc)   \
    check_proc_arity(&minim_prim_arity(prim), argc, minim_prim_proc_name(prim))

#define check_closure_arity(prim, argc)    \
    check_proc_arity(&minim_closure_arity(prim), argc, minim_closure_name(prim))

//
//  Syntax check
//

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum>)
static void check_1ary_syntax(minim_object *expr) {
    minim_object *rest = minim_cdr(expr);
    if (!minim_is_pair(rest) || !minim_is_null(minim_cdr(rest)))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> <datum> <datum>)
static void check_3ary_syntax(minim_object *expr) {
    minim_object *rest;
    
    rest = minim_cdr(expr);
    if (!minim_is_pair(rest))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
    if (!minim_is_pair(rest))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
    if (!minim_is_pair(rest) || !minim_is_null(minim_cdr(rest)))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr must be `(<name> <symbol> <datum>)`
static void check_assign(minim_object *expr) {
    minim_object *rest;

    rest = minim_cdr(expr);
    if (!minim_is_pair(rest) || !minim_is_symbol(minim_car(rest)))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
    if (!minim_is_pair(rest))
        bad_syntax_exn(expr);

    if (!minim_is_null(minim_cdr(rest)))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> (<symbol> ...) <datum>)
static void check_define_values(minim_object *expr) {
    minim_object *rest, *ids;
    
    rest = minim_cdr(expr);
    if (!minim_is_pair(rest))
        bad_syntax_exn(expr);

    ids = minim_car(rest);
    while (minim_is_pair(ids)) {
        if (!minim_is_symbol(minim_car(ids)))
            bad_syntax_exn(expr);
        ids = minim_cdr(ids);
    } 

    if (!minim_is_null(ids))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
    if (!minim_is_pair(rest) || !minim_is_null(minim_cdr(rest)))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(let . <???>)`
// Check: `expr` must be `(let-values ([(<var> ...) <val>] ...) <body> ...)`
// Does not check if each `<body>` is an expression.
// Does not check if `<body> ...` forms a list.
static void check_let_values(minim_object *expr) {
    minim_object *bindings, *bind, *ids;
    
    bindings = minim_cdr(expr);
    if (!minim_is_pair(bindings) || !minim_is_pair(minim_cdr(bindings)))
        bad_syntax_exn(expr);
    
    bindings = minim_car(bindings);
    while (minim_is_pair(bindings)) {
        bind = minim_car(bindings);
        if (!minim_is_pair(bind))
            bad_syntax_exn(expr);

        ids = minim_car(bind);
        while (minim_is_pair(ids)) {
            if (!minim_is_symbol(minim_car(ids)))
                bad_syntax_exn(expr);
            ids = minim_cdr(ids);
        } 

        if (!minim_is_null(ids))
            bad_syntax_exn(expr);

        bind = minim_cdr(bind);
        if (!minim_is_pair(bind) || !minim_is_null(minim_cdr(bind)))
            bad_syntax_exn(expr);

        bindings = minim_cdr(bindings);
    }

    if (!minim_is_null(bindings))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> ...)`
static void check_begin(minim_object *expr) {
    minim_object *rest = minim_cdr(expr);

    while (minim_is_pair(rest))
        rest = minim_cdr(rest);

    if (!minim_is_null(rest))
        bad_syntax_exn(expr);
}

static void check_lambda(minim_object *expr, short *min_arity, short *max_arity) {
    minim_object *args = minim_cadr(expr);
    int argc = 0;

    while (minim_is_pair(args)) {
        if (!minim_is_symbol(minim_car(args))) {
            fprintf(stderr, "expected a identifier for an argument:\n ");
            write_object(stderr, expr);
            fputc('\n', stderr);
            minim_shutdown(1);
        }

        args = minim_cdr(args);
        ++argc;
    }

    if (minim_is_null(args)) {
        *min_arity = argc;
        *max_arity = argc;
    } else if (minim_is_symbol(args)) {
        *min_arity = argc;
        *max_arity = ARG_MAX;
    } else {
        fprintf(stderr, "expected an identifier for the rest argument:\n ");
        write_object(stderr, expr);
        fputc('\n', stderr);
        minim_shutdown(1);
    }
}

//
//  Evaluation
//

static long apply_args() {
    minim_object *lst;

    // check if last argument is a list
    // safe since call_args >= 2
    lst = irt_call_args[irt_call_args_count - 1];

    // for every arg except the last arg: shift down by 1
    // remove the last argument from the stack
    memcpy(irt_call_args, &irt_call_args[1], (irt_call_args_count - 2) * sizeof(minim_object *));
    irt_call_args_count -= 2;

    // for the last argument: push every element of the list
    // onto the call stack
    while (minim_is_pair(lst)) {
        push_call_arg(minim_car(lst));
        lst = minim_cdr(lst);
    }

    if (!minim_is_null(lst))
        bad_type_exn("apply", "list?", lst);

    // just for safety
    irt_call_args[irt_call_args_count] = NULL;
    return irt_call_args_count;
}

// Forces `x` to be a single value, that is,
// if `x` is the result of `(values ...)` it unwraps
// the result if `x` contains one value and panics
// otherwise
static minim_object *force_single_value(minim_object* x) {
    minim_thread *th;
    if (minim_is_values(x)) {
        th = current_thread();
        if (values_buffer_count(th) != 1)
            result_arity_exn(1, values_buffer_count(th));
        return values_buffer_ref(th, 0);
    } else {
        return x;
    }
}

static long eval_exprs(minim_object *exprs, minim_object *env) {
    minim_object *it, *result;
    long argc;

    argc = 0;
    assert_no_call_args();
    for (it = exprs; !minim_is_null(it); it = minim_cdr(it)) {
        result = eval_expr(minim_car(it), env);
        push_saved_arg(force_single_value(result));
        ++argc;
    }

    prepare_call_args(argc);
    return argc;
}

minim_object *call_with_values(minim_object *producer,
                               minim_object *consumer,
                               minim_object *env) {
    minim_object *produced, *result;
    minim_thread *th;
    long stashc, i;

    stashc = stash_call_args();
    produced = call_with_args(producer, env);
    if (minim_is_values(produced)) {
        // need to unpack
        th = current_thread();
        for (i = 0; i < values_buffer_count(th); ++i)
            push_call_arg(values_buffer_ref(th, i));
    } else {
        push_call_arg(produced);
    }

    result = call_with_args(consumer, env);
    prepare_call_args(stashc);
    return result;
}

minim_object *call_with_args(minim_object *proc, minim_object *env) {
    minim_object *expr, **args, *result;
    long argc;

    proc = force_single_value(proc);
    args = irt_call_args;
    argc = irt_call_args_count;

application:

    if (minim_is_prim_proc(proc)) {
        check_prim_proc_arity(proc, argc);

        // special case for `apply`
        if (minim_prim_proc(proc) == apply_proc) {
            proc = args[0];
            argc = apply_args();
            goto application;
        }

        // special case for `eval`
        if (minim_prim_proc(proc) == eval_proc) {
            expr = args[0];
            if (argc == 2) {
                env = args[1];
                if (!minim_is_env(env))
                    bad_type_exn("eval", "environment?", env);
            }

            clear_call_args();
            return eval_expr(expr, env);
        }

        if (minim_prim_proc(proc) == current_environment_proc) {
            // special case: `current-environment`
            result = env;
        } else if (minim_prim_proc(proc) == for_each_proc) {
            // special case: `for-each`
            result = for_each(args[0], argc - 1, &args[1], env);
        } else if (minim_prim_proc(proc) == map_proc) {
            // special case: `map`
            result = map_list(args[0], argc - 1, &args[1], env);
        } else if (minim_prim_proc(proc) == andmap_proc) {
            // special case: `andmap`
            result = andmap(args[0], argc - 1, &args[1], env);
        } else if (minim_prim_proc(proc) == ormap_proc) {
            // special case: `ormap`
            result = ormap(args[0], argc - 1, &args[1], env);
        } else if (minim_prim_proc(proc) == enter_compiled_proc) {
            // special case: `enter-compiled!
            result = call_compiled(env, args[0]);
        } else if (minim_prim_proc(proc) == call_with_values_proc) {
            // special case: `call-with-values`
            result = call_with_values(args[0], args[1], env);
        } else {
            // general case:
            result = minim_prim_proc(proc)(argc, args);
        }
        
        if (argc != irt_call_args_count) {
            fprintf(stderr, "call stack corruption");
            minim_shutdown(1);
        }

        clear_call_args();
        return result;
    } else if (minim_is_closure(proc)) {
        minim_object *vars, *rest;
        int i, j;

        // check arity and extend environment
        check_closure_arity(proc, argc);
        env = make_environment(minim_closure_env(proc));
        args = irt_call_args;

        // process args
        i = 0;
        vars = minim_closure_args(proc);
        while (minim_is_pair(vars)) {
            env_define_var_no_check(env, minim_car(vars), args[i]);
            vars = minim_cdr(vars);
            ++i;
        }

        // optionally process rest argument
        if (minim_is_symbol(vars)) {
            rest = minim_null;
            for (j = argc - 1; j >= i; --j)
                rest = make_pair(args[j], rest);
            env_define_var_no_check(env, vars, rest);
        }

        // tail call (not really)
        clear_call_args();
        expr = minim_closure_body(proc);
        return eval_expr(expr, env);
    } else {
        clear_call_args();
        fprintf(stderr, "error: not a procedure\n");
        fprintf(stderr, " received:");
        write_object(stderr, proc);
        fprintf(stderr, "\n");
        minim_shutdown(1);
    }
}

static void eval_define_values(minim_object *expr, minim_object *env) {
    minim_object *result, *it;
    minim_thread *th;
    long var_count, idx;

    var_count = list_length(minim_cadr(expr));
    result = eval_expr(minim_car(minim_cddr(expr)), env);
    if (minim_is_values(result)) {;
        // multi-valued
        th = current_thread();
        if (var_count != values_buffer_count(th))
            result_arity_exn(var_count, values_buffer_count(th));

        idx = 0;
        for (it = minim_cadr(expr); !minim_is_null(it); it = minim_cdr(it)) {
            SET_NAME_IF_CLOSURE(minim_car(it), values_buffer_ref(th, idx));
            env_define_var(env, minim_car(it), values_buffer_ref(th, idx));
            ++idx;
        }
    } else {
        // single-valued
        if (var_count != 1)
            result_arity_exn(var_count, 1);
        SET_NAME_IF_CLOSURE(minim_car(minim_cadr(expr)), result);
        env_define_var(env, minim_car(minim_cadr(expr)), result);
    }

    GC_REGISTER_LOCAL_ARRAY(expr);
}

static minim_object *eval_let_values(minim_object *expr, minim_object *env) {
    minim_object *bindings, *bind, *it, *result, *env2;
    minim_thread *th;
    long var_count, idx;
    
    env2 = make_environment(env);
    for (bindings = minim_cadr(expr); !minim_is_null(bindings); bindings = minim_cdr(bindings)) {
        bind = minim_car(bindings);
        var_count = list_length(minim_car(bind));
        result = eval_expr(minim_cadr(bind), env);
        if (minim_is_values(result)) {
            // multi-valued
            th = current_thread();
            if (var_count != values_buffer_count(th))
                result_arity_exn(var_count, values_buffer_count(th));

            idx = 0;
            for (it = minim_car(bind); !minim_is_null(it); it = minim_cdr(it), ++idx) {
                SET_NAME_IF_CLOSURE(minim_car(it), values_buffer_ref(th, idx));
                env_define_var(env2, minim_car(it), values_buffer_ref(th, idx));
            }
        } else {
            // single-valued
            if (var_count != 1)
                result_arity_exn(var_count, 1);
            SET_NAME_IF_CLOSURE(minim_caar(bind), result);
            env_define_var(env2, minim_caar(bind), result);
        }
    }

    return env2;
}

static minim_object *eval_letrec_values(minim_object *expr, minim_object *env) {
    minim_object *bindings, *bind, *it, *result;
    minim_thread *th;
    long var_count, idx;

    env = make_environment(env);
    for (bindings = minim_cadr(expr); !minim_is_null(bindings); bindings = minim_cdr(bindings)) {
        bind = minim_car(bindings);
        var_count = list_length(minim_car(bind));
        result = eval_expr(minim_cadr(bind), env);
        if (minim_is_values(result)) {
            // multi-valued
            th = current_thread();
            if (var_count != values_buffer_count(th))
                result_arity_exn(var_count, values_buffer_count(th));

            idx = 0;
            for (it = minim_car(bind); !minim_is_null(it); it = minim_cdr(it), ++idx) {
                SET_NAME_IF_CLOSURE(minim_car(it), values_buffer_ref(th, idx));
                env_define_var(env, minim_car(it), values_buffer_ref(th, idx));
            }
        } else {
            // single-valued
            if (var_count != 1)
                result_arity_exn(var_count, 1);

            SET_NAME_IF_CLOSURE(minim_caar(bind), result);
            env_define_var(env, minim_caar(bind), result);
        }
    }

    return env;
}

static minim_object *eval_closure(minim_object *proc, int argc, minim_object **args) {
    minim_object *env, *rest, *vars;
    long i, j;

    env = make_environment(minim_closure_env(proc));

    // process args
    i = 0;
    vars = minim_closure_args(proc);
    while (minim_is_pair(vars)) {
        env_define_var_no_check(env, minim_car(vars), args[i]);
        vars = minim_cdr(vars);
        ++i;
    }

    // optionally process rest argument
    if (minim_is_symbol(vars)) {
        rest = minim_null;
        for (j = argc - 1; j >= i; --j)
            rest = make_pair(args[j], rest);
        env_define_var_no_check(env, vars, rest);
    }

    return env;
}

minim_object *eval_expr(minim_object *expr, minim_object *env) {
    minim_object *head, *proc, *result, **args;
    long argc;

loop:

    if (minim_is_true(expr) ||
        minim_is_false(expr) ||
        minim_is_fixnum(expr) ||
        minim_is_char(expr) ||
        minim_is_string(expr) ||
        minim_is_box(expr) ||
        minim_is_vector(expr)) {
        // self-evaluating
        return expr;
    } else if (minim_is_symbol(expr)) {
        // variable
        return env_lookup_var(env, expr);
    } else if (minim_is_null(expr)) {
        fprintf(stderr, "missing procedure expression\n");
        fprintf(stderr, "  in: ");
        write_object2(stderr, expr, 0, 0);
        minim_shutdown(1);
    } else if (minim_is_pair(expr)) {
        // special forms
        head = minim_car(expr);
        if (minim_is_symbol(head)) {
            if (head == define_values_symbol) {
                // define-values form
                check_define_values(expr);
                eval_define_values(expr, env);
                return minim_void;
            } else if (head == let_values_symbol) {
                // let-values form
                check_let_values(expr);
                env = eval_let_values(expr, env);
                expr = make_pair(begin_symbol, (minim_cddr(expr)));
                goto loop;
            } else if (head == letrec_values_symbol) {
                // letrec-values
                check_let_values(expr);
                env = eval_letrec_values(expr, env);
                expr = make_pair(begin_symbol, (minim_cddr(expr)));
                goto loop;
            } else if (head == quote_symbol) {
                // quote form
                check_1ary_syntax(expr);
                return minim_cadr(expr);
            } else if (head == quote_syntax_symbol) {
                // quote-syntax form
                check_1ary_syntax(expr);
                if (minim_is_syntax(minim_cadr(expr)))
                    return minim_cadr(expr);
                else
                    return to_syntax(minim_cadr(expr));
            } else if (head == setb_symbol) {
                // set! form
                check_assign(expr);
                env_set_var(env, minim_cadr(expr), eval_expr(minim_car(minim_cddr(expr)), env));
                return minim_void;
            } else if (head == if_symbol) {
                // if form
                check_3ary_syntax(expr);
                expr = (minim_is_false(eval_expr(minim_cadr(expr), env)) ?
                       minim_cadr(minim_cddr(expr)) :
                       minim_car(minim_cddr(expr)));
                goto loop;
            } else if (head == lambda_symbol) {
                short min_arity, max_arity;

                // lambda form
                check_lambda(expr, &min_arity, &max_arity);
                return make_closure(minim_cadr(expr),
                                        make_pair(begin_symbol, minim_cddr(expr)),
                                        env,
                                        min_arity,
                                        max_arity);
            } else if (head == begin_symbol) {
                // begin form
                check_begin(expr);
                expr = minim_cdr(expr);
                if (minim_is_null(expr))
                    return minim_void;

                while (!minim_is_null(minim_cdr(expr))) {
                    eval_expr(minim_car(expr), env);
                    expr = minim_cdr(expr);
                }

                expr = minim_car(expr);
                goto loop;
            }
        }

        proc = force_single_value(eval_expr(head, env));
        argc = eval_exprs(minim_cdr(expr), env);

application:

        if (minim_is_prim_proc(proc)) {
            check_prim_proc_arity(proc, argc);
            args = irt_call_args;
            
            // special case for `apply`
            if (minim_prim_proc(proc) == apply_proc) {
                proc = args[0];
                argc = apply_args();
                goto application;
            }

            // special case for `eval`
            if (minim_prim_proc(proc) == eval_proc) {
                expr = args[0];
                if (argc == 2) {
                    env = args[1];
                    if (!minim_is_env(env))
                        bad_type_exn("eval", "environment?", env);
                }
                
                clear_call_args();
                goto loop;
            }

            if (minim_prim_proc(proc) == current_environment_proc) {
                // special case: `current-environment`
                result = env;
            } else if (minim_prim_proc(proc) == for_each_proc) {
                // special case: `for-each`
                result = for_each(args[0], argc - 1, &args[1], env);
            } else if (minim_prim_proc(proc) == map_proc) {
                // special case: `map`
                result = map_list(args[0], argc - 1, &args[1], env);
            } else if (minim_prim_proc(proc) == andmap_proc) {
                // special case: `andmap`
                result = andmap(args[0], argc - 1, &args[1], env);
            } else if (minim_prim_proc(proc) == ormap_proc) {
                // special case: `ormap`
                result = ormap(args[0], argc - 1, &args[1], env);
            } else if (minim_prim_proc(proc) == enter_compiled_proc) {
                // special case: `enter-compiled!
                result = call_compiled(env, args[0]);
            } else if (minim_prim_proc(proc) == call_with_values_proc) {
                // special case: `call-with-values`
                result = call_with_values(args[0], args[1], env);
            } else {
                // general case:
                result = minim_prim_proc(proc)(argc, args);
            }

            if (argc != irt_call_args_count) {
                fprintf(stderr, "call stack corruption");
                minim_shutdown(1);
            }

            clear_call_args();
            return result;
        } else if (minim_is_closure(proc)) {
            // check arity and extend environment
            check_closure_arity(proc, argc);
            env = eval_closure(proc, argc, irt_call_args);

            // tail call
            clear_call_args();
            expr = minim_closure_body(proc);
            goto loop;
        } else {
            clear_call_args();
            fprintf(stderr, "error: not a procedure\n");
            fprintf(stderr, " received:");
            write_object(stderr, proc);
            fprintf(stderr, "\n at:");
            write_object(stderr, expr);
            fprintf(stderr, "\n");
            minim_shutdown(1);
        }
    } else {
        fprintf(stderr, "bad syntax\n");
        write_object(stderr, expr);
        fprintf(stderr, "\n");
        minim_shutdown(1);
    }

    fprintf(stderr, "unreachable\n");
    minim_shutdown(1);
}
