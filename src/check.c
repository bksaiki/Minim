// check.c: syntax checker

#include "minim.h"

static void assert_identifier(obj form, obj id) {
    if (!Msymbolp(id)) {
        minim_error2(Msymbol_value(Mcar(form)), "expected identifier", form, id);
    }
}

NORETURN static void bad_syntax_exn(obj form) {
    minim_error1(Msymbol_value(Mcar(form)), "bad syntax", form);
}

static void check_formals(obj e, obj args) {
    for (; Mconsp(args); args = Mcdr(args)) {
        assert_identifier(e, Mcar(args));
    }

    if (!Mnullp(args)) {
        assert_identifier(e, args);
    }
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum>)
static void check_1ary_syntax(obj e) {
    obj rib = Mcdr(e);
    if (!Mconsp(rib) || !Mnullp(Mcdr(rib)))
        bad_syntax_exn(e);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> <datum> <datum>)
static void check_3ary_syntax(obj e) {
    obj rib;
    
    rib = Mcdr(e);
    if (!Mconsp(rib))
        bad_syntax_exn(e);

    rib = Mcdr(rib);
    if (!Mconsp(rib))
        bad_syntax_exn(e);

    rib = Mcdr(rib);
    if (!Mconsp(rib) || !Mnullp(Mcdr(rib)))
        bad_syntax_exn(e);
}

// Already assumes `expr` is `(<name> . <???>)`
static void check_when(obj e) {
    obj rib;
    
    rib = Mcdr(e);
    if (!Mconsp(rib))
        bad_syntax_exn(e);

    rib = Mcdr(rib);
    if (!Mconsp(rib))
        bad_syntax_exn(e);

    rib = Mcdr(rib);
    while (Mconsp(rib))
        rib = Mcdr(rib);
    
    if (!Mnullp(rib))
        bad_syntax_exn(e);
}

