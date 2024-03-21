// jit.c: compiler to JIT code

#include "../minim.h"

//
//  Association lists
//

static mobj assq_set(mobj xs, mobj k, mobj v) {
    return Mcons(Mcons(k, v), xs);
}

static mobj assq_ref(mobj xs, mobj k) {
    for (; !minim_nullp(xs); xs = minim_cdr(xs)) {
        if (minim_caar(xs) == k)
            return minim_cdar(xs);
    }

    return minim_false;
}

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
    mobj ins, reloc, inv_reloc, *istream;
    size_t i;

    // build inverse reloc table
    inv_reloc = minim_null;
    reloc = minim_code_reloc(code);
    for (; !minim_nullp(reloc); reloc = minim_cdr(reloc)) {
        inv_reloc = assq_set(inv_reloc, minim_cdar(reloc), minim_caar(reloc));
    }

    // build instruction sequence
    ins = minim_null;
    istream = minim_code_it(code);
    for (i = 0; i < minim_code_len(code); i++) {
        mobj in, ref;
        
        // possibly replace jumps
        in = istream[i];
        if (minim_car(in) == brancha_symbol) {
            // brancha
            in = Mlist2(brancha_symbol, assq_ref(inv_reloc, minim_cadr(in)));
        } else if (minim_car(in) == branchf_symbol) {
            // branchf
            in = Mlist2(branchf_symbol, assq_ref(inv_reloc, minim_cadr(in)));
        } else if (minim_car(in) == save_cc_symbol) {
            // save-cc
            in = Mlist2(save_cc_symbol, assq_ref(inv_reloc, minim_cadr(in)));
        }

        // restore label
        ref = assq_ref(inv_reloc, &istream[i]);
        if (!minim_falsep(ref)) {
            ins = Mcons(ref, ins);
        }

        ins = Mcons(in, ins);
    }

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

static mobj write_code(mobj ins, mobj reloc, mobj arity) {
    mobj code, *istream;
    size_t i;

    // allocate code object and fill header
    code = Mcode(list_length(ins));
    minim_code_name(code) = NULL;
    minim_code_reloc(code) = minim_null;
    minim_code_arity(code) = arity;
    istream = minim_code_it(code);

    // need to recompute the reloc table for in-code addresses
    for (; !minim_nullp(reloc); reloc = minim_cdr(reloc)) {
        mobj it = ins;
        for (i = 0; i < minim_code_len(code); i++) {
            if (minim_car(it) == minim_cdar(reloc)) {
                mobj cell = Mcons(minim_caar(reloc), &istream[i]);
                minim_code_reloc(code) = Mcons(cell, minim_code_reloc(code)); 
                break;
            }
    
            it = minim_cdr(it);
        }
    }

    reloc = list_reverse(minim_code_reloc(code));
    minim_code_reloc(code) = reloc;

    // write instructions
    for (i = 0; i < minim_code_len(code); i++) {
        mobj in = minim_car(ins);
        if (minim_car(in) == brancha_symbol) {
            // brancha
            in = Mlist2(brancha_symbol, assq_ref(reloc, minim_cadr(in)));
        } else if (minim_car(in) == branchf_symbol) {
            // branchf
            in = Mlist2(branchf_symbol, assq_ref(reloc, minim_cadr(in)));
        } else if (minim_car(in) == save_cc_symbol) {
            // save-cc
            in = Mlist2(save_cc_symbol, assq_ref(reloc, minim_cadr(in)));
        }

        // add instruction
        istream[i] = in;
        ins = minim_cdr(ins);
    }

    // NULL terminator
    istream[i] = NULL;
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

static mobj resolve_refs(mobj cenv, mobj ins) {
    mobj reloc, label_map;

    reloc = minim_null;
    label_map = minim_null;

    // build unionfind of labels and eliminate redundant ones
    for (mobj it = ins; !minim_nullp(it); it = minim_cdr(it)) {
        mobj in = minim_car(it);
        if (minim_stringp(in)) {
            // label found (first one is the leading one)
            mobj l0 = in;
            reloc = assq_set(reloc, l0, minim_cadr(it));
            label_map = assq_set(label_map, l0, l0);
            // subsequent labels
            for (; minim_stringp(minim_cadr(it)); it = minim_cdr(it))
                label_map = assq_set(label_map, minim_cadr(it), l0);
        }
    }

    // resolve closures and labels
    for (mobj it = ins; !minim_nullp(it); it = minim_cdr(it)) {
        mobj in = minim_car(it);
        if (minim_car(in) == brancha_symbol) {
            // brancha: need to replace the label with the next instruction
            minim_cadr(in) = assq_ref(label_map, minim_cadr(in));
        } else if (minim_car(in) == branchf_symbol) {
            // branchf: need to replace the label with the next instruction
            minim_cadr(in) = assq_ref(label_map, minim_cadr(in));
        } else if (minim_car(in) == make_closure_symbol) {
            // closure: need to lookup JIT object to embed
            minim_cadr(in) = cenv_template_ref(cenv, minim_fixnum(minim_cadr(in)));
        } else if (minim_car(in) == save_cc_symbol) {
            // save-cc: need to replace the label with the next instruction
            minim_cadr(in) = assq_ref(label_map, minim_cadr(in));
        }
    }

    // eliminate labels (assumes head is not a label)
    for (; !minim_nullp(minim_cdr(ins)); ins = minim_cdr(ins)) {
        if (minim_stringp(minim_cadr(ins)))
            minim_cdr(ins) = minim_cddr(ins);
    }

    return reloc;   
}

//
//  Compiler
//

static mobj compile_expr2(mobj expr, mobj env, int tailp);

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
        list_set_tail(ins, compile_expr2(minim_car(minim_cdar(binds)), env, 0));
        list_set_tail(ins, Mlist1(
            Mlist2(bind_values_symbol, minim_caar(binds))
        ));
    }

    // evaluate body
    list_set_tail(ins, compile_expr2(body, env, tailp));

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
        list_set_tail(ins, compile_expr2(minim_car(minim_cdar(binds)), env, 0));
        list_set_tail(ins, Mlist1(
            Mlist2(bind_values_top_symbol, minim_caar(binds))
        ));
    }

    // evaluate body
    list_set_tail(ins, Mlist2(Mlist1(pop_symbol), Mlist1(push_env_symbol)));
    list_set_tail(ins, compile_expr2(body, env, tailp));

    // if we are not in tail position, pop the environment
    if (!tailp) {
        list_set_tail(ins, Mlist1(Mlist1(pop_env_symbol)));
    }

    return ins;
}

