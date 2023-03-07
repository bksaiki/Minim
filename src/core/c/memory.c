/*
    Memory utilities
*/

#include "../minim.h"

typedef minim_object *(*entry_proc)(minim_object *env);

struct _address_map_t {
    char *name;
    void *fn;
};

struct _address_map_t address_map[] = {
    { "env_set_var", env_set_var },
    { "env_lookup_var", env_lookup_var },
    { "make_closure", make_native_closure },
    { "", NULL }
};

//
//  Runtime
//

minim_object *call_compiled(minim_object *env, minim_object *addr) {
    entry_proc fn;

    if (!minim_is_fixnum(addr))
        bad_type_exn("enter-compiled!", "fixnum?", addr);

    // TODO: set up stack segment

    // very unsafe code is to follow
    fn = ((entry_proc) minim_fixnum(addr));
    return fn(env);
}

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

minim_object *reinstall_proc_bundle_proc(int argc, minim_object **args) {
    minim_object *it, *it2;
    char *code;
    long size, offset;

    if (!minim_is_fixnum(args[0]))
        bad_type_exn("reinstall-procedure-bundle!", "address", args[0]);

    if (!is_list(args[1]))
        bad_type_exn("reinstall-procedure-bundle!", "list of lists of integers", args[1]);

    // compute size
    for (size = 0, it = args[1]; !minim_is_null(it); it = minim_cdr(it)) {
        if (!is_list(minim_car(it)))
            bad_type_exn("install-procedure-bundle!", "list of lists of integers", args[0]);

        for (it2 = minim_car(it); !minim_is_null(it2); it2 = minim_cdr(it2)) {
            if (!minim_is_fixnum(minim_car(it2)))
                bad_type_exn("install-procedure-bundle!", "list of lists of integers", args[0]);
        }

        size += list_length(minim_car(it));
    }

    // mark bundle as write only
    code = ((char *) minim_fixnum(args[0]));
    make_page_write_only(code, size);

    // overwrite bundle
    for (offset = 0, it = args[1]; !minim_is_null(it); it = minim_cdr(it)) {
        for (it2 = minim_car(it); !minim_is_null(it2); it2 = minim_cdr(it2)) {
            // TODO: unsafe, need byte strings
            code[offset++] = ((char) minim_fixnum(minim_car(it2)));
        }
    }
    
    // mark bundle as executable
    make_page_executable(code, size);

    return minim_void;
}

minim_object *runtime_address_proc(int argc, minim_object **args) {
    char *str;
    int i;

    if (!minim_is_string(args[0]))
        bad_type_exn("runtime-address", "expected a string", args[0]);

    str = minim_string(args[0]);
    for (i = 0; address_map[i].fn != NULL; ++i) {
        if (strcmp(str, address_map[i].name) == 0)
            return make_fixnum((long) address_map[i].fn);
    }
    
    fprintf(stderr, "runtime-address: unknown runtime name\n");
    fprintf(stderr, " name: \"%s\"\n", str);
    exit(1);
}

minim_object *enter_compiled_proc(int argc, minim_object **args) {
    uncallable_prim_exn("enter-compiled!");
}
