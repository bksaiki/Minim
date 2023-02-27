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

void push_call_arg(minim_object *arg) {
    if (irt_call_args_count >= irt_call_args_size)
        resize_call_args(irt_call_args_count + 1);
    irt_call_args[irt_call_args_count++] = arg;
    irt_call_args[irt_call_args_count] = NULL;
}

void push_saved_arg(minim_object *arg) {
    if (irt_saved_args_count >= irt_saved_args_size)
        resize_saved_args(irt_saved_args_count + 2);
    irt_saved_args[irt_saved_args_count++] = arg;
}

void clear_call_args() {
    irt_call_args_count = 0;
}

void prepare_call_args(long count) {
    if (count > irt_saved_args_count) {
        fprintf(stderr, "prepare_call_args(): trying to restore %ld values, only found %ld\n",
                        count, irt_saved_args_count);
        minim_shutdown(1);
    }

    if (count < irt_call_args_size)
        resize_call_args(count + 1);
    
    memcpy(irt_call_args, &irt_saved_args[irt_saved_args_count - count], count * sizeof(minim_object*));
    irt_call_args[count] = NULL;
    irt_call_args_count = count;
    irt_saved_args_count -= count;
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

#define check_closure_proc_arity(prim, argc)    \
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
            minim_shutdown(1);
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

static long apply_args() {
    minim_object *lst;

    // check if last argument is a list
    // safe since call_args >= 2
    lst = irt_call_args[irt_call_args_count - 1];
    if (!is_list(lst))
        bad_type_exn("apply", "list?", lst);

    // for every arg except the last arg: shift down by 1
    // remove the last argument from the stack
    memcpy(irt_call_args, &irt_call_args[1], (irt_call_args_count - 2) * sizeof(minim_object *));
    irt_call_args_count -= 2;

    // for the last argument: push every element of the list
    // onto the call stack
    while (!minim_is_null(lst)) {
        push_call_arg(minim_car(lst));
        lst = minim_cdr(lst);
    }

    // just for safety
    irt_call_args[irt_call_args_count] = NULL;
    return irt_call_args_count;
}

static long eval_exprs(minim_object *exprs, minim_object *env) {
    minim_object *it, *result;
    minim_thread *th;
    long argc;

    argc = 0;
    assert_no_call_args();
    for (it = exprs; !minim_is_null(it); it = minim_cdr(it)) {
        result = eval_expr(minim_car(it), env);
        if (minim_is_values(result)) {
            th = current_thread();
            if (values_buffer_count(th) != 1)
                result_arity_exn(1, values_buffer_count(th));
            result = values_buffer_ref(th, 0);
        }

        push_saved_arg(result);
        ++argc;
    }

    prepare_call_args(argc);
    return argc;
}

minim_object *call_with_values(minim_object *producer,
                               minim_object *consumer,
                               minim_object *env) {
    minim_object *produced;
    minim_thread *th;
    long i;

    assert_no_call_args();
    produced = call_with_args(producer, env);
    if (minim_is_values(produced)) {
        // need to unpack
        th = current_thread();
        for (i = 0; i < values_buffer_count(th); ++i)
            push_call_arg(values_buffer_ref(th, i));
    } else {
        push_call_arg(produced);
    }

    return call_with_args(consumer, env);
}

minim_object *call_with_args(minim_object *proc, minim_object *env) {
    minim_object *expr, **args;
    int argc;

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

        // now it's safe to clear
        clear_call_args();

        // special case for `eval`
        if (minim_prim_proc(proc) == eval_proc) {
            expr = args[0];
            if (argc == 2) {
                env = args[1];
                if (!minim_is_env(env))
                    bad_type_exn("eval", "environment?", env);
            }
            
            return eval_expr(expr, env);
        }

        // special case for `current-environment`
        if (minim_prim_proc(proc) == current_environment_proc) {
            return env;
        }

        // special case for `for-each`
        if (minim_prim_proc(proc) == for_each_proc) {
            return for_each(args[0], argc - 1, &args[1], env);
        }

        // special case for `map`
        if (minim_prim_proc(proc) == map_proc) {
            return map_list(args[0], argc - 1, &args[1], env);
        }

        // special case for `andmap`
        if (minim_prim_proc(proc) == andmap_proc) {
            return andmap(args[0], argc - 1, &args[1], env);
        }

        // special case for `ormap`
        if (minim_prim_proc(proc) == ormap_proc) {
            return ormap(args[0], argc - 1, &args[1], env);
        }

        // special case for `call-with-values`
        if (minim_prim_proc(proc) == call_with_values_proc) {
            return call_with_values(args[0], args[1], env);
        }

        return minim_prim_proc(proc)(argc, args);
    } else if (minim_is_closure_proc(proc)) {
        minim_object *vars, *rest;
        int i, j;

        // check arity and extend environment
        check_closure_proc_arity(proc, argc);
        env = extend_env(minim_null, minim_null, minim_closure_env(proc));
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
        expr = minim_closure_body(proc);
        clear_call_args();
        return eval_expr(expr, env);
    } else {
        fprintf(stderr, "error: not a procedure\n");
        fprintf(stderr, " received:");
        write_object(stderr, proc);
        fprintf(stderr, "\n");
        minim_shutdown(1);
    }
}

