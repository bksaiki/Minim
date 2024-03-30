// jitcommon.c: JIT data structures

#include "../minim.h"

//
//  Compiler environments
//

#define cenv_length         2
#define cenv_labels(c)      (minim_vector_ref(c, 0))
#define cenv_tmpls(c)       (minim_vector_ref(c, 1))
#define cenv_num_tmpls(c)   list_length(minim_unbox(cenv_tmpls(c)))

mobj make_cenv() {
    mobj cenv = Mvector(cenv_length, NULL);
    cenv_labels(cenv) = Mbox(minim_null);
    cenv_tmpls(cenv) = Mbox(minim_null);
    return cenv;
}

mobj extend_cenv(mobj cenv) {
    mobj cenv2 = Mvector(cenv_length, NULL);
    cenv_labels(cenv2) = cenv_labels(cenv);
    cenv_tmpls(cenv2) = cenv_tmpls(cenv);
    return cenv2;
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

mobj cenv_template_add(mobj cenv, mobj jit) {
    mobj tmpl_box;
    size_t num_tmpls;

    // unbox list of templates
    tmpl_box = cenv_tmpls(cenv);
    num_tmpls = list_length(minim_unbox(tmpl_box));

    // update list
    minim_unbox(tmpl_box) = Mcons(jit, minim_unbox(tmpl_box));
    return Mfixnum(num_tmpls);
}

mobj cenv_template_ref(mobj cenv, size_t i) {
    mobj tmpls;
    size_t j;
    
    tmpls = list_reverse(minim_unbox(cenv_tmpls(cenv)));
    for (j = i; j > 0; j--) {
        if (minim_nullp(tmpls))
            minim_error1("cenv_template_ref", "index out of bounds", Mfixnum(i));
        tmpls = minim_cdr(tmpls);
    }

    return minim_car(tmpls);
}

//
//  Resolver
//

mobj resolve_refs(mobj cenv, mobj ins) {
    mobj reloc, label_map;

    reloc = minim_null;
    label_map = minim_null;

    // build unionfind of labels and eliminate redundant ones
    for (mobj it = ins; !minim_nullp(it); it = minim_cdr(it)) {
        mobj in = minim_car(it);
        if (minim_stringp(in)) {
            // label found (first one is the leading one)
            mobj l0 = in;
            reloc = assq_set(reloc, l0, minim_cadr(it));
            label_map = assq_set(label_map, l0, l0);
            // subsequent labels
            for (; minim_stringp(minim_cadr(it)); it = minim_cdr(it))
                label_map = assq_set(label_map, minim_cadr(it), l0);
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
            minim_cadr(in) = cenv_template_ref(cenv, minim_fixnum(minim_cadr(in)));
        } else if (minim_car(in) == save_cc_symbol) {
            // save-cc: need to replace the label with the next instruction
            minim_cadr(in) = minim_cdr(assq_ref(label_map, minim_cadr(in)));
        }
    }

    // eliminate labels (assumes head is not a label)
    for (; !minim_nullp(minim_cdr(ins)); ins = minim_cdr(ins)) {
        if (minim_stringp(minim_cadr(ins)))
            minim_cdr(ins) = minim_cddr(ins);
    }

    return reloc;   
}
