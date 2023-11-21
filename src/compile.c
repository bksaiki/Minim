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
static mobj literal_sym;
static mobj closure_sym;
static mobj code_sym;
static mobj foreign_sym;
static mobj arg_sym;
static mobj local_sym;
static mobj next_sym;
static mobj prev_sym;

static mobj add_sym;
static mobj sub_sym;
static mobj lookup_sym;
static mobj apply_sym;
static mobj tail_apply_sym;
static mobj cmp_sym;
static mobj push_frame_sym;
static mobj pop_frame_sym;
static mobj bindb_sym;
static mobj bind_setb_sym;
static mobj push_valueb_sym;
static mobj label_sym;
static mobj branch_sym;
static mobj ret_sym;
static mobj stash_sym;
static mobj unstash_sym;

static mobj load_sym;
static mobj ccall_sym;
static mobj mem_sym;

static mobj size_sym;
static mobj cc_reg;
static mobj env_reg;
static mobj fp_reg;
static mobj sp_reg;
static mobj argc_reg;
static mobj carg_reg;
static mobj cres_reg;

static void init_compile_globals() {
    tloc_pre = intern("$t");
    label_pre = intern("$L");

    reloc_sym = intern("$reloc");
    literal_sym = intern("$literal");
    closure_sym = intern("$closure");
    code_sym = intern("$code");
    label_sym = intern("$label");
    foreign_sym = intern("$foreign");
    arg_sym = intern("$arg");
    local_sym = intern("$local");
    next_sym = intern("$next");
    prev_sym = intern("$prev");

    add_sym = intern("+");
    sub_sym = intern("-");
    lookup_sym = intern("lookup");
    apply_sym = intern("apply");
    tail_apply_sym = intern("tapply");
    cmp_sym = intern("cmp");
    push_frame_sym = intern("push-frame!");
    pop_frame_sym = intern("pop-frame!");
    bind_setb_sym = intern("bind/set!");
    bindb_sym = intern("bind!");
    push_valueb_sym = intern("push-value!");
    branch_sym = intern("branch");
    ret_sym = intern("ret");
    stash_sym = intern("stash!");
    unstash_sym = intern("unstash!");

    load_sym = intern("load");
    ccall_sym = intern("c-call");
    mem_sym = intern("mem+");

    size_sym = intern("%size");
    cc_reg = intern("%cc");
    env_reg = intern("%env");
    argc_reg = intern("%argc");
    fp_reg = intern("%fp");
    sp_reg = intern("%sp");
    carg_reg = intern("%Carg");
    cres_reg = intern("%Cres");

    cstate_init = 1;
}

#define reloc_formp(e)      syntax_formp(reloc_sym, e)
#define literal_formp(e)    syntax_formp(literal_sym, e)
#define closure_formp(e)    syntax_formp(closure_sym, e)
#define code_formp(e)       syntax_formp(code_sym, e)
#define foreign_formp(e)    syntax_formp(foreign_sym, e)
#define carg_formp(e)       syntax_formp(carg_reg, e)
#define arg_formp(e)        syntax_formp(arg_sym, e)
#define local_formp(e)      syntax_formp(local_sym, e)
#define next_formp(e)       syntax_formp(next_sym, e)

#define lookup_formp(e)         syntax_formp(lookup_sym, e)
#define add_formp(e)            syntax_formp(add_sym, e)
#define sub_formp(e)            syntax_formp(sub_sym, e)
#define cmp_formp(e)            syntax_formp(cmp_sym, e)
#define push_frame_formp(e)     syntax_formp(push_frame_sym, e)
#define pop_frame_formp(e)      syntax_formp(pop_frame_sym, e)
#define apply_formp(e)          syntax_formp(apply_sym, e)
#define tapply_formp(e)         syntax_formp(tail_apply_sym, e)
#define label_formp(e)          syntax_formp(label_sym, e)
#define branch_formp(e)         syntax_formp(branch_sym, e)
#define bindb_formp(e)          syntax_formp(bindb_sym, e)
#define bind_setb_formp(e)      syntax_formp(bind_setb_sym, e)
#define ret_formp(e)            syntax_formp(ret_sym, e)
#define stash_formp(e)          syntax_formp(stash_sym, e)
#define unstash_formp(e)        syntax_formp(unstash_sym, e)

#define load_formp(e)           syntax_formp(load_sym, e)
#define ccall_formp(e)          syntax_formp(ccall_sym, e)
#define mem_formp(e)            syntax_formp(mem_sym, e)

#define imake_reloc(i)          Mlist2(reloc_sym, i)
#define imake_literal(o)        Mlist2(literal_sym, o)
#define imake_foreign(id)       Mlist2(foreign_sym, id)
#define imake_code(i)           Mlist2(code_sym, i)
#define imake_closure(i)        Mlist2(closure_sym, i)
#define imake_label(id)         Mlist2(label_sym, id)

#define imake_add3(dst, src1, src2)     Mlist4(add_sym, dst, src1, src2)
#define imake_add2(dst, src)            Mlist3(add_sym, dst, src)
#define imake_sub3(dst, src1, src2)     Mlist4(sub_sym, dst, src1, src2)
#define imake_sub2(dst, src)            Mlist3(sub_sym, dst, src)
#define imake_set(dst, src)             Mlist3(setb_sym, dst, src)
#define imake_bind(env, id, v)          Mlist4(bindb_sym, env, id, v)
#define imake_bset(env, id, v)          Mlist4(bind_setb_sym, env, id, v)
#define imake_load(dst, src)            Mlist3(load_sym, dst, src)
#define imake_cmp(src1, src2)           Mlist3(cmp_sym, src1, src2)
#define imake_branch(variant, tgt)      Mlist3(branch_sym, variant, tgt)
#define imake_ccall(tgt, args)          Mcons(ccall_sym, Mcons(tgt, args))
#define imake_apply(tgt, args)          Mcons(apply_sym, Mcons(tgt, args))
#define imake_tapply(tgt, args)         Mcons(tail_apply_sym, Mcons(tgt, args))

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
#define branch_eq               1
#define branch_neq              2
#define branch_ge               3
#define branch_reg              4

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
#define fstate_name(fs)         minim_vector_ref(fs, 0)
#define fstate_args(fs)         minim_vector_ref(fs, 1)
#define fstate_rest(fs)         minim_vector_ref(fs, 2)
#define fstate_asm(fs)          minim_vector_ref(fs, 3)

static mobj init_fstate() {
    mobj fstate = make_fstate();
    fstate_name(fstate) = minim_false;
    fstate_args(fstate) = minim_null;
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

// Returns the arguments of a procedure as a pair:
// the required arguments as a list and optionally the name
// of a rest argument (#f otherwise).
static mobj procedure_formals(mobj e) {
    mobj req = minim_null;
    mobj args = minim_cadr(e);
    for (; minim_consp(args); args = minim_cdr(args))
        req = Mcons(minim_car(args), req);
    return Mcons(list_reverse(req), minim_nullp(args) ? minim_false : args);
}

// Compiles a foreign procedure `(#%foreign-procedure ...)`
static mobj compile1_foreign(mobj cstate, mobj e) {
    mobj id, itypes, fstate, args, idx, exn_label, exn;
    size_t argc, i;

    id = minim_cadr(e);
    itypes = minim_car(minim_cddr(e));
    // otype = minim_cadr(minim_cddr(e));
    if (lookup_prim(minim_string(id)) == NULL)
        error1("compile1_foreign", "unknown foreign procedure", id);

    fstate = init_fstate();
    fstate_name(fstate) = string_to_symbol(id);
    fstate_args(fstate) = itypes;
    fstate_rest(fstate) = minim_false;

    // check arity
    argc = list_length(itypes);
    exn_label = cstate_gensym(cstate, label_pre);
    fstate_add_asm(fstate, imake_cmp(argc_reg, Mfixnum(argc)));
    fstate_add_asm(fstate, imake_branch(Mfixnum(branch_eq), exn_label));

    // handle arity exception
    exn = cstate_gensym(cstate, tloc_pre);
    idx = cstate_add_reloc(cstate, imake_foreign(Mstring("arity_exn")));
    fstate_add_asm(fstate, imake_set(exn, imake_reloc(idx)));
    fstate_add_asm(fstate, imake_ccall(exn, Mlist2(argc_reg, Mfixnum(argc))));
    fstate_add_asm(fstate, imake_label(exn_label));

    i = 0;
    args = minim_null;
    for (; !minim_nullp(itypes); itypes = minim_cdr(itypes)) {
        mobj loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, imake_set(loc, arg_loc(i)));
        args = Mcons(loc, args);
        i++;
    }

    // call into C
    idx = cstate_add_reloc(cstate, imake_foreign(id));
    fstate_add_asm(fstate, imake_ccall(imake_reloc(idx), list_reverse(args)));
    fstate_add_asm(fstate, Mlist1(ret_sym));

    return fstate;
}

