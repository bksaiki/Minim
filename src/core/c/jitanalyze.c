// jitanalyze.c: JIT analysis

#include "../minim.h"

static mobj free_vars(mobj expr, mobj table);
static mobj bound_vars(mobj expr, mobj table);

//
//  Free variable analysis
//

static mobj formals_to_ids(mobj formals) {
    mobj ids = minim_null;
    for (; minim_consp(formals); formals = minim_cdr(formals)) {
        ids = Mcons(minim_car(formals), ids);
    }

    if (!minim_nullp(formals))
        ids = Mcons(formals, ids);

    return list_reverse(ids);
}

static mobj merge_free_vars(mobj xs1, mobj xs2) {
    mobj fvs = xs2;

    for (; !minim_nullp(xs1); xs1 = minim_cdr(xs1)) {
        if (minim_falsep(memq(xs2, minim_car(xs1))))
            fvs = Mcons(minim_car(xs1), fvs);
    }

    return fvs;
}

static mobj remove_free_vars(mobj ks, mobj fvs) {
    mobj fvs2;
    
    fvs2 = minim_null;
    for (; !minim_nullp(fvs); fvs = minim_cdr(fvs)) {
        if (minim_falsep(memq(ks, minim_car(fvs))))
            fvs2 = Mcons(minim_car(fvs), fvs2);
    }

    return fvs2;
}

static mobj define_values_free_vars(mobj e, mobj table) {
    return remove_free_vars(
        minim_cadr(e),
        free_vars(minim_car(minim_cddr(e)), table)
    );
}

static mobj lambda_free_vars(mobj e, mobj table) {
    mobj fvs, ids, seq;
    
    ids = formals_to_ids(minim_cadr(e));
    seq = Mcons(begin_symbol, minim_cddr(e));
    fvs = remove_free_vars(ids, free_vars(seq, table));

    if (!minim_nullp(fvs))
        minim_unbox(table) = Mcons(Mcons(e, fvs), minim_unbox(table));
    return fvs;
}

static mobj case_lambda_free_vars(mobj e, mobj table) {
    mobj fvs, fvs2, ids, seq;
    
    fvs = minim_null;
    for (e = minim_cdr(e); !minim_nullp(e); e = minim_cdr(e)) {
        ids = formals_to_ids(minim_caar(e));
        seq = Mcons(begin_symbol, minim_cdar(e));
        fvs2 = remove_free_vars(ids, free_vars(seq, table));
        fvs = merge_free_vars(fvs2, fvs);
    }

    if (!minim_nullp(fvs))
        minim_unbox(table) = Mcons(Mcons(e, fvs), minim_unbox(table));
    return fvs;
}

static mobj mvlet_free_vars(mobj e, mobj table) {
    return merge_free_vars(
        remove_free_vars(
            minim_car(minim_cddr(e)),
            free_vars(minim_cadr(minim_cddr(e)), table)
        ),
        free_vars(minim_cadr(e), table)
    );
}

static mobj begin_free_vars(mobj e, mobj table) {
    mobj fvs;
    
    fvs = minim_null;
    for (e = minim_cdr(e); !minim_nullp(e); e = minim_cdr(e))
        fvs = merge_free_vars(free_vars(minim_car(e), table), fvs);
    return fvs;
}

static mobj app_free_vars(mobj e, mobj table) {
    mobj fvs = free_vars(minim_car(e), table);
    for (e = minim_cdr(e); !minim_nullp(e); e = minim_cdr(e))
        fvs = merge_free_vars(free_vars(minim_car(e), table), fvs);
    return fvs;
}

static mobj free_vars(mobj expr, mobj table) {
    if (minim_consp(expr)) {
        // special form or application
        mobj head = minim_car(expr);
        if (minim_symbolp(head)) {
            // special forms
            if (head == define_values_symbol) {
                // define-values form
                return define_values_free_vars(expr, table);
            } else if (head == setb_symbol) {
                // set! form
                return begin_free_vars(expr, table);
            } else if (head == lambda_symbol) {
                // lambda form
                return lambda_free_vars(expr, table);
            } else if (head == case_lambda_symbol) {
                // case-lambda form
                return case_lambda_free_vars(expr, table);
            } else if (head == mvcall_symbol) {
                // mv-call form
                return begin_free_vars(expr, table);
            } else if (head == mvlet_symbol) {
                // mv-let form
                return mvlet_free_vars(expr, table);
            } else if (head == mvvalues_symbol) {
                // mv-values form
                return begin_free_vars(expr, table);
            } else if (head == begin_symbol) {
                // begin form
                return begin_free_vars(expr, table);
            } else if (head == if_symbol) {
                // if form
                return begin_free_vars(expr, table);
            } else if (head == quote_symbol) {
                // quote form
                return minim_null;
            } else if (head == quote_syntax_symbol) {
                // quote-syntax form
                return minim_null;
            } else if (head == make_unbound_symbol) {
                // make-unbound form
                return minim_null;
            }
        }

        // application
        return app_free_vars(expr, table);
    } else if (minim_symbolp(expr)) {
        // symbol
        return Mlist1(expr);
    } else if (minim_boolp(expr)
        || minim_fixnump(expr)
        || minim_charp(expr)
        || minim_stringp(expr)
        || minim_boxp(expr)
        || minim_vectorp(expr)) {
        // self-evaluating
        return minim_null;
    } else {
        minim_error1("free_vars", "cannot analyze", expr);
    }
}

