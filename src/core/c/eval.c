/*
    Interpreter
*/

#include "../minim.h"

#define SET_NAME_IF_CLOSURE(name, val) { \
    if (minim_closurep(val)) { \
        if (minim_closure_name(val) == NULL) \
            minim_closure_name(val) = minim_symbol(name); \
    } \
}

//
//  Interpreter argument stack
//

static void resize_call_args(short req) {
    while (irt_call_args_size < req)  irt_call_args_size *= 2;
    irt_call_args = GC_realloc(irt_call_args, irt_call_args_size * sizeof(mobj));
}

static void resize_saved_args(short req) {
    while (irt_saved_args_size < req)  irt_saved_args_size *= 2;
    irt_saved_args = GC_realloc(irt_saved_args, irt_saved_args_size * sizeof(mobj));
}

void assert_no_call_args() {
    if (irt_call_args_count > 0) {
        fprintf(stderr, "[assert failed] assert_no_call_args(): args on stack %ld\n", irt_call_args_count);
        exit(1);
    }
}

void push_saved_arg(mobj arg) {
    if (irt_saved_args_count >= irt_saved_args_size)
        resize_saved_args(irt_saved_args_count + 2);
    irt_saved_args[irt_saved_args_count++] = arg;
}

void push_call_arg(mobj arg) {
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
    
    memcpy(irt_call_args, &irt_saved_args[irt_saved_args_count - count], count * sizeof(mobj));
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

void bad_syntax_exn(mobj expr) {
    fprintf(stderr, "%s: bad syntax\n", minim_symbol(minim_car(expr)));
    fprintf(stderr, " at: ");
    write_object2(stderr, expr, 1, 0);
    fputc('\n', stderr);
    minim_shutdown(1);
}

void bad_type_exn(const char *name, const char *type, mobj x) {
    fprintf(stderr, "%s: type violation\n", name);
    fprintf(stderr, " expected: %s\n", type);
    fprintf(stderr, " received: ");
    write_object(stderr, x);
    fputc('\n', stderr);
    minim_shutdown(1);
}

void arity_mismatch_exn(const char *name, int min_arity, int max_arity, short actual) {
    if (name != NULL) fprintf(stderr, "%s: ", name);
    fprintf(stderr, "arity mismatch\n");

    if (min_arity == max_arity) {
        fprintf(
            stderr, 
            "expected %d arguments, received %d\n",
            min_arity,
            actual
        );
    } else if (max_arity == ARG_MAX) {
        fprintf(
            stderr, 
            "expected at least %d arguments, received %d\n",
            min_arity,
            actual
        );
    } else {
        fprintf(
            stderr, 
            "expected between %d and %d arguments, received %d\n",
            min_arity,
            max_arity,
            actual
        );
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

static int check_proc_arity(int min_arity, int max_arity, int argc, const char *name) {
    if (argc > max_arity)
        arity_mismatch_exn(name, min_arity, max_arity, argc);
    if (argc < min_arity)
        arity_mismatch_exn(name, min_arity, max_arity, argc);
    return argc;
}

#define check_prim_proc_arity(prim, argc)   \
    check_proc_arity(   \
        minim_prim_argc_low(prim),  \
        minim_prim_argc_high(prim), \
        argc, \
        minim_prim_name(prim)   \
    )

#define check_prim2_proc_arity(prim, argc)   \
    check_proc_arity(   \
        minim_prim2_argc(prim),  \
        minim_prim2_argc(prim), \
        argc, \
        minim_prim2_name(prim)   \
    )

#define check_closure_arity(fn, argc)   \
    check_proc_arity(   \
        minim_closure_argc_low(fn), \
        minim_closure_argc_high(fn),    \
        argc,   \
        minim_closure_name(fn)    \
    )

//
//  Evaluation
//

static long apply_args() {
    mobj lst;

    // check if last argument is a list
    // safe since call_args >= 2
    lst = irt_call_args[irt_call_args_count - 1];

    // for every arg except the last arg: shift down by 1
    // remove the last argument from the stack
    memcpy(irt_call_args, &irt_call_args[1], (irt_call_args_count - 2) * sizeof(mobj));
    irt_call_args_count -= 2;

    // for the last argument: push every element of the list
    // onto the call stack
    while (minim_consp(lst)) {
        push_call_arg(minim_car(lst));
        lst = minim_cdr(lst);
    }

    if (!minim_nullp(lst))
        bad_type_exn("apply", "list?", lst);

    // just for safety
    irt_call_args[irt_call_args_count] = NULL;
    return irt_call_args_count;
}

// Forces `x` to be a single value, that is,
// if `x` is the result of `(values ...)` it unwraps
// the result if `x` contains one value and panics otherwise
static mobj force_single_value(mobj x) {
    minim_thread *th;
    if (minim_valuesp(x)) {
        th = current_thread();
        if (values_buffer_count(th) != 1)
            result_arity_exn(1, values_buffer_count(th));
        return values_buffer_ref(th, 0);
    } else {
        return x;
    }
}

static void get_lambda_arity(mobj e, short *min_arity, short *max_arity) {
    int argc = 0;
    for (e = minim_cadr(e); minim_consp(e); e = minim_cdr(e)) {
        ++argc;
    }

    if (minim_nullp(e)) {
        *min_arity = argc;
        *max_arity = argc;
    } else {
        *min_arity = argc;
        *max_arity = ARG_MAX;
    }
}

static long let_values_env_size(mobj e) {
    mobj bindings, bind;
    long var_count = 0;
    for (bindings = minim_cadr(e); !minim_nullp(bindings); bindings = minim_cdr(bindings)) {
        bind = minim_car(bindings);
        var_count += list_length(minim_car(bind));
    }
    return var_count;
}

static long closure_env_size(mobj fn) {
    if (minim_closure_argc_low(fn) == minim_closure_argc_high(fn)) {
        return minim_closure_argc_low(fn);
    } else {
        return minim_closure_argc_low(fn) + 1;
    }
}

static long eval_exprs(mobj exprs, mobj env) {
    mobj it, result;
    long argc;

    argc = 0;
    assert_no_call_args();
    for (it = exprs; !minim_nullp(it); it = minim_cdr(it)) {
        result = eval_expr(minim_car(it), env);
        push_saved_arg(force_single_value(result));
        ++argc;
    }

    prepare_call_args(argc);
    return argc;
}

mobj call_with_values(mobj producer, mobj consumer, mobj env) {
    mobj produced, result;
    minim_thread *th;
    long stashc, i;

    stashc = stash_call_args();
    produced = call_with_args(producer, env);
    if (minim_valuesp(produced)) {
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

mobj call_with_args(mobj proc, mobj env) {
    mobj expr, result, *args;
    long argc;

    proc = force_single_value(proc);
    args = irt_call_args;
    argc = irt_call_args_count;

application:

    if (minim_prim2p(proc)) {
        // unsafe primitive
        // no checking arity just call with C calling conventions
        check_prim2_proc_arity(proc, argc);
        args = irt_call_args;
        switch (argc) {
        case 0:
            result = ((mobj (*)()) minim_prim2_proc(proc))();
            break;
        case 1:
            result = ((mobj (*)()) minim_prim2_proc(proc))(args[0]);
            break;
        case 2:
            result = ((mobj (*)()) minim_prim2_proc(proc))(args[0], args[1]);
            break;
        case 3:
            result = ((mobj (*)()) minim_prim2_proc(proc))(args[0], args[1], args[2]);
            break;
        case 4:
            result = ((mobj (*)()) minim_prim2_proc(proc))(args[0], args[1], args[2], args[3]);
            break;
        default:
            clear_call_args();
            fprintf(stderr, "error: called unsafe primitive with too many arguments\n");
            fprintf(stderr, " received:");
            write_object(stderr, proc);
            minim_shutdown(1);
            break;
        }
        clear_call_args();
        return result;
    } else if (minim_primp(proc)) {
        check_prim_proc_arity(proc, argc);

        // special case for `apply`
        if (minim_prim(proc) == apply_proc) {
            proc = args[0];
            argc = apply_args();
            goto application;
        }

        // special case for `eval`
        if (minim_prim(proc) == eval_proc) {
            expr = args[0];
            if (argc == 2) {
                env = args[1];
                if (!minim_envp(env))
                    bad_type_exn("eval", "environment?", env);
            }

            clear_call_args();
            check_expr(expr);
            return eval_expr(expr, env);
        }

        if (minim_prim(proc) == current_environment_proc) {
            // special case: `current-environment`
            result = env;
        } else if (minim_prim(proc) == enter_compiled_proc) {
            // special case: `enter-compiled!
            result = call_compiled(env, args[0]);
        } else if (minim_prim(proc) == call_with_values_proc) {
            // special case: `call-with-values`
            result = call_with_values(args[0], args[1], env);
        } else {
            // general case:
            result = minim_prim(proc)(argc, args);
        }
        
        if (argc != irt_call_args_count) {
            fprintf(stderr, "call stack corruption");
            minim_shutdown(1);
        }

        clear_call_args();
        return result;
    } else if (minim_closurep(proc)) {
        mobj vars, rest;
        int i, j;

        // check arity and extend environment
        check_closure_arity(proc, argc);
        env = Menv2(minim_closure_env(proc), closure_env_size(proc));
        args = irt_call_args;

        // process args
        i = 0;
        vars = minim_closure_args(proc);
        while (minim_consp(vars)) {
            env_define_var_no_check(env, minim_car(vars), args[i]);
            vars = minim_cdr(vars);
            ++i;
        }

        // optionally process rest argument
        if (minim_symbolp(vars)) {
            rest = minim_null;
            for (j = argc - 1; j >= i; --j)
                rest = Mcons(args[j], rest);
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

mobj eval_expr(mobj expr, mobj env) {
    
loop:

    if (!minim_consp(expr)) {
        if (minim_truep(expr) ||
            minim_falsep(expr) ||
            minim_fixnump(expr) ||
            minim_charp(expr) ||
            minim_stringp(expr) ||
            minim_boxp(expr) ||
            minim_vectorp(expr)) {
            // self-evaluating
            return expr;
        } else if (minim_symbolp(expr)) {
            // variable
            return env_lookup_var(env, expr);
        } else if (minim_nullp(expr)) {
            // empty application
            fprintf(stderr, "missing procedure expression\n");
            fprintf(stderr, "  in: ");
            write_object2(stderr, expr, 0, 0);
            minim_shutdown(1);
        } else {
            fprintf(stderr, "bad syntax\n");
            write_object(stderr, expr);
            fprintf(stderr, "\n");
            minim_shutdown(1);
        }
    } else {
        mobj proc, head, result, it, bindings, bind, env2, *args;
        minim_thread *th;
        long var_count, idx, argc;

        // special forms
        head = minim_car(expr);
        if (minim_symbolp(head)) {
            if (head == define_values_symbol) {
                // define-values form
                var_count = list_length(minim_cadr(expr));
                result = eval_expr(minim_car(minim_cddr(expr)), env);
                if (minim_valuesp(result)) {;
                    // multi-valued
                    th = current_thread();
                    if (var_count != values_buffer_count(th))
                        result_arity_exn(var_count, values_buffer_count(th));

                    idx = 0;
                    for (it = minim_cadr(expr); !minim_nullp(it); it = minim_cdr(it), ++idx) {
                        SET_NAME_IF_CLOSURE(minim_car(it), values_buffer_ref(th, idx));
                        env_define_var(env, minim_car(it), values_buffer_ref(th, idx));
                    }
                } else {
                    // single-valued
                    if (var_count != 1)
                        result_arity_exn(var_count, 1);
                    SET_NAME_IF_CLOSURE(minim_car(minim_cadr(expr)), result);
                    env_define_var(env, minim_car(minim_cadr(expr)), result);
                }

                GC_REGISTER_LOCAL_ARRAY(expr);
                return minim_void;
            } else if (head == let_values_symbol) {
                // let-values form
                env2 = Menv2(env, let_values_env_size(expr));
                for (bindings = minim_cadr(expr); !minim_nullp(bindings); bindings = minim_cdr(bindings)) {
                    bind = minim_car(bindings);
                    var_count = list_length(minim_car(bind));
                    result = eval_expr(minim_cadr(bind), env);
                    if (minim_valuesp(result)) {
                        // multi-valued
                        th = current_thread();
                        if (var_count != values_buffer_count(th))
                            result_arity_exn(var_count, values_buffer_count(th));

                        idx = 0;
                        for (it = minim_car(bind); !minim_nullp(it); it = minim_cdr(it), ++idx) {
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
                
                expr = Mcons(begin_symbol, (minim_cddr(expr)));
                env = env2;
                goto loop;
            } else if (head == letrec_values_symbol) {
                // letrec-values
                env2 = Menv2(env, let_values_env_size(expr));
                for (bindings = minim_cadr(expr); !minim_nullp(bindings); bindings = minim_cdr(bindings)) {
                    bind = minim_car(bindings);
                    var_count = list_length(minim_car(bind));
                    result = eval_expr(minim_cadr(bind), env);
                    if (minim_valuesp(result)) {
                        // multi-valued
                        th = current_thread();
                        if (var_count != values_buffer_count(th))
                            result_arity_exn(var_count, values_buffer_count(th));

                        idx = 0;
                        for (it = minim_car(bind); !minim_nullp(it); it = minim_cdr(it), ++idx) {
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

                expr = Mcons(begin_symbol, (minim_cddr(expr)));
                goto loop;
            } else if (head == quote_symbol) {
                // quote form
                return minim_cadr(expr);
            } else if (head == quote_syntax_symbol) {
                // quote-syntax form
                if (minim_syntaxp(minim_cadr(expr)))
                    return minim_cadr(expr);
                else
                    return to_syntax(minim_cadr(expr));
            } else if (head == setb_symbol) {
                // set! form
                env_set_var(env, minim_cadr(expr), eval_expr(minim_car(minim_cddr(expr)), env));
                return minim_void;
            } else if (head == if_symbol) {
                // if form
                expr = (minim_falsep(eval_expr(minim_cadr(expr), env)) ?
                       minim_cadr(minim_cddr(expr)) :
                       minim_car(minim_cddr(expr)));
                goto loop;
            } else if (head == lambda_symbol) {
                // lambda form
                short min_arity, max_arity;
                get_lambda_arity(expr, &min_arity, &max_arity);
                return Mclosure(minim_cadr(expr),
                                        Mcons(begin_symbol, minim_cddr(expr)),
                                        env,
                                        min_arity,
                                        max_arity);
            } else if (head == begin_symbol) {
                // begin form
                expr = minim_cdr(expr);
                if (minim_nullp(expr))
                    return minim_void;

                while (!minim_nullp(minim_cdr(expr))) {
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

        if (minim_prim2p(proc)) {
            // unsafe primitive
            // no checking arity just call with C calling conventions
            check_prim2_proc_arity(proc, argc);
            args = irt_call_args;
            switch (argc) {
            case 0:
                result = ((mobj (*)()) minim_prim2_proc(proc))();
                break;
            case 1:
                result = ((mobj (*)()) minim_prim2_proc(proc))(args[0]);
                break;
            case 2:
                result = ((mobj (*)()) minim_prim2_proc(proc))(args[0], args[1]);
                break;
            case 3:
                result = ((mobj (*)()) minim_prim2_proc(proc))(args[0], args[1], args[2]);
                break;
            case 4:
                result = ((mobj (*)()) minim_prim2_proc(proc))(args[0], args[1], args[2], args[3]);
                break;
            default:
                clear_call_args();
                fprintf(stderr, "error: called unsafe primitive with too many arguments\n");
                fprintf(stderr, " received:");
                write_object(stderr, proc);
                fprintf(stderr, "\n at:");
                write_object(stderr, expr);
                minim_shutdown(1);
                break;
            }
            clear_call_args();
            return result;
        } else if (minim_primp(proc)) {
            // primitive
            check_prim_proc_arity(proc, argc);
            args = irt_call_args;
            
            // special case for `apply`
            if (minim_prim(proc) == apply_proc) {
                proc = args[0];
                argc = apply_args();
                goto application;
            }

            // special case for `eval`
            if (minim_prim(proc) == eval_proc) {
                expr = args[0];
                if (argc == 2) {
                    env = args[1];
                    if (!minim_envp(env))
                        bad_type_exn("eval", "environment?", env);
                }
                
                clear_call_args();
                check_expr(expr);
                goto loop;
            }

            if (minim_prim(proc) == current_environment_proc) {
                // special case: `current-environment`
                result = env;
            } else if (minim_prim(proc) == enter_compiled_proc) {
                // special case: `enter-compiled!
                result = call_compiled(env, args[0]);
            } else if (minim_prim(proc) == call_with_values_proc) {
                // special case: `call-with-values`
                result = call_with_values(args[0], args[1], env);
            } else {
                // general case:
                result = minim_prim(proc)(argc, args);
            }

            if (argc != irt_call_args_count) {
                fprintf(stderr, "call stack corruption");
                minim_shutdown(1);
            }

            clear_call_args();
            return result;
        } else if (minim_closurep(proc)) {
            mobj *vars, *rest;
            int i, j;

            // check arity and extend environment
            check_closure_arity(proc, argc);
            env = Menv2(minim_closure_env(proc), closure_env_size(proc));
            args = irt_call_args;

            // process args
            i = 0;
            vars = minim_closure_args(proc);
            while (minim_consp(vars)) {
                env_define_var_no_check(env, minim_car(vars), args[i]);
                vars = minim_cdr(vars);
                ++i;
            }

            // optionally process rest argument
            if (minim_symbolp(vars)) {
                rest = minim_null;
                for (j = argc - 1; j >= i; --j)
                    rest = Mcons(args[j], rest);
                env_define_var_no_check(env, vars, rest);
            }

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
    }

    fprintf(stderr, "unreachable\n");
    minim_shutdown(1);
}
