/*
    Pairs, lists
*/

#include "../minim.h"

minim_object *make_pair(minim_object *car, minim_object *cdr) {
    minim_pair_object *o = GC_alloc(sizeof(minim_pair_object));
    o->type = MINIM_PAIR_TYPE;
    o->car = car;
    o->cdr = cdr;
    return ((minim_object *) o);
}

// Returns true if the object is a list
int is_list(minim_object *x) {
    while (minim_is_pair(x)) x = minim_cdr(x);
    return minim_is_null(x);
}

long list_length(minim_object *xs) {
    minim_object *it = xs;
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

// Makes an association list.
// Unsafe: only iterates on `xs`.
minim_object *make_assoc(minim_object *xs, minim_object *ys) {
    minim_object *assoc, *it;

    if (minim_is_null(xs))
        return minim_null;

    assoc = make_pair(make_pair(minim_car(xs), minim_car(ys)), minim_null);
    it = assoc;
    while (!minim_is_null(xs = minim_cdr(xs))) {
        ys = minim_cdr(ys);
        minim_cdr(it) = make_pair(make_pair(minim_car(xs), minim_car(ys)), minim_null);
        it = minim_cdr(it);
    }

    return assoc;
}

// Copies a list.
// Unsafe: does not check if `xs` is a list.
minim_object *copy_list(minim_object *xs) {
    minim_object *head, *tail, *it;

    if (minim_is_null(xs))
        return minim_null;

    head = make_pair(minim_car(xs), minim_null);
    tail = head;
    it = xs;

    while (!minim_is_null(it = minim_cdr(it))) {
        minim_cdr(tail) = make_pair(minim_car(it), minim_null);
        tail = minim_cdr(tail);
    }
    
    return head;
}

minim_object *for_each(minim_object *proc, int argc, minim_object **args, minim_object *env) {
    minim_object **lsts;
    long len0, len;
    int i;

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
    lsts = GC_alloc(argc * sizeof(minim_object *));
    memcpy(lsts, args, argc * sizeof(minim_object *));

    assert_no_call_args();
    while (!minim_is_null(lsts[0])) {
        for (i = 0; i < argc; ++i) {
            push_call_arg(minim_car(lsts[i]));
            lsts[i] = minim_cdr(lsts[i]);
        }

        // ignore the result (side-effect only)
        call_with_args(proc, env);
    }

    return minim_void;
}

minim_object *map_list(minim_object *proc, int argc, minim_object **args, minim_object *env) {
    minim_object **lsts, *res, *head, *tail;
    minim_thread *th;
    long len0, len;
    int i;

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
    lsts = GC_alloc(argc * sizeof(minim_object *));
    memcpy(lsts, args, argc * sizeof(minim_object *));

    head = NULL;
    assert_no_call_args();
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
            minim_cdr(tail) = make_pair(res, minim_null);
            tail = minim_cdr(tail);
        } else {
            head = make_pair(res, minim_null);
            tail = head;
        }
    }

    return (head ? head : minim_null);
}

minim_object *andmap(minim_object *proc, int argc, minim_object **args, minim_object *env) {
    minim_object **lsts;
    long len0, len;
    int i;

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
    lsts = GC_alloc(argc * sizeof(minim_object *));
    memcpy(lsts, args, argc * sizeof(minim_object *));

    assert_no_call_args();
    while (!minim_is_null(lsts[0])) {
        for (i = 0; i < argc; ++i) {
            push_call_arg(minim_car(lsts[i]));
            lsts[i] = minim_cdr(lsts[i]);
        }

        // only check for false (early exit)
        if (minim_is_false(call_with_args(proc, env)))
            return minim_false;
    }

    return minim_true;
}

minim_object *ormap(minim_object *proc, int argc, minim_object **args, minim_object *env) {
    minim_object **lsts;
    long len0, len;
    int i;

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
    lsts = GC_alloc(argc * sizeof(minim_object *));
    memcpy(lsts, args, argc * sizeof(minim_object *));

    assert_no_call_args();
    while (!minim_is_null(lsts[0])) {
        for (i = 0; i < argc; ++i) {
            push_call_arg(minim_car(lsts[i]));
            lsts[i] = minim_cdr(lsts[i]);
        }

        // only check for false (early exit)
        if (!minim_is_false(call_with_args(proc, env)))
            return minim_true;
    }

    return minim_false;
}

