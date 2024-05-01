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

#define stack_frame_size(th, addt)  ((1 + tc_ac(th) + (addt)) * ptr_size)
#define stack_cushion               (8 * ptr_size)

static int stack_overflowp(mobj tc, size_t size) {
    return (uintptr_t) ptr_add(tc_sfp(tc), size) >= (uintptr_t) tc_esp(tc);
}

static void grow_stack(mobj tc, size_t size) {
    mobj srecord;
    void *stack;
    size_t req, actual;

    // compute required space
    req = size + stack_slop;
    actual = (req > stack_size) ? req : stack_size;

    // allocate stack record for previous segment
    srecord = Mcached_stack(tc_stack_base(tc), tc_stack_link(tc), tc_stack_size(tc), tc_ccont(tc));

    // allocate new stack segment
    stack = GC_alloc(actual);
    tc_stack_base(tc) = stack;
    tc_stack_link(tc) = srecord;
    tc_stack_size(tc) = actual;
    tc_sfp(tc) = tc_stack_base(tc);
    tc_esp(tc) = tc_sfp(tc) + tc_stack_size(tc) - stack_slop;
}

void reserve_stack(mobj tc, size_t argc) {
    size_t req = (1 + argc) * ptr_size;
    if (stack_overflowp(tc, req))
        grow_stack(tc, req);
}

void set_arg(mobj tc, size_t i, mobj x) {
    tc_frame_ref(tc, i) = x;
}

mobj get_arg(mobj tc, size_t i) {
    return tc_frame_ref(tc, i);
}

static void maybe_grow_stack(mobj tc, size_t argc) {
    reserve_stack(tc, tc_ac(tc) + argc);
}

static void push_arg(mobj tc, mobj x) {
    tc_frame_ref(tc, tc_ac(tc)) = x;
    tc_ac(tc) += 1;
}

static mobj pop_arg(mobj tc) {
    mobj x = tc_frame_ref(tc, tc_ac(tc) - 1);
    tc_ac(tc) -= 1;
    return x;
}

static void push_frame(mobj tc, mobj pc) {
    mobj cc;
    size_t size;

    // get current continuation and frame size
    cc = tc_ccont(tc);
    size = 1 + tc_ac(tc);

    // update current continuation, frame pointer, and argument count
    tc_ccont(tc) = Mcontinuation(cc, pc, tc_env(tc), tc);
    tc_sfp(tc) = ptr_add(tc_sfp(tc), size * ptr_size);
    tc_ra(tc) = pc;
    tc_cp(tc) = minim_void;
    tc_ac(tc) = 0;
}

static mobj do_ccall(mobj tc, mobj (*prim)()) {
    mobj *args = tc_frame(tc);
    switch (tc_ac(tc)) {
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
        minim_error1(NULL, "cannot call kernel function with 7 or more arguments", tc_cp(tc));
    }
}

static mobj do_rest(mobj tc, size_t idx) {
    size_t ac = tc_ac(tc);
    if (idx > ac) {
        minim_error2(
            "do_rest",
            "base index exceed argument count",
            Mfixnum(idx),
            Mfixnum(ac)
        );
    }

    if (idx == ac) {
        // empty
        return minim_null;
    } else {
        mobj hd, tl;

        hd = tl = Mcons(tc_frame_ref(tc, idx), NULL);
        for (idx += 1; idx < ac; idx++) {
            minim_cdr(tl) = Mcons(tc_frame_ref(tc, idx), NULL);
            tl = minim_cdr(tl);
        }

        minim_cdr(tl) = minim_null;
        return hd;
    }
}

