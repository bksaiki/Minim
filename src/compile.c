// compile.c: compiler for bootstrapping

#include "minim.h"

static void unimplemented_error(const char *what) {
    fprintf(stderr, "unimplemented: %s\n", what);
    fatal_exit();
}

static int syntax_formp(mobj head, mobj e) {
    return minim_consp(e) && minim_car(e) == head;
}

#define define_values_formp(e)      syntax_formp(define_values_sym, e)
#define letrec_values_formp(e)      syntax_formp(letrec_values_sym, e)
#define let_values_formp(e)         syntax_formp(let_values_sym, e)
#define values_formp(e)             syntax_formp(values_sym, e)
#define lambda_formp(e)             syntax_formp(lambda_sym, e)
#define begin_formp(e)              syntax_formp(begin_sym, e)
#define if_formp(e)                 syntax_formp(if_sym, e)
#define quote_formp(e)              syntax_formp(quote_sym, e)
#define setb_formp(e)               syntax_formp(setb_sym, e)
#define foreign_proc_formp(e)       syntax_formp(foreign_proc_sym, e)

//
//  Compiler state
//

int cstate_init = 0;

static mobj tloc_pre;
static mobj label_pre;
static mobj reloc_sym;
static mobj imm_sym;
static mobj literal_sym;
static mobj closure_sym;
static mobj code_sym;
static mobj foreign_sym;
static mobj arg_sym;
static mobj local_sym;
static mobj next_sym;
static mobj prev_sym;

static mobj lookup_sym;
static mobj apply_sym;
static mobj cmp_sym;
static mobj push_frame_sym;
static mobj pop_frame_sym;
static mobj bindb_sym;
static mobj push_valueb_sym;
static mobj label_sym;
static mobj branch_sym;
static mobj ret_sym;

static mobj load_sym;
static mobj ccall_sym;
static mobj mem_sym;

static mobj size_sym;
static mobj cc_reg;
static mobj env_reg;
static mobj fp_reg;
static mobj sp_reg;
static mobj carg_reg;
static mobj cres_reg;

#define reloc_formp(e)      syntax_formp(reloc_sym, e)
#define lookup_formp(e)     syntax_formp(lookup_sym, e)
#define literal_formp(e)    syntax_formp(literal_sym, e)
#define closure_formp(e)    syntax_formp(closure_sym, e)
#define code_formp(e)       syntax_formp(code_sym, e)
#define foreign_formp(e)    syntax_formp(foreign_sym, e)
#define arg_formp(e)        syntax_formp(arg_sym, e)
#define local_formp(e)      syntax_formp(local_sym, e)
#define next_formp(e)       syntax_formp(next_sym, e)
#define apply_formp(e)      syntax_formp(apply_sym, e)
#define label_formp(e)      syntax_formp(label_sym, e)
#define branch_formp(e)     syntax_formp(branch_sym, e)
#define bindb_formp(e)      syntax_formp(bindb_sym, e)
#define ret_formp(e)        syntax_formp(ret_sym, e)

#define load_formp(e)       syntax_formp(load_sym, e)
#define ccall_formp(e)      syntax_formp(ccall_sym, e)
#define mem_formp(e)        syntax_formp(mem_sym, e)

static void init_compile_globals() {
    tloc_pre = intern("$t");
    label_pre = intern("$L");

    reloc_sym = intern("$reloc");
    imm_sym = intern("$imm");
    literal_sym = intern("$literal");
    closure_sym = intern("$closure");
    code_sym = intern("$code");
    label_sym = intern("$label");
    foreign_sym = intern("$foreign");
    arg_sym = intern("$arg");
    local_sym = intern("$local");
    next_sym = intern("$next");
    prev_sym = intern("$prev");

    lookup_sym = intern("lookup");
    apply_sym = intern("apply");
    cmp_sym = intern("cmp");
    push_frame_sym = intern("push-frame!");
    pop_frame_sym = intern("pop-frame!");
    bindb_sym = intern("bind!");
    push_valueb_sym = intern("push-value!");
    branch_sym = intern("branch");
    ret_sym = intern("ret");

    load_sym = intern("load");
    ccall_sym = intern("c-call");
    mem_sym = intern("mem+");

    size_sym = intern("%size");
    cc_reg = intern("%cc");
    env_reg = intern("%env");
    fp_reg = intern("%fp");
    sp_reg = intern("%sp");
    carg_reg = intern("%Carg");
    cres_reg = intern("%Cres");

    cstate_init = 1;
}

#define c_arg(i)            Mlist2(carg_reg, Mfixnum(i))
#define arg_loc(i)          Mlist2(arg_sym, Mfixnum(i))
#define local_loc(i)        Mlist2(local_sym, Mfixnum(i))
#define next_loc(i)         Mlist2(next_sym, Mfixnum(i))
#define mem_offset(r, i)    Mlist3(mem_sym, r, Mfixnum(i))

static int immediatep(mobj e) {
    return minim_truep(e) ||
            minim_falsep(e) ||
            minim_nullp(e) ||
            minim_voidp(e);
}

static int literalp(mobj e) {
    return immediatep(e) ||
            minim_fixnump(e) ||
            minim_charp(e) ||
            minim_stringp(e);
}

#define branch_uncond           0
#define branch_neq              1
#define branch_reg              2

#define frame_let               0
#define frame_letrec            1

#define cstate_length           5
#define make_cstate()           Mvector(cstate_length, NULL)
#define cstate_name(cs)         minim_vector_ref(cstate, 0)
#define cstate_loader(cs)       minim_vector_ref(cstate, 1)
#define cstate_procs(cs)        minim_vector_ref(cstate, 2)
#define cstate_reloc(cs)        minim_vector_ref(cstate, 3)
#define cstate_gensym_idx(cs)   minim_vector_ref(cstate, 4)

static mobj init_cstate(mobj name) {
    mobj cstate = make_cstate();
    cstate_name(cstate) = name;
    cstate_loader(cstate) = minim_false;
    cstate_procs(cs) = minim_null;
    cstate_reloc(cs) = minim_null;
    cstate_gensym_idx(cs) = Mfixnum(0);
    return cstate;
}

static void cstate_add_loader(mobj cstate, mobj idx) {
    cstate_loader(cstate) = idx;
}

static size_t cstate_add_proc(mobj cstate, mobj fstate) {
    mobj procs = cstate_procs(cstate);
    if (minim_nullp(procs)) {
        cstate_procs(cstate) = Mlist1(fstate);
        return 0;
    } else {
        size_t i = 0;
        for (; !minim_nullp(minim_cdr(procs)); procs = minim_cdr(procs), i++);
        minim_cdr(procs) = Mcons(fstate, minim_null);
        return i + 1;
    }
}

static mobj cstate_add_reloc(mobj cstate, mobj datum) {
    mobj reloc, t;
    size_t i;
    
    reloc = cstate_reloc(cstate);
    if (minim_nullp(reloc)) {
        cstate_reloc(cstate) = Mlist1(datum);
        return Mfixnum(0);
    } else {
        i = 0;
        t = minim_null;
        for_each(reloc,
            if (minim_equalp(minim_car(reloc), datum))
                return Mfixnum(i);
            t = reloc;
            i++;
        );

        minim_cdr(t) = Mcons(datum, minim_null);
        return Mfixnum(i);      
    }
}

static mobj cstate_gensym(mobj cstate, mobj name) {
    size_t i = minim_fixnum(cstate_gensym_idx(cstate));
    mobj prefix = symbol_to_string(name);
    mobj index = fixnum_to_string(Mfixnum(i));
    cstate_gensym_idx(cstate) = Mfixnum(i + 1);
    return string_to_symbol(string_append(prefix, index));
}

//
//  Function state
//

#define fstate_length           4
#define make_fstate()           Mvector(fstate_length, NULL)
#define fstate_name(fs)         minim_vector_ref(fstate, 0)
#define fstate_arity(fs)        minim_vector_ref(fstate, 1)
#define fstate_rest(fs)         minim_vector_ref(fstate, 2)
#define fstate_asm(fs)          minim_vector_ref(fstate, 3)

