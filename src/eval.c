// eval.c: interpreter

#include "minim.h"

int Mimmediatep(obj x) {
    return Mtruep(x)
        || Mfalsep(x)
        || Mfixnump(x)
        || Mcharp(x)
        || Mstringp(x)
        || Mvectorp(x);
}

static void clear_values_buffer(obj tc) {
    Mtc_vc(tc) = 0;
}

static void assert_single_value(obj tc, obj x) {
    if (Mvaluesp(x)) {
        minim_error2(NULL, "values mismatch", Mfixnum(1), Mfixnum(Mtc_vc(tc)));
    }
}

static int arity_validp(obj arity, uptr argc) {
    iptr arity_val = Mfixnum_value(arity);
    if (arity_val >= 0) {
        // exact arity
        return arity_val == argc;
    } else {
        // at-least arity
        arity_val = -arity_val - 1;
        return argc >= arity_val;
    }
}

static void check_callcc(obj f) {
    if (Mprimp(f)) {
        iptr arity = Mprim_arity(f);
        if (arity < 0 ? arity < -2 : arity != 1)
            minim_error1("call/cc", "expected a procedure of at least 1 argument", f);
    } else if (Mclosurep(f)) {
        iptr arity = Mfixnum_value(Mclosure_arity(f));
        if (arity < 0 ? arity < -2 : arity != 1)
            minim_error1("call/cc", "expected a procedure of at least 1 argument", f);
    } else if (Mcontinuationp(f)) {
        // do nothing
    } else {
        minim_error1("call/cc", "expected a procedure", f);
    }
}

static void assert_thunk(const char *name, obj f) {
    if (Mprimp(f)) {
        iptr arity = Mprim_arity(f);
        if (arity < 0 ? arity != -1 : arity != 0)
            minim_error1(name, "expected a procedure of at least 0 argument", f);
    } else if (Mclosurep(f)) {
        iptr arity = Mfixnum_value(Mclosure_arity(f));
        if (arity < 0 ? arity < -1 : arity != 0)
            minim_error1(name, "expected a procedure of at least 0 argument", f);
    } else if (Mcontinuationp(f)) {
        // do nothing
    } else {
        minim_error1(name, "expected a procedure", f);
    }
}

static void check_prim_arity(obj tc, obj f, uptr arg0) {
    iptr arity, argc;

    arity = Mprim_arity(f);
    argc = (iptr) (Mtc_ac(tc) - arg0);
    if (arity >= 0) {
        // exact arity
        if (argc != arity) {
            minim_error2(
                Msymbol_value(Mprim_name(f)),
                "arity mismatch",
                Mfixnum(Mprim_arity(f)),
                Mfixnum(argc)
            );
        }
    } else {
        // at-least arity
        arity = -arity - 1;
        if (argc < arity) {
            minim_error2(
                Msymbol_value(Mprim_name(f)),
                "arity mismatch",
                Mlist2(Mintern("at-least"), Mfixnum(Mprim_arity(f))),
                Mfixnum(argc)
            );
        }
    }
}

NORETURN static void closure_arity_error(obj tc, obj f) {
    minim_error1("check_arity_error()", "unimplemented", f);
}

static uptr do_arg(obj tc, obj x) {
    uptr ac, idx;
    
    idx = Mtc_ac(tc);
    ac = Mtc_ac(tc) + 1;
    if (ac >= Mtc_aa(tc)) {
        // resize
        Mtc_aa(tc) = 2 * ac;
        Mtc_ab(tc) = GC_realloc(Mtc_ab(tc), Mtc_aa(tc) * sizeof(obj));
    }

    Mtc_ab(tc)[idx] = x;
    Mtc_ac(tc) = ac;
    return idx;
}

// extract current frame arguments as a list
// assumes: `arg0 >= 1`
static obj args_to_list(obj tc, uptr arg0) {
    obj xs;
    uptr i;

    xs = Mnull;
    for (i = Mtc_ac(tc) - 1; i >= arg0; --i)
        xs = Mcons(Mtc_ab(tc)[i], xs);

    return xs;
}

