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
static mobj imm_sym;
static mobj literal_sym;
static mobj closure_sym;
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

#define lookup_formp(e)     syntax_formp(lookup_sym, e)
#define literal_formp(e)    syntax_formp(literal_sym, e)
#define closure_formp(e)    syntax_formp(closure_sym, e)
#define arg_formp(e)        syntax_formp(arg_sym, e)
#define local_formp(e)      syntax_formp(local_sym, e)
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
    imm_sym = intern("$imm");
    literal_sym = intern("$literal");
    closure_sym = intern("$closure");
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
#define cstate_literals(cs)     minim_vector_ref(cstate, 3)
#define cstate_gensym_idx(cs)   minim_vector_ref(cstate, 4)

static mobj init_cstate(mobj name) {
    mobj cstate = make_cstate();
    cstate_name(cstate) = name;
    cstate_loader(cstate) = minim_false;
    cstate_procs(cs) = minim_null;
    cstate_literals(cs) = minim_null;
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

static mobj cstate_add_literal(mobj cstate, mobj lit) {
    mobj lits = cstate_literals(cstate);
    if (minim_nullp(lits)) {
        cstate_literals(cstate) = Mlist1(lit);
        return Mfixnum(0);
    } else {
        size_t i = 0;
        for (; !minim_nullp(minim_cdr(lits)); lits = minim_cdr(lits), i++) {
            if (minim_equalp(minim_car(lits), lit))
                return Mfixnum(i);
        }
        
        if (minim_equalp(minim_car(lits), lit)) {
            return Mfixnum(i);
        } else {
            minim_cdr(lits) = Mcons(lit, minim_null);
            return Mfixnum(i + 1);
        }
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
    mobj id, itypes, fstate, args;
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
    fstate_add_asm(fstate, Mcons(ccall_sym,
                           Mcons(Mlist2(foreign_sym, id),
                           list_reverse(args))));
    return fstate;
}

// Compiles an expression.
static mobj compile1_expr(mobj cstate, mobj fstate, mobj e) {
    mobj loc;

    fputs("expr> ", stdout);
    write_object(Mport(stdout, PORT_FLAG_OPEN), e);
    fputs("\n", stdout);

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
                mobj idx = cstate_add_literal(cstate, minim_car(ids));
                fstate_add_asm(fstate, Mlist3(setb_sym, lit, Mlist2(literal_sym, idx)));
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
        mobj f2state, arity, code;
        f2state = init_fstate();
        arity = procedure_arity(e);
        fstate_arity(f2state) = minim_car(arity);
        fstate_rest(f2state) = minim_cdr(arity);

        e = minim_cddr(e);
        e = minim_nullp(minim_cdr(e)) ? minim_car(e) : Mcons(begin_sym, e);
        code = compile1_expr(cstate, f2state, e);
        fstate_add_asm(f2state, Mlist1(ret_sym));

        loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, Mlist3(setb_sym, loc, Mlist2(closure_sym, code)));
        cstate_add_proc(cstate, f2state);
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
        idx = cstate_add_literal(cstate, id);
        fstate_add_asm(fstate, Mlist3(setb_sym, lit, Mlist2(literal_sym, idx)));
        fstate_add_asm(fstate, Mlist3(bindb_sym, lit, loc));
    } else if (quote_formp(e)) {
        // quote form
        mobj idx = cstate_add_literal(cstate, minim_cadr(e));
        loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, Mlist3(setb_sym, loc, Mlist2(literal_sym, idx)));
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
        mobj idx = cstate_add_literal(cstate, e);
        loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, Mlist3(setb_sym, lit, Mlist2(literal_sym, idx)));
        fstate_add_asm(fstate, Mlist3(setb_sym, loc, Mlist2(lookup_sym, lit)));
    } else if (literalp(e)) {
        // literals
        loc = cstate_gensym(cstate, tloc_pre);
        if (immediatep(e)) {
            fstate_add_asm(fstate, Mlist3(setb_sym, loc, Mlist2(imm_sym, e)));
        } else {
            mobj idx = cstate_add_literal(cstate, e);
            fstate_add_asm(fstate, Mlist3(setb_sym, loc, Mlist2(literal_sym, idx)));
        }
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

    fputs("top> ", stdout);
    write_object(Mport(stdout, PORT_FLAG_OPEN), e);
    fputs("\n", stdout);

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
            idx = cstate_add_literal(cstate, minim_car(ids));
            fstate_add_asm(lstate, Mlist3(setb_sym, lit, Mlist2(literal_sym, idx)));
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
                add_instr(instrs, Mlist3(load_sym, loc, Mlist2(foreign_sym, intern("env_get"))));
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
                add_instr(instrs, Mlist3(load_sym, next_loc(0), ret_label));
                add_instr(instrs, Mlist3(load_sym, next_loc(1), fp_reg));
                
                // jump to new procedure
                add_instr(instrs, Mlist3(load_sym, fp_reg, Mlist3(mem_sym, fp_reg, size_sym)));
                add_instr(instrs, Mlist3(branch_sym, Mfixnum(branch_reg), code_loc));
                add_instr(instrs, Mlist2(label_sym, ret_label));

                // cleanup
                add_instr(instrs, Mlist3(load_sym, fp_reg, Mlist3(mem_sym, fp_reg, Mlist2(intern("-"), size_sym))));
                add_instr(instrs, Mlist3(load_sym, env_reg, stash_loc));
                add_instr(instrs, Mlist3(load_sym, dst, cres_reg));
            } else if (literal_formp(src) || closure_formp(src) || arg_formp(src)) {
                // (set! _ (<type> _)) =>
                //   (load <tmp> (mem+ %fp 0))
                //   (load %fp (mem+ %fp 0)
                //   (load _ )
                add_instr(instrs, Mcons(load_sym, minim_cdr(expr)));
            } else {
                error1("compile2_proc", "unknown set! sequence", expr);
            }
        } else if (bindb_formp(expr)) {
            // (bind! _ _) =>
            //   (c-call env_set %env <name> <get>)    // foreign call
            mobj loc = cstate_gensym(cstate, tloc_pre);
            mobj val = minim_car(minim_cddr(expr));
            add_instr(instrs, Mlist3(load_sym, loc, Mlist2(foreign_sym, intern("env_get"))));
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
//  Compilation (phase 4, register allocation and frame resolution
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

static int x86_alloc_regp(mobj r) {
    return r == x86_carg0 ||
        r == x86_carg1 ||
        r == x86_carg2 ||
        r == x86_carg3 ||
        r == x86_cres;
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
                } else if (variant == closure_sym ||
                    variant == literal_sym ||
                    variant == foreign_sym ||
                    variant == arg_sym) {
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
            if (minim_consp(r) && i == minim_fixnum(minim_cadr(r))) {   // quick check for `(local _)
                found = 1;
                break;
            }
        );

        if (!found) {
            e = Mlist2(local_sym, Mfixnum(i));
            rstate_scope(rs) = Mcons(e, rstate_scope(rs));
            return e;
        }
    }

    e = Mlist2(local_sym, Mfixnum(i));
    rstate_scope(rs) = Mcons(e, rstate_scope(rs));
    rstate_fsize(rs)++;
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

