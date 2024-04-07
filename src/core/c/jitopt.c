// jitopt.c: JIT optimization

#include "../minim.h"

// L0 optimization: normalization
// - `letrec-values` expressions to `let-values` with `set!`
// - `let-values` expressions to nested `call-with-values`
// - `case-lambda` expression with one clause are converted to `lambda` expressions

// L0 optimization for sequences
static mobj jit_opt_L0_seq(mobj exprs) {
    if (minim_nullp(exprs)) {
        return minim_null;   
    } else {
        mobj hd, tl;

        hd = tl = Mcons(jit_opt_L0(minim_car(exprs)), minim_null);
        for (exprs = minim_cdr(exprs); !minim_nullp(exprs); exprs = minim_cdr(exprs)) {
            minim_cdr(tl) = Mcons(jit_opt_L0(minim_car(exprs)), minim_null);
            tl = minim_cdr(tl);
        }

        return hd;
    }
}

static mobj make_tmp_ids(mobj ids) {
    if (minim_nullp(ids)) {
        return minim_null;
    } else {
        mobj hd, tl;

        hd = tl = Mcons(Mgensym("t"), minim_null);
        for (ids = minim_cdr(ids); !minim_nullp(ids); ids = minim_cdr(ids)) {
            minim_cdr(tl) = Mcons(Mgensym("t"), minim_null);
            tl = minim_cdr(tl);
        }

        return hd;
    }
}

static mobj let_values_ids(mobj expr) {
    mobj bindings, binds, bind_it, hd, tl;

    hd = tl = minim_null;
    bindings = minim_cadr(expr);
    while (!minim_nullp(bindings)) {
        binds = minim_car(bindings);
        for (bind_it = minim_car(binds); !minim_nullp(bind_it); bind_it = minim_cdr(bind_it)) {
            if (minim_nullp(hd)) {
                hd = tl = Mcons(minim_car(bind_it), minim_null);
            } else {
                minim_cdr(tl) = Mcons(minim_car(bind_it), minim_null);
                tl = minim_cdr(tl);
            }
        }

        bindings = minim_cdr(bindings);
    }

    return hd;
}

static mobj letrec_set_expr(mobj binds) {
    mobj ids, tmps, hd, tl;
    
    ids = minim_car(binds);
    tmps = make_tmp_ids(ids);
    hd = tl = Mcons(let_values_symbol, minim_null);
    minim_cdr(tl) = Mcons(Mlist1(Mlist2(tmps, minim_cadr(binds))), minim_null);
    tl = minim_cdr(tl);
    if (minim_nullp(ids)) {
        minim_cdr(tl) = Mlist1(Mlist1(values_symbol));
    } else {
        while (!minim_nullp(ids)) {
            minim_cdr(tl) = Mcons(Mlist3(setb_symbol, minim_car(ids), minim_car(tmps)), minim_null);
            tl = minim_cdr(tl);
            ids = minim_cdr(ids);
            tmps = minim_cdr(tmps);
        }
    }

    return hd;
}

// L0 optimization for `letrec-values`
// `(letrec-values () body ...)`
//  => `(begin body ...)`
// `(letrec-values ([(id ...) expr] ...) body ...`
//  => `(let-values ([(id ...) <unbound>] ...)
//        (let-values ([(fresh ...) expr])
//          (set! id fresh)
//          ...)
//        body
//        ...)`
static mobj jit_opt_L0_letrec_values(mobj expr) {
    mobj let_ids, let_init, let_expr, let_it, bindings, binds;

    bindings = minim_cadr(expr);
    if (minim_nullp(bindings)) {
        let_expr = Mcons(begin_symbol, minim_cddr(expr));
    } else {
        // initial bindings
        let_ids = let_values_ids(expr);
        let_init = Mcons(values_symbol, make_list(list_length(let_ids), Mlist1(make_unbound_symbol)));
        let_it = Mlist1(Mlist1(Mlist2(let_ids, let_init)));
        let_expr = Mcons(let_values_symbol, let_it);

        // update bindings
        while (!minim_nullp(bindings)) {
            binds = minim_car(bindings);
            minim_cdr(let_it) = Mcons(letrec_set_expr(binds), minim_null);
            let_it = minim_cdr(let_it);
            bindings = minim_cdr(bindings);
        }

        // body
        minim_cdr(let_it) = minim_cddr(expr);   
    }

    return jit_opt_L0(let_expr);
}

