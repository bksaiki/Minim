/*
    Pairs, lists
*/

#include "../minim.h"

mobj Mcons(mobj car, mobj cdr) {
    mobj o = GC_alloc(minim_cons_size);
    minim_type(o) = MINIM_OBJ_PAIR;
    minim_car(o) = car;
    minim_cdr(o) = cdr;
    return o;
}

// Returns true if the object is a list
int minim_listp(mobj x) {
    for (; minim_consp(x); x = minim_cdr(x));
    return minim_nullp(x);
}

long list_length(mobj xs) {
    long len = 0;
    for (mobj it = xs; !minim_nullp(it); it = minim_cdr(it)) {
        if (!minim_consp(it)) {
            fprintf(stderr, "list_length: not a list");
            minim_shutdown(1);
        }
        ++len;
    }

    return len;
}

mobj list_reverse(mobj xs) {
    mobj v = minim_null;
    while (!minim_nullp(xs)) {
        v = Mcons(minim_car(xs), v);
        xs = minim_cdr(xs);
    }

    return v;
}

long improper_list_length(mobj xs) {
    long len = 0;
    for (; minim_consp(xs); xs = minim_cdr(xs))
        ++len;
    return len;
}

// Makes an association list.
// Unsafe: only iterates on `xs`.
mobj make_assoc(mobj xs, mobj ys) {
    mobj assoc, it;

    if (minim_nullp(xs))
        return minim_null;

    assoc = Mcons(Mcons(minim_car(xs), minim_car(ys)), minim_null);
    it = assoc;
    while (!minim_nullp(xs = minim_cdr(xs))) {
        ys = minim_cdr(ys);
        minim_cdr(it) = Mcons(Mcons(minim_car(xs), minim_car(ys)), minim_null);
        it = minim_cdr(it);
    }

    return assoc;
}

// Copies a list.
// Unsafe: does not check if `xs` is a list.
mobj copy_list(mobj xs) {
    mobj head, tail, it;

    if (minim_nullp(xs))
        return minim_null;

    head = Mcons(minim_car(xs), minim_null);
    tail = head;
    it = xs;

    while (!minim_nullp(it = minim_cdr(it))) {
        minim_cdr(tail) = Mcons(minim_car(it), minim_null);
        tail = minim_cdr(tail);
    }
    
    return head;
}