static mobj init_fstate() {
    mobj fstate = make_fstate();
    fstate_name(fstate) = minim_false;
    fstate_arity(fstate) = Mfixnum(0);
    fstate_rest(fstate) = minim_false;
    fstate_asm(fstate) = minim_null;
    return fstate;
}

static void fstate_add_asm(mobj fstate, mobj s) {
    mobj stmts = fstate_asm(fstate);
    if (minim_nullp(stmts)) {
        fstate_asm(fstate) = Mlist1(s);
    } else {
        for (; !minim_nullp(minim_cdr(stmts)); stmts = minim_cdr(stmts));
        minim_cdr(stmts) = Mcons(s, minim_null);
    }
}

//
//  Compilation (phase 1, flattening)
//

// Return the arity of a procedure as a pair: the number of
// normal arguments and whether or not there are rest arguments.
static mobj procedure_arity(mobj e) {
    mobj args = minim_cadr(e);
    size_t i = 0;

    while (minim_consp(args)) {
        args = minim_cdr(args);
        i++;
    }

    return Mcons(Mfixnum(i), minim_nullp(args) ? minim_false : minim_true);
}

// Compiles a foreign procedure `(#%foreign-procedure ...)`
static mobj compile1_foreign(mobj cstate, mobj e) {
    mobj id, itypes, fstate, args, idx;
    size_t argc;

    id = minim_cadr(e);
    itypes = minim_car(minim_cddr(e));
    // otype = minim_cadr(minim_cddr(e));
    if (lookup_prim(minim_string(id)) == NULL)
        error1("compile1_foreign", "unknown foreign procedure", id);

    fstate = init_fstate();
    fstate_name(fstate) = string_to_symbol(id);
    fstate_arity(fstate) = Mfixnum(list_length(itypes));
    fstate_rest(fstate) = minim_false;

    argc = 0;
    args = minim_null;
    for (; !minim_nullp(itypes); itypes = minim_cdr(itypes)) {
        mobj loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, Mlist3(setb_sym, loc, arg_loc(argc)));
        args = Mcons(loc, args);
        argc++;
    }

    // call into C
    idx = cstate_add_reloc(cstate, Mlist2(foreign_sym, id));
    fstate_add_asm(fstate, Mcons(ccall_sym,
                           Mcons(Mlist2(reloc_sym, idx),
                           list_reverse(args))));
    return fstate;
}

// Compiles an expression.
static mobj compile1_expr(mobj cstate, mobj fstate, mobj e) {
    mobj loc;

loop:
    if (letrec_values_formp(e) || let_values_formp(e)) {
        // letrec-values form (TODO: this should be different)
        // let-values form
        mobj bindings = minim_cadr(e);
        mfixnum bindc = list_length(bindings);
        mfixnum variant = letrec_values_formp(e) ? frame_letrec : frame_let;
        
        fstate_add_asm(fstate, Mlist3(push_frame_sym, Mfixnum(variant), Mfixnum(bindc)));
        for (; !minim_nullp(bindings); bindings = minim_cdr(bindings)) {
            mobj bind = minim_car(bindings);
            mobj ids = minim_car(bind);
            size_t vc = list_length(ids);
            loc = compile1_expr(cstate, fstate, minim_cadr(bind));
            if (vc > 1) {
                unimplemented_error("multiple values for let(rec)-values");
            } else if (vc == 1) {
                mobj lit = cstate_gensym(cstate, tloc_pre);
                mobj idx = cstate_add_reloc(cstate, Mlist2(literal_sym, minim_car(ids)));
                fstate_add_asm(fstate, Mlist3(setb_sym, lit, Mlist2(reloc_sym, idx)));
                fstate_add_asm(fstate, Mlist3(bindb_sym, lit, loc));
            } // else (vc == 0) {
            // evaluate without binding, as in, do nothing
            // }
        }

        e = minim_cddr(e);
        e = minim_nullp(minim_cdr(e)) ? minim_car(e) : Mcons(begin_sym, e);
        loc = compile1_expr(cstate, fstate, e);
        fstate_add_asm(fstate, Mlist1(pop_frame_sym));
    } else if (lambda_formp(e)) {
        // lambda form
        mobj f2state, arity;
        size_t idx;

        f2state = init_fstate();
        arity = procedure_arity(e);
        fstate_arity(f2state) = minim_car(arity);
        fstate_rest(f2state) = minim_cdr(arity);

        e = minim_cddr(e);
        e = minim_nullp(minim_cdr(e)) ? minim_car(e) : Mcons(begin_sym, e);
        compile1_expr(cstate, f2state, e);
        fstate_add_asm(f2state, Mlist1(ret_sym));
        idx = cstate_add_proc(cstate, f2state);

        loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, Mlist3(setb_sym, loc, Mlist2(closure_sym, Mfixnum(idx))));
    } else if (begin_formp(e)) {
        // begin form (at least 2 clauses)
        // execute statements and return the last one
        e = minim_cdr(e);
        for (; !minim_nullp(minim_cdr(e)); e = minim_cdr(e))
            compile1_expr(cstate, fstate, minim_car(e));
        e = minim_car(e);
        goto loop;
    } else if (if_formp(e)) {
        // if form
        mobj cond, ift, iff, liff, lend;
        liff = cstate_gensym(cstate, label_sym);
        lend = cstate_gensym(cstate, label_sym);
        loc = cstate_gensym(cstate, tloc_pre);

        // compile condition
        cond = compile1_expr(cstate, fstate, minim_cadr(e));
        fstate_add_asm(fstate, Mlist3(cmp_sym, cond, Mlist2(imm_sym, minim_false)));
        fstate_add_asm(fstate, Mlist3(branch_sym, Mfixnum(branch_neq), liff));
    
        // compile if-true branch
        ift = compile1_expr(cstate, fstate, minim_car(minim_cddr(e)));
        fstate_add_asm(fstate, Mlist3(setb_sym, loc, ift));
        fstate_add_asm(fstate, Mlist3(branch_sym, Mfixnum(branch_uncond), lend));

        // compile if-false branch
        fstate_add_asm(fstate, Mlist2(label_sym, liff));
        iff = compile1_expr(cstate, fstate, minim_cadr(minim_cddr(e)));
        fstate_add_asm(fstate, Mlist3(setb_sym, loc, iff));

        fstate_add_asm(fstate, Mlist2(label_sym, lend));
    } else if (setb_formp(e)) {
        // set! form
        mobj id, lit, idx;
        id = minim_cadr(e);
        loc = compile1_expr(cstate, fstate, minim_car(minim_cddr(e)));
        lit = cstate_gensym(cstate, tloc_pre);
        idx = cstate_add_reloc(cstate, Mlist2(literal_sym, id));
        fstate_add_asm(fstate, Mlist3(setb_sym, lit, Mlist2(reloc_sym, idx)));
        fstate_add_asm(fstate, Mlist3(bindb_sym, lit, loc));
    } else if (quote_formp(e)) {
        // quote form
        mobj idx = cstate_add_reloc(cstate, Mlist2(literal_sym, minim_cadr(e)));
        loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, Mlist3(setb_sym, loc, Mlist2(reloc_sym, idx)));
    } else if (foreign_proc_formp(e)) {
        // foreign procedure
        mobj f2state = compile1_foreign(cstate, e);
        size_t idx = cstate_add_proc(cstate, f2state);
        loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, Mlist3(setb_sym, loc, Mlist2(closure_sym, Mfixnum(idx))));
    } else if (minim_consp(e)) {
        // application
        mobj op, args, instr;
        args = minim_null;
        op = compile1_expr(cstate, fstate, minim_car(e));
        for (e = minim_cdr(e); !minim_nullp(e); e = minim_cdr(e))
            args = Mcons(compile1_expr(cstate, fstate, minim_car(e)), args);

        instr = Mcons(apply_sym, Mcons(op, list_reverse(args)));
        loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, Mlist3(setb_sym, loc, instr));
    } else if (minim_nullp(e)) {
        // illegal
        error1("compile1_expr", "empty application", e);
    } else if (minim_symbolp(e)) {
        // identifier
        mobj lit = cstate_gensym(cstate, tloc_pre);
        mobj idx = cstate_add_reloc(cstate, Mlist2(literal_sym, e));
        loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, Mlist3(setb_sym, lit, Mlist2(reloc_sym, idx)));
        fstate_add_asm(fstate, Mlist3(setb_sym, loc, Mlist2(lookup_sym, lit)));
    } else if (literalp(e)) {
        // literals
        mobj idx;
        loc = cstate_gensym(cstate, tloc_pre);
        if (immediatep(e)) {
            idx = cstate_add_reloc(cstate, Mlist2(imm_sym, e));
        } else {
            idx = cstate_add_reloc(cstate, Mlist2(literal_sym, e));
        }

        fstate_add_asm(fstate, Mlist3(setb_sym, loc, Mlist2(reloc_sym, idx)));
    } else {
        error1("compile1_expr", "attempting to compile garbage", e);
    }

    return loc;
}