// Already assumes `expr` is `(<name> . <???>)`
static void check_cond(obj e) {
    obj rib, cls, els;

    els = Mintern("else");
    for (rib = Mcdr(e); !Mnullp(rib); rib = Mcdr(rib)) {
        cls = Mcar(rib);
        if (!Mconsp(cls) || !Mconsp(Mcdr(cls)) || !Mlistp(Mcddr(cls)))
            bad_syntax_exn(e);

        if (Mcar(cls) == els && !Mnullp(Mcdr(rib)))
            minim_error2("cond", "`else` clause must be last", e, cls);
    }
    
    if (!Mnullp(rib))
        bad_syntax_exn(e);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr must be `(<name> <symbol> <datum>)`
static void check_setb(obj e) {
    obj rib;

    rib = Mcdr(e);
    if (!Mconsp(rib) || !Msymbolp(Mcar(rib)))
        bad_syntax_exn(e);

    rib = Mcdr(rib);
    if (!Mconsp(rib))
        bad_syntax_exn(e);

    if (!Mnullp(Mcdr(rib)))
        bad_syntax_exn(e);
}

// Already assumes `expr` is `(let-values . <???>)`
// Check: `expr` must be `(let-values ([(<var> ...) <val>] ...) <body> ...)`
// Does not check if each `<body>` is an expression.
// Does not check if `<body> ...` forms a list.
static void check_let_values(obj e) {
    obj rib, bindings, bind, ids;

    rib = Mcdr(e);
    if (!Mconsp(rib) && !Mconsp(Mcdr(rib)))
        bad_syntax_exn(e);

    bindings = Mcar(rib);
    while (Mconsp(bindings)) {
        bind = Mcar(bindings);
        if (!Mconsp(bind))
            bad_syntax_exn(e);

        for (ids = Mcar(bind); !Mnullp(ids); ids = Mcdr(ids)) 
            assert_identifier(e, Mcar(ids));

        bind = Mcdr(bind);
        if (!Mconsp(bind) || !Mnullp(Mcdr(bind)))
            bad_syntax_exn(e);

        bindings = Mcdr(bindings);
    }

    if (!Mnullp(bindings))
        bad_syntax_exn(e);
}

// Already assumes `expr` is `(let . <???>)`
// Check: `expr` must be `(let ([<var> <val>] ...) <body> ...)`
// Does not check if each `<body>` is an expression.
// Does not check if `<body> ...` forms a list.
static void check_let(obj e) {
    obj rib, bindings, bind, id;

    rib = Mcdr(e);
    if (!Mconsp(rib))
        bad_syntax_exn(e);

    if (Msymbolp(Mcar(rib))) {
        // named let
        rib = Mcdr(rib);
        if (!Mconsp(rib) || !Mconsp(Mcdr(rib)))
            bad_syntax_exn(e);
    } else {
        if (!Mconsp(Mcdr(rib)))
            bad_syntax_exn(e);
    }

    bindings = Mcar(rib);
    while (Mconsp(bindings)) {
        bind = Mcar(bindings);
        if (!Mconsp(bind))
            bad_syntax_exn(e);

        id = Mcar(bind);
        assert_identifier(e, id);

        bind = Mcdr(bind);
        if (!Mconsp(bind) || !Mnullp(Mcdr(bind)))
            bad_syntax_exn(e);

        bindings = Mcdr(bindings);
    }

    if (!Mnullp(bindings))
        bad_syntax_exn(e);
}

// Already assumes `expr` is `(letrec . <???>)`
// Check: `expr` must be `(letrec ([<var> <val>] ...) <body> ...)`
// Does not check if each `<body>` is an expression.
// Does not check if `<body> ...` forms a list.
static void check_letrec(obj e) {
    obj rib, bindings, bind, id;
    
    rib = Mcdr(e);
    if (!Mconsp(rib) || !Mconsp(Mcdr(rib)))
        bad_syntax_exn(e);
    
    bindings = Mcar(rib);
    while (Mconsp(bindings)) {
        bind = Mcar(bindings);
        if (!Mconsp(bind))
            bad_syntax_exn(e);

        id = Mcar(bind);
        assert_identifier(e, id);

        bind = Mcdr(bind);
        if (!Mconsp(bind) || !Mnullp(Mcdr(bind)))
            bad_syntax_exn(e);

        bindings = Mcdr(bindings);
    }

    if (!Mnullp(bindings))
        bad_syntax_exn(e);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> ...)`
static void check_begin(obj e) {
    obj rib;
    
    rib = Mcdr(e);
    while (Mconsp(rib))
        rib = Mcdr(rib);

    if (!Mnullp(rib))
        bad_syntax_exn(e);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> ...)`
static void check_lambda(obj e) {
    obj rib;

    rib = Mcdr(e);
    if (!Mconsp(rib))
        bad_syntax_exn(e);
    check_formals(e, Mcar(rib));

    rib = Mcdr(rib);
    if (!Mconsp(rib))
        bad_syntax_exn(e);

    while (Mconsp(rib))
        rib = Mcdr(rib);

    if (!Mnullp(rib))
        bad_syntax_exn(e);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> ...)`
static void check_case_lambda(obj e) {
    obj rib, it;
    
    rib = Mcdr(e);
    for (; Mconsp(rib); rib = Mcdr(rib)) {
        it = Mcar(rib);
        if (!Mconsp(it))
            bad_syntax_exn(e);
        check_formals(e, Mcar(it));

        it = Mcdr(it);
        if (!Mconsp(it))
            bad_syntax_exn(e);
        
        while (Mconsp(it))
            it = Mcdr(it);
        
        if (!Mnullp(it))
            bad_syntax_exn(e);
    }

    if (!Mnullp(rib))
        bad_syntax_exn(e);
}

static void check_import_spec(obj e, obj spec) {
    obj rib, hd;

    if (Mconsp(spec)) {
        hd = Mcar(spec);
        if (hd == Mintern("prefix")) {
            // (prefix <spec> <identifier)
            rib = Mcdr(spec);
            if (!Mconsp(rib) || !Mconsp(Mcdr(rib)) || !Mnullp(Mcddr(rib))) {
                bad_syntax_exn(e);
            }

            check_import_spec(e, Mcar(rib));
            assert_identifier(e, Mcadr(rib));
        } else {
            bad_syntax_exn(e);
        }
    } else if (!Msymbolp(spec) && !Mstringp(spec)) {
        bad_syntax_exn(e);
    }
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> ...)`
static void check_import(obj e) {
    obj rib, spec;

    for (rib = Mcdr(e); !Mnullp(rib); rib = Mcdr(rib)) {
        spec = Mcar(rib);
        check_import_spec(e, spec);
    }

    if (!Mnullp(rib))
        bad_syntax_exn(e);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> ...)`
static void check_export(obj e) {
    obj rib, spec;

    for (rib = Mcdr(e); !Mnullp(rib); rib = Mcdr(rib)) {
        spec = Mcar(rib);
        if (Msymbolp(spec)) {
            // do nothing
        } else if (Mlistp(spec)) {
            minim_error1("check_import()", "unimplemented", e);
        } else {
            minim_error1("check_import()", "unimplemented", e);
        }
    }

    if (!Mnullp(rib))
        bad_syntax_exn(e);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> ...)`
static void check_define_values(obj e) {
    obj rib, ids;

    rib = Mcdr(e);
    if (!Mconsp(rib))
        bad_syntax_exn(e);

    ids = Mcar(rib);
    while (Mconsp(ids)) {
        assert_identifier(e, Mcar(ids));
        ids = Mcdr(ids);
    }

    if (!Mnullp(ids))
        bad_syntax_exn(e);

    rib = Mcdr(rib);
    if (!Mconsp(rib) && !Mnullp(Mcdr(rib)))
        bad_syntax_exn(e);
}

// static void check_define_

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> ...)`
static void check_define(obj e) {
    obj rib, hd, id;

    rib = Mcdr(e);
    if (!Mconsp(rib))
        bad_syntax_exn(e);

    hd = Mcar(rib);
    if (Msymbolp(hd)) {
        // (define <id> <expr>)
        rib = Mcdr(rib);
        if (!Mconsp(rib) || !Mnullp(Mcdr(rib)))
            bad_syntax_exn(e);

        check_expr(Mcar(rib));
    } else if (Mconsp(hd)) {
        // (define (<id> . <formals>) <expr> ...)
loop:
        id = Mcar(hd);
        check_formals(e, Mcdr(hd));
        if (Msymbolp(id)) {
            // do nothing
        } else if (Mconsp(id)) {
            hd = id;
            goto loop;
        } else {
            bad_syntax_exn(e);
        }

        for (rib = Mcdr(rib); !Mnullp(rib); rib = Mcdr(rib))
            check_expr(Mcar(rib));      
    } else {
        bad_syntax_exn(e);
    }
}

void check_expr(obj e) {
    obj hd, it, it2;

    if (Mconsp(e)) {
        hd = Mcar(e);
        if (hd == Mlet_symbol) {
            check_let(e);
            if (Msymbolp(Mcadr(e))) {
                // named let
                e = Mcdr(e);
            }

            for (it = Mcadr(e); !Mnullp(it); it = Mcdr(it))
                check_expr(Mcadar(it));
            for (it = Mcddr(e); !Mnullp(it); it = Mcdr(it))
                check_expr(Mcar(it));
        } else if (hd == Mletrec_symbol) {
            check_letrec(e);
            for (it = Mcadr(e); !Mnullp(it); it = Mcdr(it))
                check_expr(Mcadar(it));
            for (it = Mcddr(e); !Mnullp(it); it = Mcdr(it))
                check_expr(Mcar(it));
        } else if (hd == Mlet_values_symbol || hd == Mletrec_values_symbol) {
            check_let_values(e);
            for (it = Mcadr(e); !Mnullp(it); it = Mcdr(it))
                check_expr(Mcadar(it));
            for (it = Mcddr(e); !Mnullp(it); it = Mcdr(it))
                check_expr(Mcar(it));
        } else if (hd == Mbegin_symbol) {
            check_begin(e);
            for (it = Mcdr(e); !Mnullp(it); it = Mcdr(it))
                check_expr(Mcar(it));
        } else if (hd == Mif_symbol) {
            check_3ary_syntax(e);
            check_expr(Mcadr(e));
            check_expr(Mcaddr(e));
            check_expr(Mcar(Mcdddr(e)));
        } else if (hd == Mwhen_symbol) {
            check_when(e);
            for (it = Mcdr(e); !Mnullp(it); it = Mcdr(it))
                check_expr(Mcar(it));
        } else if (hd == Munless_symbol) {
            check_when(e);
            for (it = Mcdr(e); !Mnullp(it); it = Mcdr(it))
                check_expr(Mcar(it));
        } else if (hd == Mcond_symbol) {
            check_cond(e);
            for (it = Mcdr(e); !Mnullp(it); it = Mcdr(it)) {
                check_expr(Mcaar(it));
                check_expr(Mcadar(it));
            }
        } else if (hd == Mand_symbol) {
            check_begin(e);
            for (it = Mcdr(e); !Mnullp(it); it = Mcdr(it))
                check_expr(Mcar(it));
        } else if (hd == Mor_symbol) {
            check_begin(e);
            for (it = Mcdr(e); !Mnullp(it); it = Mcdr(it))
                check_expr(Mcar(it));
        } else if (hd == Mlambda_symbol) {
            check_lambda(e);
            for (it = Mcddr(e); !Mnullp(it); it = Mcdr(it))
                check_expr(Mcar(it));
        } else if (hd == Mcase_lambda_symbol) {
            check_case_lambda(e);
            for (it = Mcdr(e); !Mnullp(it); it = Mcdr(it)) {
                for (it2 = Mcdar(it); !Mnullp(it2); it2 = Mcdr(it2))
                    check_expr(Mcar(it2));
            }
        } else if (hd == Msetb_symbol) {
            check_setb(e);
        } else if (hd == Mquote_symbol) {
            check_1ary_syntax(e);
        } else if (Mlistp(e)) {
            for (it = e; !Mnullp(it); it = Mcdr(it))
                check_expr(Mcar(it));
        } else {
            minim_error1(NULL, "malformed application", e);
        }
    } else if (Msymbolp(e) || Mimmediatep(e)) {
        // immediate or variable
        return;
    } else if (Mnullp(e)) {
        // empty application
        minim_error1(NULL, "empty application", e);
    } else {
        minim_error1(NULL, "bad syntax", e);
    }
}

void check_module(obj mod) {
    obj es, e, hd;

    for (es = Mmodule_body(mod); !Mnullp(es); es = Mcdr(es)) {
        e = Mcar(es);
        if (Mconsp(e)) {
            hd = Mcar(e);
            if (hd == Mimport_symbol) {
                check_import(e);
            } else if (hd == Mexport_symbol) {
                check_export(e);
            } else if (hd == Mdefine_values_symbol) {
                check_define_values(e);
            } else if (hd == Mdefine_symbol) {
                check_define(e);
            } else {
                check_expr(e);
            }
        } else {
            check_expr(e);
        }
    }
}