void for_each(mobj proc, int argc, mobj *args, mobj env) {
    mobj *lsts;
    long len0, len;
    int stashc, i;

    if (!minim_procp(proc))
        bad_type_exn("for-each", "procedure?", proc);

    for (i = 0; i < argc; ++i) {
        if (!minim_listp(args[i]))
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
    lsts = GC_alloc(argc * sizeof(mobj));
    memcpy(lsts, args, argc * sizeof(mobj));

    stashc = stash_call_args();
    while (!minim_nullp(lsts[0])) {
        for (i = 0; i < argc; ++i) {
            push_call_arg(minim_car(lsts[i]));
            lsts[i] = minim_cdr(lsts[i]);
        }

        // ignore the result (side-effect only)
        call_with_args(proc, env);
    }

    prepare_call_args(stashc);
}

mobj map_list(mobj proc, int argc, mobj *args, mobj env) {
    mobj *lsts, res, head, tail;
    minim_thread *th;
    long len0, len;
    int stashc, i;

    if (!minim_procp(proc))
        bad_type_exn("map", "procedure?", proc);

    for (i = 0; i < argc; ++i) {
        if (!minim_listp(args[i]))
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
    lsts = GC_alloc(argc * sizeof(mobj));
    memcpy(lsts, args, argc * sizeof(mobj));

    head = NULL;
    stashc = stash_call_args();
    while (!minim_nullp(lsts[0])) {
        for (i = 0; i < argc; ++i) {
            push_call_arg(minim_car(lsts[i]));
            lsts[i] = minim_cdr(lsts[i]);
        }

        res = call_with_args(proc, env);
        if (minim_valuesp(res)) {
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

mobj andmap(mobj proc, int argc, mobj *args, mobj env) {
    mobj *lsts;
    long len0, len;
    int stashc, i;

    if (!minim_procp(proc))
        bad_type_exn("andmap", "procedure?", proc);

    for (i = 0; i < argc; ++i) {
        if (!minim_listp(args[i]))
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
    lsts = GC_alloc(argc * sizeof(mobj));
    memcpy(lsts, args, argc * sizeof(mobj));

    stashc = stash_call_args();
    while (!minim_nullp(lsts[0])) {
        for (i = 0; i < argc; ++i) {
            push_call_arg(minim_car(lsts[i]));
            lsts[i] = minim_cdr(lsts[i]);
        }

        // only check for false (early exit)
        if (minim_falsep(call_with_args(proc, env))) {
            prepare_call_args(stashc);
            return minim_false;
        }
    }

    prepare_call_args(stashc);
    return minim_true;
}

mobj ormap(mobj proc, int argc, mobj *args, mobj env) {
    mobj *lsts;
    long len0, len;
    int stashc, i;

    if (!minim_procp(proc))
        bad_type_exn("ormap", "procedure?", proc);

    for (i = 0; i < argc; ++i) {
        if (!minim_listp(args[i]))
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
    lsts = GC_alloc(argc * sizeof(mobj));
    memcpy(lsts, args, argc * sizeof(mobj));

    stashc = stash_call_args();
    while (!minim_nullp(lsts[0])) {
        for (i = 0; i < argc; ++i) {
            push_call_arg(minim_car(lsts[i]));
            lsts[i] = minim_cdr(lsts[i]);
        }

        // only check for false (early exit)
        if (!minim_falsep(call_with_args(proc, env))) {
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

mobj consp_proc(mobj x) {
    return minim_consp(x) ? minim_true : minim_false;
}

mobj listp_proc(mobj x) {
    return minim_listp(x) ? minim_true : minim_false;
}

mobj car_proc(mobj x) {
    // (-> pair any)
    return minim_car(x);
}

mobj cdr_proc(mobj x) {
    // (-> pair any)
    return minim_cdr(x);
}

mobj caar_proc(mobj x) {
    // (-> (pairof pair any) any)
    return minim_caar(x);
}

mobj cadr_proc(mobj x) {
    // (-> (pairof any pair) any)
    return minim_cadr(x);
}

mobj cdar_proc(mobj x) {
    // (-> (pairof pair any) any)
    return minim_cdar(x);
}

mobj cddr_proc(mobj x) {
    // (-> (pairof any pair) any)
    return minim_cddr(x);
}

mobj caaar_proc(mobj x) {
    // (-> (pairof (pairof pair any) any) any)
    return minim_car(minim_caar(x));
}

mobj caadr_proc(mobj x) {
    // (-> (pairof any (pairof pair any)) any)
    return minim_car(minim_cadr(x));
}

mobj cadar_proc(mobj x) {
    // (-> (pairof (pairof any pair) any) any)
    return minim_car(minim_cdar(x));
}

mobj caddr_proc(mobj x) {
    // (-> (pairof any (pairof any pair)) any)
    return minim_car(minim_cddr(x));
}

mobj cdaar_proc(mobj x) {
    // (-> (pairof (pairof pair any) any) any)
    return minim_cdr(minim_caar(x));
}

mobj cdadr_proc(mobj x) {
    // (-> (pairof any (pairof pair any)) any)
    return minim_cdr(minim_cadr(x));
}

mobj cddar_proc(mobj x) {
    // (-> (pairof (pairof any pair) any) any)
    return minim_cdr(minim_cdar(x));
}

mobj cdddr_proc(mobj x) {
    // (-> (pairof any (pairof any pair)) any)
    return minim_cdr(minim_cddr(x));
}

mobj caaaar_proc(int argc, mobj *args) {
    // (-> (pairof (pairof (pairof pair any) any) any) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_car(o)) ||
        !minim_consp(minim_caar(o)) || !minim_consp(minim_car(minim_caar(o))))
        bad_type_exn("caaaar", "(pairof (pairof (pairof pair? any) any) any)", o);
    return minim_caar(minim_caar(o));
}

mobj caaadr_proc(int argc, mobj *args) {
    // (-> (pairof any (pairof (pairof pair any) any)) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_cdr(o)) ||
        !minim_consp(minim_cadr(o)) || !minim_consp(minim_car(minim_cadr(o))))
        bad_type_exn("caaadr", "(pairof any (pairof (pairof pair? any) any))", o);
    return minim_caar(minim_cadr(o));
}

mobj caadar_proc(int argc, mobj *args) {
    // (-> (pairof (pairof any (pairof pair any)) any) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_car(o)) ||
        !minim_consp(minim_cdar(o)) || !minim_consp(minim_car(minim_cdar(o))))
        bad_type_exn("caadar", "(pairof (pairof any (pairof pair? any)) any)", o);
    return minim_caar(minim_cdar(o));
}

mobj caaddr_proc(int argc, mobj *args) {
    // (-> (pairof any (pairof any (pairof pair any))) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_cdr(o)) ||
        !minim_consp(minim_cddr(o)) ||  !minim_consp(minim_car(minim_cddr(o))))
        bad_type_exn("caaddr", "(pairof any (pairof any (pairof pair? any)))", o);
    return minim_caar(minim_cddr(o));
}

mobj cadaar_proc(int argc, mobj *args) {
    // (-> (pairof (pairof (pairof any pair) any) any) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_car(o)) ||
        !minim_consp(minim_caar(o)) || !minim_consp(minim_cdr(minim_caar(o))))
        bad_type_exn("cadaar", "(pairof (pairof (pairof any pair?) any) any)", o);
    return minim_cadr(minim_caar(o));
}

mobj cadadr_proc(int argc, mobj *args) {
    // (-> (pairof any (pairof (pairof any pair) any)) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_cdr(o)) ||
        !minim_consp(minim_cadr(o)) || !minim_consp(minim_cdr(minim_cadr(o))))
        bad_type_exn("cadadr", "(pairof any (pairof (pairof any pair?) any))", o);
    return minim_cadr(minim_cadr(o));
}

mobj caddar_proc(int argc, mobj *args) {
    // (-> (pairof (pairof any (pairof any pair)) any) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_car(o)) ||
        !minim_consp(minim_cdar(o)) || !minim_consp(minim_cdr(minim_cdar(o))))
        bad_type_exn("caddar", "(pairof (pairof any (pairof any pair?) any)", o);
    return minim_cadr(minim_cdar(o));
}

mobj cadddr_proc(int argc, mobj *args) {
    // (-> (pairof any (pairof any (pairof any pair))) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_cdr(o)) ||
        !minim_consp(minim_cddr(o)) || !minim_consp(minim_cdr(minim_cddr(o))))
        bad_type_exn("cadddr", "(pairof any (pairof any (pairof any pair?)))", o);
    return minim_cadr(minim_cddr(o));
}

mobj cdaaar_proc(int argc, mobj *args) {
    // (-> (pairof (pairof (pairof pair any) any) any) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_car(o)) ||
        !minim_consp(minim_caar(o)) || !minim_consp(minim_car(minim_caar(o))))
        bad_type_exn("cdaaar", "(pairof (pairof (pairof pair? any) any) any)", o);
    return minim_cdar(minim_caar(o));
}

mobj cdaadr_proc(int argc, mobj *args) {
    // (-> (pairof any (pairof (pairof pair any) any)) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_cdr(o)) ||
        !minim_consp(minim_cadr(o)) || !minim_consp(minim_car(minim_cadr(o))))
        bad_type_exn("cdaadr", "(pairof any (pairof (pairof pair? any) any))", o);
    return minim_cdar(minim_cadr(o));
}

