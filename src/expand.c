// expand.c: expander and syntax checker for bootstrapping

#include "minim.h"

//
//  Checker
//

// Checks for a valid formal signature
// `formals := ()
//          := <id>
//          := (<id> . <formals>)`
static void check_formals(mobj form) {
    mobj *i = form;

loop:
    if (minim_consp(i)) {
        if (!minim_symbolp(minim_car(i)))
            error1("check_define_form()", "ill-formed formal parameters", form);
        
        i = minim_cdr(i);
        goto loop;
    } else if (!minim_nullp(i) && !minim_symbolp(i)) {
        error1("check_define_form()", "ill-formed formal parameters", form);
    }
}

// Assuming `e := (_ ...)`
// Expecting `e := (begin <e1> <es> ...)`
static void check_begin_form(mobj e) {

}

// Assuming `e := (_ ...)`.
// Expecting `e := (define <id> <e>)
//              := (define (id <formals> ...) <e>)
static void check_define_form(mobj e) {
    mobj rib, id;

    rib = minim_cdr(e);
    if (!minim_consp(e)) {
        error1("check_define_form()", "ill-formed define form", e);
    }

    id = minim_car(rib);
    if (minim_consp(id)) {
        // procedure syntax
        if (!minim_symbolp(minim_car(id)))
            error1("check_define_form()", "missing identifier", e);   
        check_formals(minim_cdr(id));
    } else if (!minim_symbolp(id)) {
        error1("check_define_form()", "missing identifier", e);   
    }

    rib = minim_cdr(rib);
    if (!minim_consp(rib)) {
        error1("check_define_form()", "missing body", e);
    }

    rib = minim_cdr(rib);
    if (!minim_nullp(rib)) {
        error1("check_define_form()", "multiple bodies", e);
    }
}

//
//  Expander
//

// Partial expansion of `define` form into a `define-values` form.
// Does not expand the body.
static mobj expand_define_form(mobj e) {
    mobj rib, id, body, formals;

    rib = minim_cdr(e);
    id = minim_car(rib);
    body = minim_cadr(rib);
    if (minim_consp(id)) {
        // procedure:
        // `(define (<id> . <formals>) <body>)
        //  => (define <id> (lambda <formals> <body>))`
        formals = minim_cdr(id);
        id = minim_car(id);
        body = Mlist3(lambda_sym, formals, body);
    }

    // `(define <id> <e>) => (define-values (<id>) <e>)`
    return Mlist3(define_values_sym, Mlist1(id), body);
}

//
//  Public API
//

mobj expand_expr(mobj e) {
    return e;
}

mobj expand_top(mobj e) {
    mobj head;

    if (!minim_consp(e)) {
        return e;
    }

loop:

    head = minim_car(e);
    if (head == define_sym) {
        // define form => expansion
        check_define_form(e);
        e = expand_define_form(e);
        goto loop;
    } else if (head == begin_sym) {
        // begin form => recurse (possibly splicing)
    }

    return e;
}