// L0 optimization for `let-values`
static mobj jit_opt_L0_let_values2(mobj bindings, mobj ids, mobj tmps, mobj body) {
    if (minim_nullp(bindings)) {
        return Mcons(Mcons(lambda_symbol, Mcons(ids, body)), tmps);
    } else {
        mobj bind = minim_car(bindings);
        mobj tmps2 = make_tmp_ids(minim_car(bind));
        ids = list_append2(minim_car(bind), ids);
        tmps = list_append2(tmps2, tmps);
        body = jit_opt_L0_let_values2(minim_cdr(bindings), ids, tmps, body);
        return Mlist3(
            call_with_values_symbol,
            Mlist3(lambda_symbol, minim_null, minim_cadr(bind)),
            Mlist3(lambda_symbol, tmps2, body)
        );
    }
}

// L0 optimization for `let-values`
// For each binding `[(id ...) expr]`
//  - create fresh identifiers
//  - capture the result of `expr` with `call-with-values`
// The body is transformed into `call-with-values` using the actual identifiers
static mobj jit_opt_L0_let_values(mobj expr) {
    mobj bindings, body;

    bindings = minim_cadr(expr);
    body = minim_cddr(expr);
    if (minim_nullp(bindings)) {
        expr = Mcons(begin_symbol, body);
    } else if (minim_nullp(minim_cdr(bindings))) {
        expr = Mlist3(
            call_with_values_symbol,
            Mlist3(lambda_symbol, minim_null, minim_cadr(minim_car(bindings))),
            Mcons(lambda_symbol, Mcons(minim_caar(bindings), body))
        );
    } else {
        expr = jit_opt_L0_let_values2(bindings, minim_null, minim_null, body);
    }

    return jit_opt_L0(expr);
}

// L0 optimization for a single `case-lambda` clause
static mobj jit_opt_L0_case_lambda_clause(mobj clause) {
    return Mcons(minim_car(clause), jit_opt_L0_seq(minim_cdr(clause)));
}

// L0 optimization for `case-lambda`
static mobj jit_opt_L0_case_lambda(mobj expr) {
    mobj clauses, hd, tl;
    clauses = minim_cdr(expr);
    if (minim_nullp(clauses)) {
        return expr;
    } else if (minim_nullp(minim_cdr(clauses))) {
        return jit_opt_L0(Mcons(lambda_symbol, minim_car(clauses)));
    } else {
        hd = tl = Mcons(jit_opt_L0_case_lambda_clause(minim_car(clauses)), minim_null);
        for (clauses = minim_cdr(clauses); !minim_nullp(clauses); clauses = minim_cdr(clauses)) {
            minim_cdr(tl) = Mcons(jit_opt_L0_case_lambda_clause(minim_car(clauses)), minim_null);
            tl = minim_cdr(tl);
        }
        
        return Mcons(case_lambda_symbol, hd);
    }
}

// L0 optimization for `application`
static mobj jit_opt_L0_app(mobj expr) {
    mobj hd, tl;

    hd = tl = Mcons(jit_opt_L0(minim_car(expr)), minim_null);
    for (expr = minim_cdr(expr); !minim_nullp(expr); expr = minim_cdr(expr)) {
        minim_cdr(tl) = Mcons(jit_opt_L0(minim_car(expr)), minim_null);
        tl = minim_cdr(tl);
    }

    return hd;
}