mobj cdadar_proc(int argc, mobj *args) {
    // (-> (pairof (pairof any (pairof pair any)) any) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_car(o)) ||
        !minim_consp(minim_cdar(o)) || !minim_consp(minim_car(minim_cdar(o))))
        bad_type_exn("cdadar", "(pairof (pairof any (pairof pair? any)) any)", o);
    return minim_cdar(minim_cdar(o));
}

mobj cdaddr_proc(int argc, mobj *args) {
    // (-> (pairof any (pairof any (pairof pair any))) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_cdr(o)) ||
        !minim_consp(minim_cddr(o)) ||  !minim_consp(minim_car(minim_cddr(o))))
        bad_type_exn("cdaddr", "(pairof any (pairof any (pairof pair? any)))", o);
    return minim_cdar(minim_cddr(o));
}

mobj cddaar_proc(int argc, mobj *args) {
    // (-> (pairof (pairof (pairof any pair) any) any) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_car(o)) ||
        !minim_consp(minim_caar(o)) || !minim_consp(minim_cdr(minim_caar(o))))
        bad_type_exn("cddaar", "(pairof (pairof (pairof any pair?) any) any)", o);
    return minim_cddr(minim_caar(o));
}

mobj cddadr_proc(int argc, mobj *args) {
    // (-> (pairof any (pairof (pairof any pair) any)) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_cdr(o)) ||
        !minim_consp(minim_cadr(o)) || !minim_consp(minim_cdr(minim_cadr(o))))
        bad_type_exn("cddadr", "(pairof any (pairof (pairof any pair?) any))", o);
    return minim_cddr(minim_cadr(o));
}

