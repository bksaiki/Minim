/*
    Pairs, lists
*/

#include "../minim.h"

mobj *Mcons(mobj *car, mobj *cdr) {
    minim_pair_object *o = GC_alloc(sizeof(minim_pair_object));
    o->type = MINIM_PAIR_TYPE;
    o->car = car;
    o->cdr = cdr;
    return ((mobj *) o);
}

// Returns true if the object is a list
int is_list(mobj *x) {
    while (minim_is_pair(x)) x = minim_cdr(x);
    return minim_is_null(x);
}

long list_length(mobj *xs) {
    mobj *it = xs;
    long length = 0;

    while (!minim_is_null(it)) {
        if (!minim_is_pair(it)) {
            fprintf(stderr, "list_length: not a list");
            minim_shutdown(1);
        }

        it = minim_cdr(it);
        ++length;
    }

    return length;
}

mobj list_reverse(mobj *xs) {
    mobj v = minim_null;
    while (!minim_nullp(xs)) {
        v = Mcons(minim_car(xs), v);
        xs = minim_cdr(xs);
    }

    return v;
}

long improper_list_length(mobj *xs) {
    mobj *it = xs;
    long length = 0;

    while (minim_is_pair(it)) {
        it = minim_cdr(it);
        ++length;
    }

    return length;
}

// Makes an association list.
// Unsafe: only iterates on `xs`.
mobj *make_assoc(mobj *xs, mobj *ys) {
    mobj *assoc, *it;

    if (minim_is_null(xs))
        return minim_null;

    assoc = Mcons(Mcons(minim_car(xs), minim_car(ys)), minim_null);
    it = assoc;
    while (!minim_is_null(xs = minim_cdr(xs))) {
        ys = minim_cdr(ys);
        minim_cdr(it) = Mcons(Mcons(minim_car(xs), minim_car(ys)), minim_null);
        it = minim_cdr(it);
    }

    return assoc;
}

// Copies a list.
// Unsafe: does not check if `xs` is a list.
mobj *copy_list(mobj *xs) {
    mobj *head, *tail, *it;

    if (minim_is_null(xs))
        return minim_null;

    head = Mcons(minim_car(xs), minim_null);
    tail = head;
    it = xs;

    while (!minim_is_null(it = minim_cdr(it))) {
        minim_cdr(tail) = Mcons(minim_car(it), minim_null);
        tail = minim_cdr(tail);
    }
    
    return head;
}

mobj *for_each(mobj *proc, int argc, mobj **args, mobj *env) {
    mobj **lsts;
    long len0, len;
    int stashc, i;

    if (!minim_is_proc(proc))
        bad_type_exn("for-each", "procedure?", proc);

    for (i = 0; i < argc; ++i) {
        if (!is_list(args[i]))
            bad_type_exn("for-each", "list?", args[i]);
        
        if (i == 0) {
            len0 = list_length(args[i]);
        } else {
            len = list_length(args[i]);
            if (len != len0) {
                fprintf(stderr, "for-each: lists of different lengths\n");
                fprintf(stderr, "  one list: %ld, second list: %ld\n", len0, len);
                minim_shutdown(1);
            }
        }
    }

    // stash lists since we call the procedure
    // TODO: potential for GC to lose track of the head of each list
    lsts = GC_alloc(argc * sizeof(mobj *));
    memcpy(lsts, args, argc * sizeof(mobj *));

    stashc = stash_call_args();
    while (!minim_is_null(lsts[0])) {
        for (i = 0; i < argc; ++i) {
            push_call_arg(minim_car(lsts[i]));
            lsts[i] = minim_cdr(lsts[i]);
        }

        // ignore the result (side-effect only)
        call_with_args(proc, env);
    }

    prepare_call_args(stashc);
    return minim_void;
}

