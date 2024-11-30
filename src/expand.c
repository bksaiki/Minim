// expand.c: minimal expander

#include "minim.h"

static obj condense_body(obj es) {
    if (Mnullp(Mcdr(es))) {
        // single expression => just use expression
        return Mcar(es);
    } else {
        // multiple expressions => wrap in begin
        return Mcons(Mbegin_symbol, es);
    }
}

// (cond [<test> <expr> ..+] <cls> ...)
// =>
// (if <test> (begin <expr> ..+) (cond <cls> ...))
// (cond [else <tail>])
// =>
// <tail>
static obj expand_cond(obj e) {
    obj cls, ift, iff;

    cls = Mcadr(e);
    if (Mnullp(Mcddr(e)) && Mcar(cls) == Mintern("else")) {
        // else clause
        return Mcons(Mbegin_symbol, Mcdr(cls));
    } else {
        // normal clause
        ift = Mcons(Mbegin_symbol, Mcdr(cls));
        iff = Mcons(Mcond_symbol, Mcddr(e));
        return Mlist4(Mif_symbol, Mcar(cls), ift, iff);
    }
}

// (let-values ([(id ...) <expr>] ...) <body> ...)
// => (let-values ([(id ...) <expr>] ...) (begin <body>) 
static obj expand_let_values_expr(obj e) {
    obj it, hd, tl, bind;

    it = Mcadr(e);
    hd = tl = Mlist1(Mlist2(Mcaar(it), expand_expr(Mcadar(it))));
    for (it = Mcdr(it); !Mnullp(it); it = Mcdr(it)) {
        bind = Mlist2(Mcaar(it), expand_expr(Mcadar(it)));
        Mcdr(tl) = Mlist1(bind);
        tl = Mcdr(tl);
    }

    return Mlist3(Mlet_values_symbol, hd, expand_expr(condense_body(Mcddr(e))));
}

// ([(<id> ...) <expr>] ...)
// =>
// ([(<id>) #<unbound>] ...)
static obj make_letrec_values_ids(obj bindings) {
    obj hd, tl, ids, bind;

    hd = Mnull;
    tl = NULL;
    for (; !Mnullp(bindings); bindings = Mcdr(bindings)) {
        for (ids = Mcaar(bindings); !Mnullp(ids); ids = Mcdr(ids)) {
            bind = Mlist2(Mlist1(Mcar(ids)), Mlist2(Mquote_symbol, Munbound));
            if (tl) {
                Mcdr(tl) = Mlist1(bind);
                tl = Mcdr(tl);
            } else {
                hd = tl = Mlist1(bind);
            }
        }
    }

    return hd;
}

// Generates a list of temporary identifiers from a list
// of identifiers. List must be non-empty.
static obj make_letrec_values_tids(obj ids) {
    obj hd, tl;

    hd = tl = Mlist1(Mgensym("t"));
    for (ids = Mcdr(ids); !Mnullp(ids); ids = Mcdr(ids)) {
        Mcdr(tl) = Mlist1(Mgensym("t"));
        tl = Mcdr(tl);
    }

    return hd;
}

// `[(<id> ...) <expr>]` and `<body> ...`
// =>
// (let-values ([(<tid> ...) <expr>])
//   (set! <id> <tid>)
// ...
static obj make_letrec_values_set(obj binding) {
    obj ids, e, tids, it, hd, tl;

    ids = Mcar(binding);
    e = Mcadr(binding);

    if (Mnullp(ids)) {
        return Mlist3(Mlet_values_symbol, Mlist1(Mlist2(ids, e)), Mlist2(Mquote_symbol, Mvoid));
    } else {
        tids = make_letrec_values_tids(ids);
        hd = tl = Mlist1(Mlist3(Msetb_symbol, Mcar(ids), Mcar(tids)));
        for (ids = Mcdr(ids), it = Mcdr(tids); !Mnullp(ids); ids = Mcdr(ids), it = Mcdr(it)) {
            Mcdr(tl) = Mlist1(Mlist3(Msetb_symbol, Mcar(ids), Mcar(it)));
            tl = Mcdr(tl);
        }

        return Mcons(Mlet_values_symbol, Mcons(Mlist1(Mlist2(tids, e)), hd));
    }
}


