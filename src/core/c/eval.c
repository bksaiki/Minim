/*
    Interpreter
*/

#include "../minim.h"

#define SET_NAME_IF_CLOSURE(name, o) { \
    if (minim_closurep(o)) { \
        if (minim_falsep(minim_closure_name(o))) { \
            minim_closure_name(o) = name; \
        } \
    } \
}

//
//  Runtime check
//

void bad_syntax_exn(mobj expr) {
    minim_error1(minim_symbol(minim_car(expr)), "bad syntax", expr);
}

void bad_type_exn(const char *name, const char *type, mobj x) {
    minim_error2(name, "type violation", Mstring(type), x);
}

void arity_mismatch_exn(mobj proc, size_t actual) {
    mobj code, arity;
    char *name;

    if (minim_falsep(minim_closure_name(proc))) {
        name = NULL;
    } else {
        name = minim_symbol(minim_closure_name(proc));
    }

    code = minim_closure_code(proc);
    arity = minim_code_arity(code);
    if (minim_fixnump(arity)) {
        // exact arity
        minim_error2(name, "arity mismatch", arity, Mfixnum(actual));
    } else {
        // range of arities
        mobj min_arity = minim_car(arity);
        mobj max_arity = minim_cdr(arity);
        if (minim_falsep(max_arity)) {
            // at-least arity
            minim_error2(
                name,
                "arity mismatch, expected at least",
                min_arity, Mfixnum(actual)
            );
        } else {
            // range arity
            minim_error3(
                name,
                "arity mismatch, expected between",
                min_arity, max_arity, Mfixnum(actual)
            );
        }
    }
}

void result_arity_exn(const char *name, short expected, short actual) {
    minim_error2(name, "result arity mismatch", Mfixnum(expected), Mfixnum(actual));
}

//
//  Evaluation
//

#define stack_frame_size(th, addt)  ((tc_ac(th) + (addt)) * ptr_size)
#define stack_cushion               (8 * ptr_size)

static int stack_overflowp(mobj tc, size_t size) {
    return PTR_ADD(tc_sfp(tc), size) >= tc_esp(tc);
}

static void grow_stack(mobj tc, size_t size) {
    mobj srecord;
    void *stack;
    size_t req, actual;

    // compute required space
    req = size + STACK_SLOP;
    actual = (req > STACK_SIZE) ? req : STACK_SIZE;

    // allocate stack record for previous segment
    srecord = Mcached_stack(tc_stack_base(tc), tc_stack_link(tc), tc_stack_size(tc), tc_ccont(tc));

    // allocate new stack segment
    stack = GC_alloc(req);
    tc_stack_base(tc) = stack;
    tc_stack_link(tc) = srecord;
    tc_stack_size(tc) = req;
    tc_sfp(tc) = tc_stack_base(tc);
    tc_esp(tc) = tc_sfp(tc) + tc_stack_size(tc) - STACK_SLOP;
}

void reserve_stack(mobj tc, size_t argc) {
    size_t req = argc * ptr_size;
    if (stack_overflowp(tc, req))
        grow_stack(tc, req);
}

void set_arg(mobj tc, size_t i, mobj x) {
    tc_sfp(tc)[i] = x;
}

mobj get_arg(mobj tc, size_t i) {
    return tc_sfp(tc)[i];
}

static void maybe_grow_stack(mobj tc, size_t argc) {
    reserve_stack(tc, tc_ac(tc) + argc);
}

static void push_arg(mobj tc, mobj x) {
    tc_sfp(tc)[tc_ac(tc)] = x;
    tc_ac(tc) += 1;
}

static mobj pop_arg(mobj tc) {
    mobj x = tc_sfp(tc)[tc_ac(tc) - 1];
    tc_ac(tc) -= 1;
    return x;
}

static void push_frame(mobj tc, mobj pc) {
    mobj cc;
    size_t ac;

    // get current continuation and argument count
    cc = tc_ccont(tc);
    ac = tc_ac(tc);

    // update current continuation, frame pointer, and argument count
    tc_ccont(tc) = Mcontinuation(cc, pc, tc_env(tc), tc);
    tc_sfp(tc) = PTR_ADD(tc_sfp(tc), ac * ptr_size);
    tc_cp(tc) = NULL;
    tc_ac(tc) = 0;
}

