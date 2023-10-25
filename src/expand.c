// expand.c: expander and syntax checker for bootstrapping

#include "minim.h"

static mobj expand_body(mobj form, mobj es);

static void syntax_error(const char *name,
                         const char * what,
                         mobj stx,
                         mobj sstx) {
    fprintf(stderr, "%s: %s\n", name, what);
    if (sstx == NULL) {
        fprintf(stderr, " where: ");
        write_object(th_output_port(get_thread()), stx);
        fprintf(stderr, "\n");
    } else {
        fprintf(stderr, " at: ");
        write_object(th_output_port(get_thread()), sstx);
        fprintf(stderr, "\n");
        
        fprintf(stderr, " where: ");
        write_object(th_output_port(get_thread()), stx);
        fprintf(stderr, "\n");
    }
}

static int syntax_formp(mobj head, mobj e) {
    return minim_consp(e) && minim_car(e) == head;
}

//
//  Checker
//

// Checks for a valid formal signature
// `formals := ()
//          := <id>
//          := (<id> . <formals>)`
static void check_formals(mobj form, mobj formals) {
    mobj *i = formals;

loop:
    if (minim_consp(i)) {
        if (!minim_symbolp(minim_car(i))) {
            syntax_error("check_formals",
                         "expected identifier", 
                         form, minim_car(i));
        }

        i = minim_cdr(i);
        goto loop;
    } else if (!minim_nullp(i) && !minim_symbolp(i)) {
        syntax_error("check_formals",
                     "bad formal argument list", 
                     form, formals);
    }
}

// Assuming `e := (_ ...)`
// Expecting `e := (lambda <formals> <body> ..+)`
static void check_lambda_form(mobj e) {
    mobj rib;

    rib = minim_cdr(e);
    if (!minim_consp(rib))
        syntax_error("check_lambda_form", "missing formals", e, NULL);
    check_formals(e, minim_car(rib));

    rib = minim_cdr(rib);
    if (!minim_consp(rib))
        syntax_error("check_lambda_form", "missing body", e, NULL);
    
    rib = minim_cdr(rib);
    if (!listp(rib)) {
        syntax_error("check_lambda_form",
                     "expected expression sequence",
                     e, rib);
    }
}

// Assuming `e := (_ ...)`
// Expecting `e := (if <expr> <expr> <expr>)
static void check_if_form(mobj e) {
    mobj *rest;
    
    rest = minim_cdr(e);
    if (!minim_consp(rest))
        syntax_error("check_if_form", "bad syntax", e, NULL);

    rest = minim_cdr(rest);
    if (!minim_consp(rest))
        syntax_error("check_if_form", "bad syntax", e, NULL);

    rest = minim_cdr(rest);
    if (!minim_consp(rest) || !minim_nullp(minim_cdr(rest)))
        syntax_error("check_if_form", "bad syntax", e, NULL);
}

// Assuming `e := (_ ...)`
// Expecting `e := (begin <e1> <es> ...)`
static void check_begin_form(mobj e) {
    if (!listp(minim_cdr(e))) {
        syntax_error("check_begin_form",
                     "expected expression sequence",
                     e, NULL);
    }
}

// Assuming `e := (_ ...)`.
// Expecting `e := (define <id> <e>)
//              := (define (id <formals> ...) <es> ..+)
static void check_define_form(mobj e) {
    mobj rib, id;

    rib = minim_cdr(e);
    if (!minim_consp(e))
        syntax_error("check_define_form", "bad syntax", e, NULL);

    id = minim_car(rib);
    if (minim_consp(id)) {
        // procedure syntax
        if (!minim_symbolp(minim_car(id))) {
            syntax_error("check_define_form",
                         "missing identifier",
                         e, minim_car(id));
        }
        check_formals(e, minim_cdr(id));
    } else if (!minim_symbolp(id)) {
        syntax_error("check_define_form", "bad syntax", e, id);
    }

    rib = minim_cdr(rib);
    if (!minim_consp(rib)) {
        syntax_error("check_define_form", "missing body", e, NULL);
    }

    rib = minim_cdr(rib);
    if (!minim_consp(id) && !minim_nullp(rib)) {
        syntax_error("check_define_form", "missing bodies", e, NULL);
    }
}

