/*
    Memory utilities
*/

#include "../minim.h"

//
//  Primitives
//

minim_object *install_literal_bundle_proc(int argc, minim_object **args) {
    minim_object **bundle, ***root, *it;
    long size, i;

    if (!is_list(args[0]))
        bad_type_exn("install-literal-bundle!", "list?", args[0]);

    // create bundle
    size = list_length(args[0]);
    bundle = GC_alloc(size * sizeof(minim_object *));
    for (i = 0, it = args[0]; !minim_is_null(it); it = minim_cdr(it)) {
        bundle[i] = minim_car(it);
        ++i;
    }

    // create GC root
    root = GC_alloc(sizeof(minim_object**));
    GC_register_root(root);

    return make_fixnum((long) bundle);
}

minim_object *install_proc_bundle_proc(int argc, minim_object **args) {
    minim_object *it, *it2;
    char *code;
    long size, offset;

    if (!is_list(args[0]))
        bad_type_exn("install-procedure-bundle!", "list of lists of integers", args[0]);

    // compute size
    for (size = 0, it = args[0]; !minim_is_null(it); it = minim_cdr(it)) {
        if (!is_list(minim_car(it)))
            bad_type_exn("install-procedure-bundle!", "list of lists of integers", args[0]);

        for (it2 = minim_car(it); !minim_is_null(it2); it2 = minim_cdr(it2)) {
            if (!minim_is_fixnum(minim_car(it2)))
                bad_type_exn("install-procedure-bundle!", "list of lists of integers", args[0]);
        }

        size += list_length(minim_car(it));
    }

    // create bundle
    code = alloc_page(size);
    for (offset = 0, it = args[0]; !minim_is_null(it); it = minim_cdr(it)) {
        for (it2 = minim_car(it); !minim_is_null(it2); it2 = minim_cdr(it2)) {
            // TODO: unsafe, need byte strings
            code[offset++] = ((char) minim_fixnum(minim_car(it2)));
        }
    }

    // mark bundle as executable
    make_page_executable(code, size);

    return make_fixnum((long) code);
}
