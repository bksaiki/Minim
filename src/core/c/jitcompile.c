// jitcompile.c: compiler

#include "../minim.h"

//
//  Utilities
//

static int get_formals_len(mobj e, size_t *len) {
    *len = 0;
    for (; minim_consp(e); e = minim_cdr(e))
        ++(*len);
    return !minim_nullp(e);
}

static mobj update_arity(mobj arity, size_t req_arity, int restp) {
    if (minim_nullp(arity)) {
        // no arity set
        return restp ? Mcons(Mfixnum(req_arity), minim_false) : Mfixnum(req_arity);
    } else if (minim_fixnump(arity)) {
        // single arity
        if (req_arity > minim_fixnum(arity)) {
            if (restp) {
                 return Mcons(arity, Mcons(Mfixnum(req_arity), minim_false));
            } else {
                return Mlist2(arity, Mfixnum(req_arity));
            }
        } else if (req_arity == minim_fixnum(arity)) {
            if (restp) {
                return Mcons(arity, minim_false);
            } else {
                return arity;
            }
        } else {
            if (restp) {
                return Mcons(Mfixnum(req_arity), minim_false);
            } else {
                return Mcons(Mfixnum(req_arity), arity);
            }
        }
    } else {
        // multi-arity or arity with rest
        if (req_arity > minim_fixnum(minim_car(arity))) {
            if (minim_falsep(minim_cdr(arity))) {
                return arity;
            } else {
                return Mcons(minim_car(arity), update_arity(minim_cdr(arity), req_arity, restp));
            }
        } else if (req_arity == minim_fixnum(minim_car(arity))) {
            if (restp) {
                return Mcons(minim_car(arity), minim_false);
            } else {
                return arity;
            }
        } else {
            if (restp) {
                return Mcons(minim_car(arity), minim_false);
            } else {
                return Mcons(Mfixnum(req_arity), arity);
            }
        }

        minim_error("update_arity", "unimplemented");
    }
}

//
//  Compiler
//

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
    ins = compile_expr2(minim_car(minim_cddr(expr)), env, 0);
    list_set_tail(ins, Mlist1(Mlist2(tl_bind_values_symbol, ids)));
    return with_tail_ret(ins, tailp);
}

static mobj compile_setb(mobj expr, mobj env, int tailp) {
    mobj ins = compile_expr2(minim_car(minim_cddr(expr)), env, 0);
    list_set_tail(ins, Mlist1(Mlist2(rebind_symbol, minim_cadr(expr))));
    return with_tail_ret(ins, tailp);
}

static mobj compile_lambda_clause(mobj clause, mobj env, size_t arity, int restp) {
    mobj ins, args, body;
    size_t env_size, i;

    env_size = arity + (restp ? 1 : 0);
    ins = Mlist1(Mlist2(push_env_symbol, Mfixnum(env_size)));

    // bind arguments
    env = extend_cenv(env);
    args = minim_car(clause);
    for (i = 0; i < arity; i++) {
        list_set_tail(ins, Mlist2(
            Mlist2(get_arg_symbol, Mfixnum(i)),
            Mlist2(bind_symbol, minim_car(args))
        ));

        cenv_id_add(env, minim_car(args));
        args = minim_cdr(args);
    }

    // bind rest argument
    if (restp) {
        cenv_id_add(env, args);
        list_set_tail(ins, Mlist2(
            Mlist2(do_rest_symbol, Mfixnum(arity)),
            Mlist2(bind_symbol, args)
        )); 
    }

    // reset the frame
    list_set_tail(ins, Mlist1(Mlist1(clear_frame_symbol)));

    // compile the body
    body = Mcons(begin_symbol, minim_cdr(clause));
    list_set_tail(ins, compile_expr2(body, env, 1));
    return ins;
}

