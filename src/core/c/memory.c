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
    return minim_void;
}