// (letrec-values ([(<id> ...) <expr>] ...)
//   <body> ...)
// =>
// (let-values ([(<id> ...) (values #<unbound> ...)] ...)
//   (let-values ([(<tid> ...) <expr>])
//     (set! <id> <tid>)
//     ... )
//   <body> ...)
static obj expand_letrec_values_expr(obj e) {
    obj bindings, body, hd, tl;

    bindings = Mcadr(e);
    body = Mcddr(e);

    // create initial bindings
    e = Mlist2(Mlet_values_symbol, make_letrec_values_ids(bindings));

    // create assignments
    hd = tl = Mlist1(make_letrec_values_set(Mcar(bindings)));
    for (bindings = Mcdr(bindings); !Mnullp(bindings); bindings = Mcdr(bindings)) {
        Mcdr(tl) = Mlist1(make_letrec_values_set(Mcar(bindings)));
        tl = Mcdr(tl);
    }

    Mcdr(tl) = body;
    Mcddr(e) = hd;
    return e;
}

// (let ([<id> <expr>]) ...) <body>)
// => (let-values ([(<id>) <expr>]) ...) <body>)
//
// (let ([<id> <expr>]) ...) <body> ...)
// => (let-values ([(<id>) <expr>]) ...) (begin <body> ...)
static obj expand_let_expr(obj e) {
    obj it, hd, tl, bind;

    it = Mcadr(e);
    hd = tl = Mlist1(Mlist2(Mlist1(Mcaar(it)), Mcadar(it)));
    for (it = Mcdr(it); !Mnullp(it); it = Mcdr(it)) {
        bind = Mlist2(Mlist1(Mcaar(it)), Mcadar(it));
        Mcdr(tl) = Mlist1(bind);
        tl = Mcdr(tl);
    }

    return Mcons(Mlet_values_symbol, Mcons(hd, Mcddr(e)));
}

// (let <name> ([<id> <expr>]) ...)
//   <body>
//   ...)
// =>
// (letrec ([<name> (lambda (<id> ...) <body ...)])
//   (<name> <expr> ...))
static obj expand_let_loop(obj e) {
    obj it, formals, body, tl;

    it = Mcaddr(e);
    if (Mnullp(it)) {
        // special case: no arguments
        formals = Mnull;
        body = Mlist1(Mcadr(e));
    } else {
        // at least one argument =>
        // construct the lambda formals
        formals = tl = Mcons(Mcaar(it), Mnull);
        for (it = Mcdr(it); !Mnullp(it); it = Mcdr(it)) {
            Mcdr(tl) = Mcons(Mcaar(it), Mnull);
            tl = Mcdr(tl);
        }

        // construct the body
        body = tl = Mcons(Mcadr(e), Mnull);
        for (it = Mcaddr(e); !Mnullp(it); it = Mcdr(it)) {
            Mcdr(tl) = Mcons(Mcadar(it), Mnull);
            tl = Mcdr(tl);
        }
    }

    return Mlist3(
        Mletrec_symbol,
        Mlist1(Mlist2(Mcadr(e), Mcons(Mlambda_symbol, Mcons(formals, Mcdddr(e))))),
        body
    );
}

