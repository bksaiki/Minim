/*
    Syntax
*/

#include "../minim.h"

minim_object *make_syntax(minim_object *e, minim_object *loc) {
    minim_syntax_object *o = GC_alloc(sizeof(minim_syntax_object));
    o->type = MINIM_SYNTAX_TYPE;
    o->e = e;
    o->loc = loc;
    return ((minim_object *) o);
}

// Recursively converts an object to syntax
minim_object *to_syntax(minim_object *o) {
    minim_object *it;

    switch (o->type) {
    case MINIM_SYNTAX_TYPE:
        return o;
    case MINIM_NULL_TYPE:
    case MINIM_TRUE_TYPE:
    case MINIM_FALSE_TYPE:
    case MINIM_EOF_TYPE:
    case MINIM_VOID_TYPE:
    case MINIM_SYMBOL_TYPE:
    case MINIM_FIXNUM_TYPE:
    case MINIM_CHAR_TYPE:
    case MINIM_STRING_TYPE:
        return make_syntax(o, minim_false);
    
    case MINIM_PAIR_TYPE:
        it = o;
        do {
            minim_car(it) = to_syntax(minim_car(it));
            if (!minim_is_pair(minim_cdr(it))) {
                if (!minim_is_null(minim_cdr(it)))
                    minim_cdr(it) = to_syntax(minim_cdr(it));
                return make_syntax(o, minim_false);
            }
            it = minim_cdr(it);
        } while (1);
    default:
        fprintf(stderr, "datum->syntax: cannot convert to syntax");
        exit(1);
    }
}

minim_object *strip_syntax(minim_object *o) {
    switch (o->type) {
    case MINIM_SYNTAX_TYPE:
        return strip_syntax(minim_syntax_e(o));
    case MINIM_PAIR_TYPE:
        return make_pair(strip_syntax(minim_car(o)), strip_syntax(minim_cdr(o)));
    default:
        return o;
    }
}

//
//  Primitives
//

minim_object *is_syntax_proc(minim_object *args) {
    // (-> any boolean)
    return minim_is_syntax(minim_car(args)) ? minim_true : minim_false;
}

minim_object *syntax_error_proc(minim_object *args) {
    // (-> symbol string any)
    // (-> symbol string syntax any)
    // (-> symbol string syntax syntaxs any)
    minim_object *what, *why, *where, *sub;

    what = minim_car(args);
    why = minim_cadr(args);

    if (!minim_is_symbol(what))
        bad_type_exn("syntax-error", "symbol?", what);
    if (!minim_is_string(why))
        bad_type_exn("syntax-error", "string?", why);

    fprintf(stderr, "%s: %s\n", minim_symbol(what), minim_string(why));
    if (!minim_is_null(minim_cddr(args))) {
        where = minim_car(minim_cddr(args));
        if (!minim_is_syntax(where))
            bad_type_exn("syntax-error", "syntax?", where);

        if (!minim_is_null(minim_cdr(minim_cddr(args)))) {
            sub = minim_cadr(minim_cddr(args));
            if (!minim_is_syntax(sub))
                bad_type_exn("syntax-error", "syntax?", sub);

            fputs("  at: ", stderr);
            write_object2(stderr, sub, 1, 1);
            fprintf(stderr, "\n");
        }

        fputs("  in: ", stderr);
        write_object2(stderr, where, 1, 1);
        fprintf(stderr, "\n");
    }

    exit(1);
}

minim_object *syntax_e_proc(minim_object *args) {
    // (-> syntax any)
    if (!minim_is_syntax(minim_car(args)))
        bad_type_exn("syntax-e", "syntax?", minim_car(args));
    return minim_syntax_e(minim_car(args));
}

minim_object *syntax_loc_proc(minim_object *args) {
    // (-> syntax any)
    if (!minim_is_syntax(minim_car(args)))
        bad_type_exn("syntax-loc", "syntax?", minim_car(args));
    return minim_syntax_loc(minim_car(args));
}

minim_object *to_syntax_proc(minim_object *args) {
    // (-> any syntax)
    return to_syntax(minim_car(args));
}
