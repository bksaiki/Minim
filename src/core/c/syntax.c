/*
    Syntax
*/

#include "../minim.h"

mobj Msyntax(mobj e, mobj loc) {
    mobj o = GC_alloc(minim_syntax_size);
    minim_type(o) = MINIM_OBJ_SYNTAX;
    minim_syntax_e(o) = e;
    minim_syntax_loc(o) = loc;
    return o;
}

// Converts a (possibly improper) list to syntax
static mobj pair_to_syntax(mobj o) {
    mobj hd, tl, it;

    hd = tl = Mcons(to_syntax(minim_car(o)), NULL);
    for (it = minim_cdr(o); minim_consp(it); it = minim_cdr(it)) {
        minim_cdr(tl) = Mcons(to_syntax(minim_car(it)), NULL);
        tl = minim_cdr(tl);
    }

    if (minim_nullp(it)) {
        minim_cdr(tl) = minim_null;
    } else {
        minim_cdr(tl) = to_syntax(it);
    }

    return Msyntax(hd, minim_false);
}

// Converts a vector to syntax
static mobj vector_to_syntax(mobj o) {
    mobj v = Mvector(minim_vector_len(o), NULL);
    for (long i = 0; i < minim_vector_len(o); i++)
        minim_vector_ref(v, i) = to_syntax(minim_vector_ref(o, i));
    return Msyntax(v, minim_false);
}

// Recursively strips syntax from a vector
static mobj strip_vector(mobj o) {
    mobj v = Mvector(minim_vector_len(o), NULL);
    for (long i = 0; i < minim_vector_len(o); i++)
        minim_vector_ref(v, i) = strip_syntax(minim_vector_ref(o, i));
    return v;
}

// Removes one layer of syntax, if it exists
static mobj syntax_strip_once(mobj o) {
    return minim_syntaxp(o) ? minim_syntax_e(o) : o;
}

//
//  Primitives
//

mobj syntaxp_proc(mobj x) {
    // (-> any boolean)
    return minim_syntaxp(x) ? minim_true : minim_false;
}

mobj syntax_e_proc(mobj stx) {
    // (-> syntax? any)
    return minim_syntax_e(stx);
}

mobj syntax_loc_proc(mobj stx) {
    // (-> syntax? any)
    return minim_syntax_loc(stx);
}

// Recursively converts an object to syntax
mobj to_syntax(mobj o) {
    // (-> any syntax)
    switch (minim_type(o)) {
    case MINIM_OBJ_SYNTAX:
        return o;
    case MINIM_OBJ_PAIR:
        return pair_to_syntax(o);
    case MINIM_OBJ_VECTOR:
        return vector_to_syntax(o);
    case MINIM_OBJ_BOX:
        return Msyntax(Mbox(to_syntax(minim_unbox(o))), minim_false);
    default:
        return Msyntax(o, minim_false);
    }
}

// Recursively converts syntax to an object
mobj strip_syntax(mobj o) {
    // (-> syntax any)
    switch (minim_type(o)) {
    case MINIM_OBJ_SYNTAX:
        return strip_syntax(minim_syntax_e(o));
    case MINIM_OBJ_PAIR:
        return Mcons(strip_syntax(minim_car(o)), strip_syntax(minim_cdr(o)));
    case MINIM_OBJ_BOX:
        return Mbox(strip_syntax(minim_unbox(o)));
    case MINIM_OBJ_VECTOR:
        return strip_vector(o);
    default:
        return o;
    }
}

mobj syntax_to_list(mobj stx) {
    // (-> syntax (or list #f))
    mobj hd, tl, it;

    it = syntax_strip_once(stx);
    if (minim_consp(it)) {
        hd = tl = Mcons(minim_car(it), NULL);
        it = syntax_strip_once(minim_cdr(it));
        while (minim_consp(it)) {
            minim_cdr(tl) = Mcons(minim_car(it), NULL);
            it = syntax_strip_once(minim_cdr(it));
            tl = minim_cdr(tl);
        }

        if (minim_nullp(it)) {
            minim_cdr(tl) = minim_null;
            return hd;
        } else {
            return minim_false;
        }
    } else if (minim_nullp(it)) {
        return it;
    } else {
        return minim_false;
    }
}

mobj syntax_error_proc(int argc, mobj *args) {
    // (-> (or #f symbol) string any)
    // (-> (or #f symbol) string syntax any)
    // (-> (or #f symbol) string syntax syntaxs any)
    mobj what, why, where, sub;

    what = args[0];
    why = args[1];
    if (!minim_falsep(what) && !minim_symbolp(what))
        bad_type_exn("syntax-error", "symbol?", what);
    if (!minim_stringp(why))
        bad_type_exn("syntax-error", "string?", why);

    if (minim_falsep(what))
        fprintf(stderr, "error: %s\n", minim_string(why));
    else
        fprintf(stderr, "%s: %s\n", minim_symbol(what), minim_string(why));

    if (argc >= 3) {
        where = args[2];
        if (!minim_syntaxp(where))
            bad_type_exn("syntax-error", "syntax?", where);

        if (argc == 4) {
            sub = args[3];
            if (!minim_syntaxp(sub))
                bad_type_exn("syntax-error", "syntax?", sub);

            fputs("  at: ", stderr);
            write_object(stderr, strip_syntax(sub));
            fprintf(stderr, "\n");
        }

        fputs("  in: ", stderr);
        write_object(stderr, strip_syntax(where));
        fprintf(stderr, "\n");
    }

    minim_shutdown(1);
}

// mobj to_datum_proc(int argc, mobj *args) {
//     // (-> syntax any)
//     if (!minim_syntaxp(args[0]))
//         bad_type_exn("syntax->datum", "syntax?", args[0]);
//     return strip_syntax(args[0]);
// }

// mobj syntax_e_proc(int argc, mobj *args) {
//     // (-> syntax any)
//     if (!minim_syntaxp(args[0]))
//         bad_type_exn("syntax-e", "syntax?", args[0]);
//     return minim_syntax_e(args[0]);
// }

// mobj syntax_loc_proc(int argc, mobj *args) {
//     // (-> syntax any)
//     if (!minim_syntaxp(args[0]))
//         bad_type_exn("syntax-loc", "syntax?", args[0]);
//     return minim_syntax_loc(args[0]);
// }

// mobj to_syntax_proc(int argc, mobj *args) {
//     // (-> any syntax)
//     return to_syntax(args[0]);
// }

// mobj syntax_to_list_proc(int argc, mobj *args) {
//     // (-> syntax (or #f list))
//     mobj stx, lst;
    
//     stx = args[0];
//     if (!minim_syntaxp(stx))
//         bad_type_exn("syntax->list", "syntax?", stx);

//     lst = minim_syntax_e(stx);
//     if (minim_nullp(lst))
//         return minim_null;
//     else if (!minim_consp(lst))
//         return minim_false;
//     else 
//         return syntax_to_list(lst, lst);
// }
