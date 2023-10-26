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
            minim_fixnum(e) ||
            minim_charp(e) ||
            minim_stringp(e);
}

//
//  Compiler state
//

int cstate_init = 0;
mobj tloc_sym;
mobj imm_sym;
mobj literal_sym;
mobj lookup_sym;
mobj apply_sym;
mobj bindb_sym;
mobj push_valueb_sym;

#define cstate_length            5
#define cstate_name_idx          0
#define cstate_loader_idx        1
#define cstate_procs_idx         2
#define cstate_literals_idx      3
#define cstate_gensym_idx        4

static mobj init_cstate(mobj name) {
    mobj cstate = Mvector(cstate_length, NULL);
    minim_vector_ref(cstate, cstate_name_idx) = name;
    minim_vector_ref(cstate, cstate_loader_idx) = minim_false;
    minim_vector_ref(cstate, cstate_procs_idx) = minim_null;
    minim_vector_ref(cstate, cstate_literals_idx) = minim_null;
    minim_vector_ref(cstate, cstate_gensym_idx) = Mfixnum(0);
    return cstate;
}

static void cstate_add_loader(mobj cstate, mobj fstate) {
    minim_vector_ref(cstate, cstate_loader_idx) = fstate;
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

static size_t cstate_add_literal(mobj cstate, mobj lit) {
    mobj lits;
    size_t i;
    
    lits = minim_vector_ref(cstate, cstate_literals_idx);
    if (minim_nullp(lits)) {
        minim_vector_ref(cstate, cstate_literals_idx) = Mlist1(lit);
        return 0;
    } else {
        i = 0;
        for (; !minim_nullp(minim_cdr(lits)); lits = minim_cdr(lits), i++) {
            if (minim_equalp(minim_car(lits), lit)) {
                return i;
            }
        }
        
        minim_cdr(lits) = Mcons(lit, minim_null);
        return i + 1;
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

#define fstate_length           4
#define fstate_name_idx         0
#define fstate_arity_idx        1
#define fstate_rest_idx         2
#define fstate_asm_idx          3

static mobj init_fstate(mobj name) {
    mobj fstate = Mvector(fstate_length, NULL);
    minim_vector_ref(fstate, fstate_name_idx) = name;
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
//  Compilation
//

static void unimplemented_error(const char *what) {
    fprintf(stderr, "unimplemented: %s\n", what);
    fatal_exit();
}

// Compiles expression-level syntax for a loader.
static mobj compile_expr(mobj cstate, mobj fstate, mobj e) {
    mobj op, args, instr, loc;
    size_t idx;

    if (minim_consp(e)) {
        // application
        args = minim_null;
        op = compile_expr(cstate, fstate, minim_car(e));
        for (e = minim_cdr(e); !minim_nullp(e); e = minim_cdr(e))
            args = Mcons(compile_expr(cstate, fstate, minim_car(e)), args);
    
        instr = Mcons(apply_sym, Mcons(op, list_reverse(args)));
        loc = cstate_gensym(cstate, tloc_sym);
        fstate_add_asm(fstate, Mlist3(setb_sym, loc, instr));
        return loc;
    } else if (minim_nullp(e)) {
        // illegal
        error1("compile_expr", "empty application", e);
    } else if (minim_symbolp(e)) {
        // identifier
        loc = cstate_gensym(cstate, tloc_sym);
        fstate_add_asm(fstate, Mlist3(setb_sym, loc, Mlist2(lookup_sym, e)));
        return loc;
    } else if (literalp(e)) {
        // literals
        loc = cstate_gensym(cstate, tloc_sym);
        if (immediatep(e)) {
            fstate_add_asm(fstate, Mlist3(setb_sym, loc, Mlist2(imm_sym, e)));
        } else {
            size_t idx = cstate_add_literal(cstate, e);
            fstate_add_asm(fstate, Mlist3(setb_sym, loc, Mlist2(literal_sym, e)));
        }
        
        return loc;
    } else {
        error1("compile_expr", "attempting to compile garbage", e);
    }
}

// Compiles module-level syntax for a loader.
static void compile_module_level(mobj cstate, mobj e) {
    mobj ids, loc, lit, lstate;
    size_t vc;

    lstate = minim_vector_ref(cstate, cstate_loader_idx);
    write_object(Mport(stdout, PORT_FLAG_OPEN), e);
    fputs("\n", stdout);

loop:
    if (define_values_formp(e)) {
        // define-values form
        ids = minim_cadr(e);
        vc = list_length(ids);
        loc = compile_expr(cstate, lstate, minim_car(minim_cddr(e)));
        if (vc > 1) {
            unimplemented_error("multiple values for define-values");
        } else if (vc == 1) {
            // bind the evaluation
            lit = cstate_gensym(cstate, tloc_sym);
            fstate_add_asm(lstate, Mlist3(setb_sym, lit, Mlist2(literal_sym, minim_car(ids))));
            fstate_add_asm(lstate, Mlist3(bindb_sym, loc, lit));
        } // else (vc == 0) {
        // evaluate without binding, as in, do nothing
        // }
    } else if (begin_formp(e)) {
        // begin form
        e = minim_cdr(e);
        if (minim_nullp(e)) {
            // empty => void
            loc = cstate_gensym(cstate, tloc_sym);
            fstate_add_asm(lstate, Mlist3(setb_sym, loc, Mlist2(imm_sym, minim_void)));
            fstate_add_asm(lstate, Mlist2(intern("push-value!"), loc));
        } else {
            // execute statements and return the last one
            for (; !minim_nullp(minim_cdr(e)); e = minim_cdr(e))
                compile_expr(cstate, lstate, minim_car(e));
            e = minim_car(e);
            goto loop;
        }
    } else {
        // expression
        loc = compile_expr(cstate, lstate, e);
        fstate_add_asm(lstate, Mlist2(intern("push-value!"), loc));
    }
}

// Module loaders are the bodies of modules as a nullary functions.
static void compile_module_loader(mobj cstate, mobj es) {
    mobj lstate;

    // initialize the loader
    lstate = init_fstate(minim_false);
    // cstate_add_proc(cstate, lstate);
    cstate_add_loader(cstate, lstate);

    // compiles all expressions
    for (; !minim_nullp(es); es = minim_cdr(es))
        compile_module_level(cstate, minim_car(es));
}

static void init_compile_globals() {
    tloc_sym = intern("t");
    imm_sym = intern("imm");
    literal_sym = intern("literal");
    lookup_sym = intern("lookup");
    apply_sym = intern("apply");
    bindb_sym = intern("bind!");
    push_valueb_sym = intern("push-value!");
    cstate_init = 1;
}

//
//  Public API
//

#include <stdint.h>

void compile_module(mobj op, mobj name, mobj es) {
    mobj cstate;

    // check for initialization
    if (!cstate_init) {
        init_compile_globals();
    }    

    // logging
    fputs("[compiling ", stdout);
    write_object(Mport(stdout, PORT_FLAG_OPEN), name);
    fputs("]\n", stdout);
    fflush(stdout);

    cstate = init_cstate(name);
    compile_module_loader(cstate, es);

    //logging
    write_object(Mport(stdout, PORT_FLAG_OPEN), cstate);
    fputs("\n", stdout);
    fflush(stdout);
}