// Compiles module-level syntax for a loader.
static void compile1_module_level(mobj cstate, mobj e) {
    mobj ids, loc, lit, lstate, idx, loffset;
    size_t vc;

    loffset = cstate_loader(cstate);
    lstate = list_ref(cstate_procs(cstate), loffset);

loop:
    if (define_values_formp(e)) {
        // define-values form
        ids = minim_cadr(e);
        vc = list_length(ids);
        loc = compile1_expr(cstate, lstate, minim_car(minim_cddr(e)));
        if (vc > 1) {
            unimplemented_error("multiple values for define-values");
        } else if (vc == 1) {
            // bind the evaluation
            lit = cstate_gensym(cstate, tloc_pre);
            idx = cstate_add_reloc(cstate, Mlist2(literal_sym, minim_car(ids)));
            fstate_add_asm(lstate, Mlist3(setb_sym, lit, Mlist2(reloc_sym, idx)));
            fstate_add_asm(lstate, Mlist3(bindb_sym, lit, loc));
        } // else (vc == 0) {
        // evaluate without binding, as in, do nothing
        // }
    } else if (begin_formp(e)) {
        // begin form (at least 2 clauses)
        // execute statements and return the last one
        e = minim_cdr(e);
        for (; !minim_nullp(minim_cdr(e)); e = minim_cdr(e))
            compile1_expr(cstate, lstate, minim_car(e));
        e = minim_car(e);
        goto loop;
    } else {
        // expression
        loc = compile1_expr(cstate, lstate, e);
        fstate_add_asm(lstate, Mlist2(push_valueb_sym, loc));
    }
}

// Module loaders are the bodies of modules as a nullary functions.
static void compile1_module_loader(mobj cstate, mobj es) {

    // initialize the loader
    mobj lstate = init_fstate();
    mfixnum idx = cstate_add_proc(cstate, lstate);
    cstate_add_loader(cstate, Mfixnum(idx));

    // compiles all expressions
    for (; !minim_nullp(es); es = minim_cdr(es))
        compile1_module_level(cstate, minim_car(es));

    // loader needs to return
    fstate_add_asm(lstate, Mlist1(ret_sym));
}

//
//  Compilation (phase 2, compilation to pseudo assembly)
//

#define add_instr(is, i) {  \
    is = Mcons(i, is);      \
}

static void compile2_proc(mobj cstate, mobj fstate) {
    mobj instrs = minim_null;
    mobj exprs = fstate_asm(fstate);
    for (; !minim_nullp(exprs); exprs = minim_cdr(exprs)) {
        mobj expr = minim_car(exprs);
        if (setb_formp(expr)) {
            // (set! ...)
            mobj dst = minim_cadr(expr);
            mobj src = minim_car(minim_cddr(expr));
            if (minim_symbolp(src)) {
                // (set! <loc> <src>)
                add_instr(instrs, Mcons(load_sym, minim_cdr(expr)));
            } else if (lookup_formp(src)) {
                // (set! <loc> (lookup _)) =>
                //   (c-call env_get %env <get>)   // foreign call
                //   (load <loc> %Cres)
                mobj loc = cstate_gensym(cstate, tloc_pre);
                mobj idx = cstate_add_reloc(cstate, Mlist2(foreign_sym, Mstring("env_get")));
                add_instr(instrs, Mlist3(load_sym, loc, Mlist2(reloc_sym, idx)));
                add_instr(instrs, Mlist4(ccall_sym, loc, env_reg, minim_cadr(src)));
                add_instr(instrs, Mlist3(load_sym, dst, cres_reg));
            }  else if (apply_formp(src)) {
                // (set! _ ($apply <closure> <args> ...)) =>                
                // prepare the stack for a call
                // reserve 2 words
                // move arguments into next frame
                mobj proc = minim_cadr(src);
                mobj es = minim_cddr(src);
                for (size_t i = 2; !minim_nullp(es); ++i) {
                    add_instr(instrs, Mlist3(load_sym, next_loc(i), minim_car(es)));
                    es = minim_cdr(es);
                }

                // stash environment and replace with closure environment
                mobj stash_loc = cstate_gensym(cstate, tloc_pre);
                mobj env_offset = mem_offset(proc, minim_closure_env_offset);
                add_instr(instrs, Mlist3(load_sym, stash_loc, env_reg));
                add_instr(instrs, Mlist3(load_sym, env_reg, env_offset));

                // extract closure code location
                mobj code_loc = cstate_gensym(cstate, tloc_pre);
                mobj code_offset = mem_offset(proc, minim_closure_code_offset);
                add_instr(instrs, Mlist3(load_sym, code_loc, code_offset));

                // fill return address and previous frame position
                mobj ret_label = cstate_gensym(cstate, label_pre);
                mobj label_loc = cstate_gensym(cstate, tloc_pre);
                add_instr(instrs, Mlist3(load_sym, label_loc, ret_label));
                add_instr(instrs, Mlist3(load_sym, next_loc(0), label_loc));
                add_instr(instrs, Mlist3(load_sym, next_loc(1), fp_reg));
                
                // jump to new procedure
                add_instr(instrs, Mlist3(load_sym, fp_reg, Mlist3(mem_sym, fp_reg, Mlist2(intern("-"), size_sym))));
                add_instr(instrs, Mlist3(branch_sym, Mfixnum(branch_reg), code_loc));
                add_instr(instrs, Mlist2(label_sym, ret_label));

                // cleanup
                add_instr(instrs, Mlist3(load_sym, fp_reg, Mlist3(mem_sym, fp_reg, size_sym)));
                add_instr(instrs, Mlist3(load_sym, env_reg, stash_loc));
                add_instr(instrs, Mlist3(load_sym, dst, cres_reg));
            } else if (closure_formp(src)) {
                // (set! _ ($closure <idx>)) =>
                //   (load <tmp> ($code <idx>))
                //   (c-call make_closure %env <tmp>)
                //   (load <loc> %Cres)
                mobj loc = cstate_gensym(cstate, tloc_pre);
                mobj loc2 = cstate_gensym(cstate, tloc_pre);
                mobj idx = cstate_add_reloc(cstate, Mlist2(foreign_sym, Mstring("make_closure")));
                add_instr(instrs, Mlist3(load_sym, loc, Mlist2(code_sym, minim_cadr(src))));
                add_instr(instrs, Mlist3(load_sym, loc2, Mlist2(reloc_sym, idx)));
                add_instr(instrs, Mlist4(ccall_sym, loc2, env_reg, loc));
                add_instr(instrs, Mlist3(load_sym, dst, cres_reg));
            } else if (reloc_formp(src) || arg_formp(src)) {
                // (set! _ (<type> _)) => "as-is"
                add_instr(instrs, Mcons(load_sym, minim_cdr(expr)));
            } else {
                error1("compile2_proc", "unknown set! sequence", expr);
            }
        } else if (bindb_formp(expr)) {
            // (bind! _ _) =>
            //   (c-call env_set %env <name> <get>)    // foreign call
            mobj loc = cstate_gensym(cstate, tloc_pre);
            mobj val = minim_car(minim_cddr(expr));
            mobj idx = cstate_add_reloc(cstate, Mlist2(foreign_sym, Mstring("env_set")));
            add_instr(instrs, Mlist3(load_sym, loc, Mlist2(reloc_sym, idx)));
            add_instr(instrs, Mlist5(ccall_sym, loc, env_reg, minim_cadr(expr), val));
        } else if (ret_formp(expr)) {
            // (ret) =>
            //   (load <tmp> (mem+ %fp 0))
            //   (jmp <tmp>)
            mobj loc = cstate_gensym(cstate, tloc_pre);
            add_instr(instrs, Mlist3(load_sym, loc, mem_offset(fp_reg, 0)));
            add_instr(instrs, Mlist3(branch_sym, Mfixnum(branch_reg), loc));
        } else if (ccall_formp(expr) || label_formp(expr) || branch_formp(expr)) {
            // leave as is
            add_instr(instrs, expr);
        } else {
            error1("compile2_proc", "unknown sequence", expr);
        }
    }

    fstate_asm(fstate) = list_reverse(instrs);
}