mobj *map_list(mobj *proc, int argc, mobj **args, mobj *env) {
    mobj **lsts, *res, *head, *tail;
    minim_thread *th;
    long len0, len;
    int stashc, i;

    if (!minim_is_proc(proc))
        bad_type_exn("map", "procedure?", proc);

    for (i = 0; i < argc; ++i) {
        if (!is_list(args[i]))
            bad_type_exn("map", "list?", args[i]);
        
        if (i == 0) {
            len0 = list_length(args[i]);
        } else {
            len = list_length(args[i]);
            if (len != len0) {
                fprintf(stderr, "map: lists of different lengths\n");
                fprintf(stderr, "  one list: %ld, second list: %ld\n", len0, len);
                minim_shutdown(1);
            }
        }
    }

    // stash lists since we call the procedure
    // TODO: potential for GC to lose track of the head of each list
    lsts = GC_alloc(argc * sizeof(mobj *));
    memcpy(lsts, args, argc * sizeof(mobj *));

    head = NULL;
    stashc = stash_call_args();
    while (!minim_is_null(lsts[0])) {
        for (i = 0; i < argc; ++i) {
            push_call_arg(minim_car(lsts[i]));
            lsts[i] = minim_cdr(lsts[i]);
        }

        res = call_with_args(proc, env);
        if (minim_is_values(res)) {
            th = current_thread();
            if (values_buffer_count(th) != 1) {
                fprintf(stderr, "result arity mismatch\n");
                fprintf(stderr, "  expected: 1, received: %d\n", values_buffer_count(th));
                minim_shutdown(1);
            } else {
                res = values_buffer_ref(th, 0);
            }
        }

        if (head) {
            minim_cdr(tail) = Mcons(res, minim_null);
            tail = minim_cdr(tail);
        } else {
            head = Mcons(res, minim_null);
            tail = head;
        }
    }

    prepare_call_args(stashc);
    return (head ? head : minim_null);
}

mobj *andmap(mobj *proc, int argc, mobj **args, mobj *env) {
    mobj **lsts;
    long len0, len;
    int stashc, i;

    if (!minim_is_proc(proc))
        bad_type_exn("andmap", "procedure?", proc);

    for (i = 0; i < argc; ++i) {
        if (!is_list(args[i]))
            bad_type_exn("andmap", "list?", args[i]);
        
        if (i == 0) {
            len0 = list_length(args[i]);
        } else {
            len = list_length(args[i]);
            if (len != len0) {
                fprintf(stderr, "andmap: lists of different lengths\n");
                fprintf(stderr, "  one list: %ld, second list: %ld\n", len0, len);
                minim_shutdown(1);
            }
        }
    }

    // stash lists since we call the procedure
    // TODO: potential for GC to lose track of the head of each list
    lsts = GC_alloc(argc * sizeof(mobj *));
    memcpy(lsts, args, argc * sizeof(mobj *));

    stashc = stash_call_args();
    while (!minim_is_null(lsts[0])) {
        for (i = 0; i < argc; ++i) {
            push_call_arg(minim_car(lsts[i]));
            lsts[i] = minim_cdr(lsts[i]);
        }

        // only check for false (early exit)
        if (minim_is_false(call_with_args(proc, env))) {
            prepare_call_args(stashc);
            return minim_false;
        }
    }

    prepare_call_args(stashc);
    return minim_true;
}

mobj *ormap(mobj *proc, int argc, mobj **args, mobj *env) {
    mobj **lsts;
    long len0, len;
    int stashc, i;

    if (!minim_is_proc(proc))
        bad_type_exn("ormap", "procedure?", proc);

    for (i = 0; i < argc; ++i) {
        if (!is_list(args[i]))
            bad_type_exn("ormap", "list?", args[i]);
        
        if (i == 0) {
            len0 = list_length(args[i]);
        } else {
            len = list_length(args[i]);
            if (len != len0) {
                fprintf(stderr, "ormap: lists of different lengths\n");
                fprintf(stderr, "  one list: %ld, second list: %ld\n", len0, len);
                minim_shutdown(1);
            }
        }
    }

    // stash lists since we call the procedure
    // TODO: potential for GC to lose track of the head of each list
    lsts = GC_alloc(argc * sizeof(mobj *));
    memcpy(lsts, args, argc * sizeof(mobj *));

    stashc = stash_call_args();
    while (!minim_is_null(lsts[0])) {
        for (i = 0; i < argc; ++i) {
            push_call_arg(minim_car(lsts[i]));
            lsts[i] = minim_cdr(lsts[i]);
        }

        // only check for false (early exit)
        if (!minim_is_false(call_with_args(proc, env))) {
            prepare_call_args(stashc);
            return minim_true;
        }
    }

    prepare_call_args(stashc);
    return minim_false;
}

//
//  Primitives
//

mobj *is_pair_proc(int argc, mobj **args) {
    // (-> any boolean)
    return minim_is_pair(args[0]) ? minim_true : minim_false;
}