// Compiles an expression.
static mobj compile1_expr(mobj cstate, mobj fstate, mobj e, int tailp) {
    mobj loc;

loop:
    if (let_values_formp(e)) {
        // let-values form  form
        mobj bindings, tenv;

        // create a new frame and bind to a temporary
        tenv = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, Mlist2(push_frame_sym, tenv));

        // bind the expressions
        bindings = minim_cadr(e);
        for_each(bindings,
            mobj bind = minim_car(bindings);
            mobj ids = minim_car(bind);
            size_t vc = list_length(ids);
            loc = compile1_expr(cstate, fstate, minim_cadr(bind), 0);
            if (vc == 1) {
                mobj lit = cstate_gensym(cstate, tloc_pre);
                mobj idx = cstate_add_reloc(cstate, imake_literal(minim_car(ids)));
                fstate_add_asm(fstate, imake_set(lit, imake_reloc(idx)));
                fstate_add_asm(fstate, imake_bind(tenv, lit, loc));
            } else {
                unimplemented_error("multiple values for let(rec)-values");
            }
        );

        // push frame
        fstate_add_asm(fstate, imake_set(env_reg, tenv));

        // compile body and pop frame
        loc = compile1_expr(cstate, fstate, Mcons(begin_sym, minim_cddr(e)), tailp);
        fstate_add_asm(fstate, Mlist1(pop_frame_sym));
    } else if (letrec_values_formp(e)) {
        // letrec-values form
        mobj bindings;

        // create a new frame and bind to a temporary
        fstate_add_asm(fstate, Mlist2(push_frame_sym, env_reg));

        // TOOD: poison the initial values

        // bind the expressions
        bindings = minim_cadr(e);
        for_each(bindings,
            mobj bind = minim_car(bindings);
            mobj ids = minim_car(bind);
            size_t vc = list_length(ids);
            loc = compile1_expr(cstate, fstate, minim_cadr(bind), 0);
            if (vc == 1) {
                mobj lit = cstate_gensym(cstate, tloc_pre);
                mobj idx = cstate_add_reloc(cstate, imake_literal(minim_car(ids)));
                fstate_add_asm(fstate, imake_set(lit, imake_reloc(idx)));
                fstate_add_asm(fstate, imake_bind(env_reg, lit, loc));
            } else {
                unimplemented_error("multiple values for let(rec)-values");
            }
        );

        // compile body and pop frame
        loc = compile1_expr(cstate, fstate, Mcons(begin_sym, minim_cddr(e)), tailp);
        fstate_add_asm(fstate, Mlist1(pop_frame_sym));
    } else if (lambda_formp(e)) {
        // lambda form
        mobj f2state, arity, req, rest, body, bind, lit, idx;
        mobj exn_label, exn;
        size_t argc, i, branch_variant;

        f2state = init_fstate();
        arity = procedure_formals(e);
        fstate_args(f2state) = minim_car(arity);
        fstate_rest(f2state) = minim_cdr(arity);

        // procedure: check arity
        req = fstate_args(f2state);
        rest = fstate_rest(f2state);
        branch_variant = minim_falsep(rest) ? branch_eq : branch_ge;

        // procedure: check arity
        argc = list_length(req);
        exn_label = cstate_gensym(cstate, label_pre);
        fstate_add_asm(f2state, imake_cmp(argc_reg, Mfixnum(argc)));
        fstate_add_asm(f2state, imake_branch(Mfixnum(branch_variant), exn_label));

        // procedure: handle arity exception
        exn = cstate_gensym(cstate, tloc_pre);
        idx = cstate_add_reloc(cstate, imake_foreign(Mstring("arity_exn")));
        fstate_add_asm(f2state, imake_set(exn, imake_reloc(idx)));
        fstate_add_asm(f2state, imake_ccall(exn, Mlist2(argc_reg, Mfixnum(argc))));
        fstate_add_asm(f2state, imake_label(exn_label));

        // procedure: create rest argument
        if (!minim_falsep(rest)) {
            error("compile1_proc", "rest arguments unsupported");
            // rest_val = cstate_gensym(cstate, tloc_pre);
            // idx = cstate_add_reloc(cstate, Mlist2(foreign_sym, Mstring("do_rest_arg")));
            // fstate_add_asm(f2state, Mlist3(setb_sym, c_arg(0), Mlist2(reloc_sym, idx)));
            // fstate_add_asm(f2state, Mlist3(setb_sym, c_arg(1), Mfixnum(argc)));
            // fstate_add_asm(f2state, Mlist2(ccall_sym, c_arg(0)));
            // fstate_add_asm(f2state, Mlist3(setb_sym, rest_val, cres_reg));
        }
        
        // procedure: push frame
        fstate_add_asm(f2state, Mlist2(push_frame_sym, env_reg));

        // procedure: get function pointer for binding
        bind = cstate_gensym(cstate, tloc_pre);
        idx = cstate_add_reloc(cstate, imake_foreign(Mstring("env_define")));
        fstate_add_asm(f2state, imake_set(bind, imake_reloc(idx)));

        // procedure: bind arguments in order
        i = 0;
        req = fstate_args(f2state);
        for_each(req,
            lit = cstate_gensym(cstate, tloc_pre);
            idx = cstate_add_reloc(cstate, imake_literal(minim_car(req)));
            fstate_add_asm(f2state, imake_set(lit, imake_reloc(idx)));
            fstate_add_asm(f2state, imake_ccall(bind, Mlist3(env_reg, lit, arg_loc(i))));
            ++i;
        );

        // bind rest argument
        // if (!minim_falsep(rest)) {
        //     lit = cstate_gensym(cstate, tloc_pre);
        //     idx = cstate_add_reloc(cstate, Mlist2(literal_sym, rest));
        //     fstate_add_asm(f2state, Mlist3(setb_sym, lit, Mlist2(reloc_sym, idx)));
        //     fstate_add_asm(f2state, Mlist5(ccall_sym, bind, env_reg, lit, rest_val));
        // }

        // procedure: compile body
        body = Mcons(begin_sym, minim_cddr(e));
        compile1_expr(cstate, f2state, body, 1);

        // procedure: return
        fstate_add_asm(f2state, Mlist1(pop_frame_sym));
        fstate_add_asm(f2state, Mlist1(ret_sym));

        // register procedure
        i = cstate_add_proc(cstate, f2state);
        loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, imake_set(loc, imake_closure(Mfixnum(i))));
    } else if (begin_formp(e)) {
        // begin form (at least 2 clauses)
        // execute statements and return the last one
        e = minim_cdr(e);
        for (; !minim_nullp(minim_cdr(e)); e = minim_cdr(e))
            compile1_expr(cstate, fstate, minim_car(e), 0);
        e = minim_car(e);
        goto loop;
    } else if (if_formp(e)) {
        // if form
        mobj cond, ift, iff, liff, lend, idx, lit;
        liff = cstate_gensym(cstate, label_pre);
        lend = cstate_gensym(cstate, label_pre);
        loc = cstate_gensym(cstate, tloc_pre);
        lit = cstate_gensym(cstate, tloc_pre);

        // compile condition
        cond = compile1_expr(cstate, fstate, minim_cadr(e), 0);
        idx = cstate_add_reloc(cstate, imake_literal(minim_false));
        fstate_add_asm(fstate, imake_set(lit, imake_reloc(idx)));
        fstate_add_asm(fstate, imake_cmp(cond, lit));
        fstate_add_asm(fstate, imake_branch(Mfixnum(branch_eq), liff));
    
        // compile if-true branch
        ift = compile1_expr(cstate, fstate, minim_car(minim_cddr(e)), tailp);
        fstate_add_asm(fstate, imake_set(loc, ift));
        fstate_add_asm(fstate, imake_branch(Mfixnum(branch_uncond), lend));

        // compile if-false branch
        fstate_add_asm(fstate, imake_label(liff));
        iff = compile1_expr(cstate, fstate, minim_cadr(minim_cddr(e)), tailp);
        fstate_add_asm(fstate, imake_set(loc, iff));

        // exit
        fstate_add_asm(fstate, imake_label(lend));
    } else if (setb_formp(e)) {
        // set! form
        mobj id, lit, idx;
        id = minim_cadr(e);
        loc = compile1_expr(cstate, fstate, minim_car(minim_cddr(e)), 0);
        lit = cstate_gensym(cstate, tloc_pre);
        idx = cstate_add_reloc(cstate, imake_literal(id));
        fstate_add_asm(fstate, imake_set(lit, imake_reloc(idx)));
        fstate_add_asm(fstate, imake_bset(env_reg, lit, loc));
    } else if (quote_formp(e)) {
        // quote form
        mobj idx = cstate_add_reloc(cstate, imake_literal(minim_cadr(e)));
        loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, imake_set(loc, imake_reloc(idx)));
    } else if (foreign_proc_formp(e)) {
        // foreign procedure
        mobj f2state = compile1_foreign(cstate, e);
        size_t idx = cstate_add_proc(cstate, f2state);
        loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, imake_set(loc, imake_closure(Mfixnum(idx))));
    } else if (minim_consp(e)) {
        // application
        mobj op, args, instr;
        args = minim_null;
        op = compile1_expr(cstate, fstate, minim_car(e), 0);
        for (e = minim_cdr(e); !minim_nullp(e); e = minim_cdr(e))
            args = Mcons(compile1_expr(cstate, fstate, minim_car(e), 0), args);

        loc = cstate_gensym(cstate, tloc_pre);
        if (tailp) {
            mobj idx = cstate_add_reloc(cstate, imake_literal(minim_void));
            fstate_add_asm(fstate, imake_tapply(op, list_reverse(args)));
            fstate_add_asm(fstate, imake_set(loc, imake_reloc(idx)));   // compiler requires result
        } else {
            instr = imake_apply(op, list_reverse(args));
            fstate_add_asm(fstate, imake_set(loc, instr));
        }
    } else if (minim_nullp(e)) {
        // illegal
        error1("compile1_expr", "empty application", e);
    } else if (minim_symbolp(e)) {
        // identifier
        mobj lit = cstate_gensym(cstate, tloc_pre);
        mobj idx = cstate_add_reloc(cstate, imake_literal(e));
        loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, imake_set(lit, imake_reloc(idx)));
        fstate_add_asm(fstate, imake_set(loc, Mlist2(lookup_sym, lit)));
    } else if (literalp(e)) {
        // literals
        mobj idx;
        loc = cstate_gensym(cstate, tloc_pre);
        idx = cstate_add_reloc(cstate, imake_literal(e));
        fstate_add_asm(fstate, imake_set(loc, imake_reloc(idx)));
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
        loc = compile1_expr(cstate, lstate, minim_car(minim_cddr(e)), 0);
        if (vc > 1) {
            unimplemented_error("multiple values for define-values");
        } else if (vc == 1) {
            // bind the evaluation
            lit = cstate_gensym(cstate, tloc_pre);
            idx = cstate_add_reloc(cstate, imake_literal(minim_car(ids)));
            fstate_add_asm(lstate, imake_set(lit, imake_reloc(idx)));
            fstate_add_asm(lstate, imake_bind(env_reg, lit, loc));
        } // else (vc == 0) {
        // evaluate without binding, as in, do nothing
        // }
    } else if (begin_formp(e)) {
        // begin form (at least 2 clauses)
        // execute statements and return the last one
        e = minim_cdr(e);
        for (; !minim_nullp(minim_cdr(e)); e = minim_cdr(e))
            compile1_expr(cstate, lstate, minim_car(e), 0);
        e = minim_car(e);
        goto loop;
    } else {
        // expression
        loc = compile1_expr(cstate, lstate, e, 0);
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
    mobj instrs, exprs, frames;

    // frames
    frames = minim_null;

    // translate expressions in-order
    instrs = minim_null;
    exprs = fstate_asm(fstate);
    for (; !minim_nullp(exprs); exprs = minim_cdr(exprs)) {
        mobj expr = minim_car(exprs);
        if (setb_formp(expr)) {
            // (set! ...)
            mobj dst = minim_cadr(expr);
            mobj src = minim_car(minim_cddr(expr));
            if (minim_symbolp(src)) {
                // (set! <loc> <src>)
                add_instr(instrs, imake_load(dst, src));
            } else if (lookup_formp(src)) {
                // (set! <loc> (lookup _)) =>
                //   (c-call env_get %env <get>)   // foreign call
                //   (load <loc> %Cres)
                mobj loc = cstate_gensym(cstate, tloc_pre);
                mobj idx = cstate_add_reloc(cstate, imake_foreign(Mstring("env_get")));
                add_instr(instrs, imake_load(loc, imake_reloc(idx)));
                add_instr(instrs, imake_ccall(loc, Mlist2(env_reg, minim_cadr(src))));
                add_instr(instrs, imake_load(dst, cres_reg));
            }  else if (apply_formp(src)) {
                // (set! _ ($apply <closure> <args> ...)) =>                
                // prepare the stack for a call
                // reserve 2 words
                // move arguments into next frame
                mobj proc, es, stash_loc, label_loc, ret_label;
                mobj env_offset, code_offset;
                size_t argc, i;

                proc = minim_cadr(src);
                es = minim_cddr(src);
                argc = list_length(es);
                for (i = 2; !minim_nullp(es); ++i) {
                    add_instr(instrs, imake_load(next_loc(i), minim_car(es)));
                    es = minim_cdr(es);
                }

                // stash environment and replace with closure environment
                stash_loc = cstate_gensym(cstate, tloc_pre);
                env_offset = mem_offset(proc, minim_closure_env_offset);
                add_instr(instrs, imake_load(stash_loc, env_reg));
                add_instr(instrs, imake_load(env_reg, env_offset));

                // fill return address and previous frame position
                ret_label = cstate_gensym(cstate, label_pre);
                label_loc = cstate_gensym(cstate, tloc_pre);
                add_instr(instrs, imake_load(label_loc, ret_label));
                add_instr(instrs, imake_load(next_loc(0), label_loc));
                add_instr(instrs, imake_load(next_loc(1), fp_reg));

                // set function arity
                add_instr(instrs, imake_load(argc_reg, Mfixnum(argc)));

                // extract closure code location
                code_offset = mem_offset(proc, minim_closure_code_offset);
                add_instr(instrs, imake_load(cres_reg, code_offset));
                
                // jump to new procedure
                add_instr(instrs, Mlist1(stash_sym));
                add_instr(instrs, imake_sub3(fp_reg, fp_reg, size_sym));
                add_instr(instrs, Mlist3(branch_sym, Mfixnum(branch_reg), cres_reg));                

                // cleanup
                add_instr(instrs, imake_label(ret_label));
                // callee cleans up
                // add_instr(instrs, Mlist4(add_sym, fp_reg, fp_reg, size_sym));
                add_instr(instrs, Mlist1(unstash_sym));
                add_instr(instrs, imake_load(dst, cres_reg));
                add_instr(instrs, imake_load(env_reg, stash_loc));
            } else if (closure_formp(src)) {
                // (set! _ ($closure <idx>)) =>
                //   (load <tmp> ($code <idx>))
                //   (c-call make_closure %env <tmp>)
                //   (load <loc> %Cres)
                mobj loc = cstate_gensym(cstate, tloc_pre);
                mobj loc2 = cstate_gensym(cstate, tloc_pre);
                mobj idx = cstate_add_reloc(cstate, imake_foreign(Mstring("make_closure")));
                add_instr(instrs, imake_load(loc, imake_code(minim_cadr(src))));
                add_instr(instrs, imake_load(loc2, imake_reloc(idx)));
                add_instr(instrs, imake_ccall(loc2, Mlist2(env_reg, loc)));
                add_instr(instrs, imake_load(dst, cres_reg));
            } else if (minim_fixnump(src) || reloc_formp(src) || arg_formp(src)) {
                // (set! _ (<type> _)) => "as-is"
                add_instr(instrs, imake_load(dst, src));
            } else {
                error1("compile2_proc", "unknown set! sequence", expr);
            }
        } else if (bindb_formp(expr) || bind_setb_formp(expr)) {
            // (bind! <env> <name> <val>) =>
            //   (c-call <env> <name> <val>)
            mobj env, name, val, variant, loc, idx;

            env = minim_cadr(expr);
            name = minim_car(minim_cddr(expr));
            val = minim_cadr(minim_cddr(expr));
            variant = bindb_formp(expr) ? Mstring("env_define") : Mstring("env_set");

            loc = cstate_gensym(cstate, tloc_pre);
            idx = cstate_add_reloc(cstate, imake_foreign(variant));
            add_instr(instrs, imake_load(loc, imake_reloc(idx)));
            add_instr(instrs, imake_ccall(loc, Mlist3(env, name, val)));
        } else if (tapply_formp(expr)) {
            // ($tapply <closure> <args> ...) =>                
            // prepare the stack for a _tail_ call
            // move arguments into _this_ frame
            mobj proc, es, env_offset, code_offset;
            size_t i, argc;
            
            // temporarily store the register in the next frame
            proc = minim_cadr(expr);
            es = minim_cddr(expr);
            argc = list_length(es);
            for (i = 0; !minim_nullp(es); ++i) {
                add_instr(instrs, imake_load(next_loc(i), minim_car(es)));
                es = minim_cdr(es);
            }

            // use closure environment (no need to stash since already stashed)
            // stash environment and replace with closure environment
            env_offset = mem_offset(proc, minim_closure_env_offset);
            add_instr(instrs, imake_load(env_reg, env_offset));

            // set function arity
            add_instr(instrs, imake_load(argc_reg, Mfixnum(argc)));

            // extract closure code location
            code_offset = mem_offset(proc, minim_closure_code_offset);
            add_instr(instrs, imake_load(cres_reg, code_offset));

            // move arguments into this frame
            for (i = 0; i < argc; ++i) {
                add_instr(instrs, imake_load(c_arg(0), next_loc(i)));
                add_instr(instrs, imake_load(arg_loc(i), c_arg(0)));
            }

            // jump to new procedure
            // no need to update frame info or cleanup
            add_instr(instrs, Mlist3(branch_sym, Mfixnum(branch_reg), cres_reg));
        } else if (ret_formp(expr)) {
            // (ret) =>
            //   (load <tmp> (mem+ %fp 0))
            //   (load <tmp> (mem+ %fp (- 8)))
            //   (jmp <tmp>)
            add_instr(instrs, imake_load(c_arg(0), mem_offset(fp_reg, 0)));
            add_instr(instrs, imake_load(fp_reg, mem_offset(fp_reg, -8)));
            add_instr(instrs, imake_branch(Mfixnum(branch_reg), c_arg(0)));
        } else if (push_frame_formp(expr)) {
            // (push-frame! <dst>) =>
            //   <arity check>
            //   <optionally gather rest argument>
            //   <create new frame>
            //   <bind required arguments>
            mobj ext, idx;

            // stash old environment register
            frames = Mcons(env_reg, frames);

            // create new frame
            ext = cstate_gensym(cstate, tloc_pre);
            idx = cstate_add_reloc(cstate, imake_foreign(Mstring("env_extend")));
            add_instr(instrs, imake_load(ext, imake_reloc(idx)));
            add_instr(instrs, imake_ccall(ext, Mlist1(env_reg)));
            add_instr(instrs, imake_load(minim_cadr(expr), cres_reg));
        } else if (pop_frame_formp(expr)) {
            // (pop-frame!) =>
            //   <load previous pointer>
            //   currently just an association list so cdr
            if (minim_nullp(frames))
                error1("compile2_proc", "no more frames to pop", expr);

            frames = minim_cdr(frames);
            add_instr(instrs, imake_load(env_reg, mem_offset(env_reg, minim_cdr_offset)));
        } else if (cmp_formp(expr) ||
                    ccall_formp(expr) ||
                    label_formp(expr) ||
                    branch_formp(expr) ||
                    add_formp(expr) ||
                    sub_formp(expr)) {
            // leave as is
            add_instr(instrs, expr);
        } else if (minim_consp(expr) && minim_car(expr) == push_valueb_sym) {
                // TODO
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
static mobj x86_je;
static mobj x86_jne;
static mobj x86_ge;
static mobj x86_cmp;

static mobj x86_fp;
static mobj x86_sp;
static mobj x86_env;
static mobj x86_argc;

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
    x86_je = intern("je");
    x86_jne = intern("jne");
    x86_ge = intern("jge");
    x86_cmp = intern("cmp");

    x86_fp = intern("%rbp");
    x86_sp = intern("%rsp");
    x86_env = intern("%r9");
    x86_argc = intern("%r14");

    x86_carg0 = intern("%rdi");
    x86_carg1 = intern("%rsi");
    x86_carg2 = intern("%rdx");
    x86_carg3 = intern("%rcx");
    x86_cres = intern("%rax");
}

static int reg_aliasp(mobj reg) {
    return carg_formp(reg) ||
            reg == cc_reg ||
            reg == fp_reg ||
            reg == sp_reg ||
            reg == env_reg ||
            reg == cres_reg ||
            reg == argc_reg;
}

static mobj replace_carg(mobj carg) {
    size_t i = minim_fixnum(minim_cadr(carg));
    if (i == 0) return x86_carg0;
    else if (i == 1) return x86_carg1;
    else if (i == 2) return x86_carg2;
    else if (i == 3) return x86_carg3;
    else error1("replace_carg", "too many call arguments", carg);
}

static mobj replace_alias(mobj reg) {
    if (carg_formp(reg)) return replace_carg(reg);
    else if (reg == cc_reg) error1("replace_alias", "unimplemented", reg);
    else if (reg == fp_reg) return x86_fp;
    else if (reg == sp_reg) error1("replace_alias", "unimplemented", reg);
    else if (reg == env_reg) return x86_env;
    else if (reg == cres_reg) return x86_cres;
    else if (reg == argc_reg) return x86_argc;
    else error1("replace_alias", "unknown", reg);
}

// Resolves architecture-independent register names.
static void compile3_resolve_regs(mobj fstate) {
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
        } else if (op == x86_add || op == x86_sub || op == x86_cmp) {
            mobj dst = minim_cadr(expr);
            mobj src = minim_car(minim_cddr(expr));
            if (reg_aliasp(dst))
                minim_cadr(expr) = replace_alias(dst);
            if (reg_aliasp(src))
                minim_car(minim_cddr(expr)) = replace_alias(src);
        } else if (op == x86_jmp || op == x86_je || op == x86_jne || op == x86_ge || op == x86_call) {
            mobj tgt = minim_cadr(expr);
            if (reg_aliasp(tgt))
                minim_cadr(expr) = replace_alias(tgt);
        } else if (op == label_sym || op == stash_sym || op == unstash_sym) {
            // do nothing
        } else {
            error1("compile3_resolve_regs", "unknown sequence", expr);
        }
    );
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
                add_instr(instrs, Mlist3(x86_mov, dst, src));
            } else {
                add_instr(instrs, Mcons(x86_mov, minim_cdr(expr)));
            }
        } else if (ccall_formp(expr)) {
            // (c-call <tgt> <arg> ...)
            mobj creg, args;

            // set appropriate register
            args = minim_cddr(expr);
            for (size_t i = 0; !minim_nullp(args); i++) {
                creg = replace_carg(c_arg(i));
                add_instr(instrs, Mlist3(x86_mov, creg, minim_car(args)));
                args = minim_cdr(args);
            }

            // set %Cres to target
            add_instr(instrs, Mlist3(x86_mov, x86_cres, minim_cadr(expr)));

            // stash caller-saved registers
            add_instr(instrs, Mlist1(stash_sym));

            // set %sp to be at the top of the frame
            add_instr(instrs, Mlist3(x86_mov, x86_sp, x86_fp));
            add_instr(instrs, Mlist3(x86_sub, x86_sp, size_sym));

            // call
            add_instr(instrs, Mlist2(x86_call, x86_cres));

            // unstash caller-saved registers
            add_instr(instrs, Mlist1(unstash_sym));
        } else if (branch_formp(expr)) {
            // (branch <type> <where>)
            mobj tgt = minim_car(minim_cddr(expr));
            size_t type = minim_fixnum(minim_cadr(expr));
            if (type == branch_uncond) {
                add_instr(instrs, Mlist2(x86_jmp, tgt));
            } else if (type == branch_eq) {
                add_instr(instrs, Mlist2(x86_je, tgt));
            } else if (type == branch_neq) {
                add_instr(instrs, Mlist2(x86_jne, tgt));
            } else if (type == branch_ge) {
                add_instr(instrs, Mlist2(x86_ge, tgt));
            } else if (type == branch_reg) {
                add_instr(instrs, Mlist2(x86_jmp, tgt));
            } else {
                error1("compile3_proc", "unknown branch type", minim_cadr(expr));
            }
        } else if (add_formp(expr)) {
            // (add <dst> <src1> <src2>)
            mobj dst = minim_cadr(expr);
            mobj src1 = minim_car(minim_cddr(expr));
            mobj src2 = minim_cadr(minim_cddr(expr));
            if (dst == src1) {
                // can be 2 address: (add <dst> <src2>)
                add_instr(instrs, Mlist3(x86_add, dst, src2));
            } else {
                // need to make 2 address
                add_instr(instrs, Mlist3(x86_mov, dst, src1));
                add_instr(instrs, Mlist3(x86_add, dst, src2));
            }
        } else if (sub_formp(expr)) {
            // (sub <dst> <src1> <src2>)
            mobj dst = minim_cadr(expr);
            mobj src1 = minim_car(minim_cddr(expr));
            mobj src2 = minim_cadr(minim_cddr(expr));
            if (dst == src1) {
                // can be 2 address: (add <dst> <src2>)
                add_instr(instrs, Mlist3(x86_sub, dst, src2));
            } else {
                // need to make 2 address
                add_instr(instrs, Mlist3(x86_mov, dst, src1));
                add_instr(instrs, Mlist3(x86_sub, dst, src2));
            }
        } else if (cmp_formp(expr)) {
            // (cmp <src1> <src2>)
            mobj src1 = minim_cadr(expr);
            mobj src2 = minim_car(minim_cddr(expr));
            add_instr(instrs, Mlist3(x86_cmp, src1, src2));
        } else if (label_formp(expr) ||
                    stash_formp(expr) ||
                    unstash_formp(expr)) {
            // do nothing
            add_instr(instrs, expr);
        } else {
            error1("compile3_proc", "unknown sequence", expr);
        }
    }

    fstate_asm(fstate) = list_reverse(instrs);

    // resolve architecture-independent registers
    compile3_resolve_regs(fstate);
}