//
//  Compilation (phase 3, compilation to architecture-specific assembly)
//  Currently only supporting x86-64
//

static mobj x86_add;
static mobj x86_sub;
static mobj x86_mov;
static mobj x86_lea;
static mobj x86_call;
static mobj x86_jmp;

static mobj x86_fp;
static mobj x86_env;
static mobj x86_carg0;
static mobj x86_carg1;
static mobj x86_carg2;
static mobj x86_carg3;
static mobj x86_cres;

static void init_x86_compiler_globals() {
    x86_add = intern("add");
    x86_sub = intern("sub");
    x86_mov = intern("mov");
    x86_lea = intern("lea");
    x86_call = intern("call");
    x86_jmp = intern("jmp");

    x86_fp = intern("%r13");
    x86_env = intern("%r9");
    x86_carg0 = intern("%rdi");
    x86_carg1 = intern("%rsi");
    x86_carg2 = intern("%rdx");
    x86_carg3 = intern("%rcx");
    x86_cres = intern("%rax");
}

static void compile3_proc(mobj cstate, mobj fstate) {
    mobj instrs = minim_null;
    mobj exprs = fstate_asm(fstate);
    for (; !minim_nullp(exprs); exprs = minim_cdr(exprs)) {
        mobj expr = minim_car(exprs);
        if (load_formp(expr)) {
            // (load <dst> <src>) => (mov <dst> <src>)
            mobj src = minim_car(minim_cddr(expr));
            if (mem_formp(src)) {
                // (load <dst> (mem+ <src> <offset>))
                mobj dst = minim_cadr(expr);
                if (dst == minim_cadr(src)) {
                    // (load <dst> (mem+ <dst> <offset>))
                    mobj offset = minim_car(minim_cddr(src));
                    if (minim_consp(offset)) {
                        // unsafe: assuming `<offset> = (- <pos_offset>)`
                        add_instr(instrs, Mlist3(x86_sub, dst, minim_cadr(offset)));
                    } else {
                        add_instr(instrs, Mlist3(x86_add, dst, offset));
                    }
                } else {
                    add_instr(instrs, Mlist3(x86_mov, dst, src));
                }
            } else {
                add_instr(instrs, Mcons(x86_mov, minim_cdr(expr)));
            }
        } else if (ccall_formp(expr)) {
            // (c-call <tgt> <arg> ...)
            mobj args, env;

            // stash env register
            env = cstate_gensym(cstate, tloc_pre);
            add_instr(instrs, Mlist3(x86_mov, env, env_reg));

            // set appropriate register
            args = minim_cddr(expr);
            for (size_t i = 0; !minim_nullp(args); i++) {
                if (i == 0) {
                    add_instr(instrs, Mlist3(x86_mov, x86_carg0, minim_car(args)));
                } else if (i == 1) {
                    add_instr(instrs, Mlist3(x86_mov, x86_carg1, minim_car(args)));
                } else if (i == 2) {
                    add_instr(instrs, Mlist3(x86_mov, x86_carg2, minim_car(args)));
                } else if (i == 3) {
                    add_instr(instrs, Mlist3(x86_mov, x86_carg3, minim_car(args)));
                } else {
                    error1("compile3_proc", "too many call arguments", expr);
                }

                args = minim_cdr(args);
            }

            // call
            add_instr(instrs, Mlist3(x86_mov, x86_cres, minim_cadr(expr)));
            add_instr(instrs, Mlist2(x86_call, x86_cres));

            // restore env register
            add_instr(instrs, Mlist3(x86_mov, env_reg, env));
        } else if (branch_formp(expr)) {
            // (branch <type> <where>)
            mobj tgt = minim_car(minim_cddr(expr));
            size_t type = minim_fixnum(minim_cadr(expr));
            if (type == branch_uncond) {
                add_instr(instrs, Mlist2(x86_jmp, tgt));
            } else if (type == branch_neq) {
                unimplemented_error("branch with !=");
            } else if (type == branch_reg) {
                add_instr(instrs, Mlist2(x86_jmp, tgt));
            }
        } else if (label_formp(expr)) {
            // do nothing
            add_instr(instrs, expr);
        } else {
            error1("compile3_proc", "unknown sequence", expr);
        }
    }

    fstate_asm(fstate) = list_reverse(instrs);
}

//
//  Compilation (phase 4, register allocation and frame resolution)
//  Currently only supporting x86-64
//

#define rstate_length 4
#define make_rstate()           Mvector(rstate_length, NULL)
#define rstate_lasts(r)         minim_vector_ref(r, 0)
#define rstate_scope(r)         minim_vector_ref(r, 1)
#define rstate_assigns(r)       minim_vector_ref(r, 2)
#define rstate_fsize(r)         minim_vector_ref(r, 3)

static int tregp(mobj t) {
    if (!minim_symbolp(t))
        return 0;

    mchar *s = minim_string(symbol_to_string(t));
    mchar *ps = minim_string(symbol_to_string(tloc_pre));
    for (size_t i = 0; ps[i]; i++) {
        if (s[i] != ps[i])
            return 0;
    }

    return 1;
}

static int labelp(mobj t) {
    if (!minim_symbolp(t))
        return 0;

    mchar *s = minim_string(symbol_to_string(t));
    mchar *ps = minim_string(symbol_to_string(label_pre));
    for (size_t i = 0; ps[i]; i++) {
        if (s[i] != ps[i])
            return 0;
    }

    return 1;
}

static int x86_alloc_regp(mobj r) {
    return r == x86_carg0 ||
        r == x86_carg1 ||
        r == x86_carg2 ||
        r == x86_carg3 ||
        r == x86_cres;
}

static int frame_locp(mobj loc) {
    return arg_formp(loc) || local_formp(loc) || next_formp(loc) || loc == size_sym;
}

static mobj init_rstate() {
    mobj rstate = make_rstate();
    rstate_lasts(rstate) = minim_false;
    rstate_scope(rstate) = minim_null;
    rstate_assigns(rstate) = Mlist5(Mcons(x86_carg0, minim_false),
                                    Mcons(x86_carg1, minim_false),
                                    Mcons(x86_carg2, minim_false),
                                    Mcons(x86_carg3, minim_false),
                                    Mcons(x86_cres, minim_false));
    rstate_fsize(rstate) = Mfixnum(0);
    return rstate;
}

static void rstate_set_lasts(mobj rs, mobj exprs) {
    mobj dict = minim_null;
    for (; !minim_nullp(exprs); exprs = minim_cdr(exprs)) {
        mobj expr = minim_car(exprs);
        mobj op = minim_car(expr);
        if (op == x86_mov) {
            // (mov <dst> <src>)
            mobj src = minim_car(minim_cddr(expr));
            if (minim_consp(src)) {
                mobj variant = minim_car(src);
                if (variant == mem_sym) {
                    // (mov <dst> (mem+ <src> <offset>))
                    if (tregp(src)) {
                        dict = Mcons(Mcons(minim_cadr(src), expr), dict);
                    }
                } else if (variant == code_sym || variant == reloc_sym || variant == arg_sym) {
                    // do nothing
                } else {
                    error1("last_uses", "unknown mov sequence", expr);
                }
            } else if (tregp(src)) {
                dict = Mcons(Mcons(src, expr), dict);
            } else if (minim_symbolp(src)) {
                // do nothing
            } else {
                error1("last_uses", "unknown mov sequence", expr);
            }
        } else if (op == x86_add || op == x86_sub) {
            // (add <dst> <src>)
            mobj src = minim_car(minim_cddr(expr));
            if (tregp(src)) {
                dict = Mcons(Mcons(src, expr), dict);
            }
        } else if (op == x86_jmp) {
            // (jmp <tgt>)
            if (tregp(minim_cadr(expr))) {
                dict = Mcons(Mcons(minim_cadr(expr), expr), dict);
            }
        } else if (op == x86_call || op == label_sym) {
            // do nothing
        } else {
            error1("last_uses", "unknown sequence", expr);
        }
    }

    rstate_lasts(rs) = dict;
}

