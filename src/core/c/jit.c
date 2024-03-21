// jit.c: compiler to JIT code

#include "../minim.h"

//
//  JIT object
//

mobj Mcode(size_t size) {
    mobj o = GC_alloc(minim_code_size(size));
    minim_type(o) = MINIM_OBJ_CODE;
    minim_code_len(o) = size;
    return o;
}

mobj code_to_instrs(mobj code) {
    mobj ins;
    size_t i;

    ins = minim_null;
    for (i = 0; i < minim_code_len(code); i++)
        ins = Mcons(minim_car(minim_code_ref(code, i)), ins);
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

static size_t let_values_size(mobj e) {
    mobj binds;
    size_t var_count;
    
    var_count = 0;
    binds = minim_cadr(e);
    while (!minim_nullp(binds)) {
        var_count += list_length(minim_caar(binds));
        binds = minim_cdr(binds);
    }

    return var_count;
}

static mobj write_code(mobj ins, mobj arity) {
    mobj code;
    size_t i, len;

    len = list_length(ins);
    code = Mcode(len);
    minim_code_name(code) = NULL;
    minim_code_arity(code) = arity;
    for (i = 0; i < len; i++) {
        minim_code_ref(code, i) = Mcons(minim_car(ins), minim_cdr(ins));
        ins = minim_cdr(ins);
    }

    return code;
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

static mobj with_tail_ret(mobj ins, int tailp) {
    if (tailp) {
        // tail position: must return
        list_set_tail(ins, Mlist1(Mlist1(ret_symbol)));
    }

    return ins;
}

static mobj compile_define_values(mobj expr, mobj env, int tailp) {
    mobj ids, ins;

    ids = minim_cadr(expr);
    ins = compile_expr(minim_car(minim_cddr(expr)), env, 0);
    list_set_tail(ins, Mlist1(Mlist2(bind_values_symbol, ids)));
    return with_tail_ret(ins, tailp);
}

static mobj compile_letrec_values(mobj expr, mobj env, int tailp) {
    mobj binds, body, ins;
    size_t env_size;

    env_size = let_values_size(expr);
    binds = minim_cadr(expr);
    body = Mcons(begin_symbol, minim_cddr(expr));

    // push a new environment
    ins = Mlist2(
        Mlist2(make_env_symbol, Mfixnum(env_size)),
        Mlist1(push_env_symbol)
    );

    // bind values
    for (; !minim_nullp(binds); binds = minim_cdr(binds)) {
        list_set_tail(ins, compile_expr(minim_car(minim_cdar(binds)), env, 0));
        list_set_tail(ins, Mlist1(
            Mlist2(bind_values_symbol, minim_caar(binds))
        ));
    }

    // evaluate body
    list_set_tail(ins, compile_expr(body, env, tailp));

    // if we are not in tail position, pop the environment
    if (!tailp) {
        list_set_tail(ins, Mlist1(Mlist1(pop_env_symbol)));
    }

    return ins;
}

static mobj compile_let_values(mobj expr, mobj env, int tailp) {
    mobj binds, body, ins;
    size_t env_size;

    env_size = let_values_size(expr);
    binds = minim_cadr(expr);
    body = Mcons(begin_symbol, minim_cddr(expr));

    // stash a new environment
    ins = Mlist2(
        Mlist2(make_env_symbol, Mfixnum(env_size)),
        Mlist1(push_symbol)     // use `push` to move to stack
    );

    // bind values to new environment
    // need to use special `bind-values/top` to access new environment
    for (; !minim_nullp(binds); binds = minim_cdr(binds)) {
        list_set_tail(ins, compile_expr(minim_car(minim_cdar(binds)), env, 0));
        list_set_tail(ins, Mlist1(
            Mlist2(bind_values_top_symbol, minim_caar(binds))
        ));
    }

    // evaluate body
    list_set_tail(ins, Mlist2(Mlist1(pop_symbol), Mlist1(push_env_symbol)));
    list_set_tail(ins, compile_expr(body, env, tailp));

    // if we are not in tail position, pop the environment
    if (!tailp) {
        list_set_tail(ins, Mlist1(Mlist1(pop_env_symbol)));
    }

    return ins;
}

static mobj compile_setb(mobj expr, mobj env, int tailp) {
    mobj ins = compile_expr(minim_car(minim_cddr(expr)), env, 0);
    list_set_tail(ins, Mlist1(Mlist2(rebind_symbol, minim_cadr(expr))));
    return with_tail_ret(ins, tailp);
}

static mobj compile_lambda(mobj expr, mobj env, int tailp) {
    mobj args, body, ins, idx, code, arity;
    long i, req_arity, env_size;
    int restp;
    
    // arity check
    restp = get_lambda_arity(expr, &req_arity);
    if (restp) {
        ins = Mlist1(Mlist2(check_arity_symbol, Mcons(Mfixnum(req_arity), minim_false)));
    } else {
        ins = Mlist1(Mlist2(check_arity_symbol, Mfixnum(req_arity)));
    }

    // push environment
    env_size = req_arity + (tailp ? 1 : 0);
    list_set_tail(ins, Mlist2(
        Mlist2(make_env_symbol, Mfixnum(env_size)),
        Mlist1(push_env_symbol)
    ));

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

    // reset the frame
    list_set_tail(ins, Mlist1(Mlist1(clear_frame_symbol)));

    // compile the body
    body = Mcons(begin_symbol, minim_cddr(expr));
    list_set_tail(ins, compile_expr(body, extend_cenv(env), 1));

    // resolve references
    resolve_refs(env, ins);

    // register JIT block
    arity = restp ? Mcons(Mfixnum(req_arity), minim_false) : Mfixnum(req_arity);
    code = write_code(ins, arity);
    idx = cenv_template_add(env, code);

    // instruction
    ins = Mlist1(Mlist2(make_closure_symbol, idx));
    return with_tail_ret(ins, tailp);
}

static mobj compile_begin(mobj expr, mobj env, int tailp) {
    mobj exprs, ins;
    
    exprs = minim_cdr(expr);
    if (minim_nullp(exprs)) {
        // empty body
        ins = Mlist1(Mlist2(literal_symbol, minim_void));
        return with_tail_ret(ins, tailp);
    } else {
        // non-empty body
        mobj ins_sub;
        
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
    mobj ins, cond_ins, ift_ins, iff_ins;

    // compile the parts
    cond_ins = compile_expr(minim_cadr(expr), env, 0);
    ift_ins = compile_expr(minim_car(minim_cddr(expr)), env, tailp);
    iff_ins = compile_expr(minim_cadr(minim_cddr(expr)), env, tailp);

    ins = cond_ins;
    if (tailp) {
        // tail position: returns will be handled by branches
        mobj liff = cenv_make_label(env);

        // compose the instructions together
        list_set_tail(ins, Mlist1(Mlist2(branchf_symbol, liff)));
        list_set_tail(ins, ift_ins);
        list_set_tail(ins, Mlist1(liff));
        list_set_tail(ins, iff_ins);
    } else {
        // non-tail position: subsequent instruction so need to jump
        mobj liff = cenv_make_label(env);
        mobj lend = cenv_make_label(env);

        // compose the instructions together
        list_set_tail(ins, Mlist1(Mlist2(branchf_symbol, liff)));
        list_set_tail(ins, ift_ins);
        list_set_tail(ins, Mlist1(Mlist2(brancha_symbol, lend)));
        list_set_tail(ins, Mlist1(liff));
        list_set_tail(ins, iff_ins);
        list_set_tail(ins, Mlist1(lend));
    }
    
    return ins;
}

static mobj compile_app(mobj expr, mobj env, int tailp) {
    mobj ins, label, it;
    size_t argc;

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

    // emit stack hint
    argc = list_length(minim_cdr(expr));
    list_set_tail(ins, Mlist1(Mlist2(check_stack_symbol, Mfixnum(argc))));

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

static mobj compile_lookup(mobj expr, int tailp) {
    mobj ins = Mlist1(Mlist2(lookup_symbol, expr));
    return with_tail_ret(ins, tailp);
}

static mobj compile_literal(mobj expr, int tailp) {
    mobj ins = Mlist1(Mlist2(literal_symbol, expr));
    return with_tail_ret(ins, tailp);
}

static mobj compile_expr(mobj expr, mobj env, int tailp) {
    if (minim_consp(expr)) {
        // special form or application
        mobj head = minim_car(expr);
        if (minim_symbolp(head)) {
            // special forms
            if (head == define_values_symbol) {
                // define-values form
                return compile_define_values(expr, env, tailp);
            } else if (head == letrec_values_symbol) {
                // letrec-values form
                return compile_letrec_values(expr, env, tailp);
            } else if (head == let_values_symbol) {
                // let-values form
                return compile_let_values(expr, env, tailp);
            } else if (head == setb_symbol) {
                // set! form
                return compile_setb(expr, env, tailp);
            } else if (head == lambda_symbol) {
                // lambda form
                return compile_lambda(expr, env, tailp);
            } else if (head == begin_symbol) {
                // begin form
                return compile_begin(expr, env, tailp);
            } else if (head == if_symbol) {
                // if form
                return compile_if(expr, env, tailp);
            } else if (head == quote_symbol) {
                // quote form
                return compile_literal(minim_cadr(expr), tailp);
            } else if (head == quote_syntax_symbol) {
                // quote-syntax form
                return compile_literal(to_syntax(minim_cadr(expr)), tailp);
            }
        }

        // application
        return compile_app(expr, env, tailp);
    } else if (minim_symbolp(expr)) {
        // symbol
        return compile_lookup(expr, tailp);
    } else if (minim_boolp(expr)
        || minim_fixnump(expr)
        || minim_charp(expr)
        || minim_stringp(expr)
        || minim_boxp(expr)
        || minim_vectorp(expr)) {
        // self-evaluating
        return compile_literal(expr, tailp);
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
    mobj env, ins;

    env = make_cenv();
    ins = compile_expr(expr, env, 1);
    resolve_refs(env, ins);
    return write_code(ins, Mfixnum(0));
}