// Perform L0 optimization
mobj jit_opt_L0(mobj expr) {
    if (minim_consp(expr)) {
        // special form or application
        mobj head = minim_car(expr);
        if (minim_symbolp(head)) {
            // special forms
            if (head == define_values_symbol) {
                // define-values form
                return Mlist3(head, minim_cadr(expr), jit_opt_L0(minim_car(minim_cddr(expr))));
            } else if (head == letrec_values_symbol) {
                // letrec-values form
                return jit_opt_L0_letrec_values(expr);
            } else if (head == let_values_symbol) {
                // let-values form
                return jit_opt_L0_let_values(expr);
            } else if (head == setb_symbol) {
                // set! form
                return Mlist3(head, minim_cadr(expr), jit_opt_L0(minim_car(minim_cddr(expr))));
            } else if (head == lambda_symbol) {
                // lambda form
                return Mcons(head, Mcons(minim_cadr(expr), jit_opt_L0_seq(minim_cddr(expr))));
            } else if (head == case_lambda_symbol) {
                // case-lambda form
                return jit_opt_L0_case_lambda(expr);
            } else if (head == begin_symbol) {
                // begin form
                return Mcons(begin_symbol, jit_opt_L0_seq(minim_cdr(expr)));
            } else if (head == if_symbol) {
                // if form
                return Mlist4(
                    if_symbol,
                    jit_opt_L0(minim_cadr(expr)),
                    jit_opt_L0(minim_car(minim_cddr(expr))),
                    jit_opt_L0(minim_cadr(minim_cddr(expr)))
                );
            } else if (head == quote_symbol) {
                // quote form
                return expr;
            } else if (head == quote_syntax_symbol) {
                // quote-syntax form
                return expr;
            }
        }

        // application
        return jit_opt_L0_app(expr);
    } else {
        return expr;
    }
}

// L1 optimization: `values` and `call-with-values`
// - `values` rewritten to `mv-values`
// - `call-with-values` rewritten to `mv-call`
// - optimizations of `mv-call` expressions

// L1 optimization for sequences
static mobj jit_opt_L1_seq(mobj exprs) {
    if (minim_nullp(exprs)) {
        return minim_null;   
    } else {
        mobj hd, tl;

        hd = tl = Mcons(jit_opt_L1(minim_car(exprs)), minim_null);
        for (exprs = minim_cdr(exprs); !minim_nullp(exprs); exprs = minim_cdr(exprs)) {
            minim_cdr(tl) = Mcons(jit_opt_L1(minim_car(exprs)), minim_null);
            tl = minim_cdr(tl);
        }

        return hd;
    }
}

// L1 optimization for a single `case-lambda` clause
static mobj jit_opt_L1_case_lambda_clause(mobj clause) {
    return Mcons(minim_car(clause), jit_opt_L1_seq(minim_cdr(clause)));
}

// L1 optimization for `case-lambda`
static mobj jit_opt_L1_case_lambda(mobj expr) {
    mobj clauses, hd, tl;
    clauses = minim_cdr(expr);
    if (minim_nullp(clauses)) {
        return expr;
    } else {
        hd = tl = Mcons(jit_opt_L1_case_lambda_clause(minim_car(clauses)), minim_null);
        for (clauses = minim_cdr(clauses); !minim_nullp(clauses); clauses = minim_cdr(clauses)) {
            minim_cdr(tl) = Mcons(jit_opt_L1_case_lambda_clause(minim_car(clauses)), minim_null);
            tl = minim_cdr(tl);
        }
        
        return Mcons(minim_car(expr), hd);
    }
}

// L1 optimization for `application`
static mobj jit_opt_L1_app(mobj expr) {
    mobj hd, tl;

    hd = tl = Mcons(jit_opt_L1(minim_car(expr)), minim_null);
    for (expr = minim_cdr(expr); !minim_nullp(expr); expr = minim_cdr(expr)) {
        minim_cdr(tl) = Mcons(jit_opt_L1(minim_car(expr)), minim_null);
        tl = minim_cdr(tl);
    }

    return hd;
}