mobj cdddar_proc(int argc, mobj *args) {
    // (-> (pairof (pairof any (pairof any pair)) any) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_car(o)) ||
        !minim_consp(minim_cdar(o)) || !minim_consp(minim_cdr(minim_cdar(o))))
        bad_type_exn("cdddar", "(pairof (pairof any (pairof any pair?) any)", o);
    return minim_cddr(minim_cdar(o));
}

mobj cddddr_proc(int argc, mobj *args) {
    // (-> (pairof any (pairof any (pairof any pair))) any)
    mobj *o = args[0];
    if (!minim_consp(o) || !minim_consp(minim_cdr(o)) ||
        !minim_consp(minim_cddr(o)) || !minim_consp(minim_cdr(minim_cddr(o))))
        bad_type_exn("cddddr", "(pairof any (pairof any (pairof any pair?)))", o);
    return minim_cddr(minim_cddr(o));
}

mobj set_car_proc(int argc, mobj *args) {
    // (-> pair any void)
    mobj *o = args[0];
    if (!minim_consp(o))
        bad_type_exn("set-car!", "pair?", o);
    minim_car(o) = args[1];
    return minim_void;
}

mobj set_cdr_proc(int argc, mobj *args) {
    // (-> pair any void)
    mobj *o = args[0];
    if (!minim_consp(o))
        bad_type_exn("set-cdr!", "pair?", o);
    minim_cdr(o) = args[1];
    return minim_void;
}

mobj list_proc(int argc, mobj *args) {
    // (-> any ... list)
    mobj *lst;
    int i;

    lst = minim_null;
    for (i = argc - 1; i >= 0; --i)
        lst = Mcons(args[i], lst);

    return lst;
}

mobj make_list_proc(int argc, mobj *args) {
    // (-> non-negative-integer? any list)
    mobj *lst;
    long len, i;

    if (!minim_fixnump(args[0]) || minim_fixnum(args[0]) < 0)
        bad_type_exn("make-vector", "non-negative-integer?", args[0]);
    len = minim_fixnum(args[0]);

    lst = minim_null;
    for (i = 0; i < len; ++i)
        lst = Mcons(args[1], lst);

    return lst;
}

mobj length_proc(int argc, mobj *args) {
    // (-> list non-negative-integer?)
    mobj *it;
    long length;

    length = 0;
    for (it = args[0]; minim_consp(it); it = minim_cdr(it))
        length += 1;

    if (!minim_nullp(it))
        bad_type_exn("length", "list?", args[0]);

    return Mfixnum(length);
}

mobj reverse_proc(int argc, mobj *args) {
    // (-> list list)
    mobj *head, *it;
    
    head = minim_null;
    for (it = args[0]; minim_consp(it); it = minim_cdr(it))
        head = Mcons(minim_car(it), head);

    if (!minim_nullp(it))
        bad_type_exn("reverse", "expected list?", args[0]);
    return head;
}

mobj append_proc(int argc, mobj *args) {
    // (-> list ... list)
    mobj *head, *lst_it, *it;
    int i;

    head = NULL;
    for (i = 0; i < argc; ++i) {
        if (!minim_nullp(args[i])) {
            for (it = args[i]; minim_consp(it); it = minim_cdr(it)) {
                if (head) {
                    minim_cdr(lst_it) = Mcons(minim_car(it), minim_null);
                    lst_it = minim_cdr(lst_it);
                } else {
                    head = Mcons(minim_car(it), minim_null);
                    lst_it = head;
                }
            }

            if (!minim_nullp(it))
                bad_type_exn("append", "expected list?", args[i]);
        }
    }

    return head ? head : minim_null;
}

mobj for_each_proc(int argc, mobj *args) {
    // (-> proc list list ... list)
    uncallable_prim_exn("for-each");
}

mobj map_proc(int argc, mobj *args) {
    uncallable_prim_exn("map");
}

mobj andmap_proc(int argc, mobj *args) {
    uncallable_prim_exn("andmap");
}

mobj ormap_proc(int argc, mobj *args) {
    uncallable_prim_exn("ormap");
}