mobj *is_list_proc(int argc, mobj **args) {
    // (-> any boolean)
    mobj *thing;
    for (thing = args[0]; minim_is_pair(thing); thing = minim_cdr(thing));
    return minim_is_null(thing) ? minim_true : minim_false;
}

mobj *cons_proc(int argc, mobj **args) {
    // (-> any any pair)
    return Mcons(args[0], args[1]);
}

mobj *car_proc(int argc, mobj **args) {
    // (-> pair any)
    mobj *o = args[0];
    if (!minim_is_pair(o))
        bad_type_exn("car", "pair?", o);
    return minim_car(o);
}

mobj *cdr_proc(int argc, mobj **args) {
    // (-> pair any)
    mobj *o = args[0];
    if (!minim_is_pair(o))
        bad_type_exn("cdr", "pair?", o);
    return minim_cdr(o);
}

mobj *caar_proc(int argc, mobj **args) {
    // (-> (pairof pair any) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)))
        bad_type_exn("caar", "(pairof pair? any)", o);
    return minim_caar(o);
}

mobj *cadr_proc(int argc, mobj **args) {
    // (-> (pairof any pair) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)))
        bad_type_exn("cadr", "(pairof any pair)", o);
    return minim_cadr(o);
}

mobj *cdar_proc(int argc, mobj **args) {
    // (-> (pairof pair any) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)))
        bad_type_exn("cdar", "(pairof pair? any)", o);
    return minim_cdar(o);
}

mobj *cddr_proc(int argc, mobj **args) {
    // (-> (pairof any pair) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)))
        bad_type_exn("cddr", "(pairof any pair)", o);
    return minim_cddr(o);
}

mobj *caaar_proc(int argc, mobj **args) {
    // (-> (pairof (pairof pair any) any) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) || !minim_is_pair(minim_caar(o)))
        bad_type_exn("caaar", "(pairof (pairof pair? any) any)", o);
    return minim_car(minim_caar(o));
}

mobj *caadr_proc(int argc, mobj **args) {
    // (-> (pairof any (pairof pair any)) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) || !minim_is_pair(minim_cadr(o)))
        bad_type_exn("caadr", "(pairof any (pairof pair? any))", o);
    return minim_car(minim_cadr(o));
}

mobj *cadar_proc(int argc, mobj **args) {
    // (-> (pairof (pairof any pair) any) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) || !minim_is_pair(minim_cdar(o)))
        bad_type_exn("cadar", "(pairof (pairof any pair?) any)", o);
    return minim_car(minim_cdar(o));
}

mobj *caddr_proc(int argc, mobj **args) {
    // (-> (pairof any (pairof any pair)) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) || !minim_is_pair(minim_cddr(o)))
        bad_type_exn("caddr", "(pairof any (pairof any pair))", o);
    return minim_car(minim_cddr(o));
}

mobj *cdaar_proc(int argc, mobj **args) {
    // (-> (pairof (pairof pair any) any) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) || !minim_is_pair(minim_caar(o)))
        bad_type_exn("cdaar", "(pairof (pairof pair? any) any)", o);
    return minim_cdr(minim_caar(o));
}

mobj *cdadr_proc(int argc, mobj **args) {
    // (-> (pairof any (pairof pair any)) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) || !minim_is_pair(minim_cadr(o)))
        bad_type_exn("cdadr", "(pairof any (pairof pair? any))", o);
    return minim_cdr(minim_cadr(o));
}

mobj *cddar_proc(int argc, mobj **args) {
    // (-> (pairof (pairof any pair) any) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) || !minim_is_pair(minim_cdar(o)))
        bad_type_exn("cddar", "(pairof (pairof any pair?) any)", o);
    return minim_cdr(minim_cdar(o));
}

mobj *cdddr_proc(int argc, mobj **args) {
    // (-> (pairof any (pairof any pair)) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) || !minim_is_pair(minim_cddr(o)))
        bad_type_exn("cdddr", "(pairof any (pairof any pair?))", o);
    return minim_cdr(minim_cddr(o));
}

mobj *caaaar_proc(int argc, mobj **args) {
    // (-> (pairof (pairof (pairof pair any) any) any) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_caar(o)) || !minim_is_pair(minim_car(minim_caar(o))))
        bad_type_exn("caaaar", "(pairof (pairof (pairof pair? any) any) any)", o);
    return minim_caar(minim_caar(o));
}

