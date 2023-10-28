// compile.c: compiler for bootstrapping

#include "minim.h"

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

static void unimplemented_error(const char *what) {
    fprintf(stderr, "unimplemented: %s\n", what);
    fatal_exit();
}

//
//  Compiler state
//

int cstate_init = 0;

mobj tloc_pre;
mobj label_pre;

mobj imm_sym;
mobj literal_sym;
mobj closure_sym;
mobj lookup_sym;
mobj apply_sym;
mobj cmp_sym;
mobj push_frame_sym;
mobj pop_frame_sym;
mobj bindb_sym;
mobj push_valueb_sym;
mobj label_sym;
mobj branch_sym;

static void init_compile_globals() {
    tloc_pre = intern("$t");
    label_pre = intern("$L");
    imm_sym = intern("$imm");
    literal_sym = intern("$literal");
    closure_sym = intern("$closure");
    label_sym = intern("$label");

    lookup_sym = intern("lookup");
    apply_sym = intern("apply");
    cmp_sym = intern("sym");
    push_frame_sym = intern("push-frame!");
    pop_frame_sym = intern("pop-frame!");
    bindb_sym = intern("bind!");
    push_valueb_sym = intern("push-value!");
    branch_sym = intern("branch");
    cstate_init = 1;
}

#define branch_uncond           0
#define branch_neq              1

#define frame_let               0
#define frame_letrec            1

#define cstate_length           5
#define cstate_name_idx         0
#define cstate_loader_idx       1
#define cstate_procs_idx        2
#define cstate_literals_idx     3
#define cstate_gensym_idx       4

static mobj init_cstate(mobj name) {
    mobj cstate = Mvector(cstate_length, NULL);
    minim_vector_ref(cstate, cstate_name_idx) = name;
    minim_vector_ref(cstate, cstate_loader_idx) = minim_false;
    minim_vector_ref(cstate, cstate_procs_idx) = minim_null;
    minim_vector_ref(cstate, cstate_literals_idx) = minim_null;
    minim_vector_ref(cstate, cstate_gensym_idx) = Mfixnum(0);
    return cstate;
}

static void cstate_add_loader(mobj cstate, mobj idx) {
    minim_vector_ref(cstate, cstate_loader_idx) = idx;
}

static size_t cstate_add_proc(mobj cstate, mobj fstate) {
    mobj procs;
    size_t i;
    
    procs = minim_vector_ref(cstate, cstate_procs_idx);
    if (minim_nullp(procs)) {
        minim_vector_ref(cstate, cstate_procs_idx) = Mlist1(fstate);
        return 0;
    } else {
        for (i = 0; !minim_nullp(minim_cdr(procs)); procs = minim_cdr(procs), i++);
        minim_cdr(procs) = Mcons(fstate, minim_null);
        return i + 1;
    }
}

static mobj cstate_add_literal(mobj cstate, mobj lit) {
    mobj lits;
    size_t i;
    
    lits = minim_vector_ref(cstate, cstate_literals_idx);
    if (minim_nullp(lits)) {
        minim_vector_ref(cstate, cstate_literals_idx) = Mlist1(lit);
        return Mfixnum(0);
    } else {
        i = 0;
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
    size_t i = minim_fixnum(minim_vector_ref(cstate, cstate_gensym_idx));
    mobj prefix = symbol_to_string(name);
    mobj index = fixnum_to_string(Mfixnum(i));
    minim_vector_ref(cstate, cstate_gensym_idx) = Mfixnum(i + 1);
    return string_to_symbol(string_append(prefix, index));
}

//
//  Function state
//

#define fstate_length           3
#define fstate_arity_idx        0
#define fstate_rest_idx         1
#define fstate_asm_idx          2

static mobj init_fstate() {
    mobj fstate = Mvector(fstate_length, NULL);
    minim_vector_ref(fstate, fstate_arity_idx) = Mfixnum(0);
    minim_vector_ref(fstate, fstate_rest_idx) = minim_false;
    minim_vector_ref(fstate, fstate_asm_idx) = minim_null;
    return fstate;
}

static void fstate_add_asm(mobj fstate, mobj s) {
    mobj stmts = minim_vector_ref(fstate, fstate_asm_idx);
    if (minim_nullp(stmts)) {
        minim_vector_ref(fstate, fstate_asm_idx) = Mlist1(s);
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
        minim_vector_ref(f2state, fstate_arity_idx) = minim_car(arity);
        minim_vector_ref(f2state, fstate_rest_idx) = minim_cdr(arity);

        e = minim_cddr(e);
        e = minim_nullp(minim_cdr(e)) ? minim_car(e) : Mcons(begin_sym, e);
        code = compile1_expr(cstate, f2state, e);

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
        mobj idx = cstate_add_literal(cstate, minim_cadr(e));
        loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, Mlist3(setb_sym, loc, Mlist2(literal_sym, idx)));
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
        loc = cstate_gensym(cstate, tloc_pre);
        fstate_add_asm(fstate, Mlist3(setb_sym, loc, Mlist2(lookup_sym, e)));
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

    loffset = minim_vector_ref(cstate, cstate_loader_idx);
    lstate = list_ref(minim_vector_ref(cstate, cstate_procs_idx), loffset);

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

static mobj compile2_proc(mobj cstate, mobj fstate) {

}

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

    // phase 3: architecture-specific assembly

    // phase 4: register allocation

    // phase 5: write to bitstring
}