static void clear_arg_buffer(obj tc, uptr argf) {
    uptr argc;

    // clear argument buffer starting at `argf`
    // set new limit to `argf`
    // TODO: resize when small enough?
    argc = Mtc_ac(tc) - argf;
    memset(&Mtc_ab(tc)[argf], 0, argc * sizeof(obj));
    Mtc_ac(tc) = argf;
}

// performs `apply` primitive: shifts all arguments in frame by 1
// flattens the last argument (checks that it is a list)
static void do_apply(obj tc, uptr arg0) {
    obj xs;
    uptr ac, i;

    // shift all arguments (except the last one) by 1
    ac = Mtc_ac(tc);
    xs = Mtc_ab(tc)[ac - 1];
    for (i = arg0; i < ac - 2; i++) {
        Mtc_ab(tc)[i] = Mtc_ab(tc)[i + 1];
    }

    // apply last argument
    Mtc_ac(tc) -= 2;
    while (Mconsp(xs)) {
        do_arg(tc, Mcar(xs));
        xs = Mcdr(xs);
    }
}

// Performs `values` primitive.
// If there is only 1 argument, the argument is returned.
// Otherwise, the arguments are written to the values buffer.
static obj do_values(obj tc, uptr arg0) {
    uptr vc;

    if (Mtc_vc(tc) != 0) {
        minim_error("do_values()", "values buffer is not empty");
    }

    vc = Mtc_ac(tc) - arg0;
    if (vc == 0) {
        return Mvalues;
    } else if (vc == 1) {
        return Mtc_ab(tc)[arg0];
    } else {
        // check if we need to reallocate
        if (vc >= Mtc_va(tc)) {
            Mtc_va(tc) = 2 * vc;
            Mtc_vb(tc) = GC_realloc(Mtc_vb(tc), Mtc_va(tc) * sizeof(obj));
        }

        // copy between buffers
        memcpy(Mtc_vb(tc), &Mtc_ab(tc)[arg0], vc * sizeof(obj));
        Mtc_vc(tc) = vc;

        return Mvalues;
    }
}

// Pushes all values to the argument buffer.
// This is just the reverse of `do_values()`.
static void values_to_args(obj tc) {
    uptr vc, ac;

    vc = Mtc_vc(tc);
    if (vc > 0) {
        ac = Mtc_ac(tc) + vc;
        if (ac >= Mtc_aa(tc)) {
            // resize
            Mtc_aa(tc) = 2 * ac;
            Mtc_ab(tc) = GC_realloc(Mtc_ab(tc), Mtc_aa(tc) * sizeof(obj));
        }

        // copy between buffers
        memcpy(&Mtc_ab(tc)[Mtc_ac(tc)], Mtc_vb(tc), vc * sizeof(obj));
        Mtc_ac(tc) = ac;
    }
}

// Pushes a list of the values buffer
static obj list_to_values(obj tc, obj xs) {
    uptr vc, i;

    if (Mtc_vc(tc) != 0) {
        minim_error("list_to_values()", "values buffer is not empty");
    }

    vc = list_length(xs);
    if (vc == 0) {
        return Mvalues;
    } else if (vc == 1) {
        return Mcar(xs);
    } else {
        // check if we need to reallocate
        if (vc >= Mtc_va(tc)) {
            Mtc_va(tc) = 2 * vc;
            Mtc_vb(tc) = GC_realloc(Mtc_vb(tc), Mtc_va(tc) * sizeof(obj));
        }

        // copy between buffers
        Mtc_vc(tc) = vc;
        for (i = 0; i < vc; i++) {
            Mtc_vb(tc)[i] = Mcar(xs);
            xs = Mcdr(xs);
        }

        return Mvalues;
    }
}

// converts the values buffer to a list
static obj values_to_list(void) {
    obj tc, hd, tl;
    uptr vc, i;

    tc = Mcurr_tc();
    vc = Mtc_vc(tc);
    switch (vc)
    {
    case 0:
        // null values
        return Mnull;
    
    case 1:
        // single values
        return Mlist1(Mtc_vb(tc)[0]);

    default:
        // multiple values
        hd = tl = Mlist1(Mtc_vb(tc)[0]);
        for (i = 1; i < vc; ++i) {
            Mcdr(tl) = Mlist1(Mtc_vb(tc)[i]);
            tl = Mcdr(tl);
        }

        return hd;
    }
}

