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
    
    fatal_exit();
}

static int syntax_formp(mobj head, mobj e) {
    return minim_consp(e) && minim_car(e) == head;
}

#define define_values_formp(e)      syntax_formp(define_values_sym, e)
#define letrec_values_formp(e)      syntax_formp(letrec_values_sym, e)
#define let_values_formp(e)         syntax_formp(let_values_sym, e)
#define values_formp(e)             syntax_formp(values_sym, e)
#define lambda_formp(e)             syntax_formp(lambda_sym, e)
#define begin_formp(e)              syntax_formp(begin_sym, e)
#define if_formp(e)                 syntax_formp(if_sym, e)
#define quote_formp(e)              syntax_formp(quote_sym, e)
#define setb_formp(e)               syntax_formp(setb_sym, e)
#define foreign_proc_formp(e)       syntax_formp(foreign_proc_sym, e)

#define define_formp(e)             syntax_formp(define_sym, e)
#define letrec_formp(e)             syntax_formp(letrec_sym, e)
#define let_formp(e)                syntax_formp(let_sym, e)
#define cond_formp(e)               syntax_formp(cond_sym, e)
#define and_formp(e)                syntax_formp(and_sym, e)
#define or_formp(e)                 syntax_formp(or_sym, e)

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

// Assuming `e := (_ ...)`.
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

// Assuming `e := (_ ...)`.
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

// Assuming `e := (_ ...)`.
// Expecting `e := (cond [<test> <then>] ...)
static void check_cond_form(mobj e) {
    mobj i, cl;
    for (i = minim_cdr(e); !minim_nullp(i); i = minim_cdr(i)) {
        cl = minim_car(i);
        if (!minim_consp(cl))
            syntax_error("check_cond_form", "bad clause", e, cl);
        
        cl = minim_cdr(cl);
        if (!minim_consp(cl))
            syntax_error("check_cond_form", "bad clause", e, minim_car(i));

        cl = minim_cdr(cl);
        if (!listp(cl)) {
            syntax_error("check_cond_form",
                         "expected expression sequence",
                         e, minim_car(i));
        }
    }
}

// Assuming `e := (_ ...)`.
// Expecting `e := (_ <e1> <es> ...)`
static void check_0ary_form(mobj e) {
    if (!listp(minim_cdr(e))) {
        syntax_error("check_begin_form",
                     "expected expression sequence",
                     e, NULL);
    }
}

// Assuming `e := (_ ...)`.
// Expecting `e := (set! <id> <e>)`.
static void check_setb_form(mobj e) {
    mobj rib, id;

    rib = minim_cdr(e);
    if (!minim_consp(rib))
        syntax_error("check_setb_form", "bad syntax", e, NULL);

    id = minim_car(rib);
    if (!minim_symbolp(id)) {
        syntax_error("check_setb_form", "missing identifier", e, id);
    }

    rib = minim_cdr(rib);
    if (!minim_consp(rib)) {
        syntax_error("check_setb_form", "missing body", e, NULL);
    }

    rib = minim_cdr(rib);
    if (!minim_nullp(rib)) {
        syntax_error("check_define_form", "multiple bodies", e, NULL);
    }
}

// Assuming `e := (_ ...)`.
// Expecting `e := (define <id> <e>)
//              := (define (id <formals> ...) <es> ..+)
static void check_define_form(mobj e) {
    mobj rib, id;

    rib = minim_cdr(e);
    if (!minim_consp(rib))
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
        syntax_error("check_define_form", "multiple bodies", e, NULL);
    }
}

