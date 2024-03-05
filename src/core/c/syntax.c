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

// Recursively converts an object to syntax
mobj to_syntax(mobj o) {
    mobj it;
    long i;
    
    switch (minim_type(o)) {
    case MINIM_OBJ_SYNTAX:
        return o;
    case MINIM_OBJ_SPECIAL:
    case MINIM_OBJ_CHAR:
    case MINIM_OBJ_FIXNUM:
    case MINIM_OBJ_SYMBOL:
    case MINIM_OBJ_STRING:
    case MINIM_OBJ_RECORD:
        return Msyntax(o, minim_false);
        break;
    case MINIM_OBJ_PAIR:
        it = o;
        do {
            minim_car(it) = to_syntax(minim_car(it));
            if (!minim_consp(minim_cdr(it))) {
                if (!minim_nullp(minim_cdr(it)))
                    minim_cdr(it) = to_syntax(minim_cdr(it));
                return Msyntax(o, minim_false);
            }
            it = minim_cdr(it);
        } while (1);

    case MINIM_OBJ_BOX:
        return Msyntax(Mbox(to_syntax(minim_unbox(o))), minim_false);

    case MINIM_OBJ_VECTOR:
        it = Mvector(minim_vector_len(o), NULL);
        for (i = 0; i < minim_vector_len(o); ++i)
            minim_vector_ref(it, i) = to_syntax(minim_vector_ref(o, i));
        return Msyntax(it, minim_false);
    
    default:
        fprintf(stderr, "datum->syntax: cannot convert to syntax\n");
        minim_shutdown(1);
    }
}

mobj strip_syntax(mobj o) {
    switch (minim_type(o)) {
    case MINIM_OBJ_SYNTAX:
        return strip_syntax(minim_syntax_e(o));
    case MINIM_OBJ_PAIR:
        return Mcons(strip_syntax(minim_car(o)), strip_syntax(minim_cdr(o)));
    case MINIM_OBJ_BOX:
        return Mbox(strip_syntax(minim_unbox(o)));
    case MINIM_OBJ_VECTOR:
        mobj t = Mvector(minim_vector_len(o), NULL);
        for (long i = 0; i < minim_vector_len(o); ++i)
            minim_vector_ref(t, i) = strip_syntax(minim_vector_ref(o, i));
        return t;
    default:
        return o;
    }
}

static mobj syntax_to_list(mobj head, mobj it) {
    if (minim_nullp(minim_cdr(it))) {
        return head;
    } else if (minim_consp(minim_cdr(it))) {
        return syntax_to_list(head, minim_cdr(it));
    } else if (minim_syntaxp(minim_cdr(it))) {
        minim_cdr(it) = minim_syntax_e(minim_cdr(it));
        return syntax_to_list(head, it);
    } else {
        return minim_false;
    }
}

//
//  Primitives
//

mobj syntaxp_proc(mobj x) {
    // (-> any boolean)
    return minim_syntaxp(x) ? minim_true : minim_false;
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
            write_object2(stderr, strip_syntax(sub), 1, 1);
            fprintf(stderr, "\n");
        }

        fputs("  in: ", stderr);
        write_object2(stderr, strip_syntax(where), 1, 1);
        fprintf(stderr, "\n");
    }

    minim_shutdown(1);
}

mobj to_datum_proc(int argc, mobj *args) {
    // (-> syntax any)
    if (!minim_syntaxp(args[0]))
        bad_type_exn("syntax->datum", "syntax?", args[0]);
    return strip_syntax(args[0]);
}

mobj syntax_e_proc(int argc, mobj *args) {
    // (-> syntax any)
    if (!minim_syntaxp(args[0]))
        bad_type_exn("syntax-e", "syntax?", args[0]);
    return minim_syntax_e(args[0]);
}

mobj syntax_loc_proc(int argc, mobj *args) {
    // (-> syntax any)
    if (!minim_syntaxp(args[0]))
        bad_type_exn("syntax-loc", "syntax?", args[0]);
    return minim_syntax_loc(args[0]);
}

mobj to_syntax_proc(int argc, mobj *args) {
    // (-> any syntax)
    return to_syntax(args[0]);
}

mobj syntax_to_list_proc(int argc, mobj *args) {
    // (-> syntax (or #f list))
    mobj stx, lst;
    
    stx = args[0];
    if (!minim_syntaxp(stx))
        bad_type_exn("syntax->list", "syntax?", stx);

    lst = minim_syntax_e(stx);
    if (minim_nullp(lst))
        return minim_null;
    else if (!minim_consp(lst))
        return minim_false;
    else 
        return syntax_to_list(lst, lst);
}