static obj do_special_prim(obj tc, obj f, uptr arg0) {
    obj *args, x;

    args = &Mtc_ab(tc)[arg0];
    if (f == list_prim) {
        return args_to_list(tc, arg0);
    } else if (f == values_prim) {
        return do_values(tc, arg0);
    } else if (f == void_prim) {
        return Mvoid;
    } else if (f == dynwind_prim) {
        Mtc_cc(tc) = Mdynwind_continuation(Mtc_cc(tc), Mtc_env(tc), args[0], args[1], args[2]);
        return Mvoid;
    } else if (f == callwv_prim) {
        assert_thunk("call-with-values", args[0]);
        Mtc_cc(tc) = Mcallwv_continuation(Mtc_cc(tc), Mtc_env(tc), args[0], args[1]);
        return Mvoid;
    } else if (f == exit_prim) {
        x = args[0];
        if (Mfixnump(x) && Mfixnum_value(x) >= 0 && Mfixnum_value(x) <= 0xFF) {
            minim_shutdown(Mfixnum_value(x));
        } else {
            minim_shutdown(0);
        }
    } else {
        minim_error1("do_special_prim()", "unimplemented", f);
    }
}

static obj do_prim(obj tc, obj f, uptr arg0) {
    obj *args;

    args = &Mtc_ab(tc)[arg0];
    switch (Mprim_arity(f))
    {
    case 0:
        {
            obj (*fn)(void) = Mprim_value(f);
            return fn();
        }
    case 1:
        {
            obj (*fn)(obj) = Mprim_value(f);
            return fn(args[0]);
        }
    case 2:
        {
            obj (*fn)(obj, obj) = Mprim_value(f);
            return fn(args[0], args[1]);
        }
    case 3:
        {
            obj (*fn)(obj, obj, obj) = Mprim_value(f);
            return fn(args[0], args[1], args[2]);
        }
    default:
        minim_error1("eval_expr", "primitive arity unsupported", Mfixnum(Mprim_arity(f)));
    }
}

static obj do_closure_env(obj tc, obj env, obj formals, uptr arg0) {
    obj it;
    uptr i;

    i = arg0;
    env = env_extend(env);
    for (it = formals; Mconsp(it); it = Mcdr(it)) {
        env_insert(env, Mcar(it), Mtc_ab(tc)[i]);
        i++;
    }

    if (!Mnullp(it)) {
        env_insert(env, it, args_to_list(tc, i));
    }

    return env;
}

static obj do_closure(obj tc, obj f, uptr arg0) {
    obj f_it, a_it, b_it;
    uptr argc;

    f_it = Mclosure_formals(f);
    a_it = Mclosure_arity(f);
    b_it = Mclosure_body(f);

    argc = Mtc_ac(tc) - arg0;
    while (!Mnullp(f_it)) {
        if (arity_validp(Mcar(a_it), argc)) {
            // found a clause with satisfiable arity
            return Mcons(do_closure_env(tc, Mclosure_env(f), Mcar(f_it), arg0), Mcar(b_it));
        }

        f_it = Mcdr(f_it);
        a_it = Mcdr(a_it);
        b_it = Mcdr(b_it);
    }

    closure_arity_error(tc, f);
}

static obj do_begin(obj tc) {
    obj x, k;

    k = continuation_mutable(Mtc_cc(tc));
    x = Mcontinuation_seq_value(k);
    Mtc_env(tc) = Mcontinuation_env(k);
    if (Mnullp(Mcdr(x))) {
        // evaluate last expression in tail position
        Mtc_cc(tc) = Mcontinuation_prev(k);
    } else {
        // still evaluating sequence
        Mcontinuation_seq_value(k) = Mcdr(x);
        Mtc_cc(tc) = k;
    }

    return Mcar(x);
}

