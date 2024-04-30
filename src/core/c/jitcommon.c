// jitcommon.c: JIT data structures

#include "../minim.h"

//
//  Global compiler environment
//  Represents a single compilation that may span multiple instances.
//

#define global_cenv_length          3
#define global_cenv_tmpls(c)        (minim_vector_ref(c, 0))
#define global_cenv_fvs(c)          (minim_vector_ref(c, 1))
#define global_cenv_bound(c)        (minim_vector_ref(c, 2))
#define global_cenv_num_tmpls(c)    (list_length(global_cenv_tmpls(c)))

mobj make_global_cenv() {
    mobj cenv = Mvector(global_cenv_length, NULL);
    global_cenv_tmpls(cenv) = minim_null;
    global_cenv_fvs(cenv) = minim_null;
    return cenv;
}

size_t global_cenv_add_template(mobj cenv, mobj jit) {
    size_t idx = global_cenv_num_tmpls(cenv);
    global_cenv_tmpls(cenv) = Mcons(jit, global_cenv_tmpls(cenv));
    return idx;
}

mobj global_cenv_ref_template(mobj cenv, size_t i) {
    mobj tmpls;
    size_t j;
    
    tmpls = list_reverse(global_cenv_tmpls(cenv));
    for (j = i; j > 0; j--) {
        if (minim_nullp(tmpls))
            minim_error1("cenv_template_ref", "index out of bounds", Mfixnum(i));
        tmpls = minim_cdr(tmpls);
    }

    return minim_car(tmpls);
}

void global_cenv_set_fvs(mobj cenv, mobj fvs) {
    global_cenv_fvs(cenv) = fvs;
}

mobj global_cenv_get_fvs(mobj cenv, mobj e) {
    mobj cell = assq_ref(global_cenv_fvs(cenv), e);
    return minim_falsep(cell) ? minim_null : minim_cdr(cell);
}

void global_cenv_set_bound(mobj cenv, mobj bound) {
    global_cenv_bound(cenv) = bound;
}

mobj global_cenv_get_bound(mobj cenv, mobj e) {
    mobj cell = assq_ref(global_cenv_bound(cenv), e);
    return minim_falsep(cell) ? minim_null : minim_cdr(cell);
}

//
//  Procedure-level compiler enviornment
//  Represents a single procedure
//

#define cenv_length         3
#define cenv_global(c)      (minim_vector_ref(c, 0))
#define cenv_labels(c)      (minim_vector_ref(c, 1))
#define cenv_fvs(c)         (minim_vector_ref(c, 2))

mobj make_cenv(mobj global_cenv) {
    mobj cenv = Mvector(cenv_length, NULL);
    cenv_global(cenv) = global_cenv;
    cenv_labels(cenv) = Mbox(minim_null);
    cenv_fvs(cenv) = minim_null;
    return cenv;
}

mobj cenv_global_env(mobj cenv) {
    return cenv_global(cenv);
}

mobj cenv_make_label(mobj cenv) {
    mobj label_box, label;
    size_t num_labels, len;
    
    // unbox list of labels
    label_box = cenv_labels(cenv);
    num_labels = list_length(minim_unbox(label_box));

    // create new label
    len = snprintf(NULL, 0, "%ld", num_labels);
    label = Mstring2(len + 1, 0);
    minim_string(label)[0] = 'L';
    snprintf(&minim_string(label)[1], len + 1, "%ld", num_labels);
    
    // update list
    minim_unbox(label_box) = Mcons(label, minim_unbox(label_box));
    return label;
}

void cenv_set_fvs(mobj cenv, mobj fvs) {
    cenv_fvs(cenv) = fvs;
}

//
//  Scope-level environment
//  Represents the current compile-time scope
//

#define scope_cenv_length       2
#define scope_cenv_proc(c)      (minim_vector_ref(c, 0))
#define scope_cenv_bound(c)     (minim_vector_ref(c, 1))

