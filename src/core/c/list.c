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

mobj list_reverse(const mobj xs) {
    mobj v = minim_null;
    for (mobj t = xs; !minim_nullp(t); t = minim_cdr(t))
        v = Mcons(minim_car(t), v);
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

mobj caaaar_proc(mobj x) {
    // (-> (pairof (pairof (pairof pair any) any) any) any)
    return minim_caar(minim_caar(x));
}

mobj caaadr_proc(mobj x) {
    // (-> (pairof any (pairof (pairof pair any) any)) any)
    return minim_caar(minim_cadr(x));
}

mobj caadar_proc(mobj x) {
    // (-> (pairof (pairof any (pairof pair any)) any) any)
    return minim_caar(minim_cdar(x));
}

mobj caaddr_proc(mobj x) {
    // (-> (pairof any (pairof any (pairof pair any))) any)
    return minim_caar(minim_cddr(x));
}

mobj cadaar_proc(mobj x) {
    // (-> (pairof (pairof (pairof any pair) any) any) any)
    return minim_cadr(minim_caar(x));
}

mobj cadadr_proc(mobj x) {
    // (-> (pairof any (pairof (pairof any pair) any)) any)
    return minim_cadr(minim_cadr(x));
}

mobj caddar_proc(mobj x) {
    // (-> (pairof (pairof any (pairof any pair)) any) any)
    return minim_cadr(minim_cdar(x));
}

mobj cadddr_proc(mobj x) {
    // (-> (pairof any (pairof any (pairof any pair))) any)
    return minim_cadr(minim_cddr(x));
}

mobj cdaaar_proc(mobj x) {
    // (-> (pairof (pairof (pairof pair any) any) any) any)
    return minim_cdar(minim_caar(x));
}

mobj cdaadr_proc(mobj x) {
    // (-> (pairof any (pairof (pairof pair any) any)) any)
    return minim_cdar(minim_cadr(x));
}

mobj cdadar_proc(mobj x) {
    // (-> (pairof (pairof any (pairof pair any)) any) any)
    return minim_cdar(minim_cdar(x));
}

mobj cdaddr_proc(mobj x) {
    // (-> (pairof any (pairof any (pairof pair any))) any)
    return minim_cdar(minim_cddr(x));
}

mobj cddaar_proc(mobj x) {
    // (-> (pairof (pairof (pairof any pair) any) any) any)
    return minim_cddr(minim_caar(x));
}

mobj cddadr_proc(mobj x) {
    // (-> (pairof any (pairof (pairof any pair) any)) any)
    return minim_cddr(minim_cadr(x));
}

mobj cdddar_proc(mobj x) {
    // (-> (pairof (pairof any (pairof any pair)) any) any)
    return minim_cddr(minim_cdar(x));
}

mobj cddddr_proc(mobj x) {
    // (-> (pairof any (pairof any (pairof any pair))) any)
    return minim_cddr(minim_cddr(x));
}

mobj set_car_proc(mobj p, mobj x) {
    // (-> pair any void)
    minim_car(p) = x;
    return minim_void;
}

mobj set_cdr_proc(mobj p, mobj x) {
    // (-> pair any void)
    minim_cdr(p) = x;
    return minim_void;
}

mobj make_list_proc(const mobj len, const mobj init) {
    // (-> non-negative-integer? any list)
    mobj lst = minim_null;
    for (long i = 0; i < minim_fixnum(len); ++i)
        lst = Mcons(init, lst);
    return lst;
}

mobj length_proc(const mobj x) {
    // (-> list non-negative-integer?)
    return Mfixnum(list_length(x));
}

mobj list_append2(const mobj xs, const mobj ys) {
    // (-> list list list)
    mobj it, hd, tl;

    if (minim_nullp(xs)) {
        return ys;
    } else {
        hd = tl = Mcons(minim_car(xs), NULL);
        for (it = minim_cdr(xs); !minim_nullp(it); it = minim_cdr(it)) {
            minim_cdr(tl) = Mcons(minim_car(it), NULL);
            tl = minim_cdr(tl);
        }

        minim_cdr(tl) = ys;
        return hd;
    }
}