static void check_let_arity(obj tc, obj ids, obj v) {
    iptr len, expect;

    expect = Mvaluesp(v) ? Mtc_vc(tc) : 1;
    len = list_length(ids);
    if (len != expect) {
        minim_error2(NULL, "result arity mismatch", Mfixnum(len), Mfixnum(expect));
    }
}

static void do_binds(obj tc, obj env, obj ids, obj v) {
    // bind values
    if (Mvaluesp(v)) {
        v = values_to_list();
        clear_values_buffer(tc);
        for (; !Mnullp(ids); ids = Mcdr(ids), v = Mcdr(v)) {
            if (Mclosurep(Mcar(v)) && Mfalsep(Mclosure_name(Mcar(v))))
                Mclosure_name(Mcar(v)) = Mcar(ids);
            env_insert(env, Mcar(ids), Mcar(v));
        }
    } else {
        if (Mclosurep(v) && Mfalsep(Mclosure_name(v)))
            Mclosure_name(v) = Mcar(ids);
        env_insert(env, Mcar(ids), v);
    }
}

static obj do_let(obj tc, obj v) {
    obj k, env, binds, ids;

    k = Mtc_cc(tc);
    env = Mcontinuation_let_env(k);
    binds = Mcontinuation_let_bindings(k);

    // check that we have enough ids
    ids = Mcaar(binds);
    check_let_arity(tc, ids, v);

    // bind values
    do_binds(tc, env, ids, v);

    binds = Mcdr(binds);
    if (Mnullp(binds)) {
        // no more bindings => evaluate body in tail position
        Mtc_env(tc) = env;
        Mtc_cc(tc) = Mcontinuation_prev(k);
        return Mcontinuation_let_body(k);
    } else {
        // at least one more binding
        k = continuation_mutable(k);
        Mcontinuation_let_bindings(k) = binds;
        Mtc_env(tc) = Mcontinuation_env(k);
        Mtc_cc(tc) = k;
        return Mcadar(binds);
    }
}

static void do_setb(obj tc, obj x, obj v) {
    obj env, cell;

    env = Mcontinuation_env(Mtc_cc(tc));
    cell = env_find(env, x);
    if (Mfalsep(cell)) {
        minim_error1("set!", "unbound variable", x);
    } else {
        if (Mclosurep(v) && Mfalsep(Mclosure_name(v)))
            Mclosure_name(v) = x;
        Mcdr(cell) = v;
    }
}

