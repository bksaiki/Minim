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
            exit(1);
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

minim_object *for_each(minim_object *proc, minim_object *lsts, minim_object *env) {
    minim_object *it, *args;
    minim_object **lst_arr, **arg_its;
    long argc, i, len0, len;

    if (!minim_is_proc(proc))
        bad_type_exn("for-each", "procedure?", proc);

    i = 0;
    argc = list_length(lsts);
    lst_arr = GC_alloc(argc * sizeof(minim_object *));
    arg_its = GC_alloc(argc * sizeof(minim_object *));
    for (it = lsts; !minim_is_null(it); it = minim_cdr(it)) {
        lst_arr[i] = minim_car(it);
        if (!is_list(lst_arr[i]))
            bad_type_exn("for-each", "list?", lst_arr[i]);

        if (i == 0) {
            len0 = list_length(lst_arr[i]);
        } else {
            len = list_length(lst_arr[i]);
            if (len != len0) {
                fprintf(stderr, "for-each: lists of different lengths\n");
                fprintf(stderr, "  one list: %ld, second list: %ld\n", len0, len);
                exit(1);
            }
        }

        arg_its[i] = lst_arr[i];
        ++i;
    }

    while (!minim_is_null(arg_its[0])) {
        args = make_pair(minim_car(arg_its[0]), minim_null);
        it = args;
        for (i = 1; i < argc; ++i) {
            minim_cdr(it) = make_pair(minim_car(arg_its[i]), minim_null);
            it = minim_cdr(it);
        }

        call_with_args(proc, args, env);
        for (i = 0; i < argc; ++i)
            arg_its[i] = minim_cdr(arg_its[i]);
    }

    return minim_void;
}

minim_object *map_list(minim_object *proc, minim_object *lsts, minim_object *env) {
    minim_object *head, *tail, *it, *result, *args;
    minim_object **lst_arr, **arg_its;
    minim_thread *th;
    long argc, i, len0, len;

    if (!minim_is_proc(proc))
        bad_type_exn("map", "procedure?", proc);

    i = 0;
    argc = list_length(lsts);
    lst_arr = GC_alloc(argc * sizeof(minim_object *));
    arg_its = GC_alloc(argc * sizeof(minim_object *));
    for (it = lsts; !minim_is_null(it); it = minim_cdr(it)) {
        lst_arr[i] = minim_car(it);
        if (!is_list(lst_arr[i]))
            bad_type_exn("map", "list?", lst_arr[i]);

        if (i == 0) {
            len0 = list_length(lst_arr[i]);
        } else {
            len = list_length(lst_arr[i]);
            if (len != len0) {
                fprintf(stderr, "map: lists of different lengths\n");
                fprintf(stderr, "  one list: %ld, second list: %ld\n", len0, len);
                exit(1);
            }
        }

        arg_its[i] = lst_arr[i];
        ++i;
    }

    head = NULL;
    while (!minim_is_null(arg_its[0])) {
        args = make_pair(minim_car(arg_its[0]), minim_null);
        it = args;
        for (i = 1; i < argc; ++i) {
            minim_cdr(it) = make_pair(minim_car(arg_its[i]), minim_null);
            it = minim_cdr(it);
        }

        result = call_with_args(proc, args, env);
        if (minim_is_values(result)) {
            th = current_thread();
            if (values_buffer_count(th) != 1) {
                fprintf(stderr, "result arity mismatch\n");
                fprintf(stderr, "  expected: 1, received: %d\n", values_buffer_count(th));
                exit(1);
            } else {
                result = values_buffer_ref(th, 0);
            }
        }

        if (head) {
            minim_cdr(tail) = make_pair(result, minim_null);
            tail = minim_cdr(tail);
        } else {
            head = make_pair(result, minim_null);
            tail = head;
        }

        for (i = 0; i < argc; ++i)
            arg_its[i] = minim_cdr(arg_its[i]);
    }

    return (head ? head : minim_null);
}

minim_object *andmap(minim_object *proc, minim_object *lst, minim_object *env) {
    if (!minim_is_proc(proc))
        bad_type_exn("andmap", "procedure?", proc);

    while (!minim_is_null(lst)) {
        if (!minim_is_pair(lst))
            bad_type_exn("andmap", "list?", lst);
        if (minim_is_false(call_with_args(proc, make_pair(minim_car(lst), minim_null), env)))
            return minim_false;
        lst = minim_cdr(lst);
    }

    return minim_true;
}

