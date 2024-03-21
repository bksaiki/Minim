/*
    Interpreter
*/

#include "../minim.h"

#define SET_NAME_IF_CLOSURE(name, o) { \
    if (minim_closurep(o)) { \
        mobj code = minim_closure_code(o); \
        if (minim_code_name(code) == NULL) { \
            minim_code_name(code) = minim_symbol(name); \
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

    // print name and extract arity
    code = minim_closure_code(proc);
    if (minim_code_name(code))
        fprintf(stderr, "%s: ", minim_code_name(code));

    arity = minim_code_arity(code);
    if (minim_fixnump(arity)) {
        // exact arity
        fprintf(
            stderr, 
            "expected %ld arguments, received %ld\n",
            minim_fixnum(arity),
            actual
        );
    } else {
        // range of arities
        mobj min_arity = minim_car(arity);
        mobj max_arity = minim_cdr(arity);
        if (minim_falsep(max_arity)) {
            // at-least arity
            fprintf(
                stderr, 
                "expected at least %ld arguments, received %ld\n",
                minim_fixnum(min_arity),
                actual
            );
        } else {
            // range arity
            fprintf(
                stderr, 
                "expected between %ld and %ld arguments, received %ld\n",
                minim_fixnum(min_arity),
                minim_fixnum(max_arity),
                actual
            );
        }
    }

    minim_shutdown(1);
}

void result_arity_exn(const char *name, short expected, short actual) {
    if (name == NULL) {
        fprintf(stderr, "result arity mismatch\n");
    } else {
        fprintf(stderr, "%s: result arity mismatch\n", name);
    }

    fprintf(stderr, "  expected: %d, received: %d\n", expected, actual);
    minim_shutdown(1);
}

void uncallable_prim_exn(const char *name) {
    fprintf(stderr, "%s: should not be called directly", name);
    minim_shutdown(1);
}

//
//  Evaluation
//

#define stack_frame_size(th, addt)  ((current_ac(th) + (addt)) * ptr_size)

static int stack_overflowp(size_t argc) {
    minim_thread *th;
    mobj sseg;
    
    th = current_thread();
    sseg = current_segment(th);
    return PTR_ADD(current_sfp(th), stack_frame_size(th, argc)) >= stack_seg_end(sseg);
}

static void grow_stack(size_t size) {
    minim_thread *th;
    mobj sseg;
    size_t req, actual;

    // compute required space
    th = current_thread();
    req = size + stack_seg_header_size;
    actual = (req > stack_seg_default_size) ? req : stack_seg_default_size;

    // allocate the new stack segment
    sseg = Mstack_segment(current_segment(th), actual);
    current_segment(th) = sseg;
    current_sfp(th) = stack_seg_base(sseg);
}

static void maybe_grow_stack(size_t argc) {
    minim_thread *th = current_thread();
    if (stack_overflowp(argc)) {
        grow_stack(stack_frame_size(th, argc));
    }
}

static void push_arg(mobj x) {
    minim_thread *th = current_thread();
    current_sfp(th)[current_ac(th)] = x;
    current_ac(th) += 1;
}

static mobj pop_arg() {
    minim_thread *th = current_thread();
    mobj x = current_sfp(th)[current_ac(th) - 1];
    current_ac(th) -= 1;
    return x;
}

static void push_frame(mobj env, mobj pc) {
    // get current continuation and argument count
    minim_thread *th = current_thread();
    mobj cc = current_continuation(th);
    size_t ac = current_ac(th);

    // update current continuation, frame pointer, and argument count
    cc = Mcontinuation(cc, pc, env, th);
    current_continuation(th) = cc;
    current_sfp(th) = PTR_ADD(current_sfp(th), ac * ptr_size);
    current_cp(th) = NULL;
    current_ac(th) = 0;
}

static mobj pop_frame() {
    // get current continuation
    minim_thread *th = current_thread();
    mobj cc = current_continuation(th);
    mobj *seg = current_segment(th);
    mobj *sfp = current_sfp(th);

    // pop continuation and update thread-local data
    current_continuation(th) = continuation_prev(cc);
    current_sfp(th) = continuation_sfp(cc);
    current_cp(th) = continuation_cp(cc);
    current_ac(th) = continuation_ac(cc);

    // underflow if frame pointer is not in segment
    sfp = current_sfp(th);
    if (!(((void *) sfp) >= stack_seg_base(seg) && ((void *) sfp) < stack_seg_end(seg))) {
        // just pop the current segment
        current_segment(th) = stack_seg_prev(seg);
    }

    return cc;
}

static void check_arity(mobj env, mobj spec) {
    minim_thread *th;
    size_t ac;

    th = current_thread();
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
    
    th = current_thread();
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
        fprintf(stderr, "error: called unsafe primitive with too many arguments\n");
        fprintf(stderr, " received: ");
        write_object(stderr, current_cp(th));
        minim_shutdown(1);
    }
}