//
//  Primitives
//

minim_object *is_pair_proc(int argc, minim_object **args) {
    // (-> any boolean)
    return minim_is_pair(args[0]) ? minim_true : minim_false;
}

minim_object *is_list_proc(int argc, minim_object **args) {
    // (-> any boolean)
    minim_object *thing;
    for (thing = args[0]; minim_is_pair(thing); thing = minim_cdr(thing));
    return minim_is_null(thing) ? minim_true : minim_false;
}

minim_object *cons_proc(int argc, minim_object **args) {
    // (-> any any pair)
    return make_pair(args[0], args[1]);
}

minim_object *car_proc(int argc, minim_object **args) {
    // (-> pair any)
    minim_object *o = args[0];
    if (!minim_is_pair(o))
        bad_type_exn("car", "pair?", o);
    return minim_car(o);
}

minim_object *cdr_proc(int argc, minim_object **args) {
    // (-> pair any)
    minim_object *o = args[0];
    if (!minim_is_pair(o))
        bad_type_exn("cdr", "pair?", o);
    return minim_cdr(o);
}

minim_object *caar_proc(int argc, minim_object **args) {
    // (-> (pairof pair any) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)))
        bad_type_exn("caar", "(pairof pair? any)", o);
    return minim_caar(o);
}

minim_object *cadr_proc(int argc, minim_object **args) {
    // (-> (pairof any pair) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)))
        bad_type_exn("cadr", "(pairof any pair)", o);
    return minim_cadr(o);
}

minim_object *cdar_proc(int argc, minim_object **args) {
    // (-> (pairof pair any) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)))
        bad_type_exn("cdar", "(pairof pair? any)", o);
    return minim_cdar(o);
}

minim_object *cddr_proc(int argc, minim_object **args) {
    // (-> (pairof any pair) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)))
        bad_type_exn("cddr", "(pairof any pair)", o);
    return minim_cddr(o);
}

minim_object *caaar_proc(int argc, minim_object **args) {
    // (-> (pairof (pairof pair any) any) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) || !minim_is_pair(minim_caar(o)))
        bad_type_exn("caaar", "(pairof (pairof pair? any) any)", o);
    return minim_car(minim_caar(o));
}

minim_object *caadr_proc(int argc, minim_object **args) {
    // (-> (pairof any (pairof pair any)) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) || !minim_is_pair(minim_cadr(o)))
        bad_type_exn("caadr", "(pairof any (pairof pair? any))", o);
    return minim_car(minim_cadr(o));
}

minim_object *cadar_proc(int argc, minim_object **args) {
    // (-> (pairof (pairof any pair) any) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) || !minim_is_pair(minim_cdar(o)))
        bad_type_exn("cadar", "(pairof (pairof any pair?) any)", o);
    return minim_car(minim_cdar(o));
}

minim_object *caddr_proc(int argc, minim_object **args) {
    // (-> (pairof any (pairof any pair)) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) || !minim_is_pair(minim_cddr(o)))
        bad_type_exn("caddr", "(pairof any (pairof any pair))", o);
    return minim_car(minim_cddr(o));
}

minim_object *cdaar_proc(int argc, minim_object **args) {
    // (-> (pairof (pairof pair any) any) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) || !minim_is_pair(minim_caar(o)))
        bad_type_exn("cdaar", "(pairof (pairof pair? any) any)", o);
    return minim_cdr(minim_caar(o));
}

minim_object *cdadr_proc(int argc, minim_object **args) {
    // (-> (pairof any (pairof pair any)) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) || !minim_is_pair(minim_cadr(o)))
        bad_type_exn("cdadr", "(pairof any (pairof pair? any))", o);
    return minim_cdr(minim_cadr(o));
}

minim_object *cddar_proc(int argc, minim_object **args) {
    // (-> (pairof (pairof any pair) any) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) || !minim_is_pair(minim_cdar(o)))
        bad_type_exn("cddar", "(pairof (pairof any pair?) any)", o);
    return minim_cdr(minim_cdar(o));
}

minim_object *cdddr_proc(int argc, minim_object **args) {
    // (-> (pairof any (pairof any pair)) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) || !minim_is_pair(minim_cddr(o)))
        bad_type_exn("cdddr", "(pairof any (pairof any pair?))", o);
    return minim_cdr(minim_cddr(o));
}

