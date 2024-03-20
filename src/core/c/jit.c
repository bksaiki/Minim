// jit.c: compiler to JIT code

#include "../minim.h"

//
//  Utilities
//

static int get_lambda_arity(mobj e, long *req_arity) {
    *req_arity = 0;
    for (e = minim_cadr(e); minim_consp(e); e = minim_cdr(e))
        ++(*req_arity);
    return !minim_nullp(e);
}

// static void get_lambda_arity(mobj e, short *min_arity, short *max_arity) {
//     int argc = 0;
//     for (e = minim_cadr(e); minim_consp(e); e = minim_cdr(e)) {
//         ++argc;
//     }

//     if (minim_nullp(e)) {
//         *min_arity = argc;
//         *max_arity = argc;
//     } else {
//         *min_arity = argc;
//         *max_arity = ARG_MAX;
//     }
// }

//
//  Compiler env
//

#define cenv_length     2
#define cenv_labels(c)  (minim_vector_ref(c, 0))
#define cenv_tmpls(c)   (minim_vector_ref(c, 1))

static mobj make_cenv() {
    mobj cenv = Mvector(cenv_length, NULL);
    cenv_labels(cenv) = Mbox(minim_null);
    cenv_tmpls(cenv) = Mbox(minim_null);
    return cenv;
}

static mobj extend_cenv(mobj cenv) {
    mobj cenv2 = Mvector(cenv_length, NULL);
    cenv_labels(cenv2) = cenv_labels(cenv);
    cenv_tmpls(cenv2) = cenv_tmpls(cenv);
    return cenv2;
}

static mobj cenv_make_label(mobj cenv) {
    mobj label_box, label;
    size_t num_labels, len;
    
    // unbox list of labels
    label_box = cenv_labels(cenv);
    num_labels = list_length(minim_unbox(label_box));

    // create new label
    len = snprintf(NULL, 0, "%ld", num_labels);
    label = Mstring2(len + 1, 0);
    minim_string(label)[0] = 'L';
    snprintf(&minim_string(label)[1], len + 1, "%ld", num_labels);
    
    // update list
    minim_unbox(label_box) = Mcons(label, minim_unbox(label_box));
    return label;
}

static mobj cenv_template_add(mobj cenv, mobj ins) {
    mobj tmpl_box;
    size_t num_tmpls;

    // unbox list of templates
    tmpl_box = cenv_tmpls(cenv);
    num_tmpls = list_length(minim_unbox(tmpl_box));

    // update list
    minim_unbox(tmpl_box) = Mcons(ins, minim_unbox(tmpl_box));
    return Mfixnum(num_tmpls);
}

//
//  Compiler
//

static mobj compile_expr(mobj expr, mobj env, int tailp);

static mobj compile_define_values(mobj expr, mobj env) {
    mobj ins = compile_expr(minim_car(minim_cddr(expr)), env, 0);
    fprintf(stderr, "compile_expr: define-values unimplemented\n");
    fprintf(stderr, " at: ");
    write_object(stderr, expr);
    write_object(stderr, ins);
    fprintf(stderr, "\n");
    minim_shutdown(1);
}

static mobj compile_lambda(mobj expr, mobj env) {
    mobj args, body, ins, idx;
    long i, req_arity;
    int restp;
    
    // arity check
    restp = get_lambda_arity(expr, &req_arity);
    if (restp) {
        ins = Mlist1(Mlist2(check_arity_symbol, Mcons(Mfixnum(req_arity), minim_false)));
    } else {
        ins = Mlist1(Mlist2(check_arity_symbol, Mfixnum(req_arity)));
    }

    // bind arguments
    args = minim_cadr(expr);
    for (i = 0; i < req_arity; i++) {
        list_set_tail(ins, Mlist2(
            Mlist2(get_arg_symbol, Mfixnum(i)),
            Mlist2(bind_symbol, minim_car(args))
        ));

        args = minim_cdr(args);
    }

    // bind rest argument
    if (restp) {
        list_set_tail(ins, Mlist2(
            Mlist2(do_rest_symbol, Mfixnum(req_arity)),
            Mlist2(bind_symbol, args)
        )); 
    }

    // compile the body
    body = Mcons(begin_symbol, minim_cddr(expr));
    list_set_tail(ins, compile_expr(body, extend_cenv(env), 1));

    // register JIT block
    idx = cenv_template_add(env, ins);

    write_object(stderr, idx);
    fprintf(stderr, ": ");
    write_object(stderr, ins);
    fprintf(stderr, "\n");

    // instruction
    return Mlist1(Mlist2(make_closure_symbol, idx));
}

