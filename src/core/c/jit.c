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
        mobj ref, in;

        // restore label
        ref = assq_ref(inv_reloc, Mfixnum((intptr_t) &istream[i]));
        if (!minim_falsep(ref)) {
            ins = Mcons(minim_cdr(ref), ins);
        }
        
        // possibly replace jumps
        in = istream[i];
        if (minim_car(in) == brancha_symbol) {
            // brancha
            ref = assq_ref(inv_reloc, minim_cadr(in));
            in = Mlist2(brancha_symbol, minim_cdr(ref));
        } else if (minim_car(in) == branchf_symbol) {
            // branchf
            ref = assq_ref(inv_reloc, minim_cadr(in));
            in = Mlist2(branchf_symbol, minim_cdr(ref));
        } else if (minim_car(in) == branchgt_symbol) {
            // branchgt
            ref = assq_ref(inv_reloc, minim_car(minim_cddr(in)));
            in = Mlist3(branchgt_symbol, minim_cadr(in), minim_cdr(ref));
        } else if (minim_car(in) == branchlt_symbol) {
            // branchlt
            ref = assq_ref(inv_reloc, minim_car(minim_cddr(in)));
            in = Mlist3(branchlt_symbol, minim_cadr(in), minim_cdr(ref));
        } else if (minim_car(in) == branchne_symbol) {
            // branchne
            ref = assq_ref(inv_reloc, minim_car(minim_cddr(in)));
            in = Mlist3(branchne_symbol, minim_cadr(in), minim_cdr(ref));
        } else if (minim_car(in) == save_cc_symbol) {
            // save-cc
            in = Mlist2(save_cc_symbol, minim_cdr(assq_ref(inv_reloc, minim_cadr(in))));
        }
    
        ins = Mcons(in, ins);
    }

    writeln_object(stderr, ins);
    return list_reverse(ins);
}

static mobj remove_labels(mobj ins) {
    mobj hd, tl;

    hd = tl = Mcons(minim_car(ins), minim_null);
    for (ins = minim_cdr(ins); !minim_nullp(ins); ins = minim_cdr(ins)) {
        if (!minim_stringp(minim_car(ins))) {
            minim_cdr(tl) = Mcons(minim_car(ins), minim_null);
            tl = minim_cdr(tl);
        }
    }

    return hd;
}

mobj write_code(mobj ins, mobj reloc, mobj arity) {
    mobj code, *istream;
    size_t i;

    // remove labels
    ins = remove_labels(ins);

    // allocate code object and fill header
    code = Mcode(list_length(ins));
    minim_code_reloc(code) = minim_null;
    minim_code_arity(code) = arity;
    istream = minim_code_it(code);

    // need to recompute the reloc table for in-code addresses
    for (; !minim_nullp(reloc); reloc = minim_cdr(reloc)) {
        mobj it = ins;
        for (i = 0; i < minim_code_len(code); i++) {
            if (minim_car(it) == minim_cdar(reloc)) {
                mobj cell = Mcons(minim_caar(reloc), Mfixnum((intptr_t) &istream[i]));
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
            in = Mlist2(brancha_symbol, minim_cdr(assq_ref(reloc, minim_cadr(in))));
        } else if (minim_car(in) == branchf_symbol) {
            // branchf
            in = Mlist2(branchf_symbol, minim_cdr(assq_ref(reloc, minim_cadr(in))));
        } else if (minim_car(in) == branchgt_symbol) {
            // branchgt
            in = Mlist3(branchgt_symbol, minim_cadr(in), minim_cdr(assq_ref(reloc, minim_car(minim_cddr(in)))));
        } else if (minim_car(in) == branchlt_symbol) {
            // branchlt
            in = Mlist3(branchlt_symbol, minim_cadr(in), minim_cdr(assq_ref(reloc, minim_car(minim_cddr(in)))));
        } else if (minim_car(in) == branchne_symbol) {
            // branchne
            in = Mlist3(branchne_symbol, minim_cadr(in), minim_cdr(assq_ref(reloc, minim_car(minim_cddr(in)))));
        } else if (minim_car(in) == save_cc_symbol) {
            // save-cc
            in = Mlist2(save_cc_symbol, minim_cdr(assq_ref(reloc, minim_cadr(in))));
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
//  Public API
//

mobj compile_expr(mobj expr) {
    mobj global_env, proc_env, scope_env;
    mobj fv_table;
    mobj L1, L2, L3;
    mobj ins, reloc;

    // optimization passes
    L1 = jit_opt_L0(expr);
    L2 = jit_opt_L1(L1);
    L3 = jit_opt_L2(L2);

    // prepare initial compiler environments
    global_env = make_global_cenv();
    proc_env = make_cenv(global_env);
    scope_env = make_scope_cenv(proc_env);

    // compute free variables
    fv_table = Mbox(minim_null);
    free_vars(L3, fv_table);
    global_cenv_set_fvs(global_env, minim_unbox(fv_table));

    // compile
    ins = compile_expr2(L3, scope_env, 1);
    reloc = resolve_refs(proc_env, ins);
    return write_code(ins, reloc, Mfixnum(0));
}