static void do_apply(mobj tc) {
    mobj *sfp, rest;
    size_t i, ac, req;

    // thread parameters
    sfp = tc_sfp(tc);
    ac = tc_ac(tc);

    // the first argument becomes the current procedure
    tc_cp(tc) = tc_frame_ref(tc, 0);
    if (!minim_procp(tc_cp(tc))) {
        bad_type_exn("apply", "list?", tc_cp(tc));
    }

    // save rest argument (needs to be a list)
    rest = tc_frame_ref(tc, ac - 1);
    if (!minim_listp(rest)) {
        bad_type_exn("apply", "list?", rest);
    }

    // shift arguments by 1 (since `apply` itself is consumed)
    tc_ac(tc) -= 2;
    for (i = 0; i < ac - 2; i++)
        tc_frame_ref(tc, i) = tc_frame_ref(tc, i + 1);

    // check if we have room for the application
    req = stack_frame_size(tc, list_length(rest));
    if (stack_overflowp(tc, req)) {
        grow_stack(tc, req);
        for (i = 0; i < ac; i++)
            tc_frame_ref(tc, i) = sfp[i + 1]; 
    }

    // push rest argument to stack
    for (; !minim_nullp(rest); rest = minim_cdr(rest))
        push_arg(tc, minim_car(rest));
}

static mobj do_values(mobj tc) {
    tc_vc(tc) = tc_ac(tc);
    if (tc_ac(tc) == 0) {
        tc_values(tc) = NULL;
        return minim_values;
    } else if (tc_ac(tc) == 1) {
        tc_values(tc) = NULL;
        return tc_frame_ref(tc, 0);
    } else {
        tc_values(tc) = GC_alloc(tc_vc(tc) * sizeof(mobj));
        memcpy(tc_values(tc), tc_frame(tc), tc_vc(tc) * sizeof(mobj));
        return minim_values;
    }
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
    if (minim_valuesp(x))
        result_arity_exn(NULL, 1, tc_vc(tc));
    return x;
}

// static void bind_values(mobj tc, mobj env, mobj ids, mobj res) {
//     size_t i, count;

//     count = list_length(ids);
//     if (minim_valuesp(res)) {
//         // multi-valued result
//         if (tc_vc(tc) != count)
//             result_arity_exn(NULL, count, tc_vc(tc));

//         for (i = 0; i < count; i++) {
//             SET_NAME_IF_CLOSURE(minim_car(ids), tc_values(tc)[i]);
//             env_define_var_no_check(env, minim_car(ids), tc_values(tc)[i]);
//             ids = minim_cdr(ids);
//         }
//     } else {
//         // single-valued result
//         if (count != 1)
//             result_arity_exn(NULL, count, 1);

//         SET_NAME_IF_CLOSURE(minim_car(ids), res);
//         env_define_var_no_check(env, minim_car(ids), res);
//     }
// }

static void env_bind_values(mobj tc, mobj idx, mobj valc, mobj ids, mobj val) {
    mobj bindings;
    size_t count, bidx;
    
    count = minim_fixnum(valc);
    if (minim_valuesp(val)) {
        // multi-valued result
        if (tc_vc(tc) != count) {
            result_arity_exn(NULL, count, tc_vc(tc));
        }

        bindings = minim_env_bindings(tc_env(tc));
        bidx = minim_fixnum(idx);
        for (size_t i = 0; i < count; i++) {
            mobj val = tc_values(tc)[i];
            SET_NAME_IF_CLOSURE(minim_car(ids), val);
            minim_vector_ref(bindings, bidx) = Mcons(minim_car(ids), val);
            ids = minim_cdr(ids);
            bidx += 1;
        }
    } else {
        // single-valued result
        if (count != 1) {
            result_arity_exn(NULL, count, 1);
        }

        bindings = minim_env_bindings(tc_env(tc));
        SET_NAME_IF_CLOSURE(minim_car(ids), val);
        bidx = minim_fixnum(idx);
        minim_vector_ref(bindings, bidx) = Mcons(minim_car(ids), val);
    }
}

static void env_bind(mobj tc, mobj idx, mobj id, mobj val) {
    mobj bindings = minim_env_bindings(tc_env(tc));
    minim_vector_ref(bindings, minim_fixnum(idx)) = Mcons(id, val);
}

static mobj env_lookup(mobj tc, mobj idx) {
    mobj cell = vector_ref(minim_env_bindings(tc_env(tc)), idx);
    if (minim_cdr(cell) == minim_unbound) {
        minim_error1(
            NULL,
            "cannot use before initialization",
            minim_car(cell)
        );
    }

    return cell;
}