minim_object *eval_expr(minim_object *expr, minim_object *env) {

loop:

    if (minim_is_true(expr) ||
        minim_is_false(expr) ||
        minim_is_fixnum(expr) ||
        minim_is_char(expr) ||
        minim_is_string(expr) ||
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
        minim_object *proc, *head, *result, *it, *bindings, *bind, *env2, **args;
        minim_thread *th;
        long var_count, idx;
        int argc;

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
                    if (var_count != values_buffer_count(th))
                        result_arity_exn(var_count, values_buffer_count(th));

                    idx = 0;
                    for (it = minim_cadr(expr); !minim_is_null(it); it = minim_cdr(it), ++idx) {
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
                check_let_values(expr);
                env2 = extend_env(minim_null, minim_null, env);
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
                        if (var_count != values_buffer_count(th))
                            result_arity_exn(var_count, values_buffer_count(th));

                        idx = 0;
                        for (it = minim_car(bind); !minim_is_null(it); it = minim_cdr(it), ++idx)
                            to_bind = make_pair(make_pair(minim_car(it), values_buffer_ref(th, idx)), to_bind);
                    } else {
                        // single-valued
                        if (var_count != 1)
                            result_arity_exn(var_count, 1);
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
                                minim_shutdown(1);
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

            // now it's safe to clear
            clear_call_args();

            // special case for `eval`
            if (minim_prim_proc(proc) == eval_proc) {
                expr = args[0];
                if (argc == 2) {
                    env = args[1];
                    if (!minim_is_env(env))
                        bad_type_exn("eval", "environment?", env);
                }
                goto loop;
            }

            // special case for `current-environment`
            if (minim_prim_proc(proc) == current_environment_proc) {
                return env;
            }

            // special case for `for-each`
            if (minim_prim_proc(proc) == for_each_proc) {
                return for_each(args[0], argc - 1, &args[1], env);
            }

            // special case for `map`
            if (minim_prim_proc(proc) == map_proc) {
                return map_list(args[0], argc - 1, &args[1], env);
            }

            // special case for `andmap`
            if (minim_prim_proc(proc) == andmap_proc) {
                return andmap(args[0], argc - 1, &args[1], env);
            }

            // special case for `ormap`
            if (minim_prim_proc(proc) == ormap_proc) {
                return ormap(args[0], argc - 1, &args[1], env);
            }

            // special case for `call-with-values`
            if (minim_prim_proc(proc) == call_with_values_proc) {
                return call_with_values(args[0], args[1], env);
            }

            return minim_prim_proc(proc)(argc, args);
        } else if (minim_is_closure_proc(proc)) {
            minim_object *vars, *rest;
            int i, j;

            // check arity and extend environment
            check_closure_proc_arity(proc, argc);
            env = extend_env(minim_null, minim_null, minim_closure_env(proc));
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

            // tail call
            expr = minim_closure_body(proc);
            clear_call_args();
            goto loop;
        } else {
            fprintf(stderr, "error: not a procedure\n");
            fprintf(stderr, " received:");
            write_object(stderr, proc);
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