static mobj compile_setb(mobj expr, mobj env, int tailp) {
    mobj ins = compile_expr2(minim_car(minim_cddr(expr)), env, 0);
    list_set_tail(ins, Mlist1(Mlist2(rebind_symbol, minim_cadr(expr))));
    return with_tail_ret(ins, tailp);
}

static mobj compile_lambda(mobj expr, mobj env, int tailp) {
    mobj args, body, ins, idx, reloc, code, arity;
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
    list_set_tail(ins, compile_expr2(body, extend_cenv(env), 1));

    // resolve references
    reloc = resolve_refs(env, ins);

    // register JIT block
    arity = restp ? Mcons(Mfixnum(req_arity), minim_false) : Mfixnum(req_arity);
    code = write_code(ins, reloc, arity);
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

static mobj compile_lookup(mobj expr, int tailp) {
    mobj ins = Mlist1(Mlist2(lookup_symbol, expr));
    return with_tail_ret(ins, tailp);
}

static mobj compile_literal(mobj expr, int tailp) {
    mobj ins = Mlist1(Mlist2(literal_symbol, expr));
    return with_tail_ret(ins, tailp);
}

static mobj compile_expr2(mobj expr, mobj env, int tailp) {
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
        fprintf(stderr, "compile_expr2: unimplemented\n");
        fprintf(stderr, " at: ");
        write_object(stderr, expr);
        fprintf(stderr, "\n");
        minim_shutdown(1);
    }
}

// Common idiom for custom compilation targets
static mobj compile_do_ret(mobj name, mobj arity, mobj do_instr) {
    mobj env, ins, reloc, code;

    // prepare compiler
    env = make_cenv();
    
    // hand written procedure
    ins = Mlist3(
        Mlist2(check_arity_symbol, arity),
        do_instr,
        Mlist1(ret_symbol)
    );

    // write to code
    reloc = resolve_refs(env, ins);
    code = write_code(ins, reloc, arity);
    minim_code_name(code) = name;

    // return a closure
    return Mclosure(empty_env, code);
}

// Short hand for making a function that just calls `compile_do_ret`
#define define_do_ret(fn_name, arity, do_instr) \
    mobj fn_name(mobj name) { \
        return compile_do_ret(name, arity, do_instr); \
    }

//
//  Public API
//

mobj compile_prim(const char *who, void *fn, mobj arity) {
    return minim_void;
}

mobj compile_expr(mobj expr) {
    mobj env, ins, reloc;

    env = make_cenv();
    ins = compile_expr2(expr, env, 1);
    reloc = resolve_refs(env, ins);
    return write_code(ins, reloc, Mfixnum(0));
}

define_do_ret(compile_apply, Mcons(Mfixnum(2), minim_false), Mlist1(do_apply_symbol));
define_do_ret(compile_identity, Mfixnum(1), Mlist2(get_arg_symbol, Mfixnum(0)));
define_do_ret(compile_values, Mcons(Mfixnum(0), minim_false), Mlist1(do_values_symbol));
define_do_ret(compile_void, Mfixnum(0), Mlist2(literal_symbol, minim_void));

// Custom `call-with-values` compilation
mobj compile_call_with_values(mobj name) {
    mobj env, arity, ins, label, reloc, code;

    // prepare compiler
    env = make_cenv();
    arity = Mfixnum(2);
    label = cenv_make_label(env);
    
    // hand written procedure
    ins = list_append2(
        Mlist6(
            Mlist2(check_arity_symbol, arity),      // check arity
            Mlist1(pop_symbol),                     // pop consumer
            Mlist1(set_proc_symbol),                // set consumer as procedure
            Mlist1(pop_symbol),                     // pop producer
            Mlist2(save_cc_symbol, label),          // create new frame
            Mlist1(set_proc_symbol)                 // set producer as procedure
        ),
        Mlist4(
            Mlist1(apply_symbol),                   // call and restore previous frame
            label,
            Mlist1(do_with_values_symbol),          // convert result to arguments
            Mlist1(apply_symbol)                    // call consumer
        )
    );

    // write to code
    reloc = resolve_refs(env, ins);
    code = write_code(ins, reloc, Mcons(Mfixnum(2), Mfixnum(2)));
    minim_code_name(code) = name;

    // return a closure
    return Mclosure(empty_env, code);
}

// Custom `eval` compilation
mobj compile_eval(mobj name) {
    mobj env, arity, ins, reloc, code;

    // prepare compiler
    env = make_cenv();
    arity = Mcons(Mfixnum(1), Mfixnum(2));
    
    // hand written procedure
    ins = Mlist2(
        Mlist2(check_arity_symbol, arity),
        Mlist1(do_eval_symbol)
    );

    // write to code
    reloc = resolve_refs(env, ins);
    code = write_code(ins, reloc, Mcons(Mfixnum(1), Mfixnum(2)));
    minim_code_name(code) = name;

    // return a closure
    return Mclosure(empty_env, code);
}