// Assuming `e := (_ ...)`.
// Expecting `e := (define-values <id> <e>)`.
static void check_define_values_form(mobj e) {
    mobj rib, ids;

    rib = minim_cdr(e);
    if (!minim_consp(e))
        syntax_error("check_define_values_form", "bad syntax", e, NULL);

    for (ids = minim_car(rib); minim_consp(ids); ids = minim_cdr(ids)) {
        if (!minim_symbolp(minim_car(ids))) {
            syntax_error("check_define_values_form",
                         "expected identifier",
                         e, minim_car(ids));
        }
    }

    if (!minim_nullp(ids)) {
        syntax_error("check_define_values_form",
                     "expected identifier list",
                     e, ids);
    }

    rib = minim_cdr(rib);
    if (!minim_consp(rib))  {
        syntax_error("check_define_values_form",
                     "missing body", e, NULL);
    }

    rib = minim_cdr(rib);
    if (!minim_nullp(rib)) {
        syntax_error("check_define_values_form",
                     "multiple bodies", e, NULL);
    }
}

// Assuming `e := (_ ...)`.
// Expecting `e := (let-values ([(<id> ...) <e>] ...) <body> ...)`.
static void check_let_values_form(mobj e) {
    mobj rib, bindings, bind, ids;

    rib = minim_cdr(e);
    if (!minim_consp(rib))
        syntax_error("check_let_values_form", "bad syntax", e, NULL);

    bindings = minim_car(rib);
    while (!minim_nullp(bindings)) {
        bind = minim_car(bindings);
        if (!minim_consp(bind)) {
            syntax_error("check_let_values_form",
                         "ill-formed binding",
                         e, bind);
        }

        ids = minim_car(bind);
        bind = minim_cdr(bind);
        if (!minim_consp(bind) || !minim_nullp(minim_cdr(bind))) {
            syntax_error("check_let_values_form",
                         "ill-formed binding",
                         e, minim_car(bindings));
        }

        for (; minim_consp(ids); ids = minim_cdr(ids)) {
            if (!minim_symbolp(minim_car(ids))) {
                syntax_error("check_let_values_form",
                             "expected identifier",
                             e, minim_car(ids));
            }
        }

        if (!minim_nullp(ids)) {
            syntax_error("check_let_values_form",
                         "expected identifier list",
                         e, ids);
        }

        bindings = minim_cdr(bindings);
    }

    if (!minim_nullp(bindings)) {
        syntax_error("check_let_values_form",
                     "expected list of bindings",
                     e, minim_car(rib));
    }

    rib = minim_cdr(rib);
    if (!minim_consp(rib))
        syntax_error("check_let_values_form", "missing body", e, NULL);
    
    rib = minim_cdr(rib);
    if (!listp(rib)) {
        syntax_error("check_let_values_form",
                     "expected expression sequence",
                     e, rib);
    }
}

//
//  Expander
//

// Splices in begin subexpressions.
static void splice_begin_forms(mobj e) {
    mobj t = NULL;
    for (mobj i = e; !minim_nullp(i); i = minim_cdr(i)) {
        if (syntax_formp(begin_sym, minim_car(i))) {
            // splice in begin subexpression
            check_begin_form(minim_car(i));
            if (t == NULL) {
                // first expression
                e = Mcons(minim_car(e), list_append(minim_cdar(i), minim_cdr(i)));
                t = i;
            } else {
                // any other expression
                minim_cdr(t) = list_append(minim_cdar(i), minim_cdr(i));
                i = t;
            }
        } else {
            t = i;
        }
    }
}

// Partial expansion of `define` form into a `define-values` form.
// Does not expand the body.
static mobj expand_define_form(mobj e) {
    mobj rib, id, body, formals;

    rib = minim_cdr(e);
    id = minim_car(rib);
    if (minim_consp(id)) {
        // procedure:
        // `(define (<id> . <formals>) <body> ...)
        //  => (define <id> (lambda <formals> <body> ...))`
        formals = minim_cdr(id);
        id = minim_car(id);
        body = Mcons(lambda_sym, Mcons(formals, minim_cdr(rib)));
    } else {
        body = minim_cadr(rib);
    }

    // `(define <id> <e>) => (define-values (<id>) <e>)`
    return Mlist3(define_values_sym, Mlist1(id), body);
}