mobj make_scope_cenv(mobj proc_cenv) {
    mobj cenv = Mvector(scope_cenv_length, NULL);
    scope_cenv_proc(cenv) = proc_cenv;
    scope_cenv_bound(cenv) = minim_null;
    return cenv;
}

mobj scope_cenv_extend(mobj cenv) {
    mobj cenv2 = Mvector(scope_cenv_length, NULL);
    scope_cenv_proc(cenv2) = scope_cenv_proc(cenv);
    scope_cenv_bound(cenv2) = copy_list(scope_cenv_bound(cenv));
    return cenv2;
}

mobj scope_cenv_proc_env(mobj cenv) {
    return scope_cenv_proc(cenv);
}

mobj scope_cenv_global_env(mobj cenv) {
    return cenv_global(scope_cenv_proc(cenv));
}

mobj scope_cenv_make_label(mobj cenv) {
    return cenv_make_label(scope_cenv_proc(cenv));
}

size_t scope_cenv_bind(mobj cenv, mobj id) {
    size_t idx = list_length(scope_cenv_bound(cenv));
    scope_cenv_bound(cenv) = Mcons(id, scope_cenv_bound(cenv));
    return idx;
}

mobj scope_cenv_ref(mobj cenv, mobj id) {
    
}

//
//  Resolver
//

static mobj next_non_label(mobj ins) {
    for (ins = minim_cdr(ins); !minim_nullp(ins); ins = minim_cdr(ins)) {
        if (!minim_stringp(minim_car(ins)))
            return minim_car(ins);
    }

    minim_error("next_non_label", "no next instruction");
}

mobj resolve_refs(mobj cenv, mobj ins) {
    mobj reloc, label_map;

    reloc = minim_null;
    label_map = minim_null;

    // build unionfind of labels and eliminate redundant ones
    for (mobj it = ins; !minim_nullp(it); it = minim_cdr(it)) {
        mobj in = minim_car(it);
        if (minim_stringp(in)) {
            // label found (first one is the leading one)
            reloc = assq_set(reloc, in, next_non_label(it));
            label_map = assq_set(label_map, in, in);
            // subsequent labels
            for (; minim_stringp(minim_cadr(it)); it = minim_cdr(it))
                label_map = assq_set(label_map, minim_cadr(it), in);
        }
    }

    // resolve closures and labels
    for (mobj it = ins; !minim_nullp(it); it = minim_cdr(it)) {
        mobj in = minim_car(it);
        if (minim_car(in) == brancha_symbol) {
            // brancha: need to replace the label with the next instruction
            minim_cadr(in) = minim_cdr(assq_ref(label_map, minim_cadr(in)));
        } else if (minim_car(in) == branchf_symbol) {
            // branchf: need to replace the label with the next instruction
            minim_cadr(in) = minim_cdr(assq_ref(label_map, minim_cadr(in)));
        } else if (minim_car(in) == branchgt_symbol) {
            // branchgt: need to replace the label with the next instruction
            minim_car(minim_cddr(in)) = minim_cdr(assq_ref(label_map, minim_car(minim_cddr(in))));
        } else if (minim_car(in) == branchlt_symbol) {
            // branchlt: need to replace the label with the next instruction
            minim_car(minim_cddr(in)) = minim_cdr(assq_ref(label_map, minim_car(minim_cddr(in))));
        } else if (minim_car(in) == branchne_symbol) {
            // branchne: need to replace the label with the next instruction
            minim_car(minim_cddr(in)) = minim_cdr(assq_ref(label_map, minim_car(minim_cddr(in))));
        } else if (minim_car(in) == make_closure_symbol) {
            // closure: need to lookup JIT object to embed
            minim_cadr(in) = global_cenv_ref_template(cenv_global(cenv), minim_fixnum(minim_cadr(in)));
        } else if (minim_car(in) == save_cc_symbol) {
            // save-cc: need to replace the label with the next instruction
            minim_cadr(in) = minim_cdr(assq_ref(label_map, minim_cadr(in)));
        }
    }

    return reloc;   
}