minim_object *caaaar_proc(int argc, minim_object **args) {
    // (-> (pairof (pairof (pairof pair any) any) any) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_caar(o)) || !minim_is_pair(minim_car(minim_caar(o))))
        bad_type_exn("caaaar", "(pairof (pairof (pairof pair? any) any) any)", o);
    return minim_caar(minim_caar(o));
}

minim_object *caaadr_proc(int argc, minim_object **args) {
    // (-> (pairof any (pairof (pairof pair any) any)) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cadr(o)) || !minim_is_pair(minim_car(minim_cadr(o))))
        bad_type_exn("caaadr", "(pairof any (pairof (pairof pair? any) any))", o);
    return minim_caar(minim_cadr(o));
}

minim_object *caadar_proc(int argc, minim_object **args) {
    // (-> (pairof (pairof any (pairof pair any)) any) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_cdar(o)) || !minim_is_pair(minim_car(minim_cdar(o))))
        bad_type_exn("caadar", "(pairof (pairof any (pairof pair? any)) any)", o);
    return minim_caar(minim_cdar(o));
}

minim_object *caaddr_proc(int argc, minim_object **args) {
    // (-> (pairof any (pairof any (pairof pair any))) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cddr(o)) ||  !minim_is_pair(minim_car(minim_cddr(o))))
        bad_type_exn("caaddr", "(pairof any (pairof any (pairof pair? any)))", o);
    return minim_caar(minim_cddr(o));
}

minim_object *cadaar_proc(int argc, minim_object **args) {
    // (-> (pairof (pairof (pairof any pair) any) any) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_caar(o)) || !minim_is_pair(minim_cdr(minim_caar(o))))
        bad_type_exn("cadaar", "(pairof (pairof (pairof any pair?) any) any)", o);
    return minim_cadr(minim_caar(o));
}

minim_object *cadadr_proc(int argc, minim_object **args) {
    // (-> (pairof any (pairof (pairof any pair) any)) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cadr(o)) || !minim_is_pair(minim_cdr(minim_cadr(o))))
        bad_type_exn("cadadr", "(pairof any (pairof (pairof any pair?) any))", o);
    return minim_cadr(minim_cadr(o));
}

minim_object *caddar_proc(int argc, minim_object **args) {
    // (-> (pairof (pairof any (pairof any pair)) any) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_cdar(o)) || !minim_is_pair(minim_cdr(minim_cdar(o))))
        bad_type_exn("caddar", "(pairof (pairof any (pairof any pair?) any)", o);
    return minim_cadr(minim_cdar(o));
}

minim_object *cadddr_proc(int argc, minim_object **args) {
    // (-> (pairof any (pairof any (pairof any pair))) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cddr(o)) || !minim_is_pair(minim_cdr(minim_cddr(o))))
        bad_type_exn("cadddr", "(pairof any (pairof any (pairof any pair?)))", o);
    return minim_cadr(minim_cddr(o));
}

minim_object *cdaaar_proc(int argc, minim_object **args) {
    // (-> (pairof (pairof (pairof pair any) any) any) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_caar(o)) || !minim_is_pair(minim_car(minim_caar(o))))
        bad_type_exn("cdaaar", "(pairof (pairof (pairof pair? any) any) any)", o);
    return minim_cdar(minim_caar(o));
}

minim_object *cdaadr_proc(int argc, minim_object **args) {
    // (-> (pairof any (pairof (pairof pair any) any)) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cadr(o)) || !minim_is_pair(minim_car(minim_cadr(o))))
        bad_type_exn("cdaadr", "(pairof any (pairof (pairof pair? any) any))", o);
    return minim_cdar(minim_cadr(o));
}

minim_object *cdadar_proc(int argc, minim_object **args) {
    // (-> (pairof (pairof any (pairof pair any)) any) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_cdar(o)) || !minim_is_pair(minim_car(minim_cdar(o))))
        bad_type_exn("cdadar", "(pairof (pairof any (pairof pair? any)) any)", o);
    return minim_cdar(minim_cdar(o));
}

minim_object *cdaddr_proc(int argc, minim_object **args) {
    // (-> (pairof any (pairof any (pairof pair any))) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cddr(o)) ||  !minim_is_pair(minim_car(minim_cddr(o))))
        bad_type_exn("cdaddr", "(pairof any (pairof any (pairof pair? any)))", o);
    return minim_cdar(minim_cddr(o));
}

