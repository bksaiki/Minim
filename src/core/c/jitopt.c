// jitopt.c: JIT optimization

#include "../minim.h"

// L0 optimization: normalization
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

        hd = tl = Mcons(Msymbol(minim_symbol(minim_car(ids))), minim_null);
        for (ids = minim_cdr(ids); !minim_nullp(ids); ids = minim_cdr(ids)) {
            minim_cdr(tl) = Mcons(Msymbol(minim_symbol(minim_car(ids))), minim_null);
            tl = minim_cdr(tl);
        }

        return hd;
    }
}

// L0 optimization for `letrec-values`
static mobj jit_opt_L0_letrec_values(mobj expr) {
    mobj bindings, binding, body, hd, tl;
    
    bindings = minim_cadr(expr);
    body = jit_opt_L0_seq(minim_cddr(expr));
    if (minim_nullp(bindings)) {
        return Mcons(letrec_values_symbol, Mcons(minim_null, body));
    } else {
        binding = minim_car(bindings);
        hd = tl = Mcons(Mlist2(minim_car(binding), jit_opt_L0(minim_cadr(binding))), minim_null);
        for (bindings = minim_cdr(bindings); !minim_nullp(bindings); bindings = minim_cdr(bindings)) {
            binding = minim_car(bindings);
            minim_cdr(tl) = Mcons(Mlist2(minim_car(binding), jit_opt_L0(minim_cadr(binding))), minim_null);
            tl = minim_cdr(tl);
        }

        return Mcons(letrec_values_symbol, Mcons(hd, body));
    }
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
static mobj jit_opt_L0_let_values(mobj expr) {
    expr = jit_opt_L0_let_values2(minim_cadr(expr), minim_null, minim_null, minim_cddr(expr));
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

// L1 optimization for `letrec-values`
static mobj jit_opt_L1_letrec_values(mobj expr) {
    mobj bindings, binding, body, hd, tl;
    
    bindings = minim_cadr(expr);
    body = jit_opt_L1_seq(minim_cddr(expr));
    if (minim_nullp(bindings)) {
        return Mcons(minim_car(expr), Mcons(minim_null, body));
    } else {
        binding = minim_car(bindings);
        hd = tl = Mcons(Mlist2(minim_car(binding), jit_opt_L1(minim_cadr(binding))), minim_null);
        for (bindings = minim_cdr(bindings); !minim_nullp(bindings); bindings = minim_cdr(bindings)) {
            binding = minim_car(bindings);
            minim_cdr(tl) = Mcons(Mlist2(minim_car(binding), jit_opt_L1(minim_cadr(binding))), minim_null);
            tl = minim_cdr(tl);
        }

        return Mcons(minim_car(expr), Mcons(hd, body));
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
    mobj e, consumer, head;

    e = minim_cadr(expr);
    consumer = minim_car(minim_cddr(expr));
    if (minim_consp(e)) {
        head = minim_car(e);
        if (minim_consp(head) && minim_car(head) == lambda_symbol) {
            // can assume `(mv-call ((lambda () e ...))
            mobj producer = head;
            if (minim_nullp(minim_cdr(minim_cddr(producer)))) {
                // (mv-call ((lambda () e)) consumer) => (mv-call e consumer)
                return jit_opt_L1(Mlist3(mvcall_symbol, minim_car(minim_cddr(producer)), consumer));
            } else {
                // (mv-call ((lambda () e ...) consumer => (mv-call (begin e ...) consumer)
                return jit_opt_L1(Mlist3(mvcall_symbol, Mcons(begin_symbol, minim_cddr(producer)), consumer)); 
            }
        } else if (head == mvvalues_symbol) {
            // (mv-call (mv-values e ...) consumer) => (consumer e ...)
            return jit_opt_L1(Mcons(consumer, minim_cdr(e)));
        } else if (minim_consp(consumer) && minim_car(consumer) == lambda_symbol) {
            return jit_opt_L1(
                Mcons(mvlet_symbol,
                Mcons(e,
                Mcons(minim_cadr(consumer),
                minim_cddr(consumer))))
            );
        }
    }
    
    // (mv-call e_s consumer) => (consumer e_s)
    return jit_opt_L1(Mlist2(consumer, e));
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
            } else if (head == letrec_values_symbol) {
                // letrec-values form
                return jit_opt_L1_letrec_values(expr);
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
                // mvcall form
                return jit_opt_L1_mvcall(expr);
            }
        }

        // application
        return jit_opt_L1_app(expr);
    } else {
        return expr;
    }
}