static obj eval_k(obj e) {
    obj tc, x, hd, f;
    uptr argf;

    tc = Mcurr_tc();

loop:

    // fprintf(stderr, "eval ");
    // writeln_object(stderr, e);
    // fprintf(stderr, " ");
    // writeln_object(stderr, Mtc_env(tc));

    if (Mconsp(e)) {
        // cons => syntax or application
        hd = Mcar(e);
        if (hd == Mlet_values_symbol) {
            // let-values
            // TODO: optimize for unbound
            Mtc_cc(tc) = Mlet_continuation(Mtc_cc(tc), Mtc_env(tc), Mcadr(e), Mcaddr(e));
            e = Mcadar(Mcontinuation_let_bindings(Mtc_cc(tc)));
            goto loop;
        } else if (hd == Mbegin_symbol) {
            // begin
            Mtc_cc(tc) = Mseq_continuation(Mtc_cc(tc), Mtc_env(tc), Mcddr(e));
            e = Mcadr(e);
            goto loop;
        } else if (hd == Mif_symbol) {
            // if
            Mtc_cc(tc) = Mcond_continuation(Mtc_cc(tc), Mtc_env(tc), Mcaddr(e), Mcar(Mcdddr(e)));
            e = Mcadr(e);
            goto loop;
        } else if (hd == Mlambda_symbol) {
            // lambda
            x = Mclosure(Mtc_env(tc), Mcadr(e), Mcaddr(e));
            goto do_k;
        } else if (hd == Mcase_lambda_symbol) {
            // case-lambda
            x = Mclosure2(Mtc_env(tc), Mcdr(e));
            goto do_k;
        } else if (hd == Msetb_symbol) {
            // set!
            Mtc_cc(tc) = Msetb_continuation(Mtc_cc(tc), Mtc_env(tc), Mcadr(e));
            e = Mcaddr(e);
            goto loop;
        } else if (hd == Mquote_symbol) {
            // quote
            x = Mcadr(e);
            goto do_k;
        } else {
            // application
            Mtc_cc(tc) = Mapp_continuation(Mtc_cc(tc), Mtc_env(tc), Mcdr(e), Mtc_ac(tc));
            e = Mcar(e);
            goto loop;
        }
    } else if (Msymbolp(e)) {
        // variable => lookup
        x = env_find(Mtc_env(tc), e);
        if (Mconsp(x)) {
            x = Mcdr(x);
            goto do_k;
        } else if (Mfalsep(x)) {
            minim_error(Msymbol_value(e), "unbound identifier");
        } else if (Munboundp(x)) {
            minim_error(Msymbol_value(e), "uninitialized identifier");
        } else {
            minim_error1("eval_expr", "unreachable", e);
        }
    } else if (Mimmediatep(e)) {
        // immediate => fully evaluated
        x = e;
        goto do_k;
    } else if (Mnullp(e)) {
        // empty application
        minim_error1("eval_expr", "empty application", e);
    } else {
        minim_error1("eval_expr", "cannot evaluate", e);
    }

    minim_error1("eval_expr", "unreachable", e);

do_app:
    f = Mtc_ab(tc)[argf];
    if (Mprimp(f)) {
        check_prim_arity(tc, f, argf + 1);
        if (Mprim_specialp(f)) {
            if (f == apply_prim) {
                f = Mtc_ab(tc)[argf + 1];
                if (!Mprocp(f)) {
                    minim_error1("apply", "expected procedure", f);
                }

                x = Mtc_ab(tc)[Mtc_ac(tc) - 1];
                if (!Mlistp(x)) {
                    minim_error1("apply", "last argument must be a list", x);
                }

                Mtc_ab(tc)[argf] = f;
                do_apply(tc, argf + 1);
                goto do_app;
            } else if (f == callcc_prim) {
                x = Mtc_ab(tc)[argf + 1];
                check_callcc(x);
                clear_arg_buffer(tc, argf);
                // freeze the continuation chain and construct a new continuation
                continuation_set_immutable(Mtc_cc(tc));
                Mtc_cc(tc) = Mcallcc_continuation(Mtc_cc(tc), Mtc_env(tc), Mtc_wnd(tc), Mtc_ab(tc), Mtc_aa(tc), Mtc_ac(tc));
                goto do_k;
            } else {
                x = do_special_prim(tc, f, argf + 1);
            }
        } else {
            x = do_prim(tc, f, argf + 1);
            
        }

        clear_arg_buffer(tc, argf);
        goto do_k;
    } else if (Mclosurep(f)) {
        x = do_closure(tc, f, argf + 1);
        Mtc_env(tc) = Mcar(x);
        e = Mcdr(x);
        clear_arg_buffer(tc, argf);
        goto loop;
    } else if (Mcontinuationp(f)) {
        x = do_values(tc, argf + 1);
        clear_arg_buffer(tc, argf);

        Mtc_cc(tc) = continuation_restore(tc, f);
        goto do_k;
    } else {
        minim_error1("eval_expr", "application: not a procedure", x);
    }

do_k:
    switch (Mcontinuation_type(Mtc_cc(tc))) {
    // bottom of continuation chain, so exit
    case NULL_CONT_TYPE:
        return x;

    // applications
    case APP_CONT_TYPE:
        assert_single_value(tc, x);
        do_arg(tc, x);
        Mtc_env(tc) = Mcontinuation_env(Mtc_cc(tc));
        if (Mnullp(Mcontinuation_app_it(Mtc_cc(tc)))) {
            // evaluated last argument => apply
            argf = Mcontinuation_app_idx(Mtc_cc(tc));
            Mtc_cc(tc) = Mcontinuation_prev(Mtc_cc(tc));
            goto do_app;
        } else {
            // keep evaluating arguments
            e = Mcar(Mcontinuation_app_it(Mtc_cc(tc)));
            Mtc_cc(tc) = continuation_mutable(Mtc_cc(tc));
            Mcontinuation_app_it(Mtc_cc(tc)) = Mcdr(Mcontinuation_app_it(Mtc_cc(tc)));
            goto loop;
        }

    // if expressions
    case COND_CONT_TYPE:
        assert_single_value(tc, x);
        Mtc_cc(tc) = continuation_mutable(Mtc_cc(tc));
        e = Mfalsep(x) ? Mcontinuation_cond_iff(Mtc_cc(tc)) : Mcontinuation_cond_ift(Mtc_cc(tc));
        Mtc_env(tc) = Mcontinuation_env(Mtc_cc(tc));
        Mtc_cc(tc) = Mcontinuation_prev(Mtc_cc(tc));
        goto loop;

    // begin expressions
    case SEQ_CONT_TYPE:
        clear_values_buffer(tc);
        e = do_begin(tc);
        goto loop;
    
    // let expressions
    case LET_CONT_TYPE:
        e = do_let(tc, x);
        goto loop;

    // set! expressions
    case SETB_CONT_TYPE:
        // update binding, result is void
        assert_single_value(tc, x);
        do_setb(tc, Mcontinuation_setb_name(Mtc_cc(tc)), x);
        Mtc_cc(tc) = Mcontinuation_prev(Mtc_cc(tc));
        x = Mvoid;
        goto do_k;
    
    // call/cc expressions
    case CALLCC_CONT_TYPE:
        if (Mcontinuation_capturedp(Mtc_cc(tc))) {
            // restoring captured continuation
            Mtc_wnd(tc) = Mcontinuation_callcc_winders(Mtc_cc(tc));
            Mtc_env(tc) = Mcontinuation_env(Mtc_cc(tc));
            Mtc_cc(tc) = Mcontinuation_prev(Mtc_cc(tc));
            goto do_k;
        } else {
            // capturing current continuation
            Mtc_cc(tc) = continuation_mutable(Mtc_cc(tc));
            Mcontinuation_capturedp(Mtc_cc(tc)) = 1;

            argf = do_arg(tc, x);
            do_arg(tc, Mtc_cc(tc));
        
            Mtc_env(tc) = Mcontinuation_env(Mtc_cc(tc));
            Mtc_cc(tc) = Mcontinuation_prev(Mtc_cc(tc));
            goto do_app;
        }
    
    // call-with-values expressions
    case CALLWV_CONT_TYPE:
        Mtc_cc(tc) = continuation_mutable(Mtc_cc(tc));
        if (Mprocp(Mcontinuation_callwv_producer(Mtc_cc(tc)))) {
            // first time => evaluate producer
            argf = do_arg(tc, Mcontinuation_callwv_producer(Mtc_cc(tc)));    
            Mtc_env(tc) = Mcontinuation_env(Mtc_cc(tc));
            Mcontinuation_callwv_producer(Mtc_cc(tc)) = Mfalse;
            goto do_app;
        } else {
            // evaluated producer procedure
            argf = do_arg(tc, Mcontinuation_callwv_consumer(Mtc_cc(tc)));
            if (Mvaluesp(x)) {
                values_to_args(tc);
                clear_values_buffer(tc);
            } else {
                do_arg(tc, x);
            }

            Mtc_env(tc) = Mcontinuation_env(Mtc_cc(tc));
            Mtc_cc(tc) = Mcontinuation_prev(Mtc_cc(tc));
            goto do_app;
        }

    // dynamic-wind expressions
    case DYNWIND_CONT_TYPE:
        switch (Mcontinuation_dynwind_state(Mtc_cc(tc))) {
        // first time
        case DYNWIND_NEW:
            Mtc_cc(tc) = continuation_mutable(Mtc_cc(tc));
            Mcontinuation_dynwind_state(Mtc_cc(tc)) = DYNWIND_PRE;
            
            Mtc_env(tc) = Mcontinuation_env(Mtc_cc(tc));
            argf = do_arg(tc, Mcontinuation_dynwind_pre(Mtc_cc(tc)));
            goto do_app;

        // evaluated pre thunk
        case DYNWIND_PRE:
            Mtc_cc(tc) = continuation_mutable(Mtc_cc(tc));
            Mcontinuation_dynwind_state(Mtc_cc(tc)) = DYNWIND_VAL;
            Mtc_wnd(tc) = Mcons(
                Mcons(
                    Mcontinuation_dynwind_pre(Mtc_cc(tc)),
                    Mcontinuation_dynwind_post(Mtc_cc(tc))
                ),
                Mtc_wnd(tc)
            );
    
            Mtc_env(tc) = Mcontinuation_env(Mtc_cc(tc));
            argf = do_arg(tc, Mcontinuation_dynwind_val(Mtc_cc(tc)));
            goto do_app;

        // evaluated val thunk
        case DYNWIND_VAL:
            Mtc_cc(tc) = continuation_mutable(Mtc_cc(tc));
            if (Mvaluesp(x)) {
                Mcontinuation_dynwind_val(Mtc_cc(tc)) = values_to_list();
                clear_values_buffer(tc);
            } else {
                Mcontinuation_dynwind_val(Mtc_cc(tc)) = Mlist1(x);
            }
            
            Mtc_wnd(tc) = Mcdr(Mtc_wnd(tc));
            Mcontinuation_dynwind_state(Mtc_cc(tc)) = DYNWIND_POST;
            Mtc_env(tc) = Mcontinuation_env(Mtc_cc(tc));
            argf = do_arg(tc, Mcontinuation_dynwind_post(Mtc_cc(tc)));
            goto do_app;

        // evaluated post thunk
        case DYNWIND_POST:
            clear_values_buffer(tc);
            x = list_to_values(tc, Mcontinuation_dynwind_val(Mtc_cc(tc)));
            Mtc_env(tc) = Mcontinuation_env(Mtc_cc(tc));
            Mtc_cc(tc) = Mcontinuation_prev(Mtc_cc(tc));
            goto do_k;
        
        default:
            minim_error("dynamic-wind", "unimplemented");
        }

    // reinstalling winders
    case WINDERS_CONT_TYPE:
        if (Mnullp(Mcontinuation_winders_it(Mtc_cc(tc)))) {
            // no more winders to execute
            x = list_to_values(tc, Mcontinuation_winders_values(Mtc_cc(tc)));
            Mtc_env(tc) = Mcontinuation_env(Mtc_cc(tc));
            Mtc_cc(tc) = Mcontinuation_prev(Mtc_cc(tc));
            goto do_k;
        } else {
            // need to execute a winder
            if (Mfalsep(Mcontinuation_winders_values(Mtc_cc(tc)))) {
                // first time processing winders
                Mtc_cc(tc) = continuation_mutable(Mtc_cc(tc));
                if (Mvaluesp(x)) {
                    Mcontinuation_winders_values(Mtc_cc(tc)) = values_to_list();
                    clear_values_buffer(Mtc_cc(tc));
                } else {
                    Mcontinuation_winders_values(Mtc_cc(tc)) = Mlist1(x);
                }
            }

            // Mtc_env(tc) = Mcontinuation_env(Mtc_cc(tc));
            argf = do_arg(tc, Mcar(Mcontinuation_winders_it(Mtc_cc(tc))));
            Mcontinuation_winders_it(Mtc_cc(tc)) = Mcdr(Mcontinuation_winders_it(Mtc_cc(tc)));
            goto do_app;
        }

    // unknown
    default:
        minim_error1(
            "eval_expr",
            "unimplemented continuation handler",
            Mfixnum(Mcontinuation_type(Mtc_cc(tc)))
        );
    }
}

