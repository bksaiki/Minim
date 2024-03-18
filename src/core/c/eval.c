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

static int stack_overflowp(size_t argc) {
    minim_thread *th;
    mobj sseg;
    
    th = current_thread();
    sseg = current_segment(th);
    return PTR_ADD(current_sfp(th), (current_ac(th) + argc) * ptr_size) >= stack_seg_end(sseg);
}

static void check_stack_space(size_t argc) {
    minim_thread *th = current_thread();
    if (stack_overflowp(argc)) {
        fprintf(stderr, "stack overflow: unimplemented\n");
        fprintf(
            stderr,
            " sfp=%p, ac=%ld, esp=%p\n",
            current_sfp(th),
            current_ac(th),
            stack_seg_end(current_segment(th))
        );
        minim_shutdown(1);
    }
}

static void push_arg(mobj x) {
    minim_thread *th;
    mobj *sfp;
    size_t ac;
    
    th = current_thread();
    sfp = current_sfp(th);
    ac = current_ac(th);

    sfp[ac] = x;
    current_ac(th) += 1;
}

static void push_frame(mobj env) {
    minim_thread *th;
    mobj cc;
    size_t ac;

    // get current continuation and argument count
    th = current_thread();
    cc = current_continuation(th);
    ac = current_ac(th);

    // update current continuation, frame pointer, and argument count
    cc = Mcontinuation(cc, env, th);
    current_continuation(th) = cc;
    current_sfp(th) = PTR_ADD(current_sfp(th), ac * ptr_size);
    current_cp(th) = NULL;
    current_ac(th) = 0;

    // fprintf(stderr, "saving continuation (%p)\n", cc);
    // if (cc) {
    //     fprintf(stderr, " prev=%p\n", continuation_prev(cc));  
    //     fprintf(stderr, " sfp=%p\n", continuation_sfp(cc));
    //     fprintf(stderr, " cp=%p\n", continuation_cp(cc));
    //     fprintf(stderr, " ac=%ld\n", continuation_ac(cc));
    // }
}

static void pop_frame() {
    minim_thread *th;
    mobj cc, seg, *sfp;

    // get current continuation
    th = current_thread();
    cc = current_continuation(th);
    seg = current_segment(th);
    sfp = current_sfp(th);

    // possibly need to handle underflow
    if (!(((void *) sfp) >= stack_seg_base(seg) && ((void *) sfp) < stack_seg_end(seg))) {
        fprintf(stderr, "stack underflow: unimplemented\n");
        minim_shutdown(1);
    }

    // pop continuation and update thread-local data
    current_continuation(th) = continuation_prev(cc);
    current_sfp(th) = continuation_sfp(cc);
    current_cp(th) = continuation_cp(cc);
    current_ac(th) = continuation_ac(cc);

    // fprintf(stderr, "popped continuation (%p)\n", cc);
    // if (cc) {
    //     fprintf(stderr, " prev=%p\n", continuation_prev(cc));  
    //     fprintf(stderr, " sfp=%p\n", continuation_sfp(cc));
    //     fprintf(stderr, " cp=%p\n", continuation_cp(cc));
    //     fprintf(stderr, " ac=%ld\n", continuation_ac(cc));
    // }
}

static void do_values() {
    minim_thread *th;
    mobj *sfp;
    size_t ac;

    th = current_thread();
    sfp = current_sfp(th);
    ac = current_ac(th);

    // check if the values buffer is large enough
    if (ac >= values_buffer_size(th)) {
        values_buffer_size(th) = 2 * ac;
        values_buffer(th) = GC_realloc(values_buffer(th), values_buffer_size(th) * sizeof(mobj));
    }

    // copy arguments from the stack to the values buffer
    memcpy(values_buffer(th), sfp, ac * sizeof(mobj));
    values_buffer_count(th) = ac;
}

static void values_to_args(mobj x) {
    if (minim_valuesp(x)) {
        // multi-valued
        minim_thread *th = current_thread();
        for (size_t i = 0; i < values_buffer_count(th); i++) {
            push_arg(values_buffer_ref(th, i));
        }
    } else {
        // single-valued
        push_arg(x);
    }
}