minim_object *ormap(minim_object *proc, minim_object *lst, minim_object *env) {
    if (!minim_is_proc(proc))
        bad_type_exn("ormap", "procedure?", proc);

    while (!minim_is_null(lst)) {
        if (!minim_is_pair(lst))
            bad_type_exn("ormap", "list?", lst);
        if (!minim_is_false(call_with_args(proc, make_pair(minim_car(lst), minim_null), env)))
            return minim_true;
        lst = minim_cdr(lst);
    }

    return minim_false;
}

//
//  Primitives
//

minim_object *is_pair_proc(minim_object *args) {
    // (-> any boolean)
    return minim_is_pair(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_list_proc(minim_object *args) {
    // (-> any boolean)
    minim_object *thing;
    for (thing = minim_car(args); minim_is_pair(thing); thing = minim_cdr(thing));
    return minim_is_null(thing) ? minim_true : minim_false;
}

minim_object *cons_proc(minim_object *args) {
    // (-> any any pair)
    return make_pair(minim_car(args), minim_cadr(args));
}

minim_object *car_proc(minim_object *args) {
    // (-> pair any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o))
        bad_type_exn("car", "pair?", o);
    return minim_car(o);
}

minim_object *cdr_proc(minim_object *args) {
    // (-> pair any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o))
        bad_type_exn("cdr", "pair?", o);
    return minim_cdr(o);
}

minim_object *caar_proc(minim_object *args) {
    // (-> (pairof pair any) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)))
        bad_type_exn("caar", "(pairof pair? any)", o);
    return minim_caar(o);
}

minim_object *cadr_proc(minim_object *args) {
    // (-> (pairof any pair) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)))
        bad_type_exn("cadr", "(pairof any pair)", o);
    return minim_cadr(o);
}

minim_object *cdar_proc(minim_object *args) {
    // (-> (pairof pair any) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)))
        bad_type_exn("cdar", "(pairof pair? any)", o);
    return minim_cdar(o);
}

minim_object *cddr_proc(minim_object *args) {
    // (-> (pairof any pair) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)))
        bad_type_exn("cddr", "(pairof any pair)", o);
    return minim_cddr(o);
}

minim_object *caaar_proc(minim_object *args) {
    // (-> (pairof (pairof pair any) any) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) || !minim_is_pair(minim_caar(o)))
        bad_type_exn("caaar", "(pairof (pairof pair? any) any)", o);
    return minim_car(minim_caar(o));
}

minim_object *caadr_proc(minim_object *args) {
    // (-> (pairof any (pairof pair any)) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) || !minim_is_pair(minim_cadr(o)))
        bad_type_exn("caadr", "(pairof any (pairof pair? any))", o);
    return minim_car(minim_cadr(o));
}

minim_object *cadar_proc(minim_object *args) {
    // (-> (pairof (pairof any pair) any) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) || !minim_is_pair(minim_cdar(o)))
        bad_type_exn("cadar", "(pairof (pairof any pair?) any)", o);
    return minim_car(minim_cdar(o));
}

minim_object *caddr_proc(minim_object *args) {
    // (-> (pairof any (pairof any pair)) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) || !minim_is_pair(minim_cddr(o)))
        bad_type_exn("caddr", "(pairof any (pairof any pair))", o);
    return minim_car(minim_cddr(o));
}

minim_object *cdaar_proc(minim_object *args) {
    // (-> (pairof (pairof pair any) any) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) || !minim_is_pair(minim_caar(o)))
        bad_type_exn("cdaar", "(pairof (pairof pair? any) any)", o);
    return minim_cdr(minim_caar(o));
}

minim_object *cdadr_proc(minim_object *args) {
    // (-> (pairof any (pairof pair any)) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) || !minim_is_pair(minim_cadr(o)))
        bad_type_exn("cdadr", "(pairof any (pairof pair? any))", o);
    return minim_cdr(minim_cadr(o));
}

minim_object *cddar_proc(minim_object *args) {
    // (-> (pairof (pairof any pair) any) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) || !minim_is_pair(minim_cdar(o)))
        bad_type_exn("cddar", "(pairof (pairof any pair?) any)", o);
    return minim_cdr(minim_cdar(o));
}

minim_object *cdddr_proc(minim_object *args) {
    // (-> (pairof any (pairof any pair)) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) || !minim_is_pair(minim_cddr(o)))
        bad_type_exn("cdddr", "(pairof any (pairof any pair?))", o);
    return minim_cdr(minim_cddr(o));
}

minim_object *caaaar_proc(minim_object *args) {
    // (-> (pairof (pairof (pairof pair any) any) any) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_caar(o)) || !minim_is_pair(minim_car(minim_caar(o))))
        bad_type_exn("caaaar", "(pairof (pairof (pairof pair? any) any) any)", o);
    return minim_caar(minim_caar(o));
}

minim_object *caaadr_proc(minim_object *args) {
    // (-> (pairof any (pairof (pairof pair any) any)) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cadr(o)) || !minim_is_pair(minim_car(minim_cadr(o))))
        bad_type_exn("caaadr", "(pairof any (pairof (pairof pair? any) any))", o);
    return minim_caar(minim_cadr(o));
}

minim_object *caadar_proc(minim_object *args) {
    // (-> (pairof (pairof any (pairof pair any)) any) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_cdar(o)) || !minim_is_pair(minim_car(minim_cdar(o))))
        bad_type_exn("caadar", "(pairof (pairof any (pairof pair? any)) any)", o);
    return minim_caar(minim_cdar(o));
}

minim_object *caaddr_proc(minim_object *args) {
    // (-> (pairof any (pairof any (pairof pair any))) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cddr(o)) ||  !minim_is_pair(minim_car(minim_cddr(o))))
        bad_type_exn("caaddr", "(pairof any (pairof any (pairof pair? any)))", o);
    return minim_caar(minim_cddr(o));
}

minim_object *cadaar_proc(minim_object *args) {
    // (-> (pairof (pairof (pairof any pair) any) any) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_caar(o)) || !minim_is_pair(minim_cdr(minim_caar(o))))
        bad_type_exn("cadaar", "(pairof (pairof (pairof any pair?) any) any)", o);
    return minim_cadr(minim_caar(o));
}

minim_object *cadadr_proc(minim_object *args) {
    // (-> (pairof any (pairof (pairof any pair) any)) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cadr(o)) || !minim_is_pair(minim_cdr(minim_cadr(o))))
        bad_type_exn("cadadr", "(pairof any (pairof (pairof any pair?) any))", o);
    return minim_cadr(minim_cadr(o));
}

minim_object *caddar_proc(minim_object *args) {
    // (-> (pairof (pairof any (pairof any pair)) any) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_cdar(o)) || !minim_is_pair(minim_cdr(minim_cdar(o))))
        bad_type_exn("caddar", "(pairof (pairof any (pairof any pair?) any)", o);
    return minim_cadr(minim_cdar(o));
}

minim_object *cadddr_proc(minim_object *args) {
    // (-> (pairof any (pairof any (pairof any pair))) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cddr(o)) || !minim_is_pair(minim_cdr(minim_cddr(o))))
        bad_type_exn("cadddr", "(pairof any (pairof any (pairof any pair?)))", o);
    return minim_cadr(minim_cddr(o));
}

minim_object *cdaaar_proc(minim_object *args) {
    // (-> (pairof (pairof (pairof pair any) any) any) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_caar(o)) || !minim_is_pair(minim_car(minim_caar(o))))
        bad_type_exn("cdaaar", "(pairof (pairof (pairof pair? any) any) any)", o);
    return minim_cdar(minim_caar(o));
}

minim_object *cdaadr_proc(minim_object *args) {
    // (-> (pairof any (pairof (pairof pair any) any)) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cadr(o)) || !minim_is_pair(minim_car(minim_cadr(o))))
        bad_type_exn("cdaadr", "(pairof any (pairof (pairof pair? any) any))", o);
    return minim_cdar(minim_cadr(o));
}

minim_object *cdadar_proc(minim_object *args) {
    // (-> (pairof (pairof any (pairof pair any)) any) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_cdar(o)) || !minim_is_pair(minim_car(minim_cdar(o))))
        bad_type_exn("cdadar", "(pairof (pairof any (pairof pair? any)) any)", o);
    return minim_cdar(minim_cdar(o));
}

minim_object *cdaddr_proc(minim_object *args) {
    // (-> (pairof any (pairof any (pairof pair any))) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cddr(o)) ||  !minim_is_pair(minim_car(minim_cddr(o))))
        bad_type_exn("cdaddr", "(pairof any (pairof any (pairof pair? any)))", o);
    return minim_cdar(minim_cddr(o));
}

minim_object *cddaar_proc(minim_object *args) {
    // (-> (pairof (pairof (pairof any pair) any) any) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_caar(o)) || !minim_is_pair(minim_cdr(minim_caar(o))))
        bad_type_exn("cddaar", "(pairof (pairof (pairof any pair?) any) any)", o);
    return minim_cddr(minim_caar(o));
}

minim_object *cddadr_proc(minim_object *args) {
    // (-> (pairof any (pairof (pairof any pair) any)) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cadr(o)) || !minim_is_pair(minim_cdr(minim_cadr(o))))
        bad_type_exn("cddadr", "(pairof any (pairof (pairof any pair?) any))", o);
    return minim_cddr(minim_cadr(o));
}

minim_object *cdddar_proc(minim_object *args) {
    // (-> (pairof (pairof any (pairof any pair)) any) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_car(o)) ||
        !minim_is_pair(minim_cdar(o)) || !minim_is_pair(minim_cdr(minim_cdar(o))))
        bad_type_exn("cdddar", "(pairof (pairof any (pairof any pair?) any)", o);
    return minim_cddr(minim_cdar(o));
}

minim_object *cddddr_proc(minim_object *args) {
    // (-> (pairof any (pairof any (pairof any pair))) any)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o) || !minim_is_pair(minim_cdr(o)) ||
        !minim_is_pair(minim_cddr(o)) || !minim_is_pair(minim_cdr(minim_cddr(o))))
        bad_type_exn("cddddr", "(pairof any (pairof any (pairof any pair?)))", o);
    return minim_cddr(minim_cddr(o));
}