static void do_imports(obj tc, obj mod) {
    obj es, e, hd, specs;

    for (es = Mmodule_body(mod); !Mnullp(es); es = Mcdr(es)) {
        e = Mcar(es);
        if (Mconsp(e)) {
            hd = Mcar(e);
            if (hd == Mimport_symbol) {
                specs = Mcdr(e);
                while (!Mnullp(specs)) {
                    module_import(Mcar(specs));
                    specs = Mcdr(specs);
                }
            }
        }
    }
}

static void do_exports(obj tc, obj mod) {
    obj es, e, hd, specs;

    for (es = Mmodule_body(mod); !Mnullp(es); es = Mcdr(es)) {
        e = Mcar(es);
        if (Mconsp(e)) {
            hd = Mcar(e);
            if (hd == Mexport_symbol) {
                specs = Mcdr(e);
                while (!Mnullp(specs)) {
                    module_export(mod, Mcar(specs));
                    specs = Mcdr(specs);
                }
            }
        }
    }
}

void do_module_print(obj tc, obj x) {
    if (Mvaluesp(x)) {
        x = values_to_list();
        clear_values_buffer(tc);
    } else {
        x = Mlist1(x);
    }

    for (; !Mnullp(x); x = Mcdr(x)) {
        if (!Mvoidp(x)) {
            writeln_object(Mport_file(Mtc_op(tc)), Mcar(x));
        }
    }
}

