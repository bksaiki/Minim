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

// Returns length of a (possibly improper) llist.
long list_length(mobj xs) {
    long len = 0;
    for (; minim_consp(xs); xs = minim_cdr(xs), len++);
    return len;
}

mobj list_reverse(mobj xs) {
    mobj ys = minim_null;
    for (; !minim_nullp(xs); xs = minim_cdr(xs))
        ys = Mcons(minim_car(xs), ys);
    return ys;
}

void list_set_tail(mobj xs, mobj ys) {
    mobj t;

    if (!minim_consp(xs))
        bad_type_exn("list_set_tail()", "pair?", xs);
    
    for (t = xs; minim_consp(minim_cdr(t)); t = minim_cdr(t));
    minim_cdr(t) = ys;
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

mobj make_list_proc(mobj len, mobj init) {
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

mobj list_append2(mobj xs, mobj ys) {
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
