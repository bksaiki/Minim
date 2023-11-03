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

mobj tloc_pre;
mobj label_pre;
mobj imm_sym;
mobj literal_sym;
mobj closure_sym;
mobj foreign_sym;
mobj arg_sym;
mobj local_sym;
mobj next_sym;
mobj prev_sym;

mobj lookup_sym;
mobj apply_sym;
mobj cmp_sym;
mobj push_frame_sym;
mobj pop_frame_sym;
mobj bindb_sym;
mobj push_valueb_sym;
mobj label_sym;
mobj branch_sym;
mobj ret_sym;

mobj load_sym;
mobj ccall_sym;
mobj mem_sym;

mobj size_sym;
mobj cc_reg;
mobj env_reg;
mobj fp_reg;
mobj sp_reg;
mobj carg_reg;
mobj cres_reg;

#define lookup_formp(e)     syntax_formp(lookup_sym, e)
#define literal_formp(e)    syntax_formp(literal_sym, e)
#define closure_formp(e)    syntax_formp(closure_sym, e)
#define arg_formp(e)        syntax_formp(arg_sym, e)
#define local_formp(e)      syntax_formp(local_sym, e)
#define apply_formp(e)      syntax_formp(apply_sym, e)
#define label_formp(e)      syntax_formp(lambda_sym, e)
#define branch_formp(e)     syntax_formp(branch_sym, e)
#define bindb_formp(e)      syntax_formp(bindb_sym, e)
#define ccall_formp(e)      syntax_formp(ccall_sym, e)
#define ret_formp(e)        syntax_formp(ret_sym, e)

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
    mobj fstate = Mvector(fstate_length, NULL);
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
    mobj id, itypes, otype, fstate, args;
    size_t argc;

    id = minim_cadr(e);
    itypes = minim_car(minim_cddr(e));
    otype = minim_cadr(minim_cddr(e));
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

static mobj c_call0(const char* name) {
    return Mcons(ccall_sym,
           Mcons(Mlist2(foreign_sym, Mstring(name)),
           minim_null));
}

static mobj c_call1(const char* name, mobj arg0) {
    return Mcons(ccall_sym,
           Mcons(Mlist2(foreign_sym, Mstring(name)),
           Mlist1(arg0)));
}

static mobj c_call2(const char *name, mobj arg0, mobj arg1) {
    return Mcons(ccall_sym,
           Mcons(Mlist2(foreign_sym, Mstring(name)),
           Mlist2(arg0, arg1)));
}

static mobj c_call3(const char* name, mobj arg0, mobj arg1, mobj arg2) {
    return Mcons(ccall_sym,
           Mcons(Mlist2(foreign_sym, Mstring(name)),
           Mlist3(arg0, arg1, arg2)));
}

static mobj c_call4(const char *name, mobj arg0, mobj arg1, mobj arg2, mobj arg3) {
    return Mcons(ccall_sym,
           Mcons(Mlist2(foreign_sym, Mstring(name)),
           Mlist4(arg0, arg1, arg2, arg3)));
}

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
            if (lookup_formp(src)) {
                // (set! <loc> (lookup _)) =>
                //   (load <tmp> ($local 0))        // environment ptr
                //   (c-call env_get <tmp> <get>)   // foreign call
                //   (load <loc> %Cres)
                mobj env = cstate_gensym(cstate, tloc_pre);
                add_instr(instrs, Mlist3(load_sym, env, env_reg));      // environment ptr
                add_instr(instrs, c_call2("env_get", env, minim_cadr(src)));
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
            //   (load <tmp> ($local 0))                // environment ptr
            //   (c-call env_set <tmp> <name> <get>)    // foreign call
            mobj env = cstate_gensym(cstate, tloc_pre);
            mobj val = minim_car(minim_cddr(expr));
            add_instr(instrs, Mlist3(load_sym, env, env_reg));
            add_instr(instrs, c_call3("env_set", env, minim_cadr(expr), val));
        } else if (ret_formp(expr)) {
            // (ret) =>
            //   (load <tmp> (mem+ %fp 0))
            //   (jmp <tmp>)
            mobj loc = cstate_gensym(cstate, tloc_pre);
            add_instr(instrs, Mlist3(load_sym, loc, mem_offset(fp_reg, 0)));
            add_instr(instrs, Mlist3(branch_sym, Mfixnum(branch_reg), loc));
        } else if (ccall_formp(expr) || label_formp(expr) || branch_formp(expr)) {
            // (c-call ...) => leave as is
            add_instr(instrs, expr);
        } else {
            error1("compile2_proc", "unknown sequence", expr);
        }
    }

    fstate_asm(fstate) = list_reverse(instrs);
}

//
//  Compilation (phase 3, compilation to architecture-specific assembly)
//



//
//  Public API
//

void compile_module(mobj op, mobj name, mobj es) {
    mobj cstate, procs;

    // check for initialization
    if (!cstate_init) {
        init_compile_globals();
    }    

    // logging
    fputs("[compiling ", stdout);
    write_object(Mport(stdout, PORT_FLAG_OPEN), name);
    fputs("]\n", stdout);
    fflush(stdout);

    // phase 1: flattening pass
    cstate = init_cstate(name);
    compile1_module_loader(cstate, es);

    fputs("1> ", stdout);
    write_object(Mport(stdout, PORT_FLAG_OPEN), cstate);
    fputs("\n", stdout);
    fflush(stdout);

    // phase 2: pseudo assembly pass
    for (procs = cstate_procs(procs); !minim_nullp(procs); procs = minim_cdr(procs)) {
        compile2_proc(cstate, minim_car(procs));
    }

    fputs("2> ", stdout);
    write_object(Mport(stdout, PORT_FLAG_OPEN), cstate);
    fputs("\n", stdout);
    fflush(stdout);

    // phase 3: architecture-specific assembly

    // phase 4: register allocation
}
