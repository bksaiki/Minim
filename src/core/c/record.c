/*
    Records
*/

#include "../minim.h"

minim_object *make_record(minim_object *rtd, int fieldc) {
    minim_record_object *o = GC_alloc(sizeof(minim_record_object));
    o->type = MINIM_RECORD_TYPE;
    o->rtd = rtd;
    o->fields = GC_calloc(fieldc, sizeof(minim_object*));
    o->fieldc = fieldc;
    return ((minim_object *) o);
}

//
//
//

minim_object *is_record_proc(int argc, minim_object **args) {
    // (-> any boolean)
    minim_object *rtd;

    if (!minim_is_record(args[0]))
        return minim_false;

    rtd = minim_record_rtd(args[0]);
    return (record_rtd_opaque(rtd) == minim_false) ? minim_true : minim_false;
}

minim_object *is_record_rtd_proc(int argc, minim_object **args) {
    // (-> any boolean)
    return minim_is_record_rtd(args[0]) ? minim_true : minim_false;
}

static void make_rtd_field_exn(const char *reason, minim_object *field, int index) {
    fprintf(stderr, "make-record-type-descriptor: invalid field descriptor\n");
    fprintf(stderr, " why: %s\n", reason);
    fprintf(stderr, " field desciptor: ");
    write_object(stderr, field);
    fprintf(stderr, "\n");
    fprintf(stderr, " index: %d\n", index);
    exit(1);
}

minim_object *make_rtd_proc(int argc, minim_object **args) {
    // (-> symbol (or rtd #f) (or symbol #f) boolean boolean vector rtd)
    minim_object *rtd, *field, *field_it;
    int fieldc, mutable, name_len, field_len, i;
    char *acc_name, *mut_name;

    // validate input types
    if (!minim_is_symbol(args[0]))
        bad_type_exn("make-record-type-descriptor", "symbol?", args[0]);
    if (!minim_is_record_rtd(args[1]) && !minim_is_false(args[1]))
        bad_type_exn("make-record-type-descriptor", "rtd? or #f", args[1]);
    if (!minim_is_record_rtd(args[2]) && !minim_is_false(args[2]))
        bad_type_exn("make-record-type-descriptor", "rtd? or #f", args[2]);
    if (!minim_is_bool(args[3]))
        bad_type_exn("make-record-type-descriptor", "boolean?", args[3]);
    if (!minim_is_bool(args[4]))
        bad_type_exn("make-record-type-descriptor", "boolean?", args[4]);
    if (!minim_is_vector(args[5]))
        bad_type_exn("make-record-type-descriptor", "vector?", args[5]);

    // check number of fields
    fieldc = minim_vector_len(args[5]);
    if (fieldc + record_rtd_min_size > RECORD_FIELD_MAX) {
        fprintf(stderr, "make-record-type-descriptor: too many fields provided\n");
        fprintf(stderr, " rtd name: %s\n", minim_symbol(args[0]));
        fprintf(stderr, " field count: %d\n", fieldc);
        exit(1);
    }

    // allocate record type
    rtd = make_record(minim_base_rtd, fieldc + record_rtd_min_size);
    record_rtd_name(rtd) = args[0];
    record_rtd_parent(rtd) = args[1];
    record_rtd_uid(rtd) = args[2];
    record_rtd_opaque(rtd) = args[3];
    record_rtd_sealed(rtd) = args[4];
    record_rtd_protocol(rtd) = minim_false;

    // validate fields
    for (i = 0; i < fieldc; ++i) {
        // need to validate field descriptors
        // see `minim.h` for desguaring rules
        //
        //  '(immutable <name> <accessor-name>)
        //  '(mutable <name> <accessor-name> <mutator-name>)
        //  '(immutable <name>)
        //  '(mutable <name>)
        //  <name>
        //
        field = minim_vector_ref(args[5], i);
        
        // <name> (symbol?) => '(immutable <name>)
        if (minim_is_symbol(field)) {
            field = make_pair(intern("immutable"), make_pair(field, minim_null));
        }

        // should be a list now
        if (!minim_is_pair(field))
            make_rtd_field_exn("expected a list", field, i);

        // check type first (either 'immutable or 'mutable)
        field_it = minim_car(field);
        if (!minim_is_symbol(field_it))
            make_rtd_field_exn("expected 'immutable or 'mutable", field, i);

        if (minim_is_eq(field_it, intern("immutable"))) {
            mutable = 0;
        } else if (minim_is_eq(field_it, intern("mutable"))) {
            mutable = 1;
        } else {
            make_rtd_field_exn("expected 'immutable or 'mutable", field, i);
        }

        // expecting at least one more entry
        field_it = minim_cdr(field);
        if (!minim_is_pair(field_it))
            make_rtd_field_exn("expected a list", field, i);
        if (!minim_is_symbol(minim_car(field_it)))
            make_rtd_field_exn("expected an identitifer after field type", field, i);

        // '(immutable <name>) ==> '(immutable <name> <rtd-name>-<name>)
        // '(mutable <name>) ==> '(mutable <name> <rtd-name>-<name> <rtd-name>-<name>-set!)
        if (minim_is_null(minim_cdr(field_it))) {
            name_len = strlen(minim_symbol(record_rtd_name(rtd)));
            field_len = strlen(minim_symbol(minim_car(field_it)));

            // make accessor name
            acc_name = GC_alloc_atomic((name_len + field_len + 2) * sizeof(char));
            sprintf(
                acc_name, "%s-%s",    
                minim_symbol(record_rtd_name(rtd)),
                minim_symbol(minim_car(field_it))
            );

            if (mutable) {
                // make mutator name
                mut_name = GC_alloc_atomic((name_len + field_len + 7) * sizeof(char));  
                sprintf(
                    mut_name, "%s-%s-set!",    
                    minim_symbol(record_rtd_name(rtd)),
                    minim_symbol(minim_car(field_it))
                );

                minim_cdr(field) = make_pair(intern(acc_name), make_pair(intern(mut_name), minim_null));
            } else {
                minim_cdr(field) = make_pair(intern(acc_name), minim_null);
            }
        } else {
            // expecting exactly one more entry
            field_it = minim_cdr(field_it);
            if (!minim_is_pair(field_it))
                make_rtd_field_exn("expected a list", field, i);
            if (!minim_is_symbol(minim_car(field_it)))
                make_rtd_field_exn("expected an identitifer after field type", field, i);
            if (!minim_is_null(minim_cdr(field_it)))
                make_rtd_field_exn("too many entries in field descriptor", field, i);
        }

        // field descriptor is valid
        minim_record_ref(rtd, i + record_rtd_min_size) = field;
    }

    return rtd;
}