static mobj pop_frame(mobj tc) {
    mobj cc, srecord;

    // check for underflow
    if (tc_sfp(tc) <= tc_stack_base(tc)) {
        // underflow, so need to unpack the first stack record
        srecord = tc_stack_link(tc);
        tc_stack_base(tc) = cache_stack_base(srecord);
        tc_stack_size(tc) = cache_stack_len(srecord);
        tc_stack_link(tc) = cache_stack_prev(srecord);
        tc_sfp(tc) = tc_stack_base(tc);
        tc_esp(tc) = tc_sfp(tc) + tc_stack_size(tc) - STACK_SLOP;
        cc = cache_stack_ret(srecord);
    } else {
        cc = tc_ccont(tc);
    }

    // update thread parameters
    tc_ccont(tc) = continuation_prev(cc);
    tc_sfp(tc) = continuation_sfp(cc);
    tc_cp(tc) = continuation_cp(cc);
    tc_ac(tc) = continuation_ac(cc);
    return cc;
}

static void check_arity(mobj env, mobj spec) {
    minim_thread *th;
    size_t ac;

    th = current_tc();
    ac = current_ac(th);
    if (minim_fixnump(spec)) {
        // exact arity
        if (ac != minim_fixnum(spec))
            arity_mismatch_exn(current_cp(th), ac);
    } else {
        mobj min = minim_car(spec);
        mobj max = minim_cdr(spec);
        if (minim_falsep(max)) {
            // at least arity
            if (ac < minim_fixnum(min))
                arity_mismatch_exn(current_cp(th), ac);
        } else {
            // range arity
            if (ac < minim_fixnum(min) || ac > minim_fixnum(max))
                arity_mismatch_exn(current_cp(th), ac);
        }
    }
}

static mobj do_ccall(mobj (*prim)()) {
    minim_thread *th;
    mobj *args;
    
    th = current_tc();
    args = current_sfp(th);
    switch (current_ac(th)) {
    case 0:
        return prim();
    case 1:
        return prim(args[0]);
    case 2:
        return prim(args[0], args[1]);
    case 3:
        return prim(args[0], args[1], args[2]);
    case 4:
        return prim(args[0], args[1], args[2], args[3]);
    case 5:
        return prim(args[0], args[1], args[2], args[3], args[4]);
    case 6:
        return prim(args[0], args[1], args[2], args[3], args[4], args[5]);
    default:
        minim_error1(NULL, "cannot call kernel function with 7 or more arguments", current_cp(th));
    }
}

static mobj do_rest(size_t idx) {
    minim_thread *th;
    size_t ac;

    th = current_tc();
    ac = current_ac(th);
    if (idx > ac) {
        minim_error2("do_rest", "base index exceed argument count", Mfixnum(idx), Mfixnum(ac));
    }

    if (idx == ac) {
        // empty
        return minim_null;
    } else {
        mobj hd, tl, *sfp;

        sfp = current_sfp(th);
        hd = tl = Mcons(sfp[idx], NULL);
        for (idx += 1; idx < ac; idx++) {
            minim_cdr(tl) = Mcons(sfp[idx], NULL);
            tl = minim_cdr(tl);
        }

        minim_cdr(tl) = minim_null;
        return hd;
    }
}

static void do_apply(mobj tc) {
    mobj *sfp, *nsfp, rest;
    size_t i, ac, len, req;

    // thread parameters
    sfp = tc_sfp(tc);
    ac = tc_ac(tc);

    // the first argument becomes the current procedure
    tc_cp(tc) = sfp[0];

    // shift arguments by 1 (since `apply` itself is consumed)
    rest = sfp[ac - 1];
    for (i = 0; i < ac - 2; i++)
        sfp[i] = sfp[i + 1];

    // check if we have room for the application
    tc_ac(tc) -= 2;
    len = list_length(rest);
    req = stack_frame_size(tc, len);
    if (stack_overflowp(tc, req)) {
        grow_stack(tc, req);
        nsfp = tc_sfp(tc);
        for (i = 0; i < ac; i++)
            nsfp[i] = sfp[i];
        sfp = nsfp;
    }

    // push rest argument to stack
    for (; !minim_nullp(rest); rest = minim_cdr(rest))
        push_arg(tc, minim_car(rest));
}

