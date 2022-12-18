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

minim_object *andmap_proc(minim_object *args) {
    fprintf(stderr, "andmap: should not be called directly");
    exit(1);
}

minim_object *ormap_proc(minim_object *args) {
    fprintf(stderr, "ormap: should not be called directly");
    exit(1);
}