// (letrec ([<id> <expr>] ...)
//   <body>
//   ...)
// =>
// (let ([<id> #<unbound>] ...)
//   (set! <id> <expr>)
//   ...
//   <body>
//   ...)
static obj expand_letrec_expr(obj e) {
    obj it, binds, body, tl;

    // create the initial bindings
    it = Mcadr(e);
    binds = tl = Mcons(Mlist2(Mcaar(it), Mlist2(Mquote_symbol, Munbound)), Mnull);
    for (it = Mcdr(it); !Mnullp(it); it = Mcdr(it)) {
        Mcdr(tl) = Mcons(Mlist2(Mcaar(it), Mlist2(Mquote_symbol, Munbound)), Mnull);
        tl = Mcdr(tl);
    }

    // create the body with set! expressions
    it = Mcadr(e);
    body = tl = Mcons(Mlist3(Msetb_symbol, Mcaar(it), Mcadar(it)), Mnull);
    for (it = Mcdr(it); !Mnullp(it); it = Mcdr(it)) {
        Mcdr(tl) = Mcons(Mlist3(Msetb_symbol, Mcaar(it), Mcadar(it)), Mnull);
        tl = Mcdr(tl);
    }

    Mcdr(tl) = Mcddr(e);
    return Mcons(Mlet_symbol, Mcons(binds, body));
}

// (case-lambda ...)
static obj expand_case_lambda(obj e) {
    obj rib, hd, tl;

    rib = Mcdr(e);
    if (Mnullp(rib)) {
        return Mlist1(Mcase_lambda_symbol);
    } else {
        writeln_object(stderr, rib);
        hd = tl = Mlist1(Mlist2(Mcaar(rib), expand_expr(condense_body(Mcdar(rib)))));
        for (rib = Mcdr(rib); !Mnullp(rib); rib = Mcdr(rib)) {
            Mcdr(tl) = Mlist1(Mlist2(Mcaar(rib), expand_expr(condense_body(Mcdar(rib)))));
            tl = Mcdr(tl);
        }

        return Mcons(Mcase_lambda_symbol, hd);
    }
}

// (define <id> <expr>)
// =>
// (define-values (<id>) <expr>)
//
// (define (<id> . <formals>) <expr> ...)
// =>
// (define <id> (lambda <formals> <expr> ...))
static obj expand_define_expr(obj e) {
    obj id, formals;

loop:
    id = Mcadr(e);
    if (Msymbolp(id)) {
        return Mlist3(Mdefine_values_symbol, Mlist1(id), expand_expr(Mcaddr(e)));
    } else {
        formals = Mcdr(id);
        id = Mcar(id);
        e = Mlist3(Mdefine_symbol, id, Mcons(Mlambda_symbol, Mcons(formals, Mcddr(e))));
        goto loop;
    }
}



