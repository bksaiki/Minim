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
    fprintf(stderr, "%s: bad syntax\n", minim_symbol(minim_car(expr)));
    fprintf(stderr, " at: ");
    write_object(stderr, expr);
    fputc('\n', stderr);
    minim_shutdown(1);
}

void bad_type_exn(const char *name, const char *type, mobj x) {
    fprintf(stderr, "%s: type violation\n", name);
    fprintf(stderr, " expected: %s\n", type);
    fprintf(stderr, " received: ");
    write_object(stderr, x);
    fputc('\n', stderr);
    minim_shutdown(1);
}

void arity_mismatch_exn(mobj proc, size_t actual) {
    mobj code, arity;

    // print name and extract arity
    if (minim_primp(proc)) {
        if (minim_prim_name(proc))
            fprintf(stderr, "%s: ", minim_prim_name(proc));
        arity = minim_prim_arity(proc);
    } else { // closure
        code = minim_closure_code(proc);
        if (minim_code_name(code))
            fprintf(stderr, "%s: ", minim_code_name(code));
        arity = minim_code_arity(code);
    }

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

static void do_apply() {
    minim_thread *th;
    mobj *sfp, rest;
    size_t i, ac, len;

    th = current_thread();
    sfp = current_sfp(th);
    ac = current_ac(th);

    // first, shift arguments by 1 (since `apply` itself is consumed)
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

static void check_call_with_values(mobj *args) {
    mobj producer, consumer;
    
    producer = args[0];
    if (!minim_procp(producer))
        bad_type_exn("call-with-values", "procedure?", producer);

    consumer = args[1];
    if (!minim_procp(consumer))
        bad_type_exn("call-with-values", "procedure?", consumer);
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

static long let_values_env_size(mobj e) {
    mobj bindings, bind;
    long var_count = 0;
    for (bindings = minim_cadr(e); !minim_nullp(bindings); bindings = minim_cdr(bindings)) {
        bind = minim_car(bindings);
        var_count += list_length(minim_car(bind));
    }
    return var_count;
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
        // at least arity
        if (ac < minim_fixnum(minim_car(spec)))
            arity_mismatch_exn(current_cp(th), ac);
    }
}

//
//  Evaluator
//

mobj eval_expr(mobj expr, mobj env) {
    minim_thread *th;
    mobj code, istream, ins, ty, res, label;

    // compile to instructions
    code = compile_jit(expr);
    write_object(stderr, code_to_instrs(code));
    fprintf(stderr, "\n");

    // set up instruction evaluator
    th = current_thread();
    current_ac(th) = 0;
    istream = minim_code_ref(code, 0);
    res = NULL;

// instruction processor
loop:
    ins = minim_car(istream);
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

    fprintf(stderr, "eval: ");
    write_object(stderr, ins);
    if (res) {
        fprintf(stderr, " res=");
        write_object(stderr, res);
    }
    fprintf(stderr, "\n");

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
        maybe_grow_stack(1);
        push_arg(res);
    } else if (ty == pop_symbol) {
        // pop
        res = pop_arg();
    } else if (ty == apply_symbol) {
        // apply
        if (minim_primp(current_cp(th))) {
            goto call_prim;
        } else if (minim_closurep(current_cp(th))) {
            goto call_closure;
        } else {
            goto not_procedure;
        }
    } else if (ty == ret_symbol) {
        // ret
        goto restore_frame;
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
        push_frame(env, Mcons(istream, minim_cadr(ins)));
    } else if (ty == get_arg_symbol) {
        // get-arg
        res = current_sfp(th)[minim_fixnum(minim_cadr(ins))];
    } else if (ty == do_rest_symbol) {
        // do-rest
        res = do_rest(minim_fixnum(minim_cadr(ins)));
    } else if (ty == clear_frame_symbol) {
        // clear frame
        current_cp(th) = NULL;
        current_ac(th) = 0;
        res = NULL;
    } else if (ty == brancha_symbol) {
        // brancha (jump always)
        label = minim_cadr(ins);
        goto find_label;
    } else if (ty == branchf_symbol) {
        // branchf (jump if #f)
        if (res == minim_false) {
            label = minim_cadr(ins);
            goto find_label;
        }
    } else if (ty == make_closure_symbol) {
        // make-closure
        res = Mclosure(env, minim_cadr(ins));
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
    istream = minim_cdr(istream);
    if (minim_nullp(istream)) {
        fprintf(stderr, "eval_expr(), instruction sequence unexpectedly ended\n");
        minim_shutdown(1);
    }
    goto loop;

// advances until it finds a label
find_label:
    istream = minim_cdr(istream);
    if (minim_nullp(istream)) {
        // exhausted instruction stream
        fprintf(stderr, "unable to find label: ");
        write_object(stderr, label);
        fprintf(stderr, "\n");
        minim_shutdown(1);
    } else if (minim_car(istream) == label) {
        // found label
        goto next;
    }
    
    goto find_label;

// restores previous continuation
restore_frame:
    mobj cc, pc;

    // check if stack is fully unwound
    if (minim_nullp(current_continuation(th)))
        return res;
    
    // otherwise, restore the old instruction stream and environment
    // and seek to the expected label
    cc = pop_frame();
    pc = continuation_pc(cc);
    istream = minim_car(pc);
    label = minim_cdr(pc);
    env = continuation_env(cc);
    goto find_label;

// call closure
call_closure:
    code = minim_closure_code(current_cp(th));
    // set instruction stream and env
    istream = minim_code_ref(code, 0);
    env = minim_closure_env(current_cp(th));
    // don't clear either the current procedure or argument count
    // since this is required for binding and arity check
    goto loop;

// call primitive procedure
call_prim:
    mobj *args;
    mobj (*prim)();

    // check arity
    check_arity(env, minim_prim_arity(current_cp(th)));

    // split on special cases
    prim = minim_prim(current_cp(th));
    args = current_sfp(th);
    if (prim == values_proc) {
        // special case: `values`
        do_values();
        res = minim_values;
    } else if (prim == error_proc) {
        // special case: `error`
        do_error(current_ac(th), args);
        fprintf(stderr, "unreachable\n");
        minim_shutdown(1);
    } else if (prim == syntax_error_proc) {
        // special case: `syntax-error`
        do_syntax_error(current_ac(th), args);
        fprintf(stderr, "unreachable\n");
        minim_shutdown(1);
    } else {
        // default: call based on arity
        switch (current_ac(th)) {
        case 0:
            res = prim();
            break;
        case 1:
            res = prim(args[0]);
            break;
        case 2:
            res = prim(args[0], args[1]);
            break;
        case 3:
            res = prim(args[0], args[1], args[2]);
            break;
        case 4:
            res = prim(args[0], args[1], args[2], args[3]);
            break;
        case 5:
            res = prim(args[0], args[1], args[2], args[3], args[4]);
            break;
        case 6:
            res = prim(args[0], args[1], args[2], args[3], args[4], args[5]);
            break;
        default:
            fprintf(stderr, "error: called unsafe primitive with too many arguments\n");
            fprintf(stderr, " received: ");
            write_object(stderr, current_cp(th));
            minim_shutdown(1);
            break;
        }
    }

    // clear arguments and pop frame
    current_cp(th) = NULL;
    current_ac(th) = 0;
    goto restore_frame;

not_procedure:
    fprintf(stderr, "expected procedure\n");
    fprintf(stderr, " got: ");
    write_object(stderr, current_cp(th));
    fprintf(stderr, "\n");
    minim_shutdown(1);
}