static mobj last_lookup(mobj rs, mobj reg) {
    mobj lasts = rstate_lasts(rs);
    for_each(lasts,
        if (minim_caar(lasts) == reg)
            return minim_cdar(lasts);
    )

    return NULL;
}

static mobj treg_lookup(mobj rs, mobj reg) {
    mobj scope = rstate_scope(rs);
    for_each(scope,
        if (minim_caar(scope) == reg)
            return minim_cdar(scope);
    );

    return NULL;
}

static mobj treg_assign_reg(mobj rs, mobj treg) {
    mobj assigns = rstate_assigns(rs);
    for_each(assigns,
        if (minim_falsep(minim_cdar(assigns))) {
            rstate_scope(rs) = Mcons(Mcons(treg, minim_caar(assigns)), rstate_scope(rs));
            minim_cdar(assigns) = treg;
            return minim_caar(assigns);
        }
    );

    unimplemented_error("ran out of allocable registers");
    return NULL;
}

static mobj treg_assign_local(mobj rs, mobj treg) {
    mobj s, r, e;
    mfixnum i;
    int found;

    for (i = 0; i < minim_fixnum(rstate_fsize(rs)); ++i) {
        s = rstate_scope(rs);
        found = 0;
        for_each(s,
            r = minim_cdar(s);
            if (local_formp(r) && i == minim_fixnum(minim_cadr(r))) {
                found = 1;
                break;
            }
        );

        if (!found) {
            e = Mlist2(local_sym, Mfixnum(i));
            rstate_scope(rs) = Mcons(Mcons(treg, e), rstate_scope(rs));
            return e;
        }
    }

    e = Mlist2(local_sym, Mfixnum(i));
    rstate_scope(rs) = Mcons(Mcons(treg, e), rstate_scope(rs));
    minim_fixnum(rstate_fsize(rs))++;
    return e;
}

static mobj treg_assign_or_lookup(mobj rs, mobj treg) {
    mobj assigns, maybe;

    // try to lookup in scope
    maybe = treg_lookup(rs, treg);
    if (maybe) return maybe;

    // try to assign a register
    assigns = rstate_assigns(rs);
    for_each(assigns,
        if (minim_falsep(minim_cdar(assigns))) {
            rstate_scope(rs) = Mcons(Mcons(treg, minim_caar(assigns)), rstate_scope(rs));
            minim_cdar(assigns) = treg;
            return minim_caar(assigns);
        }
    );

    // otherwise, assign to stack
    return treg_assign_local(rs, treg);
}

// Unassigns a temporary register.
static void rstate_unassign(mobj rs, mobj reg) {
    mobj s, p, as;

    s = rstate_scope(rs);
    p = NULL;
    for_each(s,
        if (minim_caar(s) == reg) {
            if (x86_alloc_regp(reg)) {
                as = rstate_assigns(rs);
                for_each(as,
                    if (minim_caar(as) == reg) {
                        minim_cdar(as) = minim_false;
                        break;
                    }
                );
            }

            if (p) minim_cdr(p) = minim_cdr(s); // remove internally
            else rstate_scope(rs) = minim_cdr(rstate_scope(rs)); // remove from front
            return;
        }
    );

    error1("rstate_unassign", "register not found", reg);
}

// Stashes all values assigned to allocable registers to the stack.
static void rstate_stash(mobj rs) {
    mobj as, t;

    as = rstate_assigns(rs);
    for_each(as,
        t = minim_cdar(as);
        if (!minim_falsep(t)) {
            minim_cdar(as) = minim_false;
            treg_assign_local(rs, t);
        }
    );
}

// For every temporary register, scan to see if we can remove it
// since it will no longer be used
static void rstate_prune_lasts(mobj rs, mobj e) {
    mobj s, p, l, as;

    s = rstate_scope(rs);
    p = NULL;
    for_each(s,
        l = last_lookup(rstate_lasts(rs), minim_caar(s));
        if (l == minim_false || l == e) {
            // need to prune
            // if register, remove assignment
            if (x86_alloc_regp(minim_cdar(s))) {
                as = rstate_assigns(rs);
                for_each(as,
                    if (minim_caar(as) == minim_cdar(s)) {
                        minim_cdar(as) = minim_false;
                        break;
                    }
                );
            }

            // update scope
            if (p) minim_cdr(p) = minim_cdr(s); // remove internally
            else rstate_scope(rs) = minim_cdr(rstate_scope(rs)); // remove from front
        } else {
            p = s;
        }
    );
}

static void compile4_opt(mobj fstate) {
    mobj instrs = minim_null;
    mobj exprs = fstate_asm(fstate);
    for_each(exprs,
        mobj expr = minim_car(exprs);
        if (minim_car(expr) == x86_mov) {
            // eliminate (mov <reg> <reg>)
            mobj dst = minim_cadr(expr);
            mobj src = minim_car(minim_cddr(expr));
            if (src != dst) add_instr(instrs, expr);
        } else {
            add_instr(instrs, expr);
        }
    );

    fstate_asm(fstate) = list_reverse(instrs);
}

static mobj replace_frame_loc(mobj loc, size_t argc, size_t localc) {
    size_t offset;

    if (loc == size_sym) {
        return Mfixnum(ptr_size * (argc + localc));
    } if (next_formp(loc)) {
        offset = ptr_size * (argc + localc + minim_fixnum(minim_cadr(loc)));
        return Mlist3(mem_sym, x86_fp, Mfixnum(offset));
    } else if (local_formp(loc)) {
        offset = ptr_size * (argc + minim_fixnum(minim_cadr(loc)));
        return Mlist3(mem_sym, x86_fp, Mfixnum(offset));
    } else if (arg_formp(loc)) {
        offset = ptr_size * minim_fixnum(minim_cadr(loc));
        return Mlist3(mem_sym, x86_fp, Mfixnum(offset));
    } else {
        error1("replace_frame_loc", "not frame location", loc);
    }
}

// Resolves frame-dependent locations like `($arg _)` or
// `($local _)` and constants like `$size`.
static void compile4_resolve_frame(mobj fstate, mobj rstate) {
    mobj exprs, expr, op;
    size_t argc, localc;

    // TODO: rest argument means +1
    argc = minim_fixnum(fstate_arity(fstate));
    localc = minim_fixnum(rstate_fsize(rstate));

    // replace values
    exprs = fstate_asm(fstate);
    for_each(exprs,
        expr = minim_car(exprs);
        op = minim_car(expr);
        if (op == x86_mov) {
            mobj dst, src;
            dst = minim_cadr(expr);
            if (frame_locp(dst))
                minim_cadr(expr) = replace_frame_loc(dst, argc, localc);

            src = minim_car(minim_cddr(expr));
            if (frame_locp(src)) {
                minim_car(minim_cddr(expr)) = replace_frame_loc(src, argc, localc);
            }
        } else if (op == x86_add || op == x86_sub) {
            mobj dst, src;
            dst = minim_cadr(expr);
            if (frame_locp(dst))
                minim_cadr(expr) = replace_frame_loc(dst, argc, localc);

            src = minim_car(minim_cddr(expr));
            if (frame_locp(src))
                minim_car(minim_cddr(expr)) = replace_frame_loc(src, argc, localc);
        } else if (op == x86_jmp || op == x86_call) {
            mobj tgt = minim_cadr(expr);
            if (frame_locp(tgt))
                minim_cadr(tgt) = replace_frame_loc(tgt, argc, localc);
        } else if (op == label_sym) {
            // do nothing
        } else {
            error1("compile4_resolve_frame", "unknown sequence", expr);
        }
    );
}