// L1 optimization for `values`
static mobj jit_opt_L1_values(mobj expr) {
    mobj vals, hd, tl;
    
    vals = minim_cdr(expr);
    if (minim_nullp(vals)) {
        return Mlist1(mvvalues_symbol);
    } else if (minim_nullp(minim_cdr(vals))) {
        return jit_opt_L1(minim_car(vals));
    } else {
        hd = tl = Mcons(mvvalues_symbol, minim_null);
        for (; !minim_nullp(vals); vals = minim_cdr(vals)) {
            minim_cdr(tl) = Mcons(jit_opt_L1(minim_car(vals)), minim_null);
            tl = minim_cdr(tl);
        }

        return hd;
    }
}

// L1 optimization for `values`
static mobj jit_opt_L1_mvcall(mobj expr) {
    mobj e, consumer, producer, body, hd, tl;

    e = minim_cadr(expr);
    consumer = minim_car(minim_cddr(expr));
    if (minim_consp(e)) {
        if (minim_consp(minim_car(e)) && minim_caar(e) == lambda_symbol) {
            producer = minim_car(e);
            if (minim_nullp(minim_cadr(producer))) {
                // producer is `(lambda () e ...)`
                body = minim_cddr(producer);
                if (minim_nullp(minim_cdr(body))) {
                    // (mv-call ((lambda () e)) consumer) => (mv-call e consumer)
                    return jit_opt_L1(Mlist3(mvcall_symbol, minim_car(body), consumer));
                } else {
                    // (mv-call ((lambda () e ...) consumer => (mv-call (begin e ...) consumer)
                    return jit_opt_L1(Mlist3(mvcall_symbol, Mcons(begin_symbol, body), consumer)); 
                }
            }
        } else if (minim_car(e) == mvvalues_symbol) {
            // (mv-call (mv-values e ...) consumer) => (consumer e ...)
            return jit_opt_L1(Mcons(consumer, minim_cdr(e)));
        } else if (minim_car(e) == begin_symbol) {
            // (mv-call (begin e ... e_last) consumer)
            // => (begin e ... (mv-call e_last consumer))
            hd = tl = Mlist1(begin_symbol);
            for (e = minim_cdr(e); !minim_nullp(minim_cdr(e)); e = minim_cdr(e)) {
                minim_cdr(tl) = Mlist1(minim_car(e));
                tl = minim_cdr(tl);
            }

            minim_cdr(tl) = Mlist1(Mlist3(mvcall_symbol, minim_car(e), consumer));
            return jit_opt_L1(hd);
        } else if (minim_car(e) == if_symbol) {
            // (mv-call (if cond ift iff) consumer)
            // => (if cond (mv-call ift consumer) (mv-call iff consumer)
            return jit_opt_L1(Mlist4(
                if_symbol,
                minim_cadr(e),
                Mlist3(mvcall_symbol, minim_car(minim_cddr(e)), consumer),
                Mlist3(mvcall_symbol, minim_cadr(minim_cddr(e)), consumer)
            ));
        } else if (minim_car(e) == lambda_symbol ||
            minim_car(e) == quote_symbol ||
            minim_car(e) == quote_syntax_symbol) {
            // (mv-call e_single consumer) => (consumer e_single)
            return jit_opt_L1(Mlist2(consumer, e));
        }

        if (minim_consp(consumer) &&
            minim_car(consumer) == lambda_symbol &&
            minim_consp(minim_cadr(consumer))) {
            // (mv-call e (lambda (id ...) body) => (mv-let e (id ...) body)
            mobj body = minim_cddr(consumer);
            return jit_opt_L1(Mlist4(
                mvlet_symbol,
                e,
                minim_cadr(consumer),
                minim_nullp(minim_cdr(body)) ? minim_car(body) : Mcons(begin_symbol, body)
            ));
        }

        return expr;
    } else {
        // (mv-call e_lit consumer) => (consumer e_lit)
        return jit_opt_L1(Mlist2(consumer, e));
    }
}