mobj *caaadr_proc(int argc, mobj **args) {
    // (-> (pairof any (pairof (pairof pair any) any)) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cadr(o)) || !minim_is_pair(minim_car(minim_cadr(o))))
        bad_type_exn("caaadr", "(pairof any (pairof (pairof pair? any) any))", o);
    return minim_caar(minim_cadr(o));
}

mobj *caadar_proc(int argc, mobj **args) {
    // (-> (pairof (pairof any (pairof pair any)) any) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_cdar(o)) || !minim_is_pair(minim_car(minim_cdar(o))))
        bad_type_exn("caadar", "(pairof (pairof any (pairof pair? any)) any)", o);
    return minim_caar(minim_cdar(o));
}

mobj *caaddr_proc(int argc, mobj **args) {
    // (-> (pairof any (pairof any (pairof pair any))) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cddr(o)) ||  !minim_is_pair(minim_car(minim_cddr(o))))
        bad_type_exn("caaddr", "(pairof any (pairof any (pairof pair? any)))", o);
    return minim_caar(minim_cddr(o));
}

mobj *cadaar_proc(int argc, mobj **args) {
    // (-> (pairof (pairof (pairof any pair) any) any) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_caar(o)) || !minim_is_pair(minim_cdr(minim_caar(o))))
        bad_type_exn("cadaar", "(pairof (pairof (pairof any pair?) any) any)", o);
    return minim_cadr(minim_caar(o));
}

mobj *cadadr_proc(int argc, mobj **args) {
    // (-> (pairof any (pairof (pairof any pair) any)) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cadr(o)) || !minim_is_pair(minim_cdr(minim_cadr(o))))
        bad_type_exn("cadadr", "(pairof any (pairof (pairof any pair?) any))", o);
    return minim_cadr(minim_cadr(o));
}

mobj *caddar_proc(int argc, mobj **args) {
    // (-> (pairof (pairof any (pairof any pair)) any) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_cdar(o)) || !minim_is_pair(minim_cdr(minim_cdar(o))))
        bad_type_exn("caddar", "(pairof (pairof any (pairof any pair?) any)", o);
    return minim_cadr(minim_cdar(o));
}

mobj *cadddr_proc(int argc, mobj **args) {
    // (-> (pairof any (pairof any (pairof any pair))) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cddr(o)) || !minim_is_pair(minim_cdr(minim_cddr(o))))
        bad_type_exn("cadddr", "(pairof any (pairof any (pairof any pair?)))", o);
    return minim_cadr(minim_cddr(o));
}

mobj *cdaaar_proc(int argc, mobj **args) {
    // (-> (pairof (pairof (pairof pair any) any) any) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_caar(o)) || !minim_is_pair(minim_car(minim_caar(o))))
        bad_type_exn("cdaaar", "(pairof (pairof (pairof pair? any) any) any)", o);
    return minim_cdar(minim_caar(o));
}

mobj *cdaadr_proc(int argc, mobj **args) {
    // (-> (pairof any (pairof (pairof pair any) any)) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cadr(o)) || !minim_is_pair(minim_car(minim_cadr(o))))
        bad_type_exn("cdaadr", "(pairof any (pairof (pairof pair? any) any))", o);
    return minim_cdar(minim_cadr(o));
}

mobj *cdadar_proc(int argc, mobj **args) {
    // (-> (pairof (pairof any (pairof pair any)) any) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_cdar(o)) || !minim_is_pair(minim_car(minim_cdar(o))))
        bad_type_exn("cdadar", "(pairof (pairof any (pairof pair? any)) any)", o);
    return minim_cdar(minim_cdar(o));
}

mobj *cdaddr_proc(int argc, mobj **args) {
    // (-> (pairof any (pairof any (pairof pair any))) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cddr(o)) ||  !minim_is_pair(minim_car(minim_cddr(o))))
        bad_type_exn("cdaddr", "(pairof any (pairof any (pairof pair? any)))", o);
    return minim_cdar(minim_cddr(o));
}

mobj *cddaar_proc(int argc, mobj **args) {
    // (-> (pairof (pairof (pairof any pair) any) any) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_caar(o)) || !minim_is_pair(minim_cdr(minim_caar(o))))
        bad_type_exn("cddaar", "(pairof (pairof (pairof any pair?) any) any)", o);
    return minim_cddr(minim_caar(o));
}

