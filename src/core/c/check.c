// check.c: syntax checker for interpreted expressions

#include "../minim.h"

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum>)
static void check_1ary_syntax(mobj expr) {
    mobj rest = minim_cdr(expr);
    if (!minim_consp(rest) || !minim_nullp(minim_cdr(rest)))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> <datum> <datum>)
static void check_3ary_syntax(mobj expr) {
    mobj rest;
    
    rest = minim_cdr(expr);
    if (!minim_consp(rest))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
    if (!minim_consp(rest))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
    if (!minim_consp(rest) || !minim_nullp(minim_cdr(rest)))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr must be `(<name> <symbol> <datum>)`
static void check_assign(mobj expr) {
    mobj rest;

    rest = minim_cdr(expr);
    if (!minim_consp(rest) || !minim_symbolp(minim_car(rest)))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
    if (!minim_consp(rest))
        bad_syntax_exn(expr);

    if (!minim_nullp(minim_cdr(rest)))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> (<symbol> ...) <datum>)
static void check_define_values(mobj expr) {
    mobj rest, ids;
    
    rest = minim_cdr(expr);
    if (!minim_consp(rest))
        bad_syntax_exn(expr);

    ids = minim_car(rest);
    while (minim_consp(ids)) {
        if (!minim_symbolp(minim_car(ids)))
            bad_syntax_exn(expr);
        ids = minim_cdr(ids);
    } 

    if (!minim_nullp(ids))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
    if (!minim_consp(rest) || !minim_nullp(minim_cdr(rest)))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(let . <???>)`
// Check: `expr` must be `(let-values ([(<var> ...) <val>] ...) <body> ...)`
// Does not check if each `<body>` is an expression.
// Does not check if `<body> ...` forms a list.
static void check_let_values(mobj expr) {
    mobj bindings, bind, ids;
    
    bindings = minim_cdr(expr);
    if (!minim_consp(bindings) || !minim_consp(minim_cdr(bindings)))
        bad_syntax_exn(expr);
    
    bindings = minim_car(bindings);
    while (minim_consp(bindings)) {
        bind = minim_car(bindings);
        if (!minim_consp(bind))
            bad_syntax_exn(expr);

        ids = minim_car(bind);
        while (minim_consp(ids)) {
            if (!minim_symbolp(minim_car(ids)))
                bad_syntax_exn(expr);
            ids = minim_cdr(ids);
        } 

        if (!minim_nullp(ids))
            bad_syntax_exn(expr);

        bind = minim_cdr(bind);
        if (!minim_consp(bind) || !minim_nullp(minim_cdr(bind)))
            bad_syntax_exn(expr);

        bindings = minim_cdr(bindings);
    }

    if (!minim_nullp(bindings))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> ...)`
static void check_begin(mobj expr) {
    mobj rest = minim_cdr(expr);

    while (minim_consp(rest))
        rest = minim_cdr(rest);

    if (!minim_nullp(rest))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> ...)`
static void check_lambda(mobj expr) {
    mobj args = minim_cadr(expr);
    for (; minim_consp(args); args = minim_cdr(args)) {
        if (!minim_symbolp(minim_car(args))) {
            fprintf(stderr, "expected a identifier for an argument:\n ");
            write_object(stderr, expr);
            fputc('\n', stderr);
            minim_shutdown(1);
        }
    }

    if (!minim_nullp(args) && !minim_symbolp(args)) {
        fprintf(stderr, "expected an identifier for the rest argument:\n ");
        write_object(stderr, expr);
        fputc('\n', stderr);
        minim_shutdown(1);
    }
}

//
//  Public
//

void check_expr(mobj expr) {
    mobj head, it;

loop:

    if (minim_consp(expr)) {
        head = minim_car(expr);
        if (head == define_values_symbol) {
            // define-values form
            check_define_values(expr);
            expr = minim_car(minim_cddr(expr));
            goto loop;
        } else if (head == let_values_symbol || head == letrec_values_symbol) {
            // let-values form
            check_let_values(expr);
            for (it = minim_cadr(expr); !minim_nullp(it); it = minim_cdr(it))
                check_expr(minim_cadr(minim_car(it)));
            expr = Mcons(begin_symbol, (minim_cddr(expr)));
            goto loop;
        } else if (head == quote_symbol) {
            // quote form
            check_1ary_syntax(expr);
        } else if (head == quote_syntax_symbol) {
            // quote-syntax form
            check_1ary_syntax(expr);
        } else if (head == setb_symbol) {
            // set! form
            check_assign(expr);
            expr = minim_car(minim_cddr(expr));
            goto loop;
        } else if (head == if_symbol) {
            // if form
            check_3ary_syntax(expr);
            check_expr(minim_cadr(expr));
            check_expr(minim_car(minim_cddr(expr)));
            expr = minim_cadr(minim_cddr(expr));
            goto loop;
        } else if (head == lambda_symbol) {
            // lambda form
            check_lambda(expr);
            expr = Mcons(begin_symbol, minim_cddr(expr));
            goto loop;
        } else if (head == begin_symbol) {
            // begin form
            check_begin(expr);
            expr = minim_cdr(expr);
            while (!minim_nullp(minim_cdr(expr))) {
                check_expr(minim_car(expr));
                expr = minim_cdr(expr);
            }

            expr = minim_car(expr);
            goto loop;
        } else {
            // application
            check_begin(expr);
            for (expr = minim_cdr(expr); !minim_nullp(expr); expr = minim_cdr(expr)) {
                check_expr(minim_car(expr));
            }
        }
    } else if (minim_truep(expr) ||
        minim_falsep(expr) ||
        minim_fixnump(expr) ||
        minim_charp(expr) ||
        minim_stringp(expr) ||
        minim_boxp(expr) ||
        minim_vectorp(expr) ||
        minim_symbolp(expr)) {
        // self-evaluating or variable
        return;
    } else if (minim_nullp(expr)) {
        // empty application
        fprintf(stderr, "missing procedure expression\n");
        fprintf(stderr, "  in: ");
        write_object(stderr, expr);
        minim_shutdown(1);
    } else {
        // unknown
        fprintf(stderr, "bad syntax\n");
        write_object(stderr, expr);
        fprintf(stderr, "\n");
        minim_shutdown(1);
    }
}