// Perform L1 optimization
mobj jit_opt_L1(mobj expr) {
    if (minim_consp(expr)) {
        // special form or application
        mobj head = minim_car(expr);
        if (minim_symbolp(head)) {
            // special forms
            if (head == define_values_symbol) {
                // define-values form
                return Mlist3(head, minim_cadr(expr), jit_opt_L1(minim_car(minim_cddr(expr))));
            } else if (head == setb_symbol) {
                // set! form
                return Mlist3(head, minim_cadr(expr), jit_opt_L1(minim_car(minim_cddr(expr))));
            } else if (head == lambda_symbol) {
                // lambda form
                return Mcons(head, Mcons(minim_cadr(expr), jit_opt_L1_seq(minim_cddr(expr))));
            } else if (head == case_lambda_symbol) {
                // case-lambda form
                return jit_opt_L1_case_lambda(expr);
            } else if (head == begin_symbol) {
                // begin form
                return Mcons(begin_symbol, jit_opt_L1_seq(minim_cdr(expr)));
            } else if (head == if_symbol) {
                // if form
                return Mlist4(
                    if_symbol,
                    jit_opt_L1(minim_cadr(expr)),
                    jit_opt_L1(minim_car(minim_cddr(expr))),
                    jit_opt_L1(minim_cadr(minim_cddr(expr)))
                );
            } else if (head == quote_symbol) {
                // quote form
                return expr;
            } else if (head == quote_syntax_symbol) {
                // quote-syntax form
                return expr;
            } else if (head == values_symbol) {
                // values form
                return jit_opt_L1_values(expr);
            } else if (head == call_with_values_symbol) {
                // call-with-values form
                return jit_opt_L1_mvcall(
                    Mlist3(
                        mvcall_symbol,
                        Mlist1(jit_opt_L1(minim_cadr(expr))),
                        jit_opt_L1(minim_car(minim_cddr(expr)))
                ));
            } else if (head == mvcall_symbol) {
                // mv-call form
                return jit_opt_L1_mvcall(expr);
            } else if (head == mvlet_symbol) {
                // mv-let
                return Mlist4(
                    mvlet_symbol,
                    jit_opt_L1(minim_cadr(expr)),
                    minim_car(minim_cddr(expr)),
                    jit_opt_L1(minim_cadr(minim_cddr(expr)))
                );
            }
        }

        // application
        return jit_opt_L1_app(expr);
    } else {
        return expr;
    }
}

// L2 optimization: closure elimination
//  - `((lambda (ids ...) body ...) args ...)`
//     => `(mv-let (values args ...) (ids ...) (begin body ...)`

// L2 optimization for sequences
static mobj jit_opt_L2_seq(mobj exprs) {
    if (minim_nullp(exprs)) {
        return minim_null;   
    } else {
        mobj hd, tl;

        hd = tl = Mcons(jit_opt_L2(minim_car(exprs)), minim_null);
        for (exprs = minim_cdr(exprs); !minim_nullp(exprs); exprs = minim_cdr(exprs)) {
            minim_cdr(tl) = Mcons(jit_opt_L2(minim_car(exprs)), minim_null);
            tl = minim_cdr(tl);
        }

        return hd;
    }
}

// L2 optimization for a single `case-lambda` clause
static mobj jit_opt_L2_case_lambda_clause(mobj clause) {
    return Mcons(minim_car(clause), jit_opt_L2_seq(minim_cdr(clause)));
}

// L2 optimization for `case-lambda`
static mobj jit_opt_L2_case_lambda(mobj expr) {
    mobj clauses, hd, tl;
    clauses = minim_cdr(expr);
    if (minim_nullp(clauses)) {
        return expr;
    } else {
        hd = tl = Mcons(jit_opt_L2_case_lambda_clause(minim_car(clauses)), minim_null);
        for (clauses = minim_cdr(clauses); !minim_nullp(clauses); clauses = minim_cdr(clauses)) {
            minim_cdr(tl) = Mcons(jit_opt_L2_case_lambda_clause(minim_car(clauses)), minim_null);
            tl = minim_cdr(tl);
        }
        
        return Mcons(minim_car(expr), hd);
    }
}