static void tl_env_bind_values(mobj tc, mobj valc, mobj ids, mobj val) {
    size_t count = minim_fixnum(valc);
    if (minim_valuesp(val)) {
        // multi-valued result
        if (tc_vc(tc) != count) {
            result_arity_exn(NULL, count, tc_vc(tc));
        }

        for (size_t i = 0; i < count; i++) {
            mobj val = tc_values(tc)[i];
            SET_NAME_IF_CLOSURE(minim_car(ids), val);
            top_env_insert(tc_tenv(tc), minim_car(ids), val);
            ids = minim_cdr(ids);
        }
    } else {
        // single-valued result
        if (count != 1) {
            result_arity_exn(NULL, count, 1);
        }

        SET_NAME_IF_CLOSURE(minim_car(ids), val);
        top_env_insert(tc_tenv(tc), minim_car(ids), val);
    }
}

static mobj env_tl_lookup(mobj tc, mobj id) {
    mobj cell = top_env_find(tc_tenv(tc), id);
    if (!minim_falsep(cell)) {
        cell = minim_car(cell); // extract (<id> . <value>) cell
        if (minim_cdr(cell) == minim_unbound)
            minim_error1(NULL, "cannot use before initialization", id);
        return cell;
    }

    minim_error1(NULL, "unbound variable", id);
}

static mobj load_reg(mobj tc, mobj *tregs, mobj ins, mobj idx) {
    switch (minim_fixnum(idx)) {
        case 0:
            return tregs[0];
        case 1:
            return tregs[1];
        case 2:
            return tregs[2];
        case 3:
            return tc_cp(tc);
        case 4:
            return Mfixnum(tc_ac(tc));
        default:
            minim_error1(NULL, "invalid register", ins);
    }
}

static void store_reg(mobj tc, mobj *tregs, mobj ins, mobj idx, mobj val) {
    switch (minim_fixnum(idx)) {
        case 0:
            tregs[0] = val;
            break;
        case 1:
            tregs[1] = val;
            break;
        case 2:
            tregs[2] = val;
            break;
        case 3:
            tc_cp(tc) = val;
            break;
        case 4:
            tc_ac(tc) = minim_fixnum(val);
            break;
        default:
            minim_error1(NULL, "invalid register", ins);
    }
}

//
//  Evaluator
//