static mobj compile_case_lambda(mobj expr, mobj env, int tailp) {
    mobj ins, clauses, label, reloc, idx, arity, code;

    ins = minim_null;
    arity = minim_null;
    label = NULL;

    // create labels for each clause
    for (clauses = minim_cdr(expr); !minim_nullp(clauses); clauses = minim_cdr(clauses)) {
        mobj branch, cl_ins;
        size_t req_arity;
        int restp;

        // compute arity of clause
        restp = get_formals_len(minim_caar(clauses), &req_arity);
        branch = restp ? branchlt_symbol : branchne_symbol;
        if (label) {
            list_set_tail(ins, Mlist1(label));
            label = cenv_make_label(env);
            list_set_tail(ins, Mlist1(Mlist3(branch, Mfixnum(req_arity), label)));
        } else {
            label = cenv_make_label(env);
            ins = Mlist2(Mlist1(get_ac_symbol), Mlist3(branch, Mfixnum(req_arity), label));
        }

        arity = update_arity(arity, req_arity, restp);
        cl_ins = compile_lambda_clause(minim_car(clauses), env, req_arity, restp);
        list_set_tail(ins, cl_ins);
    }

    // arity exception
    list_set_tail(ins, Mlist2(label, Mlist1(do_arity_error_symbol)));

    // resolve references
    reloc = resolve_refs(env, ins);

    // register JIT block
    code = write_code(ins, reloc, arity);
    idx = global_cenv_add_template(cenv_global_env(env), code);

    // instruction
    ins = Mlist1(Mlist2(make_closure_symbol, Mfixnum(idx)));
    return with_tail_ret(ins, tailp);
}

static mobj compile_mvcall(mobj expr, mobj env, int tailp) {
    mobj ins, label;

    // need to save a continuation if not in tail position
    if (tailp) {
        ins = compile_expr2(minim_cadr(expr), env, 0);
    } else {
        label = cenv_make_label(env);
        ins = Mlist1(Mlist2(save_cc_symbol, label));
        list_set_tail(ins, compile_expr2(minim_cadr(expr), env, 0));
    }
    
    list_set_tail(ins, Mlist2(Mlist1(clear_frame_symbol), Mlist1(do_with_values_symbol)));
    list_set_tail(ins, compile_expr2(minim_car(minim_cddr(expr)), env, 0));
    list_set_tail(ins, Mlist2(Mlist1(set_proc_symbol), Mlist1(apply_symbol)));

    // need a label to jump to if not in tail position
    if (!tailp) {
        list_set_tail(ins, Mlist1(label));
    }

    return ins;
}

static mobj compile_mvlet(mobj expr, mobj env, int tailp) {
    mobj ins, ids, it;
    size_t env_size;

    // evaluate producer
    ins = compile_expr2(minim_cadr(expr), env, 0);

    // extend compile environment
    env = extend_cenv(env);
    ids = minim_car(minim_cddr(expr));
    for (it = ids; !minim_nullp(it); it = minim_cdr(it))
        cenv_id_add(env, minim_car(it));

    // bind result in new environment
    env_size = list_length(ids);
    list_set_tail(ins, Mlist2(
        Mlist2(push_env_symbol, Mfixnum(env_size)),
        Mlist2(bind_values_symbol, ids)
    ));

    // evaluate body
    list_set_tail(ins, compile_expr2(minim_cadr(minim_cddr(expr)), env, tailp));
    
    // pop environment if not in tail position
    if (!tailp) {
        list_set_tail(ins, Mlist1(Mlist1(pop_env_symbol)));
    }

    return ins;
}