// L2 optimization for `application`
static mobj jit_opt_L2_app(mobj expr) {
    mobj hd, tl, args, body;

    hd = minim_car(expr);
    if (minim_consp(hd) && minim_car(hd) == lambda_symbol && minim_listp(minim_cadr(hd))) {
        // `((lambda (ids ...) body ...) args ...)`
        // => `(mv-let (values args ...) (ids ...) (begin body ...)`
        args = minim_cdr(expr);
        body = minim_cddr(hd);
        if (minim_nullp(args)) {
            expr = minim_nullp(minim_cdr(body))
                ? minim_car(body)
                : Mcons(begin_symbol, body);
        } else if (minim_nullp(minim_cdr(args))) {
            expr = Mlist4(
                mvlet_symbol,
                minim_car(args),
                minim_cadr(hd),
                minim_nullp(minim_cdr(body))
                    ? minim_car(body)
                    : Mcons(begin_symbol, body)
            );
        } else {
            expr = Mlist4(
                mvlet_symbol,
                Mcons(values_symbol, args),
                minim_cadr(hd),
                minim_nullp(minim_cdr(body))
                    ? minim_car(body)
                    : Mcons(begin_symbol, body)
            );
        }

        return jit_opt_L2(expr);
    } else {
        hd = tl = Mcons(jit_opt_L2(minim_car(expr)), minim_null);
        for (expr = minim_cdr(expr); !minim_nullp(expr); expr = minim_cdr(expr)) {
            minim_cdr(tl) = Mcons(jit_opt_L2(minim_car(expr)), minim_null);
            tl = minim_cdr(tl);
        }
    }

    return hd;
}

// Perform L2 optimization
mobj jit_opt_L2(mobj expr) {
    if (minim_consp(expr)) {
        // special form or application
        mobj head = minim_car(expr);
        if (minim_symbolp(head)) {
            // special forms
            if (head == define_values_symbol) {
                // define-values form
                return Mlist3(head, minim_cadr(expr), jit_opt_L2(minim_car(minim_cddr(expr))));
            } else if (head == setb_symbol) {
                // set! form
                return Mlist3(head, minim_cadr(expr), jit_opt_L2(minim_car(minim_cddr(expr))));
            } else if (head == lambda_symbol) {
                // lambda form
                return Mcons(head, Mcons(minim_cadr(expr), jit_opt_L2_seq(minim_cddr(expr))));
            } else if (head == case_lambda_symbol) {
                // case-lambda form
                return jit_opt_L2_case_lambda(expr);
            } else if (head == mvcall_symbol) {
                // mv-call form
                return Mlist3(
                    head,
                    jit_opt_L2(minim_cadr(expr)),
                    jit_opt_L2(minim_car(minim_cddr(expr)))
                );
            } else if (head == mvlet_symbol) {
                // mv-let form
               return Mlist4(
                    mvlet_symbol,
                    jit_opt_L2(minim_cadr(expr)),
                    minim_car(minim_cddr(expr)),
                    jit_opt_L2(minim_cadr(minim_cddr(expr)))
                );
            } else if (head == mvvalues_symbol) {
                // mv-values form
                return Mcons(head, jit_opt_L2_seq(minim_cdr(expr)));
            } else if (head == begin_symbol) {
                // begin form
                return Mcons(head, jit_opt_L2_seq(minim_cdr(expr)));
            } else if (head == if_symbol) {
                // if form
                return Mlist4(
                    if_symbol,
                    jit_opt_L2(minim_cadr(expr)),
                    jit_opt_L2(minim_car(minim_cddr(expr))),
                    jit_opt_L2(minim_cadr(minim_cddr(expr)))
                );
            } else if (head == quote_symbol) {
                // quote form
                return expr;
            } else if (head == quote_syntax_symbol) {
                // quote-syntax form
                return expr;
            }
        }

        // application
        return jit_opt_L2_app(expr);
    } else {
        return expr;
    }
}