static int reg_aliasp(mobj reg) {
    return reg == cc_reg ||
            reg == fp_reg ||
            reg == sp_reg ||
            reg == env_reg ||
            reg == cres_reg;
}

static mobj replace_alias(mobj reg) {
    if (reg == cc_reg) error1("replace_alias", "unimplemented", reg);
    else if (reg == fp_reg) return x86_fp;
    else if (reg == sp_reg) error1("replace_alias", "unimplemented", reg);
    else if (reg == env_reg) return x86_env;
    else if (reg == cres_reg) return x86_cres;
    else error1("replace_alias", "unknown", reg);
}

// Resolves architecture-independent register names.
static void compile4_resolve_regs(mobj fstate) {
    mobj exprs, expr, op;

    exprs = fstate_asm(fstate);
    for_each(exprs,
        expr = minim_car(exprs);
        op = minim_car(expr);
        if (op == x86_mov) {
            mobj dst = minim_cadr(expr);
            mobj src = minim_car(minim_cddr(expr));
            if (reg_aliasp(dst)) {
                minim_cadr(expr) = replace_alias(dst);
            } else {
                mobj variant = minim_car(dst);
                if (variant == mem_sym) {
                    mobj base = minim_cadr(dst);
                    if (reg_aliasp(base))
                        minim_cadr(dst) = replace_alias(base);
                }
            }

            if (reg_aliasp(src)) {
                minim_car(minim_cddr(expr)) = replace_alias(src);
            } else {
                mobj variant = minim_car(src);
                if (variant == mem_sym) {
                    mobj base = minim_cadr(src);
                    if (reg_aliasp(base))
                        minim_cadr(src) = replace_alias(base);
                }
            }
        } else if (op == x86_add || op == x86_sub) {
            mobj dst = minim_cadr(expr);
            mobj src = minim_car(minim_cddr(expr));
            if (reg_aliasp(dst))
                minim_cadr(expr) = replace_alias(dst);
            if (reg_aliasp(src))
                minim_car(minim_cddr(expr)) = replace_alias(src);
        } else if (op == x86_jmp || op == x86_call) {
            mobj tgt = minim_cadr(expr);
            if (reg_aliasp(tgt))
                minim_cadr(expr) = replace_alias(tgt);
        } else if (op == label_sym) {
            // do nothing
        } else {
            error1("compile4_resolve_regs", "unknown sequence", expr);
        }
    );
}

static void compile4_proc(mobj cstate, mobj fstate) {
    mobj exprs, rstate, instrs;

    // initialize register state and compute last uses
    rstate = init_rstate();
    exprs = fstate_asm(fstate);
    rstate_set_lasts(rstate, exprs);

    // linear pass to assign registers
    instrs = minim_null;
    for_each(exprs,
        mobj expr = minim_car(exprs);
        mobj op = minim_car(expr);
        if (op == x86_mov) {
            mobj dst = minim_cadr(expr);
            mobj src = minim_car(minim_cddr(expr));
            if (minim_consp(src)) {
                // (mov <dst> (<variant> ...))
                mobj variant = minim_car(src);
                if (variant == code_sym || variant == reloc_sym) {
                    // assign to either register or memory
                    if (tregp(dst))
                        dst = treg_assign_or_lookup(rstate, dst);
                    add_instr(instrs, Mlist3(op, dst, src));
                } else if (variant == arg_sym) {
                    // must assign to register
                    if (tregp(dst))
                        dst = treg_assign_reg(rstate, dst);
                    add_instr(instrs, Mlist3(op, dst, src));
                } else if (variant == mem_sym) {
                    // (mov <dst> (mem+ <base> <offset>))
                    mobj base = minim_cadr(src);
                    if (tregp(base)) {
                        base = treg_lookup(rstate, base);
                        if (minim_consp(base)) {
                            // `base` must be a register
                            rstate_unassign(rstate, minim_cadr(src));
                            base = treg_assign_reg(rstate, minim_cadr(src));
                        }

                        minim_cadr(src) = base;
                    }

                    if (tregp(dst))
                        dst = treg_assign_reg(rstate, dst);
                    add_instr(instrs, Mlist3(op, dst, src));
                } else {
                    error1("compile4_proc", "unknown mov sequence", expr);
                }
            } else if (minim_symbolp(src)) {
                // (mov <dst> <src>)
                if (tregp(src))
                    src = treg_lookup(rstate, src);

                if (tregp(dst)) {
                    if (local_formp(src)) {
                        dst = treg_assign_reg(rstate, dst);
                    } else {
                        dst = treg_assign_or_lookup(rstate, dst);
                    }
                }

                add_instr(instrs, Mlist3(op, dst, src));
            } else {
                error1("compile4_proc", "unknown mov sequence", expr);
            }
        } else if (op == x86_add || op == x86_sub) {
            // (<op> <dst> <src>)
            mobj dst = minim_cadr(expr);
            mobj src = minim_car(minim_cddr(expr));
            if (tregp(src))
                src = treg_lookup(rstate, src);

            if (tregp(dst)) {
                if (local_formp(src)) {
                    dst = treg_assign_reg(rstate, dst);
                } else {
                    dst = treg_assign_or_lookup(rstate, dst);
                }
            }

            add_instr(instrs, Mlist3(op, dst, src));
        } else if (op == x86_jmp) {
            // (jmp <tgt>)
            mobj tgt = minim_cadr(expr);
            // can call from register or memory
            if (tregp(tgt))
                tgt = treg_lookup(rstate, tgt);
            rstate_stash(rstate);
            add_instr(instrs, Mlist2(op, tgt));
        } else if (op == x86_call) {
            if (minim_cadr(expr) != x86_cres)
                unimplemented_error("cannot call from any other register than %rax");
            rstate_stash(rstate);
            add_instr(instrs, expr);
        } else if (op == label_sym) {
            // do nothing
            add_instr(instrs, expr);
        } else {
            error1("compile4_proc", "unknown sequence", expr);
        }

        rstate_prune_lasts(rstate, expr);
    );

    fstate_asm(fstate) = list_reverse(instrs);

    // remove any self-moves
    compile4_opt(fstate);

    // resolve frame-dependent locations
    compile4_resolve_frame(fstate, rstate);

    // resolve architecture-independent registers
    compile4_resolve_regs(fstate);
}

//
//  Compilation (phase 5, translation to binary)
//  Currently only supporting x86-64
//

#define PAD_LEN     16

#define lstate_length 4
#define make_lstate()               Mvector(lstate_length, NULL)
#define lstate_relocs(l)            minim_vector_ref(l, 0)
#define lstate_locals(l)            minim_vector_ref(l, 1)
#define lstate_offsets(l)           minim_vector_ref(l, 2)
#define lstate_pos(l)               minim_vector_ref(l, 3)

static mobj init_lstate() {
    mobj lstate = make_lstate();
    lstate_relocs(lstate) = minim_false;
    lstate_locals(lstate) = minim_null;
    lstate_offsets(lstate) = minim_null;
    lstate_pos(lstate) = Mfixnum(0);
    return lstate;
}

static void lstate_resolve_relocs(mobj lstate, mobj reloc) {
    mobj table, key, val;
    size_t i;
    
    table = Mvector(list_length(reloc), NULL);
    i = 0;

    for_each(reloc,
        key = minim_car(reloc);
        if (foreign_formp(key)) {
            // ($foreign <name>)
            if (minim_stringp(minim_cadr(key))) {
                val = lookup_prim(minim_string(minim_cadr(key)));
                if (!val) {
                    error1("resolve_reloc",
                           "unknown foreign procedure",
                           minim_cadr(key));
                }

                minim_vector_ref(table, i) = Mcons(key, Mfixnum((uptr) val));
            } else {
                error1("resolve_reloc", "unknown $foreign entry", key);
            }
        } else if (literal_formp(key)) {
            // ($literal <datum>)
            val = Mfixnum((uptr) minim_cadr(key));
            minim_vector_ref(table, i) = Mcons(key, val);
        } else {
            error1("resolve_reloc", "unknown entry", key);
        }

        i++;
    );

    lstate_relocs(lstate) = table;
}

