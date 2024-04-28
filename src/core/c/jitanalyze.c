// jitanalyze.c: JIT analysis

#include "../minim.h"

static mobj formals_to_ids(mobj formals) {
    mobj ids = minim_null;
    for (; minim_consp(formals); formals = minim_cdr(formals)) {
        ids = Mcons(ids, minim_null);
    }

    if (!minim_nullp(formals))
        ids = Mcons(formals, minim_null);

    return list_reverse(ids);
}

static mobj merge_free_vars(mobj xs1, mobj xs2) {
    mobj fvs = xs2;

    for (; !minim_nullp(xs1); xs1 = minim_cdr(xs1)) {
        if (!memq(xs2, minim_car(xs1)))
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

static mobj define_values_free_vars(mobj e) {
    return remove_free_vars(
        minim_cadr(e),
        free_vars(minim_car(minim_cddr(e)))
    );
}

static mobj lambda_free_vars(mobj e) {
    mobj ids, seq;
    
    ids = formals_to_ids(minim_cadr(e));
    seq = Mcons(begin_symbol, minim_cddr(e));
    return remove_free_vars(ids, free_vars(seq));
}

static mobj case_lambda_free_vars(mobj e) {
    mobj fvs, ids, seq;
    
    fvs = minim_null;
    for (e = minim_cdr(e); !minim_nullp(e); e = minim_cdr(e)) {
        ids = formals_to_ids(minim_caar(e));
        seq = Mcons(begin_symbol, minim_cdar(e));
        fvs = merge_free_vars(remove_free_vars(ids, free_vars(seq)), fvs);
    }

    return fvs;
}

static mobj mvlet_free_vars(mobj e) {
    return merge_free_vars(
        remove_free_vars(
            minim_car(minim_cddr(e)),
            free_vars(minim_cadr(minim_cddr(e)))
        ),
        free_vars(minim_cadr(e))
    );
}

static mobj begin_free_vars(mobj e) {
    mobj fvs;
    
    fvs = minim_null;
    for (e = minim_cdr(e); !minim_nullp(e); e = minim_cdr(e))
        fvs = merge_free_vars(free_vars(minim_car(e)), fvs);
    return fvs;
}

static mobj app_free_vars(mobj e) {
    mobj fvs = free_vars(minim_car(e));
    for (e = minim_cdr(e); !minim_nullp(e); e = minim_cdr(e))
        fvs = merge_free_vars(free_vars(minim_car(e)), fvs);
    return fvs;
}

mobj free_vars(mobj expr) {
    if (minim_consp(expr)) {
        // special form or application
        mobj head = minim_car(expr);
        if (minim_symbolp(head)) {
            // special forms
            if (head == define_values_symbol) {
                // define-values form
                return define_values_free_vars(expr);
            } else if (head == setb_symbol) {
                // set! form
                return begin_free_vars(expr);
            } else if (head == lambda_symbol) {
                // lambda form
                return lambda_free_vars(expr);
            } else if (head == case_lambda_symbol) {
                // case-lambda form
                return case_lambda_free_vars(expr);
            } else if (head == mvcall_symbol) {
                // mv-call form
                return begin_free_vars(expr);
            } else if (head == mvlet_symbol) {
                // mv-let form
                return mvlet_free_vars(expr);
            } else if (head == mvvalues_symbol) {
                // mv-values form
                return begin_free_vars(expr);
            } else if (head == begin_symbol) {
                // begin form
                return begin_free_vars(expr);
            } else if (head == if_symbol) {
                // if form
                return begin_free_vars(expr);
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
        return app_free_vars(expr);
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
