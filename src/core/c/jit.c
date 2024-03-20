// jit.c: compiler to JIT code

#include "../minim.h"

//
//  JIT object
//

mobj Mjit(size_t size) {
    mobj o = GC_alloc(minim_jit_size(size));
    minim_type(o) = MINIM_OBJ_JIT;
    minim_jit_len(o) = size;
    return o;
}

mobj jit_to_instrs(mobj jit) {
    mobj ins;
    size_t i;

    ins = minim_null;
    for (i = 0; i < minim_jit_len(jit); i++)
        ins = Mcons(minim_car(minim_jit_ref(jit, i)), ins);
    return list_reverse(ins);
}

//
//  Utilities
//

static int get_lambda_arity(mobj e, long *req_arity) {
    *req_arity = 0;
    for (e = minim_cadr(e); minim_consp(e); e = minim_cdr(e))
        ++(*req_arity);
    return !minim_nullp(e);
}

static mobj write_jit(mobj ins) {
    mobj jit;
    size_t i, len;

    len = list_length(ins);
    jit = Mjit(len);
    for (i = 0; i < len; i++) {
        minim_jit_ref(jit, i) = Mcons(minim_car(ins), minim_cdr(ins));
        ins = minim_cdr(ins);
    }

    return jit;
}

//
//  Compiler env
//

#define cenv_length         2
#define cenv_labels(c)      (minim_vector_ref(c, 0))
#define cenv_tmpls(c)       (minim_vector_ref(c, 1))
#define cenv_num_tmpls(c)   list_length(minim_unbox(cenv_tmpls(c)))

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

static mobj cenv_template_add(mobj cenv, mobj jit) {
    mobj tmpl_box;
    size_t num_tmpls;

    // unbox list of templates
    tmpl_box = cenv_tmpls(cenv);
    num_tmpls = list_length(minim_unbox(tmpl_box));

    // update list
    minim_unbox(tmpl_box) = Mcons(jit, minim_unbox(tmpl_box));
    return Mfixnum(num_tmpls);
}

static mobj cenv_template_ref(mobj cenv, size_t i) {
    mobj tmpls;
    size_t j;
    
    tmpls = list_reverse(minim_unbox(cenv_tmpls(cenv)));
    for (j = i; j > 0; j--) {
        if (minim_nullp(tmpls)) {
            fprintf(stderr, "cenv_template_ref(): index out of bounds %ld\n", i);
            minim_shutdown(1);
        }

        tmpls = minim_cdr(tmpls);
    }

    return minim_car(tmpls);
}

//
//  Resolver
//

static void resolve_refs(mobj cenv, mobj ins) {
    mobj in, jit;

    for (; !minim_nullp(ins); ins = minim_cdr(ins)) {
        in = minim_car(ins);
        if (minim_car(in) == make_closure_symbol) {
            // closure: need to lookup JIT object to embed
            jit = cenv_template_ref(cenv, minim_fixnum(minim_cadr(in)));
            minim_cadr(in) = jit;
        }
    }
}

//
//  Compiler
//

static mobj compile_expr(mobj expr, mobj env, int tailp);

static mobj compile_define_values(mobj expr, mobj env) {
    mobj ids, ins;

    ids = minim_cadr(expr);
    ins = compile_expr(minim_car(minim_cddr(expr)), env, 0);
    list_set_tail(ins, Mlist1(Mlist3(bind_values_symbol, Mfixnum(list_length(ids)), ids)));
    return ins;
}

static mobj compile_lambda(mobj expr, mobj env) {
    mobj args, body, ins, idx, jit;
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

    fprintf(stderr, "%ld: ", cenv_num_tmpls(env));
    write_object(stderr, ins);
    fprintf(stderr, "\n");

    // resolve references
    resolve_refs(env, ins);

    // register JIT block
    jit = write_jit(ins);
    idx = cenv_template_add(env, jit);

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
    mobj ins, cond_ins, ift_ins, iff_ins, liff, lend;

    // compile the parts
    cond_ins = compile_expr(minim_cadr(expr), env, 0);
    ift_ins = compile_expr(minim_car(minim_cddr(expr)), env, tailp);
    iff_ins = compile_expr(minim_cadr(minim_cddr(expr)), env, tailp);

    // labels
    liff = cenv_make_label(env);
    lend = cenv_make_label(env);

    // compose the instructions together
    ins = cond_ins;
    list_set_tail(ins, Mlist1(Mlist2(branchf_symbol, liff)));
    list_set_tail(ins, ift_ins);
    list_set_tail(ins, Mlist1(Mlist2(brancha_symbol, lend)));
    list_set_tail(ins, Mlist1(liff));
    list_set_tail(ins, iff_ins);
    list_set_tail(ins, Mlist1(lend));
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
    list_set_tail(ins, Mlist1(Mlist1(set_proc_symbol)));

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
            // special forms
            if (head == lambda_symbol) {
                // lambda form
                return compile_lambda(expr, env);
            } else if (head == begin_symbol) {
                // begin form
                return compile_begin(expr, env, tailp);
            } else if (head == if_symbol) {
                // if form
                return compile_if(expr, env, tailp);
            } else if (head == quote_symbol) {
                // quote form
                return Mlist1(Mlist2(literal_symbol, minim_cadr(expr)));
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

static mobj compile_top(mobj expr, mobj env) {
    if (minim_consp(expr)) {
        // special form or application
        mobj head = minim_car(expr);
        if (minim_symbolp(head)) {
            // special forms
            if (head == define_values_symbol) {
                // define-values form
                return compile_define_values(expr, env);
            }
        }
    }

    // expression context otherwise
    return compile_expr(expr, env, 1);
}

//
//  Public API
//

mobj compile_jit(mobj expr) {
    mobj env, ins;

    env = make_cenv();
    ins = compile_top(expr, env);
    resolve_refs(env, ins);
    return write_jit(ins);
}