//
//  Compilation (phase 4, register allocation and frame resolution)
//  Currently only supporting x86-64
//

#define rstate_length 3
#define make_rstate()           Mvector(rstate_length, NULL)
#define rstate_lasts(r)         minim_vector_ref(r, 0)
#define rstate_scope(r)         minim_vector_ref(r, 1)
#define rstate_fsize(r)         minim_vector_ref(r, 2)

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

static int frame_locp(mobj loc) {
    return arg_formp(loc) || local_formp(loc) || next_formp(loc) || loc == size_sym;
}

static mobj init_rstate() {
    mobj rstate = make_rstate();
    rstate_lasts(rstate) = minim_false;
    rstate_scope(rstate) = minim_null;
    rstate_fsize(rstate) = Mfixnum(0);
    return rstate;
}

static mobj update_uses(mobj uses, mobj treg, mobj expr) {
    mobj entry = list_assq(treg, uses);
    if (minim_falsep(entry)) {
        return Mcons(Mcons(treg, expr), uses);
    } else {
        minim_cdr(entry) = expr;
        return uses;
    }
}

static void rstate_set_lasts(mobj rs, mobj exprs) {
    mobj uses, expr, op;

    uses = minim_null;
    for_each(exprs,
        expr = minim_car(exprs);
        op = minim_car(expr);
        if (op == x86_mov) {
            mobj dst = minim_cadr(expr);
            mobj src = minim_car(minim_cddr(expr));
            if (tregp(dst))
                uses = update_uses(uses, dst, expr);

            if (minim_consp(src)) {
                mobj variant = minim_car(src);
                if (variant == mem_sym) {
                    // (mov <dst> (mem+ <src> <offset>))
                    src = minim_cadr(src);
                    if (tregp(src))
                        uses = update_uses(uses, src, expr);
                } else if (variant == code_sym ||
                            variant == reloc_sym ||
                            variant == arg_sym ||
                            variant == next_sym) {
                    // do nothing
                } else {
                    error1("rstate_set_lasts", "unknown mov sequence", expr);
                }
            } else if (tregp(src)) {
                uses = update_uses(uses, src, expr);
            } else if (minim_symbolp(src) || minim_fixnump(src)) {
                // do nothing
            } else {
                error1("rstate_set_lasts", "unknown mov sequence", expr);
            }
        } else if (op == x86_add || op == x86_sub || op == x86_cmp) {
            mobj dst = minim_cadr(expr);
            mobj src = minim_car(minim_cddr(expr));

            if (tregp(dst))
                uses = update_uses(uses, dst, expr);
            if (tregp(src))
                uses = update_uses(uses, src, expr);
        } else if (op == x86_jmp || op == x86_je || op == x86_jne || op == x86_ge || op == x86_call) {
            mobj tgt = minim_cadr(expr);
            if (tregp(tgt))
                uses = update_uses(uses, tgt, expr);
        } else if (op == label_sym || op == stash_sym || op == unstash_sym) {
            // do nothing
        } else {
            error1("rstate_set_lasts", "unknown sequence", expr);
        }
    );

    rstate_lasts(rs) = uses;
}

