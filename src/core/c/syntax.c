/*
    Syntax
*/

#include "../minim.h"

mobj *make_syntax(mobj *e, mobj *loc) {
    minim_syntax_object *o = GC_alloc(sizeof(minim_syntax_object));
    o->type = MINIM_SYNTAX_TYPE;
    o->e = e;
    o->loc = loc;
    return ((mobj *) o);
}

mobj *make_pattern_var(mobj *value, mobj *depth) {
    minim_pattern_var_object *o = GC_alloc(sizeof(minim_pattern_var_object));
    o->type = MINIM_PATTERN_VAR_TYPE;
    o->value = value;
    o->depth = depth;
    return ((mobj *) o);
}

// Recursively converts an object to syntax
mobj *to_syntax(mobj *o) {
    mobj *it;
    long i;

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
    case MINIM_RECORD_TYPE:
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

    case MINIM_BOX_TYPE:
        return make_syntax(make_box(to_syntax(minim_box_contents(o))), minim_false);

    case MINIM_VECTOR_TYPE:
        it = make_vector(minim_vector_len(o), NULL);
        for (i = 0; i < minim_vector_len(o); ++i)
            minim_vector_ref(it, i) = to_syntax(minim_vector_ref(o, i));
        return make_syntax(it, minim_false);

    default:
        fprintf(stderr, "datum->syntax: cannot convert to syntax\n");
        minim_shutdown(1);
    }
}

mobj *strip_syntax(mobj *o) {
    mobj *t;
    long i;

    switch (o->type) {
    case MINIM_SYNTAX_TYPE:
        return strip_syntax(minim_syntax_e(o));
    case MINIM_PAIR_TYPE:
        return Mcons(strip_syntax(minim_car(o)), strip_syntax(minim_cdr(o)));
    case MINIM_BOX_TYPE:
        return make_box(strip_syntax(minim_box_contents(o)));
    case MINIM_VECTOR_TYPE:
        t = make_vector(minim_vector_len(o), NULL);
        for (i = 0; i < minim_vector_len(o); ++i)
            minim_vector_ref(t, i) = strip_syntax(minim_vector_ref(o, i));
        return t;
    default:
        return o;
    }
}

static mobj *syntax_to_list(mobj *head, mobj *it) {
    if (minim_is_null(minim_cdr(it))) {
        return head;
    } else if (minim_is_pair(minim_cdr(it))) {
        return syntax_to_list(head, minim_cdr(it));
    } else if (minim_is_syntax(minim_cdr(it))) {
        minim_cdr(it) = minim_syntax_e(minim_cdr(it));
        return syntax_to_list(head, it);
    } else {
        return minim_false;
    }
}

//
//  Primitives
//

mobj *is_syntax_proc(int argc, mobj **args) {
    // (-> any boolean)
    return minim_is_syntax(args[0]) ? minim_true : minim_false;
}

mobj *syntax_error_proc(int argc, mobj **args) {
    // (-> (or #f symbol) string any)
    // (-> (or #f symbol) string syntax any)
    // (-> (or #f symbol) string syntax syntaxs any)
    mobj *what, *why, *where, *sub;

    what = args[0];
    why = args[1];
    if (!minim_is_false(what) && !minim_symbolp(what))
        bad_type_exn("syntax-error", "symbol?", what);
    if (!minim_is_string(why))
        bad_type_exn("syntax-error", "string?", why);

    if (minim_is_false(what))
        fprintf(stderr, "error: %s\n", minim_string(why));
    else
        fprintf(stderr, "%s: %s\n", minim_symbol(what), minim_string(why));

    if (argc >= 3) {
        where = args[2];
        if (!minim_is_syntax(where))
            bad_type_exn("syntax-error", "syntax?", where);

        if (argc == 4) {
            sub = args[3];
            if (!minim_is_syntax(sub))
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

mobj *to_datum_proc(int argc, mobj **args) {
    // (-> syntax any)
    if (!minim_is_syntax(args[0]))
        bad_type_exn("syntax->datum", "syntax?", args[0]);
    return strip_syntax(args[0]);
}

mobj *syntax_e_proc(int argc, mobj **args) {
    // (-> syntax any)
    if (!minim_is_syntax(args[0]))
        bad_type_exn("syntax-e", "syntax?", args[0]);
    return minim_syntax_e(args[0]);
}

mobj *syntax_loc_proc(int argc, mobj **args) {
    // (-> syntax any)
    if (!minim_is_syntax(args[0]))
        bad_type_exn("syntax-loc", "syntax?", args[0]);
    return minim_syntax_loc(args[0]);
}

mobj *to_syntax_proc(int argc, mobj **args) {
    // (-> any syntax)
    return to_syntax(args[0]);
}

mobj *syntax_to_list_proc(int argc, mobj **args) {
    // (-> syntax (or #f list))
    mobj *stx, *lst;
    
    stx = args[0];
    if (!minim_is_syntax(stx))
        bad_type_exn("syntax->list", "syntax?", stx);

    lst = minim_syntax_e(stx);
    if (minim_is_null(lst))
        return minim_null;
    else if (!minim_is_pair(lst))
        return minim_false;
    else 
        return syntax_to_list(lst, lst);
}

mobj *is_pattern_var_proc(int argc, mobj **args) {
    return minim_is_pattern_var(args[0]) ? minim_true : minim_false;
}

mobj *make_pattern_var_proc(int argc, mobj **args) {
    // (-> any non-negative-integer? pattern-var?)
    mobj *value, *depth;
    
    value = args[0];
    depth = args[1];
    if (!minim_is_fixnum(depth) || minim_fixnum(depth) < 0)
        bad_type_exn("make-pattern-var", "non-negative-integer?", depth);
    return make_pattern_var(value, depth);
}

mobj *pattern_var_value_proc(int argc, mobj **args) {
    // (-> pattern-var? any)
    mobj *var = args[0];
    if (!minim_is_pattern_var(var))
        bad_type_exn("pattern-variable-value", "pattern-variable?", var);
    return minim_pattern_var_value(var);   
}

mobj *pattern_var_depth_proc(int argc, mobj **args) {
    // (-> pattern-var? any)
    mobj *var = args[0];
    if (!minim_is_pattern_var(var))
        bad_type_exn("pattern-variable-depth", "pattern-variable?", var);
    return minim_pattern_var_depth(var);   
}
