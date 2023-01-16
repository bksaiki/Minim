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

minim_object *make_pattern_var(minim_object *value, minim_object *depth) {
    minim_pattern_var_object *o = GC_alloc(sizeof(minim_pattern_var_object));
    o->type = MINIM_PATTERN_VAR_TYPE;
    o->value = value;
    o->depth = depth;
    return ((minim_object *) o);
}

// Recursively converts an object to syntax
minim_object *to_syntax(minim_object *o) {
    minim_object *it;
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

    case MINIM_VECTOR_TYPE:
        it = make_vector(minim_vector_len(o), NULL);
        for (i = 0; i < minim_vector_len(o); ++i)
            minim_vector_ref(it, i) = to_syntax(minim_vector_ref(o, i));
        return make_syntax(it, minim_false);

    default:
        fprintf(stderr, "datum->syntax: cannot convert to syntax");
        minim_shutdown(1);
    }
}

minim_object *strip_syntax(minim_object *o) {
    minim_object *t;
    long i;

    switch (o->type) {
    case MINIM_SYNTAX_TYPE:
        return strip_syntax(minim_syntax_e(o));
    case MINIM_PAIR_TYPE:
        return make_pair(strip_syntax(minim_car(o)), strip_syntax(minim_cdr(o)));
    case MINIM_VECTOR_TYPE:
        t = make_vector(minim_vector_len(o), NULL);
        for (i = 0; i < minim_vector_len(o); ++i)
            minim_vector_ref(t, i) = strip_syntax(minim_vector_ref(o, i));
        return t;
    default:
        return o;
    }
}

static minim_object *syntax_to_list(minim_object *head, minim_object *it) {
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

minim_object *is_syntax_proc(int argc, minim_object **args) {
    // (-> any boolean)
    return minim_is_syntax(args[0]) ? minim_true : minim_false;
}

minim_object *syntax_error_proc(int argc, minim_object **args) {
    // (-> (or #f symbol) string any)
    // (-> (or #f symbol) string syntax any)
    // (-> (or #f symbol) string syntax syntaxs any)
    minim_object *what, *why, *where, *sub;

    what = args[0];
    why = args[1];
    if (!minim_is_false(what) && !minim_is_symbol(what))
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

minim_object *syntax_e_proc(int argc, minim_object **args) {
    // (-> syntax any)
    if (!minim_is_syntax(args[0]))
        bad_type_exn("syntax-e", "syntax?", args[0]);
    return minim_syntax_e(args[0]);
}

minim_object *syntax_loc_proc(int argc, minim_object **args) {
    // (-> syntax any)
    if (!minim_is_syntax(args[0]))
        bad_type_exn("syntax-loc", "syntax?", args[0]);
    return minim_syntax_loc(args[0]);
}

minim_object *to_syntax_proc(int argc, minim_object **args) {
    // (-> any syntax)
    return to_syntax(args[0]);
}

minim_object *to_datum_proc(int argc, minim_object **args) {
    // (-> syntax any)
    if (!minim_is_syntax(args[0]))
        bad_type_exn("syntax->datum", "syntax?", args[0]);
    return strip_syntax(args[0]);
}

minim_object *syntax_to_list_proc(int argc, minim_object **args) {
    // (-> syntax (or #f list))
    minim_object *stx, *lst;
    
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

minim_object *is_pattern_var_proc(int argc, minim_object **args) {
    return minim_is_pattern_var(args[0]) ? minim_true : minim_false;
}

minim_object *make_pattern_var_proc(int argc, minim_object **args) {
    // (-> any non-negative-integer? pattern-var?)
    minim_object *value, *depth;
    
    value = args[0];
    depth = args[1];
    if (!minim_is_fixnum(depth) || minim_fixnum(depth) < 0)
        bad_type_exn("make-pattern-var", "non-negative-integer?", depth);
    return make_pattern_var(value, depth);
}

minim_object *pattern_var_value_proc(int argc, minim_object **args) {
    // (-> pattern-var? any)
    minim_object *var = args[0];
    if (!minim_is_pattern_var(var))
        bad_type_exn("pattern-variable-value", "pattern-variable?", var);
    return minim_pattern_var_value(var);   
}

minim_object *pattern_var_depth_proc(int argc, minim_object **args) {
    // (-> pattern-var? any)
    minim_object *var = args[0];
    if (!minim_is_pattern_var(var))
        bad_type_exn("pattern-variable-depth", "pattern-variable?", var);
    return minim_pattern_var_depth(var);   
}