static mobj last_lookup(mobj rs, mobj reg) {
    mobj lasts = rstate_lasts(rs);
    for_each(lasts,
        if (minim_caar(lasts) == reg)
            return minim_cdar(lasts);
    )

    error1("last_lookup", "unknown key", Mlist2(reg, rstate_lasts(rs)));
}

static mobj treg_lookup_unsafe(mobj rs, mobj reg) {
    mobj scope = rstate_scope(rs);
    for_each(scope,
        if (minim_caar(scope) == reg)
            return minim_cdar(scope);
    );

    return NULL;
}

static mobj treg_lookup(mobj rs, mobj reg) {
    mobj maybe = treg_lookup_unsafe(rs, reg);
    if (!maybe) error1("treg_lookup", "unknown register", reg);
    return maybe;
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

        if (!found)
            break;
    }

    e = Mlist2(local_sym, Mfixnum(i));
    rstate_scope(rs) = Mcons(Mcons(treg, e), rstate_scope(rs));
    if (found) minim_fixnum(rstate_fsize(rs))++;
    return e;
}

// static mobj unnamed_assign_local(mobj rs, 

static mobj treg_assign_or_lookup(mobj rs, mobj treg) {
    mobj maybe;

    // try to lookup in scope
    maybe = treg_lookup_unsafe(rs, treg);
    if (maybe) return maybe;

    // otherwise, assign to stack
    return treg_assign_local(rs, treg);
}