static mobj lstate_reloc_ref(mobj lstate, mobj idx) {
    return minim_vector_ref(lstate_relocs(lstate), minim_fixnum(idx));
}

static void lstate_add_local(mobj lstate, mobj key, mobj offset) {
    lstate_locals(lstate) = Mcons(Mcons(key, offset), lstate_locals(lstate));
}

static void lstate_add_offset(mobj lstate, mobj key, mobj imm) {
    lstate_offsets(lstate) = Mcons(Mcons(key, imm), lstate_offsets(lstate));
}

static mobj lstate_lookup_local(mobj lstate, mobj key) {
    mobj locals = lstate_locals(lstate);
    for_each(locals,
        if (code_formp(key)) {
            if (minim_equalp(key, minim_caar(locals)))
                return minim_cdar(locals);
        } else {
            if (key == minim_caar(locals))
                return minim_cdar(locals);
        }
    );

    error1("lstate_lookup_local", "key not found", key);
}

static mobj x86_encode_rex(int w, int r, int x, int b) {
    mfixnum i = (1 << 7) | ((w != 0) << 3) | ((r != 0) << 2) | ((x != 0) << 1) | (b != 0);
    return Mfixnum(i & 0xFF);
}

static mobj x86_encode_reg(mobj reg) {
    if (reg == x86_cres) return Mfixnum(0);
    else if (reg == x86_carg0) return Mfixnum(7);
    else if (reg == x86_carg1) return Mfixnum(6);
    else if (reg == x86_carg2) return Mfixnum(2);
    else if (reg == x86_carg3) return Mfixnum(1);
    else if (reg == x86_env) return Mfixnum(1);
    else if (reg == x86_fp) return Mfixnum(5);
    else error1("x86_encode_reg", "unsupported", reg);
}

static int x86_encode_x(mobj reg) {
    if (reg == x86_cres) return 0;
    else if (reg == x86_carg0) return 0;
    else if (reg == x86_carg1) return 0;
    else if (reg == x86_carg2) return 0;
    else if (reg == x86_carg3) return 0;
    else if (reg == x86_env) return 1;
    else if (reg == x86_fp) return 1;
    else error1("x86_encode_x", "unsupported", reg);
}

static mobj x86_encode_imm64(mobj imm) {
    mobj bytes = minim_null;
    mobj quot = Mfixnum(256);
    for (size_t i = 0; i < 8; i++) {
        bytes = Mcons(fix_rem(imm, quot), bytes);
        imm = fix_div(imm, quot);
    }

    return list_reverse(bytes);
}

static mobj x86_encode_imm32(mobj imm) {
    mobj bytes = minim_null;
    mobj quot = Mfixnum(256);
    for (size_t i = 0; i < 4; i++) {
        bytes = Mcons(fix_rem(imm, quot), bytes);
        imm = fix_div(imm, quot);
    }

    if (!minim_zerop(imm))
        error1("x86_encode_imm32", "value larger than 32 bits", imm);
    return list_reverse(bytes);
}

static mobj x86_encode_modrm(mobj mod, mobj reg, mobj rm) {
    if (minim_fixnum(mod) < 0 || minim_fixnum(mod) > 3)
        error1("x86_encode_modrm", "illegal mod field", mod);
    if (minim_fixnum(reg) < 0 || minim_fixnum(reg) > 7)
        error1("x86_encode_modrm", "illegal reg field", reg);
    if (minim_fixnum(rm) < 0 || minim_fixnum(rm) > 7)
        error1("x86_encode_modrm", "illegal rm field", rm);
    return Mfixnum((minim_fixnum(mod) << 6) + (minim_fixnum(reg) << 3) + minim_fixnum(rm));
}

static mobj x86_encode_io(mobj op, mobj reg) {
    return fix_add(op, reg);
}

static mobj x86_encode_mov(mobj dst, mobj src) {
    mobj rex = x86_encode_rex(1, x86_encode_x(src), 0, x86_encode_x(dst));
    mobj modrm = x86_encode_modrm(Mfixnum(3), x86_encode_reg(src), x86_encode_reg(dst));
    return Mlist3(rex, Mfixnum(0x89), modrm);
}

static mobj x86_encode_mov_imm64(mobj dst, mobj imm) {
    mobj rex = x86_encode_rex(1, 0, 0, x86_encode_x(dst));
    mobj op = x86_encode_io(Mfixnum(0xB8), x86_encode_reg(dst));
    return Mcons(rex, Mcons(op, x86_encode_imm64(imm)));
}

static mobj x86_encode_mov_rel(mobj dst, mobj src, mobj lstate, mobj where) {
    mobj imm = x86_encode_imm32(Mfixnum(0));
    mobj rex = x86_encode_rex(1, x86_encode_x(dst), 0, 0);
    mobj op = fix_add(fix_mul(x86_encode_reg(dst), Mfixnum(8)), Mfixnum(5));
    mobj instr = Mcons(rex, Mcons(Mfixnum(0x8B), Mcons(op, imm)));
    lstate_add_offset(lstate, Mcons(src, where), imm);
    return instr;
}

static mobj x86_encode_load(mobj dst, mobj src, mobj off) {
    mobj rex = x86_encode_rex(1, x86_encode_x(src), 0, x86_encode_x(dst));
    mobj modrm = x86_encode_modrm(Mfixnum(1), x86_encode_reg(src), x86_encode_reg(dst));
    if (minim_fixnum(off) >= 0 && minim_fixnum(off) < 127) {
        return Mlist4(rex, Mfixnum(0x8B), modrm, off);
    } else if (minim_fixnum(off) >= -128 && minim_fixnum(off) < -1) {
        return Mlist4(rex, Mfixnum(0x8B), modrm, Mfixnum(minim_fixnum(off) + 256));
    } else {
        error1("x86_encode_load", "unsupported mov offset", off);
    }
}

static mobj x86_encode_store(mobj dst, mobj src, mobj off) {
    mobj rex = x86_encode_rex(1, x86_encode_x(src), 0, x86_encode_x(dst));
    mobj modrm = x86_encode_modrm(Mfixnum(1), x86_encode_reg(src), x86_encode_reg(dst));
    if (minim_fixnum(off) >= 0 && minim_fixnum(off) < 127) {
        return Mlist4(rex, Mfixnum(0x89), modrm, off);
    } else if (minim_fixnum(off) >= -128 && minim_fixnum(off) < -1) {
        return Mlist4(rex, Mfixnum(0x89), modrm, Mfixnum(minim_fixnum(off) + 256));
    } else {
        error1("x86_encode_store", "unsupported mov offset", off);
    }
}

static mobj x86_encode_add(mobj dst, mobj src) {
    mobj rex = x86_encode_rex(1, x86_encode_x(src), 0, x86_encode_x(dst));
    mobj modrm = x86_encode_modrm(Mfixnum(3), x86_encode_reg(src), x86_encode_reg(dst));
    return Mlist3(rex, Mfixnum(0x1), modrm);
}

static mobj x86_encode_add_imm(mobj dst, mobj imm) {
    mobj op = x86_encode_io(Mfixnum(0xC0), x86_encode_reg(dst));
    mobj rex = x86_encode_rex(1, 0, 0, x86_encode_x(dst));
    if (minim_fixnum(imm) >= 0 && minim_fixnum(imm) < 256) {
        return Mlist4(rex, Mfixnum(0x83), op, imm);
    } else {
        error1("compile5_proc", "unsupported add offset", imm);
    }
}

static mobj x86_encode_sub(mobj dst, mobj src) {
    mobj rex = x86_encode_rex(1, x86_encode_x(src), 0, x86_encode_x(dst));
    mobj modrm = x86_encode_modrm(Mfixnum(3), x86_encode_reg(src), x86_encode_reg(dst));
    return Mlist3(rex, Mfixnum(0x29), modrm);
}

static mobj x86_encode_sub_imm(mobj dst, mobj imm) {
    mobj op = x86_encode_io(Mfixnum(0xE8), x86_encode_reg(dst));
    mobj rex = x86_encode_rex(1, 0, 0, x86_encode_x(dst));
    if (minim_fixnum(imm) >= 0 && minim_fixnum(imm) < 256) {
        return Mlist4(rex, Mfixnum(0x83), op, imm);
    } else {
        error1("compile5_proc", "unsupported sub offset", imm);
    }
}

