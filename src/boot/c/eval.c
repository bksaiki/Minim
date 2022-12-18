/*
    Interpreter
*/

#include "../minim.h"

#define SET_NAME_IF_CLOSURE(name, val) {                    \
    if (minim_is_closure_proc(val)) {                       \
        if (minim_closure_name(val) == NULL)                \
            minim_closure_name(val) = minim_symbol(name);   \
    }                                                       \
}

//
//  Runtime check
//

void bad_syntax_exn(minim_object *expr) {
    fprintf(stderr, "%s: bad syntax\n", minim_symbol(minim_car(expr)));
    fprintf(stderr, " at: ");
    write_object2(stderr, expr, 1, 0);
    fputc('\n', stderr);
    exit(1);
}

void bad_type_exn(const char *name, const char *type, minim_object *x) {
    fprintf(stderr, "%s: type violation\n", name);
    fprintf(stderr, " expected: %s\n", type);
    fprintf(stderr, " received: ");
    write_object(stderr, x);
    fputc('\n', stderr);
    exit(1);
}

static void arity_mismatch_exn(const char *name, proc_arity *arity, short actual) {
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

    exit(1);
}

int check_proc_arity(proc_arity *arity, minim_object *args, const char *name) {
    int min_arity, max_arity, argc;

    min_arity = proc_arity_min(arity);
    max_arity = proc_arity_max(arity);
    argc = 0;

    while (!minim_is_null(args)) {
        if (argc >= max_arity)
            arity_mismatch_exn(name, arity, max_arity + list_length(args));
        args = minim_cdr(args);
        ++argc;
    }

    if (argc < min_arity)
        arity_mismatch_exn(name, arity, argc);

    return argc;
}

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
// Check: `expr` must be `(<name> <datum> <datum>)
static void check_2ary_syntax(minim_object *expr) {
    minim_object *rest;
    
    rest = minim_cdr(expr);
    if (!minim_is_pair(rest))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
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
// Check: `expr` must be either
//  (ii) `(<name> (<symbol> ...) <datum> ...)
//  (i)  `(<name> <symbol> <datum>)`
static void check_define(minim_object *expr) {
    minim_object *rest, *id;
    
    rest = minim_cdr(expr);
    if (!minim_is_pair(rest))
        bad_syntax_exn(expr);

    id = minim_car(rest);
    rest = minim_cdr(rest);
    if (!minim_is_pair(rest))
        bad_syntax_exn(expr);

    if (minim_is_symbol(id) && !minim_is_null(minim_cdr(rest)))
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

// Already assumes `expr` is `(cond . <???>)`
// Check: `expr` must be `(cond [<test> <clause> ...] ...)`
// Does not check if each `<clause>` is an expression.
// Does not check if each `<clause> ...` forms a list.
// Checks that only `else` appears in the tail position
static void check_cond(minim_object *expr) {
    minim_object *rest, *datum;

    rest = minim_cdr(expr);
    while (minim_is_pair(rest)) {
        datum = minim_car(rest);
        if (!minim_is_pair(datum) || !minim_is_pair(minim_cdr(datum)))
            bad_syntax_exn(expr);

        datum = minim_car(datum);
        if (minim_is_symbol(datum) &&
            minim_car(datum) == else_symbol &&
            !minim_is_null(minim_cdr(rest))) {
            fprintf(stderr, "cond: else clause must be last");
            fprintf(stderr, " at: ");
            write_object2(stderr, expr, 1, 0);
            fputc('\n', stderr);
            exit(1);
        }

        rest = minim_cdr(rest);
    }

    if (!minim_is_null(rest))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(let . <???>)`
// Check: `expr` must be `(let ([<var> <val>] ...) <body> ...)`
// Does not check if each `<body>` is an expression.
// Does not check if `<body> ...` forms a list.
static void check_let(minim_object *expr) {
    minim_object *bindings, *bind;
    
    bindings = minim_cdr(expr);
    if (!minim_is_pair(bindings) || !minim_is_pair(minim_cdr(bindings)))
        bad_syntax_exn(expr);
    
    bindings = minim_car(bindings);
    while (minim_is_pair(bindings)) {
        bind = minim_car(bindings);
        if (!minim_is_pair(bind) || !minim_is_symbol(minim_car(bind)))
            bad_syntax_exn(expr);

        bind = minim_cdr(bind);
        if (!minim_is_pair(bind) || !minim_is_null(minim_cdr(bind)))
            bad_syntax_exn(expr);

        bindings = minim_cdr(bindings);
    }

    if (!minim_is_null(bindings))
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

// Already assumes `expr` is `(let . <???>)`
// Check: `expr` must be `(let <name> ([<var> <val>] ...) <body> ...)`
// Just check that <name> is a symbol and then call
// `check_loop` starting at (<name> ...)
static void check_let_loop(minim_object *expr) {
    if (!minim_is_pair(minim_cdr(expr)) && !minim_is_symbol(minim_cadr(expr)))
        bad_syntax_exn(expr);
    check_let(minim_cdr(expr));
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
            exit(1);
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
        exit(1);
    }
}

//
//  Evaluation
//

static minim_object *let_vars(minim_object *bindings) {
    return (minim_is_null(bindings) ?
            minim_null :
            make_pair(minim_caar(bindings),
                      let_vars(minim_cdr(bindings))));
}

static minim_object *let_vals(minim_object *bindings) {
    return (minim_is_null(bindings) ?
            minim_null :
            make_pair(minim_car(minim_cdar(bindings)),
                      let_vals(minim_cdr(bindings))));
}

// (define (<name> . <formals>) . <body>)
// => (define <name> (lambda <formals> . <body>))
// (define <name> <body>)
// => (define-values (<name>) <body>)
static minim_object *expand_define(minim_object *define) {
    minim_object *id, *rest;

    id = minim_cadr(define);
    rest = minim_cddr(define);
    if (minim_is_symbol(id)) {
        // value
        return make_pair(define_values_symbol,
               make_pair(make_pair(id, minim_null),
               rest));
    } else {
        minim_object *name, *formals, *body;

        // procedure
        name = minim_car(minim_cadr(define));
        formals = minim_cdr(minim_cadr(define));
        body = minim_cddr(define);

        return expand_define(
                make_pair(define_symbol,
                    make_pair(name,
                    make_pair(
                            make_pair(lambda_symbol,
                            make_pair(formals,
                            body)),
                    minim_null))));
    }
}

// (let ((<var> <val>) ...) . <body>)
// => (let-values (((<var>) <val>) ...) . <body>)
static minim_object *expand_let(minim_object *form, minim_object *let) {
    minim_object *vars, *vals, *bindings, *bind, *it;

    vars = let_vars(minim_cadr(let));
    vals = let_vals(minim_cadr(let));
    if (minim_is_null(vars)) {
        bindings = minim_null;
    } else {
        bind = make_pair(make_pair(minim_car(vars), minim_null), make_pair(minim_car(vals), minim_null));
        bindings = make_pair(bind, minim_null);
        vars = minim_cdr(vars);
        vals = minim_cdr(vals);
        for (it = bindings; !minim_is_null(vars); vars = minim_cdr(vars), vals = minim_cdr(vals)) {
            bind = make_pair(make_pair(minim_car(vars), minim_null), make_pair(minim_car(vals), minim_null));
            minim_cdr(it) = make_pair(bind, minim_null);
            it = minim_cdr(it);
        }
    }

    return make_pair(form, make_pair(bindings, minim_cddr(let)));
}

// (let <name> ((<var> <val>) ...) . <body>)
// => (letrec-values (((<name>) (lambda (<var> ...) . <body>))) (<name> <val> ...))
static minim_object *expand_let_loop(minim_object *let) {
    minim_object *name, *formals, *vals, *body;

    name = minim_cadr(let);
    formals = let_vars(minim_car(minim_cddr(let)));
    vals = let_vals(minim_car(minim_cddr(let)));
    body = minim_cdr(minim_cddr(let));

    return make_pair(letrec_values_symbol,
           make_pair(                                   // expr rib                      
                make_pair(                              // bindings
                    make_pair(                          // binding
                        make_pair(name, minim_null),    // name 
                    make_pair(                          // value
                        make_pair(lambda_symbol,
                        make_pair(formals,
                        body)),
                    minim_null)),
                minim_null),
           make_pair(make_pair(name, vals),             // call
           minim_null)));
}

static minim_object *apply_args(minim_object *args) {
    minim_object *head, *tail, *it;

    if (minim_is_null(minim_cdr(args))) {
        if (!is_list(minim_car(args)))
            bad_type_exn("apply", "list?", minim_car(args));

        return minim_car(args);
    }

    head = make_pair(minim_car(args), minim_null);
    tail = head;
    it = args;

    while (!minim_is_null(minim_cdr(it = minim_cdr(it)))) {
        minim_cdr(tail) = make_pair(minim_car(it), minim_null);
        tail = minim_cdr(tail);
    }

    if (!is_list(minim_car(it)))
        bad_type_exn("apply", "list?", minim_car(it));

    minim_cdr(tail) = minim_car(it);
    return head;
}

static minim_object *eval_exprs(minim_object *exprs, minim_object *env) {
    minim_object *head, *tail, *it, *result;
    minim_thread *th;

    if (minim_is_null(exprs))
        return minim_null;

    head = NULL; tail = NULL;
    for (it = exprs; !minim_is_null(it); it = minim_cdr(it)) {
        result = eval_expr(minim_car(it), env);
        if (minim_is_values(result)) {
            th = current_thread();
            if (values_buffer_count(th) != 1) {
                fprintf(stderr, "result arity mismatch\n");
                fprintf(stderr, "  expected: 1, received: %d\n", values_buffer_count(th));
                exit(1);
            } else {
                result = values_buffer_ref(th, 0);
            }
        }

        if (head == NULL) {
            head = make_pair(result, minim_null);
            tail = head;
        } else {
            minim_cdr(tail) = make_pair(result, minim_null);   
            tail = minim_cdr(tail);
        }
    }

    return head;
}

minim_object *call_with_values(minim_object *producer,
                               minim_object *consumer,
                               minim_object *env) {
    minim_object *produced, *args, *tail;
    minim_thread *th;

    produced = call_with_args(producer, minim_null, env);
    if (minim_is_values(produced)) {
        // need to unpack
        th = current_thread();
        if (values_buffer_count(th) == 0) {
            args = minim_null;
        } else if (values_buffer_count(th) == 1) {
            args = make_pair(values_buffer_ref(th, 0), minim_null);
        } else if (values_buffer_count(th) == 2) {
            args = make_pair(values_buffer_ref(th, 0),
                   make_pair(values_buffer_ref(th, 1), minim_null));
        } else {
            // slow path
            args = make_pair(values_buffer_ref(th, 0), minim_null);
            tail = args;
            for (int i = 1; i < values_buffer_count(th); ++i) {
                minim_cdr(tail) = make_pair(values_buffer_ref(th, i), minim_null);
                tail = minim_cdr(tail);
            }
        }
    } else {
        args = make_pair(produced, minim_null);
    }

    return call_with_args(consumer, args, env);
}

minim_object *call_with_args(minim_object *proc, minim_object *args, minim_object *env) {
    minim_object *expr;

application:

    if (minim_is_prim_proc(proc)) {
        check_prim_proc_arity(proc, args);

        // special case for `eval`
        if (minim_prim_proc(proc) == eval_proc) {
            expr = minim_car(args);
            if (!minim_is_null(minim_cdr(args)))
                env = minim_cadr(args);
            return eval_expr(expr, env);
        }

        // special case for `apply`
        if (minim_prim_proc(proc) == apply_proc) {
            proc = minim_car(args);
            args = apply_args(minim_cdr(args));
            goto application;
        }

        // special case for `current-environment`
        if (minim_prim_proc(proc) == current_environment_proc) {
            return env;
        }

        // special case for `andmap`
        if (minim_prim_proc(proc) == andmap_proc) {
            return andmap(minim_car(args), minim_cadr(args), env);
        }

        // special case for `ormap`
        if (minim_prim_proc(proc) == ormap_proc) {
            return ormap(minim_car(args), minim_cadr(args), env);
        }

        return minim_prim_proc(proc)(args);
    } else if (minim_is_closure_proc(proc)) {
        minim_object *vars;

        // check arity and extend environment
        check_closure_proc_arity(proc, args);
        env = extend_env(minim_null, minim_null, minim_closure_env(proc));

        // process args
        vars = minim_closure_args(proc);
        while (minim_is_pair(vars)) {
            env_define_var_no_check(env, minim_car(vars), minim_car(args));
            vars = minim_cdr(vars);
            args = minim_cdr(args);
        }

        // optionally process rest argument
        if (minim_is_symbol(vars))
            env_define_var_no_check(env, vars, copy_list(args));

        // tail call (not really)
        expr = minim_closure_body(proc);
        return eval_expr(expr, env);
    } else {
        fprintf(stderr, "not a procedure\n");
        exit(1);
    }
}

minim_object *eval_expr(minim_object *expr, minim_object *env) {

loop:

    if (minim_is_true(expr) || minim_is_false(expr) ||
        minim_is_fixnum(expr) ||
        minim_is_char(expr) ||
        minim_is_string(expr)) {
        // self-evaluating
        return expr;
    } else if (minim_is_symbol(expr)) {
        // variable
        return env_lookup_var(env, expr);
    } else if (minim_is_null(expr)) {
        fprintf(stderr, "missing procedure expression\n");
        fprintf(stderr, "  in: ");
        write_object2(stderr, expr, 0, 0);
        exit(1);
    } else if (minim_is_pair(expr)) {
        minim_object *proc, *head, *args, *result, *it, *bindings, *bind, *env2;
        minim_thread *th;
        long var_count, idx;

        // special forms
        head = minim_car(expr);
        if (minim_is_symbol(head)) {
            if (head == define_values_symbol) {
                // define-values form
                check_define_values(expr);
                var_count = list_length(minim_cadr(expr));
                result = eval_expr(minim_car(minim_cddr(expr)), env);
                if (minim_is_values(result)) {;
                    // multi-valued
                    th = current_thread();
                    if (var_count != values_buffer_count(th)) {
                        fprintf(stderr, "result arity mismatch\n");
                        fprintf(stderr, "  expected: %ld, received: %d\n",
                                        var_count, values_buffer_count(th));
                        exit(1);
                    }

                    idx = 0;
                    for (it = minim_cadr(expr); !minim_is_null(it); it = minim_cdr(it), ++idx) {
                        SET_NAME_IF_CLOSURE(minim_car(it), values_buffer_ref(th, idx));
                        env_define_var(env, minim_car(it), values_buffer_ref(th, idx));
                    }
                } else {
                    // single-valued
                    if (var_count != 1) {
                        fprintf(stderr, "result arity mismatch\n");
                        fprintf(stderr, "  expected: %ld, received: 1\n", var_count);
                        exit(1);
                    }
                    
                    SET_NAME_IF_CLOSURE(minim_car(minim_cadr(expr)), result);
                    env_define_var(env, minim_car(minim_cadr(expr)), result);
                }

                GC_REGISTER_LOCAL_ARRAY(expr);
                return minim_void;
            } else if (head == let_values_symbol) {
                // let-values form
                check_let_values(expr);
                env2 = extend_env(minim_null, minim_null, env);
                for (bindings = minim_cadr(expr); !minim_is_null(bindings); bindings = minim_cdr(bindings)) {
                    bind = minim_car(bindings);
                    var_count = list_length(minim_car(bind));
                    result = eval_expr(minim_cadr(bind), env);
                    if (minim_is_values(result)) {
                        // multi-valued
                        th = current_thread();
                        if (var_count != values_buffer_count(th)) {
                            fprintf(stderr, "result arity mismatch\n");
                            fprintf(stderr, "  expected: %ld, received: %d\n",
                                            var_count, values_buffer_count(th));
                            exit(1);
                        }

                        idx = 0;
                        for (it = minim_car(bind); !minim_is_null(it); it = minim_cdr(it), ++idx) {
                            SET_NAME_IF_CLOSURE(minim_car(it), values_buffer_ref(th, idx));
                            env_define_var(env2, minim_car(it), values_buffer_ref(th, idx));
                        }
                    } else {
                        // single-valued
                        if (var_count != 1) {
                            fprintf(stderr, "result arity mismatch\n");
                            fprintf(stderr, "  expected: %ld, received: 1\n", var_count);
                            exit(1);
                        }
                        
                        SET_NAME_IF_CLOSURE(minim_caar(bind), result);
                        env_define_var(env2, minim_caar(bind), result);
                    }
                }
                
                expr = make_pair(begin_symbol, (minim_cddr(expr)));
                env = env2;
                goto loop;
            } else if (head == letrec_values_symbol) {
                // letrec-values
                minim_object *to_bind;

                check_let_values(expr);
                to_bind = minim_null;
                env = extend_env(minim_null, minim_null, env);
                for (bindings = minim_cadr(expr); !minim_is_null(bindings); bindings = minim_cdr(bindings)) {
                    bind = minim_car(bindings);
                    var_count = list_length(minim_car(bind));
                    result = eval_expr(minim_cadr(bind), env);
                    if (minim_is_values(result)) {
                        // multi-valued
                        th = current_thread();
                        if (var_count != values_buffer_count(th)) {
                            fprintf(stderr, "result arity mismatch\n");
                            fprintf(stderr, "  expected: %ld, received: %d\n",
                                            var_count, values_buffer_count(th));
                            exit(1);
                        }

                        idx = 0;
                        for (it = minim_car(bind); !minim_is_null(it); it = minim_cdr(it), ++idx)
                            to_bind = make_pair(make_pair(minim_car(it), values_buffer_ref(th, idx)), to_bind);
                    } else {
                        // single-valued
                        if (var_count != 1) {
                            fprintf(stderr, "result arity mismatch\n");
                            fprintf(stderr, "  expected: %ld, received: 1\n", var_count);
                            exit(1);
                        }
                        
                        to_bind = make_pair(make_pair(minim_caar(bind), result), to_bind);
                    }
                }

                for (it = to_bind; !minim_is_null(it); it = minim_cdr(it)) {
                    SET_NAME_IF_CLOSURE(minim_caar(it), minim_cdar(it));
                    env_define_var(env, minim_caar(it), minim_cdar(it));
                }

                expr = make_pair(begin_symbol, (minim_cddr(expr)));
                goto loop;
            } else if (head == quote_symbol) {
                // quote form
                check_1ary_syntax(expr);
                return minim_cadr(expr);
            } else if (head == syntax_symbol || head == quote_syntax_symbol) {
                // quote-syntax form
                check_1ary_syntax(expr);
                if (minim_is_syntax(minim_cadr(expr)))
                    return minim_cadr(expr);
                else
                    return make_syntax(minim_cadr(expr), minim_false);
            } else if (head == syntax_loc_symbol) {
                // syntax/loc form
                check_2ary_syntax(expr);
                return make_syntax(minim_car(minim_cddr(expr)), minim_cadr(expr));
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
                return make_closure_proc(minim_cadr(expr),
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
            } else if (minim_is_true(boot_expander(current_thread()))) {
                // expanded forms
                if (head == define_symbol) {
                    // define form
                    check_define(expr);
                    expr = expand_define(expr);
                    goto loop;
                } else if (head == let_symbol) {
                    if (minim_is_pair(minim_cdr(expr)) && minim_is_symbol(minim_cadr(expr))) {
                        // let loop form
                        check_let_loop(expr);
                        expr = expand_let_loop(expr);
                        goto loop;
                    } else {
                        // let form
                        check_let(expr);
                        expr = expand_let(let_values_symbol, expr);
                        goto loop;
                    }
                } else if (head == letrec_symbol) {
                    // letrec form
                    check_let(expr);
                    expr = expand_let(letrec_values_symbol, expr);
                    goto loop;
                } else if (head == cond_symbol) {
                    // cond form
                    check_cond(expr);
                    expr = minim_cdr(expr);
                    while (!minim_is_null(expr)) {
                        if (minim_caar(expr) == else_symbol) {
                            if (!minim_is_null(minim_cdr(expr))) {
                                fprintf(stderr, "else clause must be last");
                                exit(1);
                            }
                            expr = make_pair(begin_symbol, minim_cdar(expr));
                            goto loop;
                        } else if (!minim_is_false(eval_expr(minim_caar(expr), env))) {
                            expr = make_pair(begin_symbol, minim_cdar(expr));
                            goto loop;
                        }
                        expr = minim_cdr(expr);
                    }

                    return minim_void;
                } else if (head == and_symbol) {
                    // and form
                    check_begin(expr);
                    expr = minim_cdr(expr);
                    if (minim_is_null(expr))
                        return minim_true;

                    while (!minim_is_null(minim_cdr(expr))) {
                        result = eval_expr(minim_car(expr), env);
                        if (minim_is_false(result))
                            return result;
                        expr = minim_cdr(expr);
                    }

                    expr = minim_car(expr);
                    goto loop;
                } else if (head == or_symbol) {
                    // or form
                    check_begin(expr);
                    expr = minim_cdr(expr);
                    if (minim_is_null(expr))
                        return minim_false;

                    while (!minim_is_null(minim_cdr(expr))) {
                        result = eval_expr(minim_car(expr), env);
                        if (!minim_is_false(result))
                            return result;
                        expr = minim_cdr(expr);
                    }

                    expr = minim_car(expr);
                    goto loop;
                }
            } 
        }
        
        proc = eval_expr(head, env);
        args = eval_exprs(minim_cdr(expr), env);

application:

        if (minim_is_prim_proc(proc)) {
            check_prim_proc_arity(proc, args);

            // special case for `eval`
            if (minim_prim_proc(proc) == eval_proc) {
                expr = minim_car(args);
                if (!minim_is_null(minim_cdr(args)))
                    env = minim_cadr(args);
                goto loop;
            }

            // special case for `apply`
            if (minim_prim_proc(proc) == apply_proc) {
                proc = minim_car(args);
                args = apply_args(minim_cdr(args));
                goto application;
            }

            // special case for `current-environment`
            if (minim_prim_proc(proc) == current_environment_proc) {
                return env;
            }

            // special case for `andmap`
            if (minim_prim_proc(proc) == andmap_proc) {
                return andmap(minim_car(args), minim_cadr(args), env);
            }

            // special case for `ormap`
            if (minim_prim_proc(proc) == ormap_proc) {
                return ormap(minim_car(args), minim_cadr(args), env);
            }

            // special case for `call-with-values`
            if (minim_prim_proc(proc) == call_with_values_proc) {
                return call_with_values(minim_car(args), minim_cadr(args), env);
            }

            return minim_prim_proc(proc)(args);
        } else if (minim_is_closure_proc(proc)) {
            minim_object *vars;

            // check arity and extend environment
            check_closure_proc_arity(proc, args);
            env = extend_env(minim_null, minim_null, minim_closure_env(proc));

            // process args
            vars = minim_closure_args(proc);
            while (minim_is_pair(vars)) {
                env_define_var_no_check(env, minim_car(vars), minim_car(args));
                vars = minim_cdr(vars);
                args = minim_cdr(args);
            }

            // optionally process rest argument
            if (minim_is_symbol(vars))
                env_define_var_no_check(env, vars, copy_list(args));

            // tail call
            expr = minim_closure_body(proc);
            goto loop;
        } else {
            fprintf(stderr, "not a procedure\n");
            exit(1);
        }
    } else {
        fprintf(stderr, "bad syntax\n");
        exit(1);
    }

    fprintf(stderr, "unreachable\n");
    exit(1);
}