minim_object *set_car_proc(minim_object *args) {
    // (-> pair any void)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o))
        bad_type_exn("set-car!", "pair?", o);
    minim_car(o) = minim_cadr(args);
    return minim_void;
}

minim_object *set_cdr_proc(minim_object *args) {
    // (-> pair any void)
    minim_object *o = minim_car(args);
    if (!minim_is_pair(o))
        bad_type_exn("set-cdr!", "pair?", o);
    minim_cdr(o) = minim_cadr(args);
    return minim_void;
}

minim_object *list_proc(minim_object *args) {
    // (-> any ... list)
    return args;
}

minim_object *length_proc(minim_object *args) {
    // (-> list non-negative-integer?)
    minim_object *it;
    long length;

    length = 0;
    for (it = minim_car(args); minim_is_pair(it); it = minim_cdr(it))
        length += 1;

    if (!minim_is_null(it))
        bad_type_exn("length", "list?", minim_car(args));
    return make_fixnum(length);
}

minim_object *reverse_proc(minim_object *args) {
    // (-> list list)
    minim_object *head, *it;
    
    head = minim_null;
    for (it = minim_car(args); minim_is_pair(it); it = minim_cdr(it))
        head = make_pair(minim_car(it), head);

    if (!minim_is_null(it))
        bad_type_exn("reverse", "expected list?", minim_car(args));
    return head;
}