// Reserves an unnamed local.
static mobj unnamed_assign_local(mobj rs) {
    return treg_assign_local(rs, minim_false);
}

// Unreserves an unnamed local
static void unnamed_unassign(mobj rs, mobj local) {
    mobj s, p;
    
    s = rstate_scope(rs);
    p = NULL;
    for_each(s,
        if (minim_cdar(s) == local) {
            if (p) minim_cdr(p) = minim_cdr(s); // remove internally
            else rstate_scope(rs) = minim_cdr(rstate_scope(rs)); // remove from front
            return;
        }
    );

    error1("unnamed_unassign", "register not found", local);
}

// For every temporary register, scan to see if we can remove it
// since it will no longer be used
static void rstate_prune_lasts(mobj rs, mobj e) {
    mobj s, ns;

    s = rstate_scope(rs);
    ns = minim_null;

    for_each(s,
        if (minim_symbolp(minim_caar(s))) {
            if (last_lookup(rs, minim_caar(s)) == e) {
                continue;
            }
        }

        ns = Mcons(minim_car(s), ns);
    );

    rstate_scope(rs) = list_reverse(ns);
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
        return Mfixnum(ptr_size * (2 + argc + localc));
    } if (next_formp(loc)) {
        offset = -ptr_size * (2 + argc + localc + minim_fixnum(minim_cadr(loc)));
        return Mlist3(mem_sym, x86_fp, Mfixnum(offset));
    } else if (local_formp(loc)) {
        offset = -ptr_size * (2 + argc + minim_fixnum(minim_cadr(loc)));
        return Mlist3(mem_sym, x86_fp, Mfixnum(offset));
    } else if (arg_formp(loc)) {
        offset = -ptr_size * (2 + minim_fixnum(minim_cadr(loc)));
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
    argc = list_length(fstate_args(fstate));

    // need stack alignment
    localc = minim_fixnum(rstate_fsize(rstate));
    if ((localc + argc) % 2) localc++;

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
        } else if (op == x86_add || op == x86_sub || op == x86_cmp) {
            mobj dst, src;
            dst = minim_cadr(expr);
            if (frame_locp(dst))
                minim_cadr(expr) = replace_frame_loc(dst, argc, localc);

            src = minim_car(minim_cddr(expr));
            if (frame_locp(src))
                minim_car(minim_cddr(expr)) = replace_frame_loc(src, argc, localc);
        } else if (op == x86_jmp || op == x86_je || op == x86_jne || op == x86_ge || op == x86_call) {
            mobj tgt = minim_cadr(expr);
            if (frame_locp(tgt))
                minim_cadr(expr) = replace_frame_loc(tgt, argc, localc);
        } else if (op == label_sym) {
            // do nothing
        } else {
            error1("compile4_resolve_frame", "unknown sequence", expr);
        }
    );
}