static mobj eval_istream(mobj tc, mobj *istream) {
    mobj ins, ty;   // instruction stream, instruction type
    mobj cc;        // current continuation
    mobj penv;      // run-time environment upon entry

    // caller saved registers:
    //  %r0 [%res] - result
    //  %r1 [%t0] - scratch
    //  %r2 [%t1] - scratch
    //  %r3 [%cp] - current procedure
    //  %r4 [%ac] - argument count / scratch
    mobj tregs[tmp_reg_count];

    // temporary (non-register) locations
    mobj v0, v1;
    
    // setup interpreter
    tc = current_tc();
    tc_ra(tc) = NULL;
    tc_cp(tc) = minim_void;
    tc_ac(tc) = 0;

    // need to stash for when we exit this function
    penv = tc_env(tc);

    // possibly handle long jump back into the procedure
    if (setjmp(*tc_reentry(tc)) != 0) {
        // long jumped from somewhere mysterious
        // valid long jumps require an immediate function application
        goto application;
    }

// instruction processor
loop:
    ins = *istream;
    if (!minim_consp(ins)) {
        minim_error1(NULL, "executing non-bytecode", ins);
    }

    ty = minim_car(ins);
    if (ty == literal_symbol) {
        // literal
        tregs[0] = minim_cadr(ins);
    } else if (ty == lookup_symbol) {
        // lookup
        tregs[0] = minim_cdr(env_lookup(tc, minim_cadr(ins)));
    } else if (ty == lookup_cell_symbol) {
        // lookup-cell
        tregs[0] = env_lookup(tc, minim_cadr(ins));
    } else if (ty == tl_lookup_symbol) {
        // top-level lookup
        tregs[0] = minim_cdr(env_tl_lookup(tc, minim_cadr(ins)));
    } else if (ty == tl_lookup_cell_symbol) {
        // top-level lookup
        tregs[0] = env_tl_lookup(tc, minim_cadr(ins));
    } else if (ty == set_proc_symbol) {
        // set-proc
        tc_cp(tc) = force_single_value(tc, tregs[0]);
    } else if (ty == push_symbol) {
        // push
        tregs[0] = force_single_value(tc, tregs[0]);
        push_arg(tc, tregs[0]);
    } else if (ty == pop_symbol) {
        // pop
        tregs[0] = pop_arg(tc);
    } else if (ty == mov_symbol) {
        // move
        v0 = load_reg(tc, tregs, ins, minim_car(minim_cddr(ins)));
        store_reg(tc, tregs, ins, minim_cadr(ins), v0);
    } else if (ty == closure_set_symbol) {
        // closure-set!
        v0 = load_reg(tc, tregs, ins, minim_cadr(ins));
        v1 = load_reg(tc, tregs, ins, minim_cadr(minim_cddr(ins)));
        minim_closure_ref(v0, minim_fixnum(minim_car(minim_cddr(ins)))) = v1;
    } else if (ty == apply_symbol) {
        // apply
application:
        if (minim_closurep(tc_cp(tc))) {
            goto call_closure;
        } else {
            goto not_procedure;
        }
    } else if (ty == ret_symbol) {
        // ret
        goto restore_frame;
    } else if (ty == ccall_symbol) {
        // ccall
        tregs[0] = do_ccall(tc, minim_cadr(ins));
    } else if (ty == bind_symbol) {
        // bind
        env_bind(tc, minim_cadr(ins), minim_car(minim_cddr(ins)), tregs[0]);
        tregs[0] = minim_void;
    } else if (ty == bind_values_symbol) {
        // bind-values
        env_bind_values(tc, minim_cadr(ins), minim_car(minim_cddr(ins)), minim_cadr(minim_cddr(ins)), tregs[0]);
        tregs[0] = minim_void;
    } else if (ty == tl_bind_values_symbol) {
        // tl-bind-values
        tl_env_bind_values(tc, minim_cadr(ins), minim_car(minim_cddr(ins)), tregs[0]);
        tregs[0] = minim_void;
    } else if (ty == rebind_symbol) {
        // rebind
        env_set_var(tc_env(tc), minim_cadr(ins), tregs[0]);
        tregs[0] = minim_void;
    } else if (ty == make_env_symbol) {
        // make-env
        tregs[0] = Menv(tc_env(tc), minim_fixnum(minim_cadr(ins)));
    } else if (ty == push_env_symbol) {
        // push-env
        tc_env(tc) = Menv(tc_env(tc), minim_fixnum(minim_cadr(ins)));
    } else if (ty == pop_env_symbol) {
        // pop-env
        tc_env(tc) = minim_env_prev(tc_env(tc));
    } else if (ty == save_cc_symbol) {
        // save-cc
        push_frame(tc, (mobj*) minim_fixnum(minim_cadr(ins)));
    } else if (ty == get_ac_symbol) {
        // get-ac
        tregs[0] = (mobj*) tc_ac(tc);
    } else if (ty == get_arg_symbol) {
        // get-arg
        tregs[0] = tc_frame_ref(tc, minim_fixnum(minim_cadr(ins)));
    } else if (ty == set_arg_symbol) {
        // set-arg
        tc_frame_ref(tc, minim_fixnum(minim_cadr(ins))) = tregs[0];
    } else if (ty == get_tenv_symbol) {
        // get-tenv
        tregs[0] = tc_tenv(tc);
    } else if (ty == set_tenv_symbol) {
        // set-tenv
        tc_tenv(tc) = tregs[0];
    } else if (ty == do_apply_symbol) {
        // do-apply
        do_apply(tc);
        goto application;
    } else if (ty == do_arity_error_symbol) {
        // do-apply
        arity_mismatch_exn(tc_cp(tc), tc_ac(tc));
    } else if (ty == do_eval_symbol) {
        // do-eval
        tregs[0] = Mclosure(tc_tenv(tc), compile_expr(tc_frame_ref(tc, 0)), 0);
    } else if (ty == do_raise_symbol) {
        // do-raise
        goto do_raise;
    } else if (ty == do_rest_symbol) {
        // do-rest
        tregs[0] = do_rest(tc, minim_fixnum(minim_cadr(ins)));
    } else if (ty == do_values_symbol) {
        // do-values
        tregs[0] = do_values(tc);
    } else if (ty == do_with_values_symbol) {
        // do-with-values
        values_to_args(tc, tregs[0]);
    } else if (ty == clear_frame_symbol) {
        // clear frame
        tc_cp(tc) = minim_void;
        tc_ac(tc) = 0;
    } else if (ty == brancha_symbol) {
        // brancha (jump always)
        istream = (mobj*) minim_fixnum(minim_cadr(ins));
        goto loop;
    } else if (ty == branchf_symbol) {
        // branchf (jump if #f)
        if (tregs[0] == minim_false) {
            istream = (mobj*) minim_fixnum(minim_cadr(ins));
            goto loop;
        }
    } else if (ty == branchgt_symbol) {
        // branchgt (jump if greater than)
        if (((mfixnum) tregs[0]) > minim_fixnum(minim_cadr(ins))) {
            istream = (mobj*) minim_fixnum(minim_car(minim_cddr(ins)));
            goto loop;
        }
    } else if (ty == branchlt_symbol) {
        // branchlt (jump if less than)
        if (((mfixnum) tregs[0]) < minim_fixnum(minim_cadr(ins))) {
            istream = (mobj*) minim_fixnum(minim_car(minim_cddr(ins)));
            goto loop;
        }
    } else if (ty == branchne_symbol) {
        // branchne (jump if not equal)
        if (((mfixnum) tregs[0]) != minim_fixnum(minim_cadr(ins))) {
            istream = (mobj*) minim_fixnum(minim_car(minim_cddr(ins)));
            goto loop;
        }
    } else if (ty == make_closure_symbol) {
        // make-closure
        tregs[0] = Mclosure(tc_env(tc), minim_cadr(ins), minim_fixnum(minim_car(minim_cddr(ins))));
    } else if (ty == check_stack_symbol) {
        // check stack
        maybe_grow_stack(tc, minim_fixnum(minim_cadr(ins)));
    } else {
        minim_error1(NULL, "invalid bytecode", ins);
    }

// move to next instruction
// bail if the instruction stream is empty
    istream++;
    if (*istream == NULL) {
        minim_error(NULL, "bytecode out of bounds");
    }
    goto loop;

// call closure
call_closure:
    // set instruction stream and env
    istream = minim_code_it(minim_closure_code(tc_cp(tc)));
    tc_env(tc) = minim_closure_env(tc_cp(tc));
    // don't clear either the current procedure or argument count
    // since this is required for binding and arity check
    goto loop;

// performs `do-raise` instruction
do_raise:
    // check if an error handler has been installed
    if (minim_falsep(tc_error_handler(tc))) {
        boot_error_proc(
            minim_false,
            Mstring("unhandled exception"),
            tc_frame_ref(tc, 0)
        );
    }

    // set procedure to handler and call
    tc_cp(tc) = tc_error_handler(tc);
    goto application;

// restores previous continuation
restore_frame:
    if (tc_sfp(tc) <= tc_stack_base(tc)) {
        // we underflowed, so we need to unpack the previous stack
        mobj srecord = tc_stack_link(tc);
        if (minim_nullp(srecord)) {
            // no previous stack so we should exit
            // make sure to pop entry frame
            tc_env(tc) = penv;
            return tregs[0];
        }

        tc_stack_base(tc) = cache_stack_base(srecord);
        tc_stack_size(tc) = cache_stack_len(srecord);
        tc_stack_link(tc) = cache_stack_prev(srecord);
        tc_sfp(tc) = tc_stack_base(tc);
        tc_esp(tc) = tc_sfp(tc) + tc_stack_size(tc) - stack_slop;
        cc = cache_stack_ret(srecord);
    } else {
        cc = tc_ccont(tc);
    }

    // restore instruction stream and environment
    istream = tc_ra(tc);
    tc_env(tc) = continuation_env(cc);

    // update thread parameters
    tc_ccont(tc) = continuation_prev(cc);
    tc_sfp(tc) = continuation_sfp(cc);
    tc_cp(tc) = continuation_cp(cc);
    tc_ac(tc) = continuation_ac(cc);
    goto loop;

not_procedure:
    minim_error1(NULL, "expected procedure", tc_cp(tc));
}

mobj eval_expr(mobj tc, mobj expr) {
    mobj code = compile_expr(expr);
    return eval_istream(tc, minim_code_it(code));
}