obj expand_expr(obj e) {
    obj hd, tl, it;

loop:

    if (Mconsp(e)) {
        hd = Mcar(e);
        if (hd == Mlet_symbol) {
            if (Msymbolp(Mcadr(e))) {
                // named let => convert to letrec
                e = expand_let_loop(e);
            } else if (Mnullp(Mcadr(e))) {
                // empty bindings => just use body
                e = condense_body(Mcddr(e));
            } else {
                // at least one binding
                e = expand_let_expr(e);   
            }

            goto loop;
        } else if (hd == Mletrec_symbol) {
            // letrec => transforms to let
            if (Mnullp(Mcadr(e))) {
                // empty bindings => just use body
                e = condense_body(Mcddr(e));
            } else {
                // at least one binding
                e = expand_letrec_expr(e);
            }

            goto loop;
        } else if (hd == Mlet_values_symbol) {
            if (Mnullp(Mcadr(e))) {
                // empty bindings => just use body
                e = condense_body(Mcddr(e));
                goto loop;
            } else {
                // at least one binding
                return expand_let_values_expr(e);
            }
        } else if (hd == Mletrec_values_symbol) {
            if (Mnullp(Mcadr(e))) {
                // empty bindings => just use body
                return condense_body(Mcddr(e));
            } else {
                // at least one binding
                e = expand_letrec_values_expr(e);
                goto loop;
            }
        } else if (hd == Mbegin_symbol) {
            if (Mnullp(Mcdr(e))) {
                // no body expression
                return Mlist2(Mquote_symbol, Mvoid);
            } else if (Mnullp(Mcddr(e))) {
                // one body expression
                e = Mcadr(e);
                goto loop;
            } else {
                // multiple body expressions
                hd = tl = Mcons(Mbegin_symbol, Mnull);
                for (it = Mcdr(e); !Mnullp(it); it = Mcdr(it)) {
                    Mcdr(tl) = Mcons(expand_expr(Mcar(it)), Mnull);
                    tl = Mcdr(tl);
                }
        
                return hd;
            }
        } else if (hd == Mif_symbol) {
            return Mlist4(
                Mif_symbol,
                expand_expr(Mcadr(e)),
                expand_expr(Mcaddr(e)),
                expand_expr(Mcar(Mcdddr(e)))
            );
        } else if (hd == Mwhen_symbol) {
            e = Mlist4(Mif_symbol, Mcadr(e), Mcons(Mbegin_symbol, Mcddr(e)), Mlist2(Mquote_symbol, Mvoid));
            goto loop;
        } else if (hd == Munless_symbol) {
            e = Mlist4(Mif_symbol, Mcadr(e), Mlist2(Mquote_symbol, Mvoid), Mcons(Mbegin_symbol, Mcddr(e)));
            goto loop;
        } else if (hd == Mcond_symbol) {
            if (Mnullp(Mcdr(e))) {
                // empty cond => void
                return Mlist2(Mquote_symbol, Mvoid);
            } else {
                // reduce by 1 clause
                e = expand_cond(e);
                goto loop;
            }
        } else if (hd == Mand_symbol) {
            if (Mnullp(Mcdr(e))) {
                // (and) => #t
                return Mtrue;
            } else if (Mnullp(Mcddr(e))) {
                // (and e) => e
                return Mcadr(e);
            } else {
                // (and e es ...) => (if e (and es ...) #f)
                e = Mlist4(Mif_symbol, Mcadr(e), Mcons(Mand_symbol, Mcddr(e)), Mfalse);
                goto loop;
            }
        } else if (hd == Mor_symbol) {
            if (Mnullp(Mcdr(e))) {
                // (or) => #f
                return Mfalse;
            } else if (Mnullp(Mcddr(e))) {
                // (or e) => e
                return Mcadr(e);
            } else {
                // (or e es ...) => (if e #t (or es ...))
                e = Mlist4(Mif_symbol, Mcadr(e), Mtrue, Mcons(Mor_symbol, Mcddr(e)));
                goto loop;
            }
        } else if (hd == Mlambda_symbol) {
            return Mlist3(Mlambda_symbol, Mcadr(e), expand_expr(condense_body(Mcddr(e))));
        } else if (hd == Mcase_lambda_symbol) {
            return expand_case_lambda(e);
        } else if (hd == Msetb_symbol) {
            return Mlist3(Msetb_symbol, Mcadr(e), expand_expr(Mcaddr(e)));
        } else if (hd == Mquote_symbol) {
            return e;
        } else {
            hd = tl = Mcons(expand_expr(Mcar(e)), Mnull);
            for (it = Mcdr(e); !Mnullp(it); it = Mcdr(it)) {
                Mcdr(tl) = Mcons(expand_expr(Mcar(it)), Mnull);
                tl = Mcdr(tl);
            }
        
            return hd;
        }
    } else if (Msymbolp(e) || Mimmediatep(e)) {
        // immediate or variable
        return e;
    } else {
        minim_error1("expand_expr", "unreachable", e);
    }
}

void expand_module(obj mod) {
    obj es, e, hd;

    for (es = Mmodule_body(mod); !Mnullp(es); es = Mcdr(es)) {
        e = Mcar(es);
        if (Mconsp(e)) {
            hd = Mcar(e);
            if (hd == Mimport_symbol) {
                // import => do nothing
            } else if (hd == Mexport_symbol) {
                // export => do nothing
            } else if (hd == Mdefine_values_symbol) {
                // define-values
                e = Mlist3(Mdefine_values_symbol, Mcadr(e), expand_expr(Mcaddr(e)));
            } else if (hd == Mdefine_symbol) {
                // define
                e = expand_define_expr(e);
            } else {
                e = expand_expr(e);
            }
        } else {
            e = expand_expr(e);
        }

        Mcar(es) = e;
    }
}