static mobj do_rest(size_t idx) {
    minim_thread *th;
    size_t ac;

    th = current_thread();
    ac = current_ac(th);
    if (idx > ac) {
        fprintf(stderr, "do_rest(): starting index out of bounds\n");
        fprintf(stderr, " idx=%ld, ac=%ld\n", idx, ac);
        minim_shutdown(1);
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

static void do_apply() {
    minim_thread *th;
    mobj *sfp, rest;
    size_t i, ac, len;

    th = current_thread();
    sfp = current_sfp(th);
    ac = current_ac(th);

    // the first argument becomes the current procedure
    current_cp(th) = sfp[0];

    // shift arguments by 1 (since `apply` itself is consumed)
    rest = sfp[ac - 1];
    for (i = 0; i < ac - 2; i++)
        sfp[i] = sfp[i + 1];

    // check if we have room for the application
    current_ac(th) -= 2;
    len = list_length(rest);
    if (stack_overflowp(len)) {
        mobj *nsfp;

        grow_stack(stack_frame_size(th, len));
        nsfp = current_sfp(th);
        for (i = 0; i < ac; i++)
            nsfp[i] = sfp[i];
        sfp = nsfp;
    }

    // push rest argument to stack
    for (; !minim_nullp(rest); rest = minim_cdr(rest))
        push_arg(minim_car(rest));
}

static void do_values() {
    minim_thread *th = current_thread();
    mobj *sfp = current_sfp(th);
    size_t ac = current_ac(th);

    // check if the values buffer is large enough
    if (ac >= values_buffer_size(th)) {
        values_buffer_size(th) = 2 * ac;
        values_buffer(th) = GC_realloc(values_buffer(th), values_buffer_size(th) * sizeof(mobj));
    }

    // copy arguments from the stack to the values buffer
    memcpy(values_buffer(th), sfp, ac * sizeof(mobj));
    values_buffer_count(th) = ac;
}

static void values_to_args(mobj x) {
    if (minim_valuesp(x)) {
        // multi-valued
        minim_thread *th = current_thread();
        for (size_t i = 0; i < values_buffer_count(th); i++) {
            push_arg(values_buffer_ref(th, i));
        }
    } else {
        // single-valued
        push_arg(x);
    }
}

// Forces `x` to be a single value, that is,
// if `x` is the result of `(values ...)` it unwraps
// the result if `x` contains one value and panics otherwise
static mobj force_single_value(mobj x) {
    if (minim_valuesp(x)) {
        minim_thread *th = current_thread();
        if (values_buffer_count(th) != 1)
            result_arity_exn(NULL, 1, values_buffer_count(th));

        values_buffer_count(th) = 0;
        return values_buffer_ref(th, 0);
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

        th = current_thread();
        if (count != values_buffer_count(th))
            result_arity_exn(NULL, count, values_buffer_count(th));

        i = 0;
        for (; !minim_nullp(ids); ids = minim_cdr(ids)) {
            SET_NAME_IF_CLOSURE(minim_car(ids), values_buffer_ref(th, i));
            env_define_var(env, minim_car(ids), values_buffer_ref(th, i));
            i += 1;
        }
    } else {
        // single-valued result
        if (count != 1) {
            result_arity_exn(NULL, count, 1);
        } else {
            SET_NAME_IF_CLOSURE(minim_car(ids), res);
            env_define_var(env, minim_car(ids), res);
        }
    }
}

//
//  Evaluator
//

mobj eval_expr(mobj expr, mobj env) {
    minim_thread *th;
    mobj *istream;
    mobj code, ins, ty, res, penv;

    // compile to instructions
    code = compile_expr(expr);

    // set up instruction evaluator
    th = current_thread();
    current_ac(th) = 0;
    istream = minim_code_it(code);
    penv = env;
    res = NULL;

// instruction processor
loop:
    ins = *istream;
    if (!minim_consp(ins)) {
        if (minim_stringp(ins)) {
            // label (skip)
            goto next;
        } else {
            fprintf(stderr, "not bytecode: ");
            write_object(stderr, ins);
            fprintf(stderr, "\n");
            minim_shutdown(1);
        }
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
        res = force_single_value(res);
        current_cp(th) = res;
    } else if (ty == push_symbol) {
        // push
        push_arg(force_single_value(res));
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
        env_define_var(env, minim_cadr(ins), res);
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
        if (current_continuation(th) == minim_null) {
            res = penv;
        } else {
            res = continuation_env(current_continuation(th));
        }
    } else if (ty == do_apply_symbol) {
        // do-apply
        do_apply();
        goto application;
    } else if (ty == do_error_symbol) {
        // do-error
        do_error(current_ac(th), current_sfp(th));
        goto unreachable;
    } else if (ty == do_eval_symbol) {
        // do-eval
        goto do_eval;
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
        fprintf(stderr, "invalid bytecode: ");
        write_object(stderr, ins);
        fprintf(stderr, "\n");
        minim_shutdown(1);
    }

// move to next instruction
// bail if the instruction stream is empty
next:
    istream++;
    if (*istream == NULL) {
        fprintf(stderr, "eval_expr(), instruction sequence unexpectedly ended\n");
        minim_shutdown(1);
    }
    goto loop;

// call closure
call_closure:
    code = minim_closure_code(current_cp(th));
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

// restores previous continuation
restore_frame:
    mobj cc;

    // check if stack is fully unwound
    if (minim_nullp(current_continuation(th))) {
        // this is the only normal exit point from this function
        return res;
    }
    
    // otherwise, restore the old instruction stream and environment
    // and seek to the expected label
    cc = pop_frame();
    env = continuation_env(cc);
    istream = continuation_pc(cc);
    goto loop;

not_procedure:
    fprintf(stderr, "expected procedure\n");
    fprintf(stderr, " got: ");
    write_object(stderr, current_cp(th));
    fprintf(stderr, "\n");
    minim_shutdown(1);

unreachable:
    fprintf(stderr, "unreachable");
    minim_shutdown(1);
}
