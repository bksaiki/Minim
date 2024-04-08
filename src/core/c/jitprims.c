// jitprims.c: JIT primitives

#include "../minim.h"

// Common idiom for custom compilation targets
static mobj compile_do_ret(mobj name, mobj arity, mobj do_instr) {
    mobj env, label, ins, reloc, code, cl;

    // prepare compiler
    env = make_cenv();
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
    cl = Mclosure(base_env, code);
    minim_closure_name(cl) = name;
    return cl;
}

mobj compile_prim(const char *who, void *fn, mobj arity) {
    return compile_do_ret(intern(who), arity, Mlist2(ccall_symbol, fn));
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
    env = make_cenv();
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
    cl = Mclosure(base_env, code);
    minim_closure_name(cl) = name;
    return cl;
}

// Custom `values` compilation
mobj compile_values(mobj name) {
    mobj env, ins, reloc, code, cl;

    // prepare compiler
    env = make_cenv();
    
    // hand written procedure
    ins = Mlist2(
        Mlist1(do_values_symbol),
        Mlist1(ret_symbol)
    );

    // write to code
    reloc = resolve_refs(env, ins);
    code = write_code(ins, reloc, Mcons(Mfixnum(0), minim_false));

    // return a closure
    cl = Mclosure(base_env, code);
    minim_closure_name(cl) = name;
    return cl;
}

// Custom `eval` compilation
mobj compile_eval(mobj name) {
    mobj env, label, ins, reloc, code, cl;

    // prepare compiler
    env = make_cenv();
    label = cenv_make_label(env);
    
    // hand written procedure
    ins = Mlist6(
        Mlist1(get_ac_symbol),
        Mlist3(branchlt_symbol, Mfixnum(1), label),
        Mlist3(branchgt_symbol, Mfixnum(2), label),
        Mlist1(do_eval_symbol),
        label,
        Mlist1(do_arity_error_symbol)
    );

    // write to code
    reloc = resolve_refs(env, ins);
    code = write_code(ins, reloc, Mlist2(Mfixnum(1), Mfixnum(2)));

    // return a closure
    cl = Mclosure(base_env, code);
    minim_closure_name(cl) = name;
    return cl;
}