static mobj compile_begin(mobj expr, mobj env, int tailp) {
    mobj exprs = minim_cdr(expr);
    if (minim_nullp(exprs)) {
        // empty body
        return Mlist2(literal_symbol, minim_void);
    } else {
        // non-empty body
        mobj ins, ins_sub;
        
        // all except last is not in tail position
        ins = minim_null;
        while (!minim_nullp(minim_cdr(exprs))) {
            ins_sub = compile_expr(minim_car(exprs), env, 0);
            ins = list_append2(ins, ins_sub);
            exprs = minim_cdr(exprs);
        }

        // last expression is in tail position
        ins_sub = compile_expr(minim_car(exprs), env, tailp);
        return list_append2(ins, ins_sub);
    }
}

static mobj compile_if(mobj expr, mobj env, int tailp) {
    mobj ins, cond_ins, ift_ins, iff_ins, label;

    // compile the parts
    cond_ins = compile_expr(minim_cadr(expr), env, 0);
    ift_ins = compile_expr(minim_car(minim_cddr(expr)), env, tailp);
    iff_ins = compile_expr(minim_cadr(minim_cddr(expr)), env, tailp);
    label = cenv_make_label(env);

    // compose the instructions together
    ins = cond_ins;
    list_set_tail(ins, Mlist1(Mlist2(branchf_symbol, label)));
    list_set_tail(ins, ift_ins);
    list_set_tail(ins, Mlist1(label));
    list_set_tail(ins, iff_ins);
    return ins;
}

static mobj compile_app(mobj expr, mobj env, int tailp) {
    mobj ins, label, it;

    // need to save a continuation if not in tail position
    if (tailp) {
        ins = minim_null;
    } else {
        label = cenv_make_label(env);
        ins = Mlist1(Mlist2(save_cc_symbol, label));
    }

    // compute procedure
    ins = list_append2(ins, compile_expr(minim_car(expr), env, 0));
    list_set_tail(ins, Mlist1(Mlist1(push_symbol)));

    // compute arguments
    for (it = minim_cdr(expr); !minim_nullp(it); it = minim_cdr(it)) {
        list_set_tail(ins, compile_expr(minim_car(it), env, 0));
        list_set_tail(ins, Mlist1(Mlist1(push_symbol)));
    }

    // apply procedure
    list_set_tail(ins, Mlist1(Mlist1(apply_symbol)));

    // need a label to jump to if not in tail position
    if (!tailp) {
        list_set_tail(ins, Mlist1(label));
    }

    return ins;
}

static mobj compile_expr(mobj expr, mobj env, int tailp) {
    if (minim_consp(expr)) {
        // special form or application
        mobj head = minim_car(expr);
        if (minim_symbolp(head)) {
            // possible special forms
            if (head == define_values_symbol) {
                // define-values form
                return compile_define_values(expr, env);
            } else if (head == lambda_symbol) {
                // lambda form
                return compile_lambda(expr, env);
            } else if (head == begin_symbol) {
                // begin form
                return compile_begin(expr, env, tailp);
            } else if (head == if_symbol) {
                // if form
                return compile_if(expr, env, tailp);
            }
        }

        // application
        return compile_app(expr, env, tailp);
    } else if (minim_symbolp(expr)) {
        // symbol
        return Mlist1(Mlist2(lookup_symbol, expr));
    } else if (minim_boolp(expr)
        || minim_fixnump(expr)
        || minim_charp(expr)
        || minim_stringp(expr)
        || minim_boxp(expr)
        || minim_vectorp(expr)) {
        // self-evaluating
        return Mlist1(Mlist2(literal_symbol, expr));
    } else {
        fprintf(stderr, "compile_expr: unimplemented\n");
        fprintf(stderr, " at: ");
        write_object(stderr, expr);
        fprintf(stderr, "\n");
        minim_shutdown(1);
    }
}

//
//  Public API
//

mobj compile_jit(mobj expr) {
    return compile_expr(expr, make_cenv(), 1);
}