mobj jit_free_vars(mobj expr, mobj table) {
    return free_vars(expr, table);
}

//
//  Bound variable analysis
//

static mobj define_values_bound_vars(mobj e, mobj table) {
    return list_append2(
        minim_cadr(e),
        bound_vars(minim_car(minim_cddr(e)), table)
    );
}

static mobj lambda_bound_vars(mobj e, mobj table) {
    mobj ids = formals_to_ids(minim_cadr(e));
    mobj bound = bound_vars(minim_car(minim_cddr(e)), table);
    bound = list_append2(ids, bound);

    minim_unbox(table) = Mcons(Mcons(e, Mlist1(bound)), minim_unbox(table));
    return minim_null;
}

static mobj case_lambda_bound_vars(mobj e, mobj table) {
    mobj clause_bound = minim_null;
    for (mobj clauses = minim_cdr(e); !minim_nullp(clauses); clauses = minim_cdr(clauses)) {
        mobj ids = formals_to_ids(minim_caar(clauses));
        mobj bound = list_append2(ids, bound_vars(Mcons(begin_symbol, minim_cdar(clauses)), table));
        clause_bound = Mcons(list_append2(ids, bound), clause_bound);
    }

    minim_unbox(table) = Mcons(
        Mcons(e, list_reverse(clause_bound)),
        minim_unbox(table)
    );

    return minim_null;
}

static mobj mvlet_bound_vars(mobj e, mobj table) {
    return list_append2(
        bound_vars(minim_cadr(e), table),
        list_append2(
            minim_car(minim_cddr(e)),
            bound_vars(minim_cadr(minim_cddr(e)), table)
        )
    );
}

static mobj begin_bound_vars(mobj e, mobj table) {
    mobj bound = minim_null;
    for (e = minim_cdr(e); !minim_nullp(e); e = minim_cdr(e))
        bound = list_append2(bound, bound_vars(minim_car(e), table));
    return bound;
}

static mobj app_bound_vars(mobj e, mobj table) {
    mobj bound = bound_vars(minim_car(e), table);
    for (e = minim_cdr(e); !minim_nullp(e); e = minim_cdr(e))
        bound = list_append2(bound, bound_vars(minim_car(e), table));
    return bound;
}

static mobj bound_vars(mobj expr, mobj table) {
    if (minim_consp(expr)) {
        // special form or application
        mobj head = minim_car(expr);
        if (minim_symbolp(head)) {
            // special forms
            if (head == define_values_symbol) {
                // define-values form
                return define_values_bound_vars(expr, table);
            } else if (head == setb_symbol) {
                // set! form
                return begin_bound_vars(expr, table);
            } else if (head == lambda_symbol) {
                // lambda form
                return lambda_bound_vars(expr, table);
            } else if (head == case_lambda_symbol) {
                // case-lambda form
                return case_lambda_bound_vars(expr, table);
            } else if (head == mvcall_symbol) {
                // mv-call form
                return begin_bound_vars(expr, table);
            } else if (head == mvlet_symbol) {
                // mv-let form
                return mvlet_bound_vars(expr, table);
            } else if (head == mvvalues_symbol) {
                // mv-values form
                return begin_bound_vars(expr, table);
            } else if (head == begin_symbol) {
                // begin form
                return begin_bound_vars(expr, table);
            } else if (head == if_symbol) {
                // if form
                return begin_bound_vars(expr, table);
            } else if (head == quote_symbol) {
                // quote form
                return minim_null;
            } else if (head == quote_syntax_symbol) {
                // quote-syntax form
                return minim_null;
            } else if (head == make_unbound_symbol) {
                // make-unbound form
                return minim_null;
            }
        }

        // application
        return app_bound_vars(expr, table);
    } else if (minim_symbolp(expr)) {
        // symbol
        return minim_null;
    } else if (minim_boolp(expr)
        || minim_fixnump(expr)
        || minim_charp(expr)
        || minim_stringp(expr)
        || minim_boxp(expr)
        || minim_vectorp(expr)) {
        // self-evaluating
        return minim_null;
    } else {
        minim_error1("bound_vars", "cannot analyze", expr);
    }
}

mobj jit_bound_vars(mobj expr, mobj table) {
    return bound_vars(expr, table);
}