static mobj compile_mvvalues(mobj expr, mobj env, int tailp) {
    mobj vals, ins;
    
    vals = minim_cdr(expr);
    if (minim_nullp(vals)) {
        ins = Mlist1(Mlist1(do_values_symbol));
    } else {
        ins = Mlist1(Mlist2(check_stack_symbol, Mfixnum(list_length(vals))));
        for (; !minim_nullp(vals); vals = minim_cdr(vals)) {
            list_set_tail(ins, compile_expr2(minim_car(vals), env, 0));
            list_set_tail(ins, Mlist1(Mlist1(push_symbol)));
        }

        list_set_tail(ins, Mlist2(Mlist1(do_values_symbol), Mlist1(clear_frame_symbol)));
    }

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
            ins_sub = compile_expr2(minim_car(exprs), env, 0);
            ins = list_append2(ins, ins_sub);
            exprs = minim_cdr(exprs);
        }

        // last expression is in tail position
        ins_sub = compile_expr2(minim_car(exprs), env, tailp);
        return list_append2(ins, ins_sub);
    }
}

static mobj compile_if(mobj expr, mobj env, int tailp) {
    mobj ins, cond_ins, ift_ins, iff_ins;

    // compile the parts
    cond_ins = compile_expr2(minim_cadr(expr), env, 0);
    ift_ins = compile_expr2(minim_car(minim_cddr(expr)), env, tailp);
    iff_ins = compile_expr2(minim_cadr(minim_cddr(expr)), env, tailp);

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
    ins = list_append2(ins, compile_expr2(minim_car(expr), env, 0));
    list_set_tail(ins, Mlist1(Mlist1(set_proc_symbol)));

    // emit stack hint
    argc = list_length(minim_cdr(expr));
    list_set_tail(ins, Mlist1(Mlist2(check_stack_symbol, Mfixnum(argc))));

    // compute arguments
    for (it = minim_cdr(expr); !minim_nullp(it); it = minim_cdr(it)) {
        list_set_tail(ins, compile_expr2(minim_car(it), env, 0));
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

static mobj compile_lookup(mobj id, mobj env, int tailp) {
    mobj ins, ref;

    ref = cenv_id_ref(env, id);
    if (minim_falsep(ref)) {
        // top-level symbol
        ins = Mlist1(Mlist2(tl_lookup_symbol, id));
    } else {
        // local symbol
        ins = Mlist1(Mlist2(lookup_symbol, ref));
    }

    return with_tail_ret(ins, tailp);
}

static mobj compile_literal(mobj expr, int tailp) {
    mobj ins = Mlist1(Mlist2(literal_symbol, expr));
    return with_tail_ret(ins, tailp);
}

mobj compile_expr2(mobj expr, mobj env, int tailp) {
    if (minim_consp(expr)) {
        // special form or application
        mobj head = minim_car(expr);
        if (minim_symbolp(head)) {
            // special forms
            if (head == define_values_symbol) {
                // define-values form
                return compile_define_values(expr, env, tailp);
            } else if (head == setb_symbol) {
                // set! form
                return compile_setb(expr, env, tailp);
            } else if (head == lambda_symbol) {
                // lambda form
                return compile_case_lambda(Mlist2(case_lambda_symbol, minim_cdr(expr)), env, tailp);
            } else if (head == case_lambda_symbol) {
                // case-lambda form
                return compile_case_lambda(expr, env, tailp);
            } else if (head == mvcall_symbol) {
                // mv-call form
                return compile_mvcall(expr, env, tailp);
            } else if (head == mvlet_symbol) {
                // mv-let form
                return compile_mvlet(expr, env, tailp);
            } else if (head == mvvalues_symbol) {
                // mv-values form
                return compile_mvvalues(expr, env, tailp);
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
            } else if (head == make_unbound_symbol) {
                // make-unbound form
                return compile_literal(minim_unbound, tailp);
            }
        }

        // application
        return compile_app(expr, env, tailp);
    } else if (minim_symbolp(expr)) {
        // symbol
        return compile_lookup(expr, env, tailp);
    } else if (minim_boolp(expr)
        || minim_fixnump(expr)
        || minim_charp(expr)
        || minim_stringp(expr)
        || minim_boxp(expr)
        || minim_vectorp(expr)) {
        // self-evaluating
        return compile_literal(expr, tailp);
    } else {
        minim_error1("compile_expr", "cannot compile", expr);
    }
}