mobj *cddadr_proc(int argc, mobj **args) {
    // (-> (pairof any (pairof (pairof any pair) any)) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cadr(o)) || !minim_is_pair(minim_cdr(minim_cadr(o))))
        bad_type_exn("cddadr", "(pairof any (pairof (pairof any pair?) any))", o);
    return minim_cddr(minim_cadr(o));
}

mobj *cdddar_proc(int argc, mobj **args) {
    // (-> (pairof (pairof any (pairof any pair)) any) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_cdar(o)) || !minim_is_pair(minim_cdr(minim_cdar(o))))
        bad_type_exn("cdddar", "(pairof (pairof any (pairof any pair?) any)", o);
    return minim_cddr(minim_cdar(o));
}

mobj *cddddr_proc(int argc, mobj **args) {
    // (-> (pairof any (pairof any (pairof any pair))) any)
    mobj *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cddr(o)) || !minim_is_pair(minim_cdr(minim_cddr(o))))
        bad_type_exn("cddddr", "(pairof any (pairof any (pairof any pair?)))", o);
    return minim_cddr(minim_cddr(o));
}

mobj *set_car_proc(int argc, mobj **args) {
    // (-> pair any void)
    mobj *o = args[0];
    if (!minim_is_pair(o))
        bad_type_exn("set-car!", "pair?", o);
    minim_car(o) = args[1];
    return minim_void;
}

mobj *set_cdr_proc(int argc, mobj **args) {
    // (-> pair any void)
    mobj *o = args[0];
    if (!minim_is_pair(o))
        bad_type_exn("set-cdr!", "pair?", o);
    minim_cdr(o) = args[1];
    return minim_void;
}

mobj *list_proc(int argc, mobj **args) {
    // (-> any ... list)
    mobj *lst;
    int i;

    lst = minim_null;
    for (i = argc - 1; i >= 0; --i)
        lst = Mcons(args[i], lst);

    return lst;
}

mobj *make_list_proc(int argc, mobj **args) {
    // (-> non-negative-integer? any list)
    mobj *lst;
    long len, i;

    if (!minim_is_fixnum(args[0]) || minim_fixnum(args[0]) < 0)
        bad_type_exn("make-vector", "non-negative-integer?", args[0]);
    len = minim_fixnum(args[0]);

    lst = minim_null;
    for (i = 0; i < len; ++i)
        lst = Mcons(args[1], lst);

    return lst;
}

mobj *length_proc(int argc, mobj **args) {
    // (-> list non-negative-integer?)
    mobj *it;
    long length;

    length = 0;
    for (it = args[0]; minim_is_pair(it); it = minim_cdr(it))
        length += 1;

    if (!minim_is_null(it))
        bad_type_exn("length", "list?", args[0]);

    return Mfixnum(length);
}

mobj *reverse_proc(int argc, mobj **args) {
    // (-> list list)
    mobj *head, *it;
    
    head = minim_null;
    for (it = args[0]; minim_is_pair(it); it = minim_cdr(it))
        head = Mcons(minim_car(it), head);

    if (!minim_is_null(it))
        bad_type_exn("reverse", "expected list?", args[0]);
    return head;
}

mobj *append_proc(int argc, mobj **args) {
    // (-> list ... list)
    mobj *head, *lst_it, *it;
    int i;

    head = NULL;
    for (i = 0; i < argc; ++i) {
        if (!minim_is_null(args[i])) {
            for (it = args[i]; minim_is_pair(it); it = minim_cdr(it)) {
                if (head) {
                    minim_cdr(lst_it) = Mcons(minim_car(it), minim_null);
                    lst_it = minim_cdr(lst_it);
                } else {
                    head = Mcons(minim_car(it), minim_null);
                    lst_it = head;
                }
            }

            if (!minim_is_null(it))
                bad_type_exn("append", "expected list?", args[i]);
        }
    }

    return head ? head : minim_null;
}

mobj *for_each_proc(int argc, mobj **args) {
    // (-> proc list list ... list)
    uncallable_prim_exn("for-each");
}

mobj *map_proc(int argc, mobj **args) {
    uncallable_prim_exn("map");
}

mobj *andmap_proc(int argc, mobj **args) {
    uncallable_prim_exn("andmap");
}

mobj *ormap_proc(int argc, mobj **args) {
    uncallable_prim_exn("ormap");
}