static void do_values(mobj tc) {
    tc_vc(tc) = tc_ac(tc);
    tc_values(tc) = GC_alloc(tc_vc(tc) * sizeof(mobj));
    memcpy(tc_values(tc), tc_sfp(tc), tc_vc(tc) * sizeof(mobj));
}

static void values_to_args(mobj tc, mobj x) {
    if (minim_valuesp(x)) {
        // multi-valued
        for (size_t i = 0; i < tc_vc(tc); i++)
            push_arg(tc, tc_values(tc)[i]);
    } else {
        // single-valued
        push_arg(tc, x);
    }
}

// Forces `x` to be a single value, that is,
// if `x` is the result of `(values ...)` it unwraps
// the result if `x` contains one value and panics otherwise
static mobj force_single_value(mobj tc, mobj x) {
    if (tc_vc(tc) != 1)
        result_arity_exn(NULL, 1, tc_vc(tc));

    if (minim_valuesp(x)) {
        return tc_values(tc)[0];
    } else {
        return x;
    }
}

static void bind_values(mobj env, mobj ids, mobj res) {
    size_t count = list_length(ids);
    if (minim_valuesp(res)) {
        // multi-valued result
        minim_thread *th;
        size_t i;

        th = current_tc();
        if (count != values_buffer_count(th))
            result_arity_exn(NULL, count, values_buffer_count(th));

        i = 0;
        for (; !minim_nullp(ids); ids = minim_cdr(ids)) {
            SET_NAME_IF_CLOSURE(minim_car(ids), values_buffer_ref(th, i));
            env_define_var_no_check(env, minim_car(ids), values_buffer_ref(th, i));
            i += 1;
        }
    } else {
        // single-valued result
        if (count != 1) {
            result_arity_exn(NULL, count, 1);
        } else {
            SET_NAME_IF_CLOSURE(minim_car(ids), res);
            env_define_var_no_check(env, minim_car(ids), res);
        }
    }
}

//
//  Evaluator
//