static void compile4_proc(mobj cstate, mobj fstate) {
    mobj exprs, rstate, instrs, stash;

    // initialize register state and compute last uses
    rstate = init_rstate();
    exprs = fstate_asm(fstate);
    stash = minim_false;
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
                if (variant == reloc_sym ||
                    variant == code_sym ||
                    variant == arg_sym ||
                    variant == next_sym) {
                    if (tregp(dst)) {
                        add_instr(instrs, Mlist3(x86_mov, x86_cres, src));
                        dst = treg_assign_or_lookup(rstate, dst);
                        src = x86_cres;
                    }

                    add_instr(instrs, Mlist3(op, dst, src));
                } else if (variant == mem_sym) {
                    // (mov <dst> (mem+ <base> <offset>))
                    mobj base = minim_cadr(src);
                    if (tregp(base)) {
                        base = treg_lookup(rstate, base);
                        if (minim_consp(base)) {
                            // `base` must be a register
                            add_instr(instrs, Mlist3(x86_mov, x86_carg0, base));
                            base = x86_carg0;
                        }
                    }

                    src = Mlist3(mem_sym, base, minim_car(minim_cddr(src)));
                    if (tregp(dst)) {
                        add_instr(instrs, Mlist3(x86_mov, x86_cres, src));
                        dst = treg_assign_or_lookup(rstate, dst);
                        src = x86_cres;
                    }

                    add_instr(instrs, Mlist3(op, dst, src));
                } else {
                    error1("compile4_proc", "unknown mov sequence", expr);
                }
            } else if (minim_symbolp(src)) {
                if (tregp(src)) {
                    src = treg_lookup(rstate, src);
                }

                if (minim_consp(dst) || tregp(dst)) {
                    // assigning to memory
                    if (tregp(dst))
                        dst = treg_assign_or_lookup(rstate, dst);

                    if (minim_consp(dst) && (minim_consp(src) || labelp(src))) {
                        // memory to memory requires a fix
                        add_instr(instrs, Mlist3(x86_mov, x86_cres, src));
                        src = x86_cres;
                    }
                }

                add_instr(instrs, Mlist3(op, dst, src));
            } else if (minim_fixnump(src)) {
                // leave as-is
                if (tregp(dst)) {
                    dst = treg_assign_or_lookup(rstate, dst);
                }

                add_instr(instrs, Mlist3(op, dst, src));
            } else {
                error1("compile4_proc", "unknown mov sequence", expr);
            }
        } else if (op == x86_add || op == x86_sub) {
            mobj dst = minim_cadr(expr);
            mobj src = minim_car(minim_cddr(expr));

            if (tregp(src)) {
                src = treg_lookup(rstate, src);
            }

            if (tregp(dst)) {
                if (local_formp(src)) {
                    add_instr(instrs, Mlist3(x86_mov, x86_cres, src));
                    src = x86_cres;
                }
                
                dst = treg_assign_or_lookup(rstate, dst);
            }

            add_instr(instrs, Mlist3(op, dst, src));
        } else if (op == x86_cmp) {
            // (cmp <src1> <src2>)
            mobj src1 = minim_cadr(expr);
            mobj src2 = minim_car(minim_cddr(expr));

            if (tregp(src1))
                src1 = treg_lookup(rstate, src1);
            if (tregp(src2))
                src2 = treg_lookup(rstate, src2);

            // at least one must be a register
            if (minim_consp(src1) && minim_consp(src2)) {
                add_instr(instrs, Mlist3(x86_mov, x86_cres, src1));
                src1 = x86_cres;
            }

            add_instr(instrs, Mlist3(op, src1, src2));
        } else if (op == x86_jmp || op == x86_je || op == x86_jne || op == x86_ge) {
            // (jmp <tgt>)
            // prefer to call from register
            mobj tgt = minim_cadr(expr);
            if (tregp(tgt))
                tgt = treg_lookup(rstate, tgt);

            // add call
            add_instr(instrs, Mlist2(op, tgt));
        } else if (op == x86_call) {
            if (minim_cadr(expr) != x86_cres)
                unimplemented_error("cannot call from any other register than %rax");
            add_instr(instrs, expr);
        } else if (op == stash_sym) {
            mobj loc;

            if (!minim_falsep(stash))
                error1("compile4_proc", "stash already occupired", stash);
            
            // stash %env
            loc = unnamed_assign_local(rstate);
            add_instr(instrs, Mlist3(x86_mov, loc, x86_env));
            stash = Mcons(x86_env, loc);
        } else if (op == unstash_sym) {
            if (minim_falsep(stash))
                error1("compile4_proc", "cannot unstash empty stash", stash);
            
            // unassign %env
            unnamed_unassign(rstate, minim_cdr(stash));
            add_instr(instrs, Mlist3(x86_mov, x86_env, minim_cdr(stash)));
            stash = minim_false;
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
    mfixnum i = (1 << 6) | ((w != 0) << 3) | ((r != 0) << 2) | ((x != 0) << 1) | (b != 0);
    return Mfixnum(i & 0xFF);
}

static mobj x86_encode_reg(mobj reg) {
    if (reg == x86_cres) return Mfixnum(0);
    else if (reg == x86_carg0) return Mfixnum(7);
    else if (reg == x86_carg1) return Mfixnum(6);
    else if (reg == x86_carg2) return Mfixnum(2);
    else if (reg == x86_carg3) return Mfixnum(1);
    else if (reg == x86_env) return Mfixnum(1);
    else if (reg == x86_sp) return Mfixnum(4);
    else if (reg == x86_fp) return Mfixnum(5);
    else if (reg == x86_argc) return Mfixnum(6);
    else error1("x86_encode_reg", "unsupported", reg);
}

static int x86_encode_x(mobj reg) {
    if (reg == x86_cres) return 0;
    else if (reg == x86_carg0) return 0;
    else if (reg == x86_carg1) return 0;
    else if (reg == x86_carg2) return 0;
    else if (reg == x86_carg3) return 0;
    else if (reg == x86_env) return 1;
    else if (reg == x86_fp) return 0;
    else if (reg == x86_sp) return 0;
    else if (reg == x86_argc) return 1;
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
    if (minim_fixnum(imm) < -2147483648 || minim_fixnum(imm) > 2147483647) {
        error1("x86_encode_imm32", "value cannot fit in 32 bits", imm);
    } else if (minim_fixnum(imm) < 0) {
        imm = Mfixnum(minim_fixnum(imm) + 0x100000000);
    }

    for (size_t i = 0; i < 4; i++) {
        bytes = Mcons(fix_rem(imm, quot), bytes);
        imm = fix_div(imm, quot);
    }

    return list_reverse(bytes);
}