// Assuming `e := (_ ...)`.
// Expecting `e := (define-values <id> <e>)`.
static void check_define_values_form(mobj e) {
    mobj rib, ids;

    rib = minim_cdr(e);
    if (!minim_consp(rib))
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
// Expecting `e := (let <name> ([<id> <e>] ...) <body> ...)
//              := (let ([<id> <e>] ...) <body> ...)`
static void check_let_form(mobj e) {
    mobj rib, bindings, bind, id;

    rib = minim_cdr(e);
    if (!minim_consp(rib))
        syntax_error("check_let_form", "bad syntax", e, NULL);

    id = minim_car(rib);
    if (minim_symbolp(id)) {
        rib = minim_cdr(rib);
    }

    bindings = minim_car(rib);
    while (!minim_nullp(bindings)) {
        bind = minim_car(bindings);
        if (!minim_consp(bind)) {
            syntax_error("check_let_form",
                         "ill-formed binding",
                         e, bind);
        }

        id = minim_car(bind);
        if (!minim_symbolp(id)) {
            syntax_error("check_let_form", "expected identifier", e, id);
        }

        bind = minim_cdr(bind);
        if (!minim_consp(bind) || !minim_nullp(minim_cdr(bind))) {
            syntax_error("check_let_form",
                         "ill-formed binding",
                         e, minim_car(bindings));
        }

        bindings = minim_cdr(bindings);
    }

    if (!minim_nullp(bindings)) {
        syntax_error("check_let_form",
                     "expected list of bindings",
                     e, minim_car(rib));
    }

    rib = minim_cdr(rib);
    if (!minim_consp(rib))
        syntax_error("check_let_form", "missing body", e, NULL);
    
    rib = minim_cdr(rib);
    if (!listp(rib)) {
        syntax_error("check_let_form",
                     "expected expression sequence",
                     e, rib);
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

// Assuming `e := (_ ...)`.
// Expecting `e := (quote <datum)`
static void check_quote_form(mobj e) {
    mobj rib = minim_cdr(e);
    if (!minim_nullp(minim_cdr(rib))) {
        syntax_error("check_quote_form", "only a single datum expected", e, NULL);
    }
}

// Assuming `e := (_ ...)`.
// Expecting `e := (#%foreign-procedure <id> (<ctypes> ...) <ctype>`.
static void check_foreign_proc_form(mobj e) {
    mobj rib, id, itypes, itype, otype;
    
    rib = minim_cdr(e);
    if (!minim_consp(rib)) {
        syntax_error("check_foreign_proc_form",
                     "bad syntax",
                     e, NULL);
    }

    id = minim_car(rib);
    if (!minim_stringp(id)) {
        syntax_error("check_foreign_proc_form",
                     "expected string identifier",
                     e, id);
    }

    rib = minim_cdr(rib);
    if (!minim_consp(rib)) {
        syntax_error("check_foreign_proc_form",
                     "expected types",
                     e, NULL);
    }

    itypes = minim_car(rib);
    for (; !minim_nullp(itypes); itypes = minim_cdr(itypes)) {
        itype = minim_car(itypes);
        if (!minim_symbolp(itype)) {
            syntax_error("check_foreign_proc_form",
                         "expected type identifier",
                         e, itype);
        }

        if (itype != intern("object")) {
            syntax_error("check_foreign_proc_form",
                         "unknown type identifier",
                         e, itype);   
        }
    }

    if (!minim_nullp(itypes)) {
        syntax_error("check_let_values_form",
                     "expected list of type identifiers",
                     e, minim_car(rib));
    }

    rib = minim_cdr(rib);
    if (!minim_consp(rib)) {
        syntax_error("check_foreign_proc_form",
                     "expected types",
                     e, NULL);
    }

    otype = minim_car(rib);
    if (!minim_symbolp(otype)) {
        syntax_error("check_foreign_proc_form",
                     "expected type identifier",
                     e, otype);
    }

    if (otype != intern("object")) {
        syntax_error("check_foreign_proc_form",
                     "unknown type identifier",
                     e, otype);   
    }

    if (!minim_nullp(minim_cdr(rib))) {
        syntax_error("check_foreign_proc_form",
                     "bad syntax", e, NULL);
    }
}

//
//  Expander
//

// Splices in begin subexpressions.
static void splice_begin_forms(mobj e) {
    mobj t = NULL;
    for (mobj i = e; !minim_nullp(i); i = minim_cdr(i)) {
        if (begin_formp(minim_car(i))) {
            // splice in begin subexpression
            check_0ary_form(minim_car(i));
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

// Partial expansion of named `let` form into `let-values` form.
// `(let <name> ([<id> <e>] ...) <body> ...)
//  => (let-values ([(<name>) (lambda (<id> ...) <body> ...)])
//       (<name> <e> ...))`
static mobj expand_named_let_form(mobj e) {
    mobj rib, id, vars, vals, bindings, bind, *proc;

    // store id
    rib = minim_cdr(e);
    id = minim_car(rib);

    // collect variables and values from bindings
    rib = minim_cdr(rib);
    bindings = minim_car(rib);
    vars = vals = minim_null;
    while (!minim_nullp(bindings)) {
        bind = minim_car(bindings);
        vars = Mcons(minim_car(bind), vars);
        vals = Mcons(minim_cadr(bind), vals);
        bindings = minim_cdr(bindings);
    }

    // constructed backwards as usual
    vars = list_reverse(vars);
    vals = list_reverse(vals);

    // compose syntax
    proc = Mcons(lambda_sym, Mcons(vars, minim_cdr(rib)));
    return Mlist3(let_values_sym,
                  Mlist1(Mlist2(Mlist1(id), proc)),
                  Mcons(id, vals));
}

// Partial expansion of `let` form into `let-values` form.
// `(let <name> ([<id> <e>] ...) <body> ...)
//  => (let-values ([(<id>) <e>] ...) <body> ...)`
static mobj expand_let_form(mobj e) {
    mobj rib, bindings, bind;

    rib = minim_cdr(e);
    if (minim_symbolp(minim_car(rib)))
        return expand_named_let_form(e);

    bindings = minim_car(rib);
    while (!minim_nullp(bindings)) {
        bind = minim_car(bindings);
        minim_car(bind) = Mlist1(minim_car(bind));
        bindings = minim_cdr(bindings);
    }

    return Mcons(let_values_sym, rib);
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

// Recursive expansion of `and` form.
// `(and) => #t`
// `(and <e>) => <e>`
// `(and <e> <es> ...) => (if <e> (and <es> ...) #f)`
static mobj expand_and_form(mobj e) {
    mobj cls = minim_cdr(e);
    if (minim_nullp(cls)) {
        return minim_true;
    } else if (minim_nullp(minim_cdr(cls))) {
        return expand_expr(minim_car(cls));
    } else { 
        return Mlist4(if_sym,
                      expand_expr(minim_car(cls)),
                      expand_and_form(Mcons(and_sym, minim_cdr(cls))),
                      minim_false);
    }
}

// Recursive expansion of `or` form.
// `(or) => #f`
// `(or <e>) => <e>`
// `(or <e> <es> ...) => (let-values ([(<tmp>) <e>]) (if <tmp> <tmp> (or <es> ...)))`
static mobj expand_or_form(mobj e) {
    mobj cls, gen, rec;
    
    cls = minim_cdr(e);
    if (minim_nullp(cls)) {
        return minim_false;
    } else if (minim_nullp(minim_cdr(cls))) {
        return expand_expr(minim_car(cls));
    } else {
        gen = intern("$T$");
        rec = expand_or_form(Mcons(or_sym, minim_cdr(cls)));
        return Mlist3(let_values_sym,
                      Mlist1(Mlist2(Mlist1(gen), expand_expr(minim_car(cls)))),
                      Mlist4(if_sym, gen, gen, rec));
    }
}

// Recursive expansion of `cond` form.
static mobj expand_cond_form(mobj e) {
    mobj cls, cl, ift, iff;
    
    cls = minim_cdr(e);
    if (minim_nullp(cls)) {
        return Mlist1(void_sym);
    } else {
        cl = minim_car(cls);
        ift = expand_expr(Mcons(begin_sym, minim_cdr(cl)));
        if (minim_car(cl) == intern("else")) {
            if (!minim_nullp(minim_cdr(cls))) {
                syntax_error("expand_cond_form",
                             "`else` clause must be last",
                             e, cl);
            }
            return ift;
        }

        iff = expand_cond_form(Mcons(cond_sym, minim_cdr(cls)));
        return Mlist4(if_sym, minim_car(cl), ift, iff);
    }
}

// Expands an expression sequence.
// Internal `define-values` forms are expanded into `let-values` forms.
static mobj expand_body(mobj form, mobj es) {
    mobj t = NULL;
    for (mobj i = es; !minim_nullp(i); i = minim_cdr(i)) {
        mobj e = minim_car(i);
loop:
        if (define_formp(e)) {
            // define form => expansion
            check_define_form(e);
            e = expand_define_form(e);
            goto loop;
        } else if (define_values_formp(e)) {
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
    mobj i;

    if (!minim_consp(e))
        return e;

loop:
    if (lambda_formp(e)) {
        // lambda form => recurse
        check_lambda_form(e);
        splice_begin_forms(minim_cdr(e));
        minim_cddr(e) = expand_body(e, minim_cddr(e));
    } else if (let_formp(e)) {
        // let form => recurse
        check_let_form(e);
        e = expand_let_form(e);
        goto loop;
    } else if (letrec_values_formp(e) || let_values_formp(e)) {
        // let-values form => recurse
        check_let_values_form(e);
        e = expand_let_values_form(e);
    } else if (begin_formp(e)) {
        // begin form => recurse
        check_0ary_form(e);
        splice_begin_forms(e);
        if (minim_nullp(minim_cdr(e))) {
            // empty sequence
            e = Mlist1(void_sym);
        } else {
            e = expand_body(e, minim_cdr(e));
            if (minim_nullp(minim_cddr(e))) {
                // sequence of length 1
                e = minim_cadr(e);
            }
        }
    } else if (and_formp(e)) {
        // and form => recurse
        check_0ary_form(e);
        e = expand_and_form(e);
    } else if (or_formp(e)) {
        // or form => recurse
        check_0ary_form(e);
        e = expand_or_form(e);
    } else if (cond_formp(e)) {
        // cond form => recurse
        check_cond_form(e);
        e = expand_cond_form(e);
    } else if (if_formp(e)) {
        // if form => recurse
        check_if_form(e);
        e = Mlist4(if_sym,
                   expand_expr(minim_cadr(e)),
                   expand_expr(minim_car(minim_cddr(e))),
                   expand_expr(minim_cadr(minim_cddr(e))));
    } else if (setb_formp(e)) {
        // set! form => recurse
        check_setb_form(e);
        minim_car(minim_cddr(e)) = expand_expr(minim_car(minim_cddr(e)));
    } else if (quote_formp(e)) {
        // quote form
        check_quote_form(e);
    } else if (foreign_proc_formp(e)) {
        // foreign procedure form
        check_foreign_proc_form(e);
    } else {
        // operator application
        for (i = e; !minim_nullp(i); i = minim_cdr(i)) {
            minim_car(i) = expand_expr(minim_car(i));
        }
    }

    return e;
}

mobj expand_top(mobj e) {
    mobj t, i;

    if (!minim_consp(e))
        return e;

loop:
    if (define_formp(e)) {
        // define form => expansion
        check_define_form(e);
        e = expand_define_form(e);
        goto loop;
    } else if (define_values_formp(e)) {
        // define-values form => expand value
        check_define_values_form(e);
        t = minim_cddr(e);
        minim_car(t) = expand_expr(minim_car(t));
    } else if (begin_formp(e)) {
        // begin form => recurse
        check_0ary_form(e);
        splice_begin_forms(e);
        if (minim_nullp(minim_cdr(e))) {
            // empty sequence
            e = Mlist1(void_sym);
            goto loop;
        } else if (minim_nullp(minim_cddr(e))) {
            // sequence of length 1
            e = minim_cadr(e);
            goto loop;
        } else {
            for (i = minim_cdr(e); !minim_nullp(i); i = minim_cdr(i))
                minim_car(i) = expand_top(minim_car(i));
        }
    } else {
        e = expand_expr(e);   
    }

    return e;
}
