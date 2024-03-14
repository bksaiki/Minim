/*
    Interpreter
*/

#include "../minim.h"

#define SET_NAME_IF_CLOSURE(name, val) { \
    if (minim_closurep(val) && minim_closure_name(val) == NULL) { \
        minim_closure_name(val) = minim_symbol(name); \
    } \
}

//
//  Runtime check
//

void assert_no_call_args() {
    if (eval_buffer_count(current_thread()) > 0) {
        fprintf(
            stderr,
            "[assert failed] assert_no_call_args(): args on stack %d\n",
            eval_buffer_count(current_thread())
        );

        exit(1);
    }
}

void bad_syntax_exn(mobj expr) {
    fprintf(stderr, "%s: bad syntax\n", minim_symbol(minim_car(expr)));
    fprintf(stderr, " at: ");
    write_object(stderr, expr);
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

void result_arity_exn(const char *name, short expected, short actual) {
    if (name == NULL) {
        fprintf(stderr, "result arity mismatch\n");
    } else {
        fprintf(stderr, "%s: result arity mismatch\n", name);
    }

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

static void push_arg(mobj x) {
    minim_thread *th = current_thread();
    if (eval_buffer_count(th) >= eval_buffer_size(th)) {
        eval_buffer_size(th) = 2 * eval_buffer_count(th);
        eval_buffer(th) = GC_realloc(eval_buffer(th), eval_buffer_size(th) * sizeof(mobj));
    }

    eval_buffer_ref(th, eval_buffer_count(th)) = x;
    eval_buffer_count(th)++;
}

static void save_frame(mobj env) {
    minim_thread *th;
    mobj prev, curr;

    // allocation new frame
    th = current_thread();
    prev = current_continuation(th);
    curr = Mcontinuation(prev, env, eval_buffer_count(th));

    fprintf(stderr, "new cont frame %p => %p (%d)\n", prev, curr, eval_buffer_count(th));

    // copy arguments into frame and clear the eval stack
    memcpy(continuation_saved(curr), eval_buffer(th), eval_buffer_count(th));
    eval_buffer_count(th) = 0;

    // update thread info
    current_continuation(th) = curr;
}

static void resume_frame() {
    minim_thread *th;
    mobj frame;

    // get frame
    th = current_thread();
    frame = current_continuation(th);

    // copy all values in frame to the eval stack and update the parameters
    eval_buffer_count(th) = continuation_len(frame);
    memcpy(eval_buffer(th), continuation_saved(frame), eval_buffer_count(th));

    fprintf(stderr, "popping cont frame %p => %p\n", frame, continuation_prev(frame));

    // update thread info
    current_continuation(th) = continuation_prev(frame);
}

static void do_values() {
    minim_thread *th = current_thread();
    if (eval_buffer_count(th) >= values_buffer_size(th)) {
        values_buffer_size(th) = 2 * eval_buffer_count(th);
        values_buffer(th) = GC_realloc(values_buffer(th), values_buffer_size(th) * sizeof(mobj));
    }

    memcpy(values_buffer(th), eval_buffer(th), eval_buffer_count(th));
    values_buffer_count(th) = eval_buffer_count(th);
}

// static long apply_args() {
//     mobj lst;

//     // check if last argument is a list
//     // safe since call_args >= 2
//     lst = irt_call_args[irt_call_args_count - 1];

//     // for every arg except the last arg: shift down by 1
//     // remove the last argument from the stack
//     memcpy(irt_call_args, &irt_call_args[1], (irt_call_args_count - 2) * sizeof(mobj));
//     irt_call_args_count -= 2;

//     // for the last argument: push every element of the list
//     // onto the call stack
//     while (minim_consp(lst)) {
//         push_call_arg(minim_car(lst));
//         lst = minim_cdr(lst);
//     }

//     if (!minim_nullp(lst))
//         bad_type_exn("apply", "list?", lst);

//     // just for safety
//     irt_call_args[irt_call_args_count] = NULL;
//     return irt_call_args_count;
// }

// Forces `x` to be a single value, that is,
// if `x` is the result of `(values ...)` it unwraps
// the result if `x` contains one value and panics otherwise
static mobj force_single_value(mobj x) {
    minim_thread *th;
    if (minim_valuesp(x)) {
        th = current_thread();
        if (values_buffer_count(th) != 1)
            result_arity_exn(NULL, 1, values_buffer_count(th));
        values_buffer_count(th) = 0;
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

static void bind_values(const char *name, mobj env, mobj result, mobj binds) {
    long bindc = list_length(binds);
    if (minim_valuesp(result)) {
        // multi-valued
        minim_thread *th = current_thread();
        if (bindc != values_buffer_count(th))
            result_arity_exn(name, bindc, values_buffer_count(th));

        for (long idx = 0; !minim_nullp(binds); ++idx) {
            SET_NAME_IF_CLOSURE(minim_car(binds), values_buffer_ref(th, idx));
            env_define_var(env, minim_car(binds), values_buffer_ref(th, idx));
            binds = minim_cdr(binds);
        }
    } else {
        // single-valued
        if (bindc != 1)
            result_arity_exn(name, bindc, 1);
        SET_NAME_IF_CLOSURE(minim_car(binds), result);
        env_define_var(env, minim_car(binds), result);
    }
}

// static long eval_exprs(mobj exprs, mobj env) {
//     mobj it, result;
//     long argc;

//     argc = 0;
//     assert_no_call_args();
//     for (it = exprs; !minim_nullp(it); it = minim_cdr(it)) {
//         result = eval_expr(minim_car(it), env);
//         push_saved_arg(force_single_value(result));
//         ++argc;
//     }

//     prepare_call_args(argc);
//     return argc;
// }

mobj eval_expr(mobj expr, mobj env) {
    mobj *args, proc;
    long argc;

    // HACK: call-with-values will call with a procedure at `expr`
    if (minim_procp(expr)) {
        proc = expr;
        args = eval_buffer(current_thread());
        argc = eval_buffer_count(current_thread());
        goto application;
    }

loop:

    fprintf(stderr, "%d %d ", eval_buffer_count(current_thread()), values_buffer_count(current_thread()));
    write_object(stderr, expr);
    fprintf(stderr, "\n");

    if (minim_consp(expr)) {
        // special forms
        mobj head = minim_car(expr);
        if (minim_symbolp(head)) {
            if (head == define_values_symbol) {
                // define-values form
                mobj result = eval_expr(minim_car(minim_cddr(expr)), env);
                bind_values("define-values", env, result, minim_cadr(expr));
                return minim_void;
            } else if (head == let_values_symbol) {
                // let-values form
                mobj env2 = Menv2(env, let_values_env_size(expr));
                mobj bindings = minim_cadr(expr);
                while (!minim_nullp(bindings)) {
                    mobj bind = minim_car(bindings);
                    mobj result = eval_expr(minim_cadr(bind), env);
                    bind_values("let-values", env2, result, minim_car(bind));
                    bindings = minim_cdr(bindings);
                }

                expr = Mcons(begin_symbol, minim_cddr(expr));
                env = env2;
                goto loop;
            } else if (head == letrec_values_symbol) {
                // letrec-values forms
                mobj env2 = Menv2(env, let_values_env_size(expr));
                mobj bindings = minim_cadr(expr);
                while (!minim_nullp(bindings)) {
                    mobj bind = minim_car(bindings);
                    mobj result = eval_expr(minim_cadr(bind), env2);
                    bind_values("letrec-values", env2, result, minim_car(bind));
                    bindings = minim_cdr(bindings);
                }

                expr = Mcons(begin_symbol, minim_cddr(expr));
                env = env2;
                goto loop;
            } else if (head == quote_symbol) {
                // quote form
                return minim_cadr(expr);
            } else if (head == quote_syntax_symbol) {
                // quote-syntax form
                return to_syntax(minim_cadr(expr));
            } else if (head == setb_symbol) {
                // set! form
                env_set_var(env, minim_cadr(expr), eval_expr(minim_car(minim_cddr(expr)), env));
                return minim_void;
            } else if (head == if_symbol) {
                // if form
                if (minim_falsep(eval_expr(minim_cadr(expr), env))) {
                    expr = minim_cadr(minim_cddr(expr));
                } else {
                    expr = minim_car(minim_cddr(expr));
                }

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

        save_frame(env);
        proc = force_single_value(eval_expr(head, env));
        for (mobj it = minim_cdr(expr); !minim_nullp(it); it = minim_cdr(it))
            push_arg(force_single_value(eval_expr(minim_car(it), env)));
        
        args = eval_buffer(current_thread());
        argc = eval_buffer_count(current_thread());

application:

        if (minim_primp(proc)) {
            // primitives
            mobj result;

            check_prim_proc_arity(proc, argc);    
            if (minim_prim(proc) == apply_proc) {
                // special case for `apply`
                proc = force_single_value(args[0]);
                if (!minim_procp(proc)) {
                    fprintf(stderr, "apply: expected a procedure\n");
                    fprintf(stderr, " received:");
                    write_object(stderr, proc);
                    fprintf(stderr, "\n");
                    minim_shutdown(1);
                }

                fprintf(stderr, "apply: unimplemented\n");
                minim_shutdown(1);
                // argc = apply_args();
                goto application;
            } else if (minim_prim(proc) == eval_proc) {
                // special case for `eval`
                expr = args[0];
                if (argc == 2) {
                    env = args[1];
                    if (!minim_envp(env))
                        bad_type_exn("eval", "environment?", env);
                }
                
                resume_frame();
                check_expr(expr);
                goto loop;
            } else if (minim_prim(proc) == values_proc) {
                // special case: `values`
                do_values();
                resume_frame();
                return minim_values;
            } else if (minim_prim(proc) == call_with_values_proc) {
                // special case: `call-with-values`
                // mobj producer, consumer;
                // long stashc;
                fprintf(stderr, "call-with-values: unimplemented\n");
                minim_shutdown(1);
                
                // producer = force_single_value(args[0]);
                // consumer = args[1];
                // if (!minim_procp(producer)) {
                //     fprintf(stderr, "call-with-values: expected a procedure\n");
                //     fprintf(stderr, " received:");
                //     write_object(stderr, producer);
                //     fprintf(stderr, "\n");
                //     minim_shutdown(1);
                // }

                // // call the producer and process results
                // stashc = stash_call_args();
                // result = eval_expr(producer, env);
                // if (minim_valuesp(result)) {
                //     // need to unpack
                //     minim_thread *th = current_thread();
                //     for (long i = 0; i < values_buffer_count(th); ++i)
                //         push_call_arg(values_buffer_ref(th, i));
                // } else {
                //     push_call_arg(result);
                // }

                // consumer = force_single_value(consumer);
                // if (minim_primp(consumer)) {
                //     fprintf(stderr, "call-with-values: expected a procedure\n");
                //     fprintf(stderr, " received:");
                //     write_object(stderr, consumer);
                //     fprintf(stderr, "\n");
                //     minim_shutdown(1);
                // }

                // // TODO: this is not in tail position
                // result = eval_expr(consumer, env);
                // prepare_call_args(stashc);
                // clear_call_args();
                // return result;
            } else if (minim_prim(proc) == current_environment) {
                // special case: `current-environment`
                resume_frame();
                return env;
            } else if (minim_prim(proc) == error_proc) {
                // special case: `error`
                do_error(argc, args);
                fprintf(stderr, "unreachable\n");
                minim_shutdown(1);
            } else if (minim_prim(proc) == syntax_error_proc) {
                // special case: `syntax-error`
                do_syntax_error(argc, args);
                fprintf(stderr, "unreachable\n");
                minim_shutdown(1);
            } else {
                switch (argc) {
                case 0:
                    result = ((mobj (*)()) minim_prim(proc))();
                    break;
                case 1:
                    result = ((mobj (*)()) minim_prim(proc))(args[0]);
                    break;
                case 2:
                    result = ((mobj (*)()) minim_prim(proc))(args[0], args[1]);
                    break;
                case 3:
                    result = ((mobj (*)()) minim_prim(proc))(args[0], args[1], args[2]);
                    break;
                case 4:
                    result = ((mobj (*)()) minim_prim(proc))(args[0], args[1], args[2], args[3]);
                    break;
                case 5:
                    result = ((mobj (*)()) minim_prim(proc))(args[0], args[1], args[2], args[3], args[4]);
                    break;
                case 6:
                    result = ((mobj (*)()) minim_prim(proc))(args[0], args[1], args[2], args[3], args[4], args[5]);
                    break;
                default:
                    fprintf(stderr, "error: called unsafe primitive with too many arguments\n");
                    fprintf(stderr, " received: ");
                    write_object(stderr, proc);
                    fprintf(stderr, "\n at: ");
                    write_object(stderr, expr);
                    minim_shutdown(1);
                    break;
                }
                
                resume_frame();
                return result;
            }
        } else if (minim_closurep(proc)) {
            mobj *vars, *rest;
            int i, j;

            // check arity and extend environment
            check_closure_arity(proc, argc);
            env = Menv2(minim_closure_env(proc), closure_env_size(proc));
            args = eval_buffer(current_thread());

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
            resume_frame();
            expr = minim_closure_body(proc);
            goto loop;
        } else {
            fprintf(stderr, "error: not a procedure\n");
            fprintf(stderr, " received: ");
            write_object(stderr, proc);
            fprintf(stderr, "\n at: ");
            write_object(stderr, expr);
            fprintf(stderr, "\n");
            minim_shutdown(1);
        }
    } else if (minim_symbolp(expr)) {
        // variable
        return env_lookup_var(env, expr);
    } else if (minim_boolp(expr)
        || minim_fixnump(expr)
        || minim_charp(expr)
        || minim_stringp(expr)
        || minim_boxp(expr)
        || minim_vectorp(expr)) {
        // self-evaluating
        return expr;
    } else if (minim_nullp(expr)) {
        // empty application
        fprintf(stderr, "missing procedure expression\n");
        fprintf(stderr, "  in: ");
        write_object(stderr, expr);
        minim_shutdown(1);
    } else {
        // invalid
        fprintf(stderr, "bad syntax\n");
        write_object(stderr, expr);
        fprintf(stderr, "\n");
        minim_shutdown(1);
    }

    fprintf(stderr, "unreachable\n");
    minim_shutdown(1);
}
