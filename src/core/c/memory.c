/*
    Memory utilities
*/

#include "../minim.h"

typedef mobj *(*entry_proc)();

struct address_map_t {
    char *name;
    void *fn;
};

struct address_map_t address_map[] = {
    { "env_define_var", env_define_var },
    { "env_set_var", env_set_var },
    { "env_lookup_var", env_lookup_var },
    { "make_closure", make_native_closure },
    { "check_arity", check_native_closure_arity },
    { "make_rest_argument", make_rest_argument },
    { "make_environment", make_environment },
    { "", NULL }
};

//
//  Runtime
//

mobj *make_rest_argument(mobj *args[], short argc) {
    mobj *lst;
    
    lst = minim_null;
    for (short i = argc - 1; i >= 0; --i)
        lst = Mcons(args[i], lst);

    return lst;
}

void check_native_closure_arity(short argc, mobj *fn) {
    struct proc_arity *arity;
    int min_arity, max_arity;

    arity = &minim_native_closure_arity(fn);
    min_arity = proc_arity_min(arity);
    max_arity = proc_arity_max(arity);

    // TODO: better arity error
    if (argc > max_arity)
        arity_mismatch_exn(minim_native_closure_name(fn), arity, argc);

    if (argc < min_arity)
        arity_mismatch_exn(minim_native_closure_name(fn), arity, argc);
}

#ifdef MINIM_X86_64
static mobj *call_compiled_x86_64(entry_proc fn, mobj *env) {
    mobj *res;

    // stash all callee-preserved registers
    // move `env` to `%r14`

    __asm__ (
        "movq %1, %%r14\n\t"
        "call *%2\n\t"
        "mov %0, %%rax\n\t"
        : "=r" (res)
        : "r" (env), "r" (fn)
        : "%rbx", "%r12", "%r13", "%r14", "%r15"
    );

    return res;
}
#endif

mobj *call_compiled(mobj *env, mobj *addr) {
    entry_proc fn;

    if (!minim_fixnump(addr))
        bad_type_exn("enter-compiled!", "fixnum?", addr);

    // very unsafe code is to follow
    #ifdef MINIM_X86_64
        fn = ((entry_proc) minim_fixnum(addr));
        return call_compiled_x86_64(fn, env);
    #else
    #error architecure unsupported
    #endif
}

//
//  Primitives
//

mobj *install_literal_bundle_proc(int argc, mobj **args) {
    mobj **bundle, ***root, *it;
    long size, i;

    if (!is_list(args[0]))
        bad_type_exn("install-literal-bundle!", "list?", args[0]);

    // create bundle
    size = list_length(args[0]);
    bundle = GC_alloc(size * sizeof(mobj *));
    for (i = 0, it = args[0]; !minim_nullp(it); it = minim_cdr(it)) {
        bundle[i] = minim_car(it);
        ++i;
    }

    // create GC root
    root = GC_alloc(sizeof(mobj**));
    GC_register_root(root);

    return Mfixnum((long) bundle);
}

mobj *install_proc_bundle_proc(int argc, mobj **args) {
    mobj *it, *it2;
    char *code;
    long size, offset;

    if (!is_list(args[0]))
        bad_type_exn("install-procedure-bundle!", "list of lists of integers", args[0]);

    // compute size
    for (size = 0, it = args[0]; !minim_nullp(it); it = minim_cdr(it)) {
        if (!is_list(minim_car(it)))
            bad_type_exn("install-procedure-bundle!", "list of lists of integers", args[0]);

        for (it2 = minim_car(it); !minim_nullp(it2); it2 = minim_cdr(it2)) {
            if (!minim_fixnump(minim_car(it2)))
                bad_type_exn("install-procedure-bundle!", "list of lists of integers", args[0]);
        }

        size += list_length(minim_car(it));
    }

    // create bundle
    code = alloc_page(size);
    for (offset = 0, it = args[0]; !minim_nullp(it); it = minim_cdr(it)) {
        for (it2 = minim_car(it); !minim_nullp(it2); it2 = minim_cdr(it2)) {
            // TODO: unsafe, need byte strings
            code[offset++] = ((char) minim_fixnum(minim_car(it2)));
        }
    }

    // mark bundle as executable
    make_page_executable(code, size);

    return Mfixnum((long) code);
}

mobj *reinstall_proc_bundle_proc(int argc, mobj **args) {
    mobj *it, *it2;
    char *code;
    long size, offset;

    if (!minim_fixnump(args[0]))
        bad_type_exn("reinstall-procedure-bundle!", "address", args[0]);

    if (!is_list(args[1]))
        bad_type_exn("reinstall-procedure-bundle!", "list of lists of integers", args[1]);

    // compute size
    for (size = 0, it = args[1]; !minim_nullp(it); it = minim_cdr(it)) {
        if (!is_list(minim_car(it)))
            bad_type_exn("install-procedure-bundle!", "list of lists of integers", args[0]);

        for (it2 = minim_car(it); !minim_nullp(it2); it2 = minim_cdr(it2)) {
            if (!minim_fixnump(minim_car(it2)))
                bad_type_exn("install-procedure-bundle!", "list of lists of integers", args[0]);
        }

        size += list_length(minim_car(it));
    }

    // mark bundle as write only
    code = ((char *) minim_fixnum(args[0]));
    make_page_write_only(code, size);

    // overwrite bundle
    for (offset = 0, it = args[1]; !minim_nullp(it); it = minim_cdr(it)) {
        for (it2 = minim_car(it); !minim_nullp(it2); it2 = minim_cdr(it2)) {
            // TODO: unsafe, need byte strings
            code[offset++] = ((char) minim_fixnum(minim_car(it2)));
        }
    }
    
    // mark bundle as executable
    make_page_executable(code, size);

    return minim_void;
}

mobj *runtime_address_proc(int argc, mobj **args) {
    char *str;
    int i;

    if (!minim_stringp(args[0]))
        bad_type_exn("runtime-address", "expected a string", args[0]);
    str = minim_string(args[0]);

    // search constant addresses
    for (i = 0; address_map[i].fn != NULL; ++i) {
        if (strcmp(str, address_map[i].name) == 0)
            return Mfixnum((long) address_map[i].fn);
    }

    // search dynamic addresses
    if (strcmp(str, "minim_values") == 0) {
        return Mfixnum((long) minim_values);
    } else if (strcmp(str, "minim_void") == 0) {
        return Mfixnum((long) minim_void);
    } else if (strcmp(str, "current_thread") == 0) {
        return Mfixnum((long) current_thread());
    }
    
    fprintf(stderr, "runtime-address: unknown runtime name\n");
    fprintf(stderr, " name: \"%s\"\n", str);
    exit(1);
}

mobj *enter_compiled_proc(int argc, mobj **args) {
    uncallable_prim_exn("enter-compiled!");
}