minim_object *append_proc(minim_object *args) {
    // (-> list ... list)
    minim_object *head, *lst_it, *it, *it2;

    head = NULL;
    for (it = args; !minim_is_null(it); it = minim_cdr(it)) {
        if (!minim_is_null(minim_car(it))) {
            for (it2 = minim_car(it); minim_is_pair(it2); it2 = minim_cdr(it2)) {
                if (head) {
                    minim_cdr(lst_it) = make_pair(minim_car(it2), minim_null);
                    lst_it = minim_cdr(lst_it);
                } else {
                    head = make_pair(minim_car(it2), minim_null);
                    lst_it = head;
                }
            }

            if (!minim_is_null(it2))
                bad_type_exn("append", "expected list?", minim_car(it));
        }
    }

    return head ? head : minim_null;
}

minim_object *for_each_proc(minim_object *args) {
    // (-> proc list list ... list)
    fprintf(stderr, "andmap: should not be called directly");
    exit(1);
}

minim_object *map_proc(minim_object *args) {
    fprintf(stderr, "andmap: should not be called directly");
    exit(1);
}

minim_object *andmap_proc(minim_object *args) {
    fprintf(stderr, "andmap: should not be called directly");
    exit(1);
}

minim_object *ormap_proc(minim_object *args) {
    fprintf(stderr, "ormap: should not be called directly");
    exit(1);
}