// static mobj lookup_

static void compile4_proc(mobj cstate, mobj fstate) {
    mobj exprs, rstate, instrs;

    // initialize register state and compute last uses
    rstate = init_rstate();
    exprs = fstate_asm(fstate);
    rstate_set_lasts(rstate, exprs);

    printf("> "); write_object(Mport(stdout, 0x0), fstate_asm(fstate)); printf("\n");
    write_object(Mport(stdout, 0x0), rstate); fprintf(stdout, "\n");

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
                if (variant == closure_sym || variant == literal_sym || variant == foreign_sym) {
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
                        if (minim_consp(base))
                            error1("compile4_proc", "expected a base register", src);
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
                    if (minim_consp(src)) { // quick check for `($local _)`
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
                if (minim_consp(src)) { // quick check for `($local _)`
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
            add_instr(instrs, Mlist2(op, tgt));
        } else if (op == x86_call) {
            if (minim_cadr(expr) != x86_cres)
                unimplemented_error("cannot call from any other register than %rax");
            rstate_stash(rstate);
            add_instr(instrs, expr);
        } else if (op == label_sym) {
            // do nothing
        } else {
            error1("compile4_proc", "unknown sequence", expr);
        }

        rstate_prune_lasts(rstate, expr);
    );

    fstate_asm(fstate) = list_reverse(instrs);
    printf("< "); write_object(Mport(stdout, 0x0), fstate_asm(fstate)); printf("\n");
}

//
//  Public API
//

void compile_module(mobj op, mobj name, mobj es) {
    mobj cstate, procs;

    // check for initialization
    if (!cstate_init) {
        init_compile_globals();
        init_x86_compiler_globals();
    }    

    // logging
    fputs("[compiling ", stdout);
    write_object(th_output_port(get_thread()), name);
    fputs("]\n", stdout);
    fflush(stdout);

    // phase 1: flattening pass
    cstate = init_cstate(name);
    compile1_module_loader(cstate, es);

    fputs("1> ", stdout);
    write_object(th_output_port(get_thread()), cstate);
    fputs("\n\n", stdout);
    fflush(stdout);

    // phase 2: pseudo assembly pass
    for (procs = cstate_procs(procs); !minim_nullp(procs); procs = minim_cdr(procs)) {
        compile2_proc(cstate, minim_car(procs));
    }

    fputs("2> ", stdout);
    write_object(th_output_port(get_thread()), cstate);
    fputs("\n\n", stdout);
    fflush(stdout);

    // phase 3: architecture-specific assembly
    for (procs = cstate_procs(procs); !minim_nullp(procs); procs = minim_cdr(procs)) {
        compile3_proc(cstate, minim_car(procs));
    }

    fputs("3> ", stdout);
    write_object(th_output_port(get_thread()), cstate);
    fputs("\n\n", stdout);
    fflush(stdout);

    // phase 4: register allocation
    for (procs = cstate_procs(procs); !minim_nullp(procs); procs = minim_cdr(procs)) {
        compile4_proc(cstate, minim_car(procs));
    }

    fputs("4> ", stdout);
    write_object(th_output_port(get_thread()), cstate);
    fputs("\n\n", stdout);
    fflush(stdout);
}