static mobj eval_istream(mobj tc, mobj *istream) {
    mobj tc, ins, ty, res, penv;
    
    // setup interpreter
    tc = current_tc();
    tc_cp(tc) = NULL;
    tc_ac(tc) = 0;
    penv = tc_env(tc);
    res = NULL;

    // push entry continuation frame
    push_frame(tc, minim_null);

    // possibly handle long jump back into the procedure
    if (setjmp(*tc_reentry(tc)) != 0) {
        // long jumped from somewhere mysterious
        // only valid long jumps require an immediate function application
        goto application;
    }

// instruction processor
loop:
    ins = *istream;
    if (!minim_consp(ins)) {
        // TODO: this check should not be required
        if (minim_stringp(ins))
            goto next;
        minim_error1(NULL, "executing non-bytecode", ins);
    }

    ty = minim_car(ins);
    if (ty == literal_symbol) {
        // literal
        res = minim_cadr(ins);
    } else if (ty == lookup_symbol) {
        // lookup
        res = env_lookup_var(env, minim_cadr(ins));
    } else if (ty == set_proc_symbol) {
        // set-proc
        res = force_single_value(tc, res);
        current_cp(tc) = res;
    } else if (ty == push_symbol) {
        // push
        res = force_single_value(res);
        push_arg(res);
    } else if (ty == pop_symbol) {
        // pop
        res = pop_arg();
    } else if (ty == apply_symbol) {
        // apply
application:
        if (minim_closurep(current_cp(th))) {
            goto call_closure;
        } else {
            goto not_procedure;
        }
    } else if (ty == ret_symbol) {
        // ret
        goto restore_frame;
    } else if (ty == ccall_symbol) {
        // ccall
        res = do_ccall(minim_cadr(ins));
    } else if (ty == bind_symbol) {
        // bind
        env_define_var_no_check(env, minim_cadr(ins), res);
        res = minim_void;
    } else if (ty == bind_values_symbol) {
        // bind-values
        bind_values(env, minim_cadr(ins), res);
        res = minim_void;
    } else if (ty == bind_values_top_symbol) {
        // bind-values/top (special variant for `let-values`)
        bind_values(current_sfp(th)[current_ac(th) - 1], minim_cadr(ins), res);
        res = minim_void;
    } else if (ty == rebind_symbol) {
        // rebind
        env_set_var(env, minim_cadr(ins), res);
        res = minim_void;
    } else if (ty == make_env_symbol) {
        // make-env
        res = Menv2(env, minim_fixnum(minim_cadr(ins)));
    } else if (ty == push_env_symbol) {
        // push-env
        env = res;
    } else if (ty == pop_env_symbol) {
        // pop-env
        env = minim_env_prev(env);
    } else if (ty == save_cc_symbol) {
        // save-cc
        push_frame(env, minim_cadr(ins));
    } else if (ty == get_arg_symbol) {
        // get-arg
        res = current_sfp(th)[minim_fixnum(minim_cadr(ins))];
    } else if (ty == get_env_symbol) {
        // get-env
        if (minim_nullp(current_continuation(th))) {
            res = penv;
        } else {
            res = continuation_env(current_continuation(th));
        }
    } else if (ty == do_apply_symbol) {
        // do-apply
        do_apply();
        goto application;
    } else if (ty == do_eval_symbol) {
        // do-eval
        goto do_eval;
    } else if (ty == do_raise_symbol) {
        // do-raise
        goto do_raise;
    } else if (ty == do_rest_symbol) {
        // do-rest
        res = do_rest(minim_fixnum(minim_cadr(ins)));
    } else if (ty == do_values_symbol) {
        // do-values
        do_values();
        res = minim_values;
    } else if (ty == do_with_values_symbol) {
        // do-with-values
        values_to_args(res);
        goto application;
    } else if (ty == clear_frame_symbol) {
        // clear frame
        current_cp(th) = NULL;
        current_ac(th) = 0;
        res = NULL;
    } else if (ty == brancha_symbol) {
        // brancha (jump always)
        istream = minim_cadr(ins);
        goto loop;
    } else if (ty == branchf_symbol) {
        // branchf (jump if #f)
        if (res == minim_false) {
            istream = minim_cadr(ins);
            goto loop;
        }
    } else if (ty == make_closure_symbol) {
        // make-closure
        res = Mclosure(env, minim_cadr(ins));
    } else if (ty == check_stack_symbol) {
        // check stack
        maybe_grow_stack(minim_fixnum(minim_cadr(ins)));
    } else if (ty == check_arity_symbol) {
        // check arity
        check_arity(env, minim_cadr(ins));
    } else {
        minim_error1(NULL, "invalid bytecode", ins);
    }

// move to next instruction
// bail if the instruction stream is empty
next:
    istream++;
    if (*istream == NULL) {
        minim_error(NULL, "bytecode out of bounds");
    }
    goto loop;

// call closure
call_closure:
    mobj code = minim_closure_code(current_cp(th));
    // set instruction stream and env
    istream = minim_code_it(code);
    env = minim_closure_env(current_cp(th));
    // don't clear either the current procedure or argument count
    // since this is required for binding and arity check
    goto loop;

// performs `do-eval` instruction
do_eval:
    // compile expression into a nullary function and
    // call it in tail position
    mobj *args = current_sfp(th);
    env = current_ac(th) == 2 ? args[1] : env;
    current_cp(th) = Mclosure(env, compile_expr(args[0]));
    current_ac(th) = 0;
    goto application;

// performs `do-raise` instruction
do_raise:
    // check if an error handler has been installed
    args = current_sfp(th);
    if (minim_falsep(error_handler(th))) {
        boot_error_proc(minim_false, Mstring("unhandled exception"), args[0]);
    }

    // set procedure to handler and call
    current_cp(th) = error_handler(th);
    goto application;

// restores previous continuation
restore_frame:
    // restore the old instruction stream and environment
    // and seek to the expected label
    mobj cc = pop_frame();
    env = continuation_env(cc);
    istream = continuation_pc(cc);
    if (minim_nullp(istream)) {
        // we reached the entry continuation frame
        // so the "next" thing to do is exit
        return res;
    }

    // otherwise execute at *istream
    goto loop;

not_procedure:
    minim_error1(NULL, "expected procedure", current_cp(th));
}

mobj eval_expr(mobj expr, mobj env) {
    mobj code = compile_expr(expr);
    return eval_istream(minim_code_it(code), env);
}
