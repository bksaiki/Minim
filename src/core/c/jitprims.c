// jitprims.c: JIT primitives

#include "../minim.h"

// Common idiom for custom compilation targets
static mobj compile_do_ret(mobj name, mobj arity, mobj do_instr) {
    mobj env, label, ins, reloc, code, cl;

    // prepare compiler
    env = make_cenv(make_global_cenv());
    label = cenv_make_label(env);
    
    // hand written procedure
    ins = Mlist6(
        Mlist1(get_ac_symbol),
        Mlist3(branchne_symbol, arity, label),
        do_instr,
        Mlist1(ret_symbol),
        label,
        Mlist1(do_arity_error_symbol)
    );

    // write to code
    reloc = resolve_refs(env, ins);
    code = write_code(ins, reloc, arity);

    // return a closure
    cl = Mclosure(base_env, code, 0);
    minim_closure_name(cl) = name;
    return cl;
}

mobj compile_prim(const char *who, void *fn, mobj arity) {
    return compile_do_ret(intern(who), arity, Mlist2(ccall_symbol, Mfixnum((intptr_t) fn)));
}

// Short hand for making a function that just calls `compile_do_ret`
#define define_do_ret(fn_name, arity, do_instr) \
    mobj fn_name(mobj name) { \
        return compile_do_ret(name, arity, do_instr); \
    }

define_do_ret(compile_identity, Mfixnum(1), Mlist2(get_arg_symbol, Mfixnum(0)))
define_do_ret(compile_raise, Mfixnum(1), Mlist1(do_raise_symbol))
define_do_ret(compile_void, Mfixnum(0), Mlist2(literal_symbol, minim_void))

// Custom `apply` compilation
mobj compile_apply(mobj name) {
    mobj env, label, ins, reloc, code, cl;

    // prepare compiler
    env = make_cenv(make_global_cenv());
    label = cenv_make_label(env);
    
    // hand written procedure
    ins = Mlist6(
        Mlist1(get_ac_symbol),
        Mlist3(branchlt_symbol, Mfixnum(2), label),
        Mlist1(do_apply_symbol),
        Mlist1(ret_symbol),
        label,
        Mlist1(do_arity_error_symbol)
    );

    // write to code
    reloc = resolve_refs(env, ins);
    code = write_code(ins, reloc, Mcons(Mfixnum(2), minim_false));

    // return a closure
    cl = Mclosure(base_env, code, 0);
    minim_closure_name(cl) = name;
    return cl;
}

// Custom `values` compilation
mobj compile_values(mobj name) {
    mobj env, ins, reloc, code, cl;

    // prepare compiler
    env = make_cenv(make_global_cenv());
    
    // hand written procedure
    ins = Mlist2(
        Mlist1(do_values_symbol),
        Mlist1(ret_symbol)
    );

    // write to code
    reloc = resolve_refs(env, ins);
    code = write_code(ins, reloc, Mcons(Mfixnum(0), minim_false));

    // return a closure
    cl = Mclosure(base_env, code, 0);
    minim_closure_name(cl) = name;
    return cl;
}

// Custom `eval` compilation
mobj compile_eval(mobj name) {
    mobj env, Larity2, Lpop_tenv, Larity_err, ins, reloc, code, cl;

    // prepare compiler
    env = make_cenv(make_global_cenv());
    Larity2 = cenv_make_label(env);
    Larity_err = cenv_make_label(env);
    Lpop_tenv = cenv_make_label(env);
    
    // hand written
    ins = list_append2(
        Mlist2(
            Mlist1(get_ac_symbol),
            Mlist3(branchne_symbol, Mfixnum(1), Larity2)
        ),
        // %ac == 1 (in tail position)
        list_append2(Mlist4(
            Mlist1(do_eval_symbol),
            Mlist1(clear_frame_symbol),
            Mlist1(set_proc_symbol),
            Mlist1(apply_symbol)
        ),
        // %ac == 2 (not in tail position since we need to restore top-level env)
        // need to use an additional stack frame location for swapping
        //  sfp[2]: env
        //  sfp[1]: expr
        list_append2(
            list_append2(Mlist6(
                Larity2,
                Mlist3(branchne_symbol, Mfixnum(2), Larity_err),
                Mlist2(get_arg_symbol, Mfixnum(1)),
                Mlist2(set_arg_symbol, Mfixnum(2)), // new %tenv at sfp[3]
                Mlist1(get_tenv_symbol),
                Mlist2(set_arg_symbol, Mfixnum(1)) // old %tenv at sfp[2]
            ),
            list_append2(Mlist6(
                Mlist2(get_arg_symbol, Mfixnum(2)),
                Mlist1(set_tenv_symbol),
                Mlist1(do_eval_symbol),
                Mlist2(save_cc_symbol, Lpop_tenv),
                Mlist1(set_proc_symbol),
                Mlist1(apply_symbol)
            ),
            Mlist6(
                Lpop_tenv,
                Mlist2(set_arg_symbol, Mfixnum(0)), // result at sfp[1]
                Mlist2(get_arg_symbol, Mfixnum(1)), // old %tenv
                Mlist1(set_tenv_symbol),
                Mlist2(get_arg_symbol, Mfixnum(0)), // fetch result
                Mlist1(ret_symbol)
            ))
        ),
        // %ac == 0 or %ac > 2
        Mlist2(
            Larity_err,
            Mlist1(do_arity_error_symbol)
        )
    )));

    // write to code
    reloc = resolve_refs(env, ins);
    code = write_code(ins, reloc, Mlist2(Mfixnum(1), Mfixnum(2)));

    // return a closure
    cl = Mclosure(base_env, code, 0);
    minim_closure_name(cl) = name;
    return cl;
}