static mobj x86_encode_call(mobj tgt) {
    mobj modrm, rex;
    modrm = x86_encode_modrm(Mfixnum(3), Mfixnum(2), x86_encode_reg(tgt));
    if (x86_encode_x(tgt)) {
        rex = x86_encode_rex(0, 0, 0, 1);
        return Mlist3(rex, Mfixnum(0xFF), modrm);
    } else {
        return Mlist2(Mfixnum(0xFF), modrm);
    }
}

static mobj x86_encode_jmp(mobj tgt) {
    mobj modrm, rex;
    modrm = x86_encode_modrm(Mfixnum(3), Mfixnum(4), x86_encode_reg(tgt));
    if (x86_encode_x(tgt)) {
        rex = x86_encode_rex(0, 0, 0, 1);
        return Mlist3(rex, Mfixnum(0xFF), modrm);
    } else {
        return Mlist2(Mfixnum(0xFF), modrm);
    }
}

static size_t instr_bytes(mobj instrs) {
    size_t i = 0;
    for_each(instrs, i += list_length(minim_car(instrs)));
    return i;
}

static void lstate_resolve_offsets(mobj lstate) {
    mobj offsets = lstate_offsets(lstate);
    for_each(offsets,
        mobj src = minim_caar(minim_car(offsets));
        mobj dst = minim_cdar(minim_car(offsets));
        mobj imm = minim_cdr(minim_car(offsets));

        mobj src_off = lstate_lookup_local(lstate, src);
        mobj dst_off = lstate_lookup_local(lstate, dst);
        mobj offset = fix_sub(src_off, dst_off);
        mobj bytes = x86_encode_imm32(offset);
        
        for_each(imm,
            minim_car(imm) = minim_car(bytes);
            bytes = minim_cdr(bytes);
        );
    );
}

static void compile5_proc(mobj cstate, mobj lstate, mobj fstate) {
    mobj instrs, exprs;
    size_t len;

    instrs = minim_null;
    exprs = fstate_asm(fstate);
    for_each(exprs,
        mobj expr = minim_car(exprs);
        mobj op = minim_car(expr);
        if (op == x86_mov) {
            // mov <dst> <src>
            mobj dst, src;
            dst = minim_cadr(expr);
            src = minim_car(minim_cddr(expr));
            if (minim_consp(src)) {
                mobj variant = minim_car(src);
                if (variant == code_sym) {
                    // (mov <dst> ($code <idx>))
                    // => (mov <dst> [rip+<offset>])
                    size_t offset = instr_bytes(instrs) + minim_fixnum(lstate_pos(lstate));
                    lstate_add_local(lstate, expr, Mfixnum(offset));
                    add_instr(instrs, x86_encode_mov_rel(dst, src, lstate, expr));
                } else if (variant == reloc_sym) {
                    // (mov <dst> ($reloc <idx>))
                    // => (mov <dst> <addr>)
                    mobj addr = minim_cdr(lstate_reloc_ref(lstate, minim_cadr(src)));
                    add_instr(instrs, x86_encode_mov_imm64(dst, addr));
                } else if (variant == mem_sym) {
                    // (mov <dst> (mem+ <base> <offset>))
                    // => (mov <dst> [<base>+<offset>])
                    mobj base, offset;
                    if (minim_consp(dst))
                        error1("compile5_proc", "too many memory locations", expr);

                    base = minim_cadr(src);
                    offset = minim_car(minim_cddr(src));
                    add_instr(instrs, x86_encode_load(dst, base, offset));
                } else {
                    error1("compile5_proc", "unknown mov instruction", expr);
                }
            } else if (labelp(src)) {
                // (mov <dst> <label)
                size_t offset;

                if (minim_consp(dst))
                    error1("compile5_proc", "too many memory locations", expr);
                offset = instr_bytes(instrs) + minim_fixnum(lstate_pos(lstate));
                lstate_add_local(lstate, expr, Mfixnum(offset));
                add_instr(instrs, x86_encode_mov_rel(dst, src, lstate, expr));
            } else {
                if (minim_consp(dst)) {
                    // (mov (mem+ <base> <offset>) <src>)
                    // => (mov [<base>+<offset>] <src>)
                    mobj base = minim_cadr(dst);
                    mobj offset = minim_car(minim_cddr(dst));
                    add_instr(instrs, x86_encode_store(base, src, offset));
                } else {
                    // (mov <dst> <src>)
                    add_instr(instrs, x86_encode_mov(dst, src));
                }
            }
        } else if (op == x86_add) {
            // add <dst> <src>
            mobj dst = minim_cadr(expr);
            mobj src = minim_car(minim_cddr(expr));
            if (minim_symbolp(src)) {
                add_instr(instrs, x86_encode_add(dst, src));
            } else {
                add_instr(instrs, x86_encode_add_imm(dst, src));
            }
        } else if (op == x86_sub) {
            // sub <dst> <src>
            mobj dst = minim_cadr(expr);
            mobj src = minim_car(minim_cddr(expr));
            if (minim_symbolp(src)) {
                add_instr(instrs, x86_encode_sub(dst, src));
            } else {
                add_instr(instrs, x86_encode_sub_imm(dst, src));
            }
        } else if (op == x86_call) {
            // call <tgt>
            add_instr(instrs, x86_encode_call(minim_cadr(expr)));
        } else if (op == x86_jmp) {
            // jmp <tgt>
            add_instr(instrs, x86_encode_jmp(minim_cadr(expr)));
        } else if (op == label_sym) {
            // empty sequence, need to register location
            size_t offset = instr_bytes(instrs) + minim_fixnum(lstate_pos(lstate));
            lstate_add_local(lstate, minim_cadr(expr), Mfixnum(offset));
        } else {
            error1("compile5_proc", "unknown instruction", expr);
        }
    );

    // add NOP to meet padding requirement
    len = instr_bytes(instrs);
    while (len % PAD_LEN) {
        instrs = Mcons(Mlist1(Mfixnum(0x90)), instrs);
        len++;
    }

    // replace instructions
    fstate_asm(fstate) = list_reverse(instrs);
    lstate_pos(lstate) = Mfixnum(minim_fixnum(lstate_pos(lstate)) + len);
}

static void compile5_module(mobj cstate) {
    mobj lstate, procs;
    size_t i;

    // initialize linker state and resolve relocation table
    lstate = init_lstate();
    lstate_resolve_relocs(lstate, cstate_reloc(cstate));

    // compile each procedure
    i = 0;
    procs = cstate_procs(cstate);
    for_each(procs,
        lstate_add_local(lstate, Mlist2(code_sym, Mfixnum(i)), lstate_pos(lstate));
        compile5_proc(cstate, lstate, minim_car(procs));
        i++;
    );

    // compute offsets relative to first instruction
    lstate_resolve_offsets(lstate);

    write_object(Mport(stdout, 0x0), lstate);
    fprintf(stdout, "\n");
}

//
//  Public API
//

mobj compile_module(mobj op, mobj name, mobj es) {
    mobj cstate, procs;

    // check for initialization
    if (!cstate_init) {
        init_compile_globals();
        init_x86_compiler_globals();
    }

    // phase 1: flattening pass
    cstate = init_cstate(name);
    compile1_module_loader(cstate, es);

    // phase 2: pseudo assembly pass
    procs = cstate_procs(cstate);
    for_each(procs, compile2_proc(cstate, minim_car(procs)));

    // phase 3: architecture-specific assembly
    procs = cstate_procs(cstate);
    for_each(procs, compile3_proc(cstate, minim_car(procs)));

    // phase 4: register allocation
    procs = cstate_procs(cstate);
    for_each(procs, compile4_proc(cstate, minim_car(procs)));

    return cstate;
}

void install_module(mobj cstate) {
    // check for initialization
    if (!cstate_init) {
        init_compile_globals();
        init_x86_compiler_globals();
    } 

    // phase 5: translation to binary
    compile5_module(cstate);

    // write_object(th_output_port(get_thread()), cstate);
    // fputs("\n\n", stdout);
    // fflush(stdout);
}