void do_module_body(obj mod) {
    obj tc, k0, env, es, e, hd, x;

    tc = Mcurr_tc();
    k0 = Mtc_cc(tc);
    env = Mtc_env(tc);

    // evaluate the body
    for (es = Mmodule_body(mod); !Mnullp(es); es = Mcdr(es)) {
        e = Mcar(es);
        if (Mconsp(e)) {
            hd = Mcar(e);
            if (hd == Mimport_symbol || hd == Mexport_symbol) {
                // skip
                continue;
            } else if (hd == Mdefine_values_symbol) {
                // define-values
                x = eval_k(Mcaddr(e));
                check_let_arity(tc, Mcadr(e), x);
                do_binds(tc, env, Mcadr(e), x);
            } else {
                // expression
                x = eval_k(e);
                do_module_print(tc, x);
            }
        } else {
            // expression
            x = eval_k(e);
            do_module_print(tc, x);
        }

        Mtc_env(tc) = env;
        Mtc_cc(tc) = k0;
    }
}

obj eval_expr(obj e) {
    obj tc, k, env, v, ab;
    uptr aa, ac;

    // check and expand expression
    check_expr(e);
    e = expand_expr(e);

    // stash mutable thread context info
    tc = Mcurr_tc();
    k = Mtc_cc(tc);
    env = Mtc_env(tc);
    ab = Mtc_ab(tc);
    aa = Mtc_aa(tc);
    ac = Mtc_ac(tc);

    // create new argument buffer
    Mtc_aa(tc) = INIT_ARGS_BUFFER_LEN;
    Mtc_ab(tc) = GC_malloc(Mtc_aa(tc) * sizeof(obj));
    Mtc_ac(tc) = 0;

    // evaluate
    Mtc_cc(tc) = Mnull_continuation(env);
    v = eval_k(e);

    // restore mutable thread context info
    Mtc_cc(tc) = k;
    Mtc_env(tc) = env;
    Mtc_ab(tc) = ab;
    Mtc_aa(tc) = aa;
    Mtc_ac(tc) = ac;
    
    return v;
}

void eval_module(obj mod) {
    obj tc, k, env, ab;
    uptr aa, ac;

    // check and expand module
    check_module(mod);
    expand_module(mod);

    // stash mutable thread context info
    tc = Mcurr_tc();
    k = Mtc_cc(tc);
    env = Mtc_env(tc);
    ab = Mtc_ab(tc);
    aa = Mtc_aa(tc);
    ac = Mtc_ac(tc);
    
    // create new argument buffer
    Mtc_aa(tc) = INIT_ARGS_BUFFER_LEN;
    Mtc_ab(tc) = GC_malloc(Mtc_aa(tc) * sizeof(obj));
    Mtc_ac(tc) = 0;

    // set up thread context
    Mtc_env(tc) = empty_env();
    Mtc_cc(tc) = Mnull_continuation(Mtc_env(tc));

    // evaluate
    Mmodule_env(mod) = empty_env();
    do_imports(tc, mod);
    do_module_body(mod);
    do_exports(tc, mod);

    // restore mutable thread context info
    Mtc_cc(tc) = k;
    Mtc_env(tc) = env;
    Mtc_ab(tc) = ab;
    Mtc_aa(tc) = aa;
    Mtc_ac(tc) = ac;
}