static mobj x86_encode_imm8(mobj imm) {
    if (minim_fixnum(imm) < -128 || minim_fixnum(imm) > 127) {
        error1("x86_encode_imm32", "value cannot fit in 8 bits", imm);
    } else if (minim_fixnum(imm) < 0) {
        return Mfixnum(minim_fixnum(imm) + 256);
    } else {
        return imm;
    }
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

static mobj x86_encode_mov_imm(mobj dst, mobj src) {
    mobj rex = x86_encode_rex(1, 0, 0, x86_encode_x(dst));
    mobj modrm = x86_encode_modrm(Mfixnum(3), Mfixnum(0), x86_encode_reg(dst));
    return Mcons(rex, Mcons(Mfixnum(0xC7), Mcons(modrm, x86_encode_imm32(src))));
    
}

static mobj x86_encode_mov_imm64(mobj dst, mobj imm) {
    mobj rex = x86_encode_rex(1, 0, 0, x86_encode_x(dst));
    mobj op = x86_encode_io(Mfixnum(0xB8), x86_encode_reg(dst));
    return Mcons(rex, Mcons(op, x86_encode_imm64(imm)));
}

static mobj x86_encode_pc_rel(mobj dst, mobj src, mobj lstate, mobj where) {
    mobj imm = x86_encode_imm32(Mfixnum(0));
    mobj rex = x86_encode_rex(1, x86_encode_x(dst), 0, 0);
    mobj op = fix_add(fix_mul(x86_encode_reg(dst), Mfixnum(8)), Mfixnum(5));
    mobj instr = Mcons(rex, Mcons(Mfixnum(0x8D), Mcons(op, imm)));
    lstate_add_offset(lstate, Mcons(src, where), imm);
    return instr;
}

static mobj x86_encode_load(mobj dst, mobj src, mobj off) {
    mobj rex = x86_encode_rex(1, x86_encode_x(dst), 0, x86_encode_x(src));
    mobj modrm = x86_encode_modrm(Mfixnum(1), x86_encode_reg(dst), x86_encode_reg(src));
    if (minim_fixnum(off) >= -128 && minim_fixnum(off) <= 127) {
        return Mlist4(rex, Mfixnum(0x8B), modrm, x86_encode_imm8(off));
    } else {
        error1("x86_encode_load", "unsupported mov offset", off);
    }
}

static mobj x86_encode_store(mobj dst, mobj src, mobj off) {
    mobj rex = x86_encode_rex(1, x86_encode_x(src), 0, x86_encode_x(dst));
    mobj modrm = x86_encode_modrm(Mfixnum(1), x86_encode_reg(src), x86_encode_reg(dst));
    if (minim_fixnum(off) >= -128 && minim_fixnum(off) <= 127) {
        return Mlist4(rex, Mfixnum(0x89), modrm, x86_encode_imm8(off));
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
    if (minim_fixnum(imm) >= -128 && minim_fixnum(imm) <= 127) {
        return Mlist4(rex, Mfixnum(0x83), op, x86_encode_imm8(imm));
    } else {
        error1("compile5_proc", "unsupported add offset", imm);
    }
}

static mobj x86_encode_sub(mobj dst, mobj src) {
    mobj rex = x86_encode_rex(1, x86_encode_x(src), 0, x86_encode_x(dst));
    mobj modrm = x86_encode_modrm(Mfixnum(3), x86_encode_reg(src), x86_encode_reg(dst));
    return Mlist3(rex, Mfixnum(0x29), modrm);
}

static mobj x86_encode_sub_mem(mobj base, mobj offset, mobj src) {
    mobj rex = x86_encode_rex(1, x86_encode_x(src), 0, x86_encode_x(base));
    mobj modrm = x86_encode_modrm(Mfixnum(1), x86_encode_reg(src), x86_encode_reg(base));
    if (minim_fixnum(offset) >= -128 && minim_fixnum(offset) <= 127) {
        return Mlist4(rex, Mfixnum(0x29), modrm, x86_encode_imm8(offset));
    } else {
        error1("compile5_proc", "unsupported sub offset", offset);
    }
}

static mobj x86_encode_sub_imm(mobj dst, mobj imm) {
    mobj op = x86_encode_io(Mfixnum(0xE8), x86_encode_reg(dst));
    mobj rex = x86_encode_rex(1, 0, 0, x86_encode_x(dst));
    if (minim_fixnum(imm) >= -128 && minim_fixnum(imm) <= 127) {
        return Mlist4(rex, Mfixnum(0x83), op, x86_encode_imm8(imm));
    } else {
        error1("compile5_proc", "unsupported sub imm", imm);
    }
}

static mobj x86_encode_sub_mem_imm(mobj dst, mobj offset, mobj imm) {
    mobj op = x86_encode_io(Mfixnum(0x68), x86_encode_reg(dst));
    mobj rex = x86_encode_rex(1, 0, 0, x86_encode_x(dst));
    if (minim_fixnum(imm) >= -128 && minim_fixnum(imm) <= 127) {
        if (minim_fixnum(offset) >= -128 && minim_fixnum(offset) <= 127) {
            return Mlist5(rex, Mfixnum(0x83), op, x86_encode_imm8(offset), x86_encode_imm8(imm));
        } else {
            error1("compile5_proc", "unsupported sub offset", offset);
        }
    } else {
        error1("compile5_proc", "unsupported sub imm", imm);
    }
}

static mobj x86_encode_cmp(mobj src1, mobj src2) {
    mobj rex = x86_encode_rex(1, x86_encode_x(src2), 0, x86_encode_x(src1));
    mobj modrm = x86_encode_modrm(Mfixnum(3), x86_encode_reg(src2), x86_encode_reg(src1));
    return Mlist3(rex, Mfixnum(0x39), modrm);
}

static mobj x86_encode_cmp_imm(mobj src, mobj imm) {
    mobj rex = x86_encode_rex(1, 0, 0, x86_encode_x(src));
    mobj op = x86_encode_io(Mfixnum(0xF8), x86_encode_reg(src));
    if (minim_fixnum(imm) >= -128 && minim_fixnum(imm) <= 127) {
        return Mlist4(rex, Mfixnum(0x83), op, x86_encode_imm8(imm));
    } else {
        error1("compile5_proc", "unsupported sub offset", imm);
    }
}

static mobj x86_encode_cmp_rel(mobj src1, mobj src2, mobj imm) {
    mobj rex = x86_encode_rex(1, x86_encode_x(src1), 0, x86_encode_x(src2));
    mobj modrm = x86_encode_modrm(Mfixnum(1), x86_encode_reg(src1), x86_encode_reg(src2));
    if (minim_fixnum(imm) >= -128 && minim_fixnum(imm) <= 127) {
        return Mlist4(rex, Mfixnum(0x3B), modrm, x86_encode_imm8(imm));
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

static mobj x86_encode_jmp_mem(mobj base, mobj offset) {
    mobj instr, modrm, rex;
    modrm = x86_encode_modrm(Mfixnum(2), Mfixnum(4), x86_encode_reg(base));
    instr = Mcons(Mfixnum(0xFF), Mcons(modrm, x86_encode_imm32(offset)));
    if (x86_encode_x(base)) {
        rex = x86_encode_rex(0, 0, 0, 1);
        instr = Mcons(rex, instr);
    }

    return instr;
}

static mobj x86_encode_jmp_rel(mobj lstate, mobj src, mobj where) {
    mobj instr, imm;
    imm = x86_encode_imm32(Mfixnum(0));
    instr = Mcons(Mfixnum(0xE9), imm);
    lstate_add_offset(lstate, Mcons(where, src), imm);
    return instr;
}

static mobj x86_encode_je(mobj lstate, mobj src, mobj where) {
    mobj instr, imm;
    imm = x86_encode_imm32(Mfixnum(0));
    instr = Mcons(Mfixnum(0x0F), Mcons(Mfixnum(0x84), imm));
    lstate_add_offset(lstate, Mcons(where, src), imm);
    return instr;
}

static mobj x86_encode_jne(mobj lstate, mobj src, mobj where) {
    mobj instr, imm;
    imm = x86_encode_imm32(Mfixnum(0));
    instr = Mcons(Mfixnum(0x0F), Mcons(Mfixnum(0x85), imm));
    lstate_add_offset(lstate, Mcons(where, src), imm);
    return instr;
}

static mobj x86_encode_jge(mobj lstate, mobj src, mobj where) {
    mobj instr, imm;
    imm = x86_encode_imm32(Mfixnum(0));
    instr = Mcons(Mfixnum(0x0F), Mcons(Mfixnum(0x8D), imm));
    lstate_add_offset(lstate, Mcons(where, src), imm);
    return instr;
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
    size_t len, offset;

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
                    // => (lea <dst> rip <offset>)
                    add_instr(instrs, x86_encode_pc_rel(dst, src, lstate, expr));
                    offset = instr_bytes(instrs) + minim_fixnum(lstate_pos(lstate));
                    lstate_add_local(lstate, expr, Mfixnum(offset));
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
                if (minim_consp(dst))
                    error1("compile5_proc", "too many memory locations", expr);

                add_instr(instrs, x86_encode_pc_rel(dst, src, lstate, expr));
                offset = instr_bytes(instrs) + minim_fixnum(lstate_pos(lstate));
                lstate_add_local(lstate, expr, Mfixnum(offset));
            } else if (minim_consp(dst)) {
                // (mov (mem+ <base> <offset>) <src>)
                // => (mov [<base>+<offset>] <src>)
                mobj base = minim_cadr(dst);
                mobj offset = minim_car(minim_cddr(dst));
                add_instr(instrs, x86_encode_store(base, src, offset));
            } else if (minim_fixnump(src)) {
                // (mov <dst> <imm>)
                if (minim_consp(src))
                    error1("compile5_proc", "move imm to mem unsupported", expr);
                add_instr(instrs, x86_encode_mov_imm(dst, src));
            } else {
                // (mov <dst> <src>)
                add_instr(instrs, x86_encode_mov(dst, src));
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
            if (minim_consp(dst)) {
                mobj base = minim_cadr(dst);
                mobj offset = minim_car(minim_cddr(dst));
                if (minim_symbolp(src)) {
                    add_instr(instrs, x86_encode_sub_mem(base, offset, src));
                } else if (minim_fixnump(src)) {
                    add_instr(instrs, x86_encode_sub_mem_imm(base, offset, src));
                } else {
                    error1("compile5_proc", "unknown sub form", expr);
                }
            } else {
                if (minim_symbolp(src)) {
                    add_instr(instrs, x86_encode_sub(dst, src));
                } else if (minim_fixnump(src)) {
                    add_instr(instrs, x86_encode_sub_imm(dst, src));
                } else {
                    error1("compile5_proc", "unknown sub form", expr);
                }
            }
        } else if (op == x86_cmp) {
            // cmp <src1> <src2>
            mobj src1 = minim_cadr(expr);
            mobj src2 = minim_car(minim_cddr(expr));
            if (minim_symbolp(src1) && minim_symbolp(src2)) {
                add_instr(instrs, x86_encode_cmp(src1, src2));
            } else if (minim_symbolp(src1) && mem_formp(src2)) {
                mobj base = minim_cadr(src2);
                mobj offset = minim_car(minim_cddr(src2));
                add_instr(instrs, x86_encode_cmp_rel(src1, base, offset));
            } else if (minim_symbolp(src1) && minim_fixnump(src2)) {
                add_instr(instrs, x86_encode_cmp_imm(src1, src2));
            } else {
                error1("compile5_proc", "unknown cmp form", expr);
            }
        } else if (op == x86_call) {
            // call <tgt>
            add_instr(instrs, x86_encode_call(minim_cadr(expr)));
        } else if (op == x86_jmp) {
            // jmp <tgt>
            mobj tgt = minim_cadr(expr);
            if (labelp(tgt)) {
                add_instr(instrs, x86_encode_jmp_rel(lstate, expr, tgt));
                offset = instr_bytes(instrs) + minim_fixnum(lstate_pos(lstate));
                lstate_add_local(lstate, expr, Mfixnum(offset));
            } else if (minim_symbolp(tgt)) {
                add_instr(instrs, x86_encode_jmp(tgt));
            } else {
                add_instr(instrs, x86_encode_jmp_mem(minim_cadr(tgt), minim_car(minim_cddr(tgt))));
            }
        } else if (op == x86_je) {
            // jne <tgt>
            mobj tgt = minim_cadr(expr);
            if (labelp(tgt)) {
                add_instr(instrs, x86_encode_je(lstate, expr, tgt));
                offset = instr_bytes(instrs) + minim_fixnum(lstate_pos(lstate));
                lstate_add_local(lstate, expr, Mfixnum(offset));
            } else {
                error1("compile5_proc", "unknown jne form", expr);
            }
        } else if (op == x86_jne) {
            // jne <tgt>
            mobj tgt = minim_cadr(expr);
            if (labelp(tgt)) {
                add_instr(instrs, x86_encode_jne(lstate, expr, tgt));
                offset = instr_bytes(instrs) + minim_fixnum(lstate_pos(lstate));
                lstate_add_local(lstate, expr, Mfixnum(offset));
            } else {
                error1("compile5_proc", "unknown jne form", expr);
            }
        } else if (op == x86_ge) {
            // jge <tgt>
            mobj tgt = minim_cadr(expr);
            if (labelp(tgt)) {
                add_instr(instrs, x86_encode_jge(lstate, expr, tgt));
                offset = instr_bytes(instrs) + minim_fixnum(lstate_pos(lstate));
                lstate_add_local(lstate, expr, Mfixnum(offset));
            } else {
                error1("compile5_proc", "unknown jne form", expr);
            }
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

static void compile5_module(mobj cstate, mobj lstate) {
    mobj procs;
    size_t i;

    // resolve relocation table
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

    // procs = cstate_procs(cstate);
    // for_each(procs,
    //     write_object(Mport(stdout, 0x0), fstate_asm(minim_car(procs)));
    //     fprintf(stdout, "\n");
    // );
}

static void write_module(mobj cstate, void *page) {
    mobj procs, instrs, instr, byte;
    unsigned char *ptr = page;
    
    procs = cstate_procs(cstate);
    for_each(procs,
        instrs = fstate_asm(minim_car(procs));
        for_each(instrs,
            instr = minim_car(instrs);
            for_each(instr,
                byte = minim_car(instr);
                if (minim_fixnum(byte) < 0 || minim_fixnum(byte) >= 256)
                    error1("write_module", "invalid byte", byte);
                *ptr = minim_fixnum(byte);
                ++ptr;
            );
        );
    );
}

//
//  Public API
//

mobj compile_module(mobj name, mobj es) {
    mobj cstate, procs;

    // check for initialization
    if (!cstate_init) {
        init_compile_globals();
        init_x86_compiler_globals();
    }

    // phase 1: flattening pass
    cstate = init_cstate(name);
    compile1_module_loader(cstate, es);

    // write_object(Mport(stdout, 0x0), cstate);
    // printf("\n");

    // phase 2: pseudo assembly pass
    procs = cstate_procs(cstate);
    for_each(procs, compile2_proc(cstate, minim_car(procs)));

    // write_object(Mport(stdout, 0x0), cstate);
    // printf("\n");

    // phase 3: architecture-specific assembly
    procs = cstate_procs(cstate);
    for_each(procs, compile3_proc(cstate, minim_car(procs)));

    // write_object(Mport(stdout, 0x0), cstate);
    // printf("\n");

    // phase 4: register allocation
    procs = cstate_procs(cstate);
    for_each(procs, compile4_proc(cstate, minim_car(procs)));

    // write_object(Mport(stdout, 0x0), cstate);
    // printf("\n");

    return cstate;
}

void *install_module(mobj cstate) {
    mobj lstate;
    void *page;
    size_t size;

    // check for initialization
    if (!cstate_init) {
        init_compile_globals();
        init_x86_compiler_globals();
    }

    // initialize linker
    lstate = init_lstate();

    // phase 5: translation to binary
    compile5_module(cstate, lstate);

    // write page
    size = minim_fixnum(lstate_pos(lstate));
    page = alloc_page(size);
    if (page == NULL)
        error("install_module", "could not allocate page");

    write_module(cstate, page);
    if (make_page_executable(page, size) < 0)
        error("install_module", "could not make page executable");

    return page;
}