static void do_apply() {
    minim_thread *th;
    mobj *sfp, rest;
    size_t i, ac;

    th = current_thread();
    sfp = current_sfp(th);
    ac = current_ac(th);

    // first, shift arguments by 1 (since `apply` itself is consumed)
    rest = sfp[ac - 1];
    for (i = 0; i < ac - 2; i++)
        sfp[i] = sfp[i + 1];

    // check if we have room for the application
    current_ac(th) -= 2;
    if (stack_overflowp(list_length(rest))) {
        fprintf(stderr, "do_apply(): stack overflow unimplemented\n");
        minim_shutdown(1);
    }

    // push rest argument to stack
    for (; !minim_nullp(rest); rest = minim_cdr(rest))
        push_arg(minim_car(rest));
}

static void check_call_with_values(mobj *args) {
    mobj producer, consumer;
    
    producer = args[0];
    if (!minim_procp(producer))
        bad_type_exn("call-with-values", "procedure?", producer);

    consumer = args[1];
    if (!minim_procp(consumer))
        bad_type_exn("call-with-values", "procedure?", consumer);
}

// Forces `x` to be a single value, that is,
// if `x` is the result of `(values ...)` it unwraps
// the result if `x` contains one value and panics otherwise
static mobj force_single_value(mobj x) {
    if (minim_valuesp(x)) {
        minim_thread *th = current_thread();
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

static void bind_values(const char *name, mobj env, mobj ids, mobj vals) {
    minim_thread *th;
    long bindc, idx;

    bindc = list_length(ids);
    if (minim_valuesp(vals)) {
        // multi-valued
        th = current_thread();
        if (bindc != values_buffer_count(th)) {
            result_arity_exn(name, bindc, values_buffer_count(th));
        }

        idx = 0;
        for (; !minim_nullp(ids); ids = minim_cdr(ids)) {
            SET_NAME_IF_CLOSURE(minim_car(ids), values_buffer_ref(th, idx));
            env_define_var(env, minim_car(ids), values_buffer_ref(th, idx));
            idx += 1;
        }

        values_buffer_count(th) = 0;
    } else {
        // single-valued
        if (bindc != 1) {
            result_arity_exn(name, bindc, 1);
        }

        SET_NAME_IF_CLOSURE(minim_car(ids), vals);
        env_define_var(env, minim_car(ids), vals);
    }
}

mobj eval_expr(mobj expr, mobj env) {
    minim_thread *th;

    // HACK: call-with-values will invoke producer with `eval_expr`
    th = current_thread();
    if (minim_procp(expr)) {
        push_frame(env);
        current_cp(current_thread()) = expr;
        goto application;
    }

loop:

    if (minim_consp(expr)) {
        // special forms
        mobj head = minim_car(expr);
        if (minim_symbolp(head)) {
            if (head == define_values_symbol) {
                // define-values form
                mobj result = eval_expr(minim_car(minim_cddr(expr)), env);
                bind_values("define-values", env, minim_cadr(expr), result);
                return minim_void;
            } else if (head == let_values_symbol) {
                // let-values form
                mobj env2 = Menv2(env, let_values_env_size(expr));
                mobj bindings = minim_cadr(expr);
                while (!minim_nullp(bindings)) {
                    mobj bind = minim_car(bindings);
                    mobj result = eval_expr(minim_cadr(bind), env);
                    bind_values("let-values", env2, minim_car(bind), result);
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
                    bind_values("letrec-values", env2, minim_car(bind), result);
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

        // fprintf(stderr, "eval: ");
        // write_object(stderr, expr);
        // fprintf(stderr, "\n");

        // save the current continuation and check if we need a new stack segment
        push_frame(env);
        check_stack_space(list_length(minim_cdr(expr)));

        // evaluate the current application
        current_cp(th) = force_single_value(eval_expr(head, env));
        for (mobj it = minim_cdr(expr); !minim_nullp(it); it = minim_cdr(it)) {
            push_arg(force_single_value(eval_expr(minim_car(it), env)));
        }

application:

        if (minim_primp(current_cp(th))) {
            // primitives
            mobj *args, result;
            mobj (*prim)();

            prim = minim_prim(current_cp(th));
            args = current_sfp(th);
            check_prim_proc_arity(current_cp(th), current_ac(th));    
            if (prim == apply_proc) {
                mobj rest;

                // special case for `apply`
                current_cp(th) = force_single_value(args[0]);
                if (!minim_procp(current_cp(th)))
                    bad_type_exn("apply", "procedure?", env);

                // last argument must be a list
                rest = args[current_ac(th) - 1];
                if (!minim_listp(rest))
                    bad_type_exn("apply", "list?", rest);

                // push list argument to stack
                do_apply();
                goto application;
            } else if (prim == eval_proc) {
                // special case for `eval`
                expr = args[0];
                if (current_ac(th) == 2) {
                    env = args[1];
                    if (!minim_envp(env))
                        bad_type_exn("eval", "environment?", env);
                }
                
                pop_frame();
                check_expr(expr);
                goto loop;
            } else if (prim == values_proc) {
                // special case: `values`
                do_values();
                pop_frame();
                return minim_values;
            } else if (prim == call_with_values_proc) {
                // special case: `call-with-values`
                mobj producer, consumer, result;

                // check and stash procedures
                check_call_with_values(args);
                producer = args[0];
                consumer = args[1];

                // clear current frame and call consumer
                pop_frame();
                result = eval_expr(producer, env);
                
                // create a new frame, push values to arguments, and call producer
                push_frame(env);
                check_stack_space(values_buffer_count(th));
                values_to_args(result);
                current_cp(th) = consumer;
                goto application;
            } else if (prim == current_environment) {
                // special case: `current-environment`
                pop_frame();
                return env;
            } else if (prim == error_proc) {
                // special case: `error`
                do_error(current_ac(th), args);
                fprintf(stderr, "unreachable\n");
                minim_shutdown(1);
            } else if (prim == syntax_error_proc) {
                // special case: `syntax-error`
                do_syntax_error(current_ac(th), args);
                fprintf(stderr, "unreachable\n");
                minim_shutdown(1);
            } else {
                switch (current_ac(th)) {
                case 0:
                    result = prim();
                    break;
                case 1:
                    result = prim(args[0]);
                    break;
                case 2:
                    result = prim(args[0], args[1]);
                    break;
                case 3:
                    result = prim(args[0], args[1], args[2]);
                    break;
                case 4:
                    result = prim(args[0], args[1], args[2], args[3]);
                    break;
                case 5:
                    result = prim(args[0], args[1], args[2], args[3], args[4]);
                    break;
                case 6:
                    result = prim(args[0], args[1], args[2], args[3], args[4], args[5]);
                    break;
                default:
                    fprintf(stderr, "error: called unsafe primitive with too many arguments\n");
                    fprintf(stderr, " received: ");
                    write_object(stderr, current_cp(th));
                    fprintf(stderr, "\n at: ");
                    write_object(stderr, expr);
                    minim_shutdown(1);
                    break;
                }
                
                pop_frame();
                return result;
            }
        } else if (minim_closurep(current_cp(th))) {
            mobj closure, *args, *vars, rest;
            long i, j;

            closure = current_cp(th);
            args = current_sfp(th);

            // check arity and extend environment
            check_closure_arity(closure, current_ac(th));
            env = Menv2(minim_closure_env(closure), closure_env_size(closure));

            // process args
            i = 0;
            for (vars = minim_closure_args(closure); minim_consp(vars); vars = minim_cdr(vars)) {
                env_define_var_no_check(env, minim_car(vars), args[i]);
                ++i;
            }

            // optionally process rest argument
            if (minim_symbolp(vars)) {
                rest = minim_null;
                for (j = current_ac(th) - 1; j >= i; --j)
                    rest = Mcons(args[j], rest);
                env_define_var_no_check(env, vars, rest);
            }

            // tail call
            expr = minim_closure_body(closure);
            pop_frame();
            goto loop;
        } else {
            fprintf(stderr, "error: not a procedure\n");
            fprintf(stderr, " received: ");
            write_object(stderr, current_cp(th));
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