minim_object *cddaar_proc(int argc, minim_object **args) {
    // (-> (pairof (pairof (pairof any pair) any) any) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_caar(o)) || !minim_is_pair(minim_cdr(minim_caar(o))))
        bad_type_exn("cddaar", "(pairof (pairof (pairof any pair?) any) any)", o);
    return minim_cddr(minim_caar(o));
}

minim_object *cddadr_proc(int argc, minim_object **args) {
    // (-> (pairof any (pairof (pairof any pair) any)) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cadr(o)) || !minim_is_pair(minim_cdr(minim_cadr(o))))
        bad_type_exn("cddadr", "(pairof any (pairof (pairof any pair?) any))", o);
    return minim_cddr(minim_cadr(o));
}

minim_object *cdddar_proc(int argc, minim_object **args) {
    // (-> (pairof (pairof any (pairof any pair)) any) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_cdar(o)) || !minim_is_pair(minim_cdr(minim_cdar(o))))
        bad_type_exn("cdddar", "(pairof (pairof any (pairof any pair?) any)", o);
    return minim_cddr(minim_cdar(o));
}

minim_object *cddddr_proc(int argc, minim_object **args) {
    // (-> (pairof any (pairof any (pairof any pair))) any)
    minim_object *o = args[0];
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cddr(o)) || !minim_is_pair(minim_cdr(minim_cddr(o))))
        bad_type_exn("cddddr", "(pairof any (pairof any (pairof any pair?)))", o);
    return minim_cddr(minim_cddr(o));
}

minim_object *set_car_proc(int argc, minim_object **args) {
    // (-> pair any void)
    minim_object *o = args[0];
    if (!minim_is_pair(o))
        bad_type_exn("set-car!", "pair?", o);
    minim_car(o) = args[1];
    return minim_void;
}

minim_object *set_cdr_proc(int argc, minim_object **args) {
    // (-> pair any void)
    minim_object *o = args[0];
    if (!minim_is_pair(o))
        bad_type_exn("set-cdr!", "pair?", o);
    minim_cdr(o) = args[1];
    return minim_void;
}

minim_object *list_proc(int argc, minim_object **args) {
    // (-> any ... list)
    minim_object *lst;
    int i;

    lst = minim_null;
    for (i = argc - 1; i >= 0; --i)
        lst = make_pair(args[i], lst);

    return lst;
}

minim_object *length_proc(int argc, minim_object **args) {
    // (-> list non-negative-integer?)
    minim_object *it;
    long length;

    length = 0;
    for (it = args[0]; minim_is_pair(it); it = minim_cdr(it))
        length += 1;

    if (!minim_is_null(it))
        bad_type_exn("length", "list?", args[0]);

    return make_fixnum(length);
}

minim_object *reverse_proc(int argc, minim_object **args) {
    // (-> list list)
    minim_object *head, *it;
    
    head = minim_null;
    for (it = args[0]; minim_is_pair(it); it = minim_cdr(it))
        head = make_pair(minim_car(it), head);

    if (!minim_is_null(it))
        bad_type_exn("reverse", "expected list?", args[0]);
    return head;
}

minim_object *append_proc(int argc, minim_object **args) {
    // (-> list ... list)
    minim_object *head, *lst_it, *it;
    int i;

    head = NULL;
    for (i = 0; i < argc; ++i) {
        if (!minim_is_null(args[i])) {
            for (it = args[i]; minim_is_pair(it); it = minim_cdr(it)) {
                if (head) {
                    minim_cdr(lst_it) = make_pair(minim_car(it), minim_null);
                    lst_it = minim_cdr(lst_it);
                } else {
                    head = make_pair(minim_car(it), minim_null);
                    lst_it = head;
                }
            }

            if (!minim_is_null(it))
                bad_type_exn("append", "expected list?", args[i]);
        }
    }

    return head ? head : minim_null;
}

minim_object *for_each_proc(int argc, minim_object **args) {
    // (-> proc list list ... list)
    uncallable_prim_exn("for-each");
}

minim_object *map_proc(int argc, minim_object **args) {
    uncallable_prim_exn("map");
}

minim_object *andmap_proc(int argc, minim_object **args) {
    uncallable_prim_exn("andmap");
}

minim_object *ormap_proc(int argc, minim_object **args) {
    uncallable_prim_exn("ormap");
}