// Expansion of `let-values` form recursing with `expand-expr`.
static mobj expand_let_values_form(mobj e) {
    mobj rib, bindings, bind;

    rib = minim_cdr(e);
    bindings = minim_car(rib);
    while (!minim_nullp(bindings)) {
        bind = minim_car(bindings);
        minim_cadr(bind) = expand_expr(minim_cadr(bind));
        bindings = minim_cdr(bindings);
    }

    splice_begin_forms(rib);
    minim_cdr(rib) = expand_body(e, minim_cdr(rib));
    return e;
}

// Expands an expression sequence.
// Internal `define-values` forms are expanded into `let-values` forms.
static mobj expand_body(mobj form, mobj es) {
    mobj t = NULL;
    for (mobj i = es; !minim_nullp(i); i = minim_cdr(i)) {
        mobj e = minim_car(i);
loop:
        if (syntax_formp(define_sym, e)) {
            // define form => expansion
            check_define_form(e);
            e = expand_define_form(e);
            goto loop;
        } else if (syntax_formp(define_values_sym, e)) {
            if (minim_nullp(minim_cdr(i))) {
                syntax_error("expand_body",
                             "last form is not an expression",
                             form, minim_car(i));
            }

            // `(<before> ... (define-values (<ids> ...) <body>) <after> ...)
            //  => (<before> ... (let-values (((<ids> ...) <body>)) <after> ...))`
            e = Mcons(let_values_sym, Mcons(Mlist1(minim_cdr(e)), minim_cdr(i)));
            e = expand_expr(e);
            if (t == NULL) {
                // first expression in sequence
                return Mlist1(e);
            } else {
                // any other expression
                minim_cdr(t) = Mlist1(e);
                return es;
            }
        } else {
            minim_car(i) = expand_expr(e);
            t = i;
        }
    }

    return es;
}

//
//  Public API
//

mobj expand_expr(mobj e) {
    mobj t, i;

    write_object(Mport(stdout, 0x0), e);
    fputc('\n', stdout);

    if (!minim_consp(e))
        return e;

    if (syntax_formp(lambda_sym, e)) {
        // lambda form => recurse
        check_lambda_form(e);
        splice_begin_forms(minim_cdr(e));
        minim_cddr(e) = expand_body(e, minim_cddr(e));
    } else if (syntax_formp(let_values_sym, e) ||
                syntax_formp(letrec_values_sym, e)) {
        // let-values form => recurse
        check_let_values_form(e);
        e = expand_let_values_form(e);
    } else if (syntax_formp(begin_sym, e)) {
        // begin form => recurse
        check_begin_form(e);
        splice_begin_forms(e);
        minim_cddr(e) = expand_body(e, minim_cddr(e));
    } else if (syntax_formp(if_sym, e)) {
        // if form => recurse
        check_if_form(e);
        e = Mlist4(minim_car(e),
                   expand_expr(minim_cadr(e)),
                   expand_expr(minim_car(minim_cddr(e))),
                   expand_expr(minim_cadr(minim_cddr(e))));
    }

    return e;
}

mobj expand_top(mobj e) {
    mobj t, i;

    write_object(Mport(stdout, 0x0), e);
    fputc('\n', stdout);

    if (!minim_consp(e))
        return e;

loop:

    if (syntax_formp(define_sym, e)) {
        // define form => expansion
        check_define_form(e);
        e = expand_define_form(e);
        goto loop;
    } else if (syntax_formp(define_values_sym, e)) {
        // define-values form => expand value
        check_define_values_form(e);
        t = minim_cddr(e);
        minim_car(t) = expand_expr(minim_car(t));
    } else if (syntax_formp(begin_sym, e)) {
        // begin form => recurse
        check_begin_form(e);
        splice_begin_forms(e);
        for (i = minim_cdr(e); !minim_nullp(i); i = minim_cdr(i))
            minim_car(i) = expand_top(minim_car(i));
    }

    return e;
}
