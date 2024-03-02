/*
    Records
*/

#include "../minim.h"

mobj Mrecord(mobj rtd, int fieldc) {
    mobj o = GC_alloc(minim_record_size(fieldc));
    minim_type(o) = MINIM_OBJ_RECORD;
    minim_record_rtd(o) = rtd;
    minim_record_count(o) = fieldc;
    return o;
}

// Record type descriptors are records, but they
// return `#f` when passed to `record?`.
static int true_recordp(mobj o) {
    mobj *rtd;

    if (!minim_recordp(o))
        return 0;

    rtd = minim_record_rtd(o);
    return record_rtd_opaque(rtd) == minim_false;
}

// Returns 1 when `o` is a record value, and 0 otherwise.
int record_valuep(mobj o) {
    return minim_recordp(o) && minim_record_rtd(o) != minim_base_rtd;
}

// Returns 1 when `o` is a record type descriptor, and 0 otherwise.
int record_rtdp(mobj o) {
    return minim_recordp(o) && minim_record_rtd(o) == minim_base_rtd;
}

//
//
//

mobj is_record_proc(int argc, mobj *args) {
    // (-> any boolean)
    return true_recordp(args[0]) ? minim_true : minim_false;
}

mobj is_record_rtd_proc(int argc, mobj *args) {
    // (-> any boolean)
    return record_rtdp(args[0]) ? minim_true : minim_false;
}

mobj is_record_value_proc(int argc, mobj *args) {
    // (-> any boolean)
    return record_valuep(args[0]) ? minim_true : minim_false;
}

static void make_rtd_field_exn(const char *reason, mobj *field, int index) {
    fprintf(stderr, "make-record-type-descriptor: invalid field descriptor\n");
    fprintf(stderr, " why: %s\n", reason);
    fprintf(stderr, " field desciptor: ");
    write_object(stderr, field);
    fprintf(stderr, "\n");
    fprintf(stderr, " index: %d\n", index);
    exit(1);
}

mobj make_rtd_proc(int argc, mobj *args) {
    // (-> symbol (or rtd #f) (or symbol #f) boolean boolean vector rtd)
    mobj rtd, field, field_it;
    int fieldc, i;

    // validate input types
    if (!minim_symbolp(args[0]))
        bad_type_exn("make-record-type-descriptor", "symbol?", args[0]);
    if (!record_rtdp(args[1]) && !minim_falsep(args[1]))
        bad_type_exn("make-record-type-descriptor", "rtd? or #f", args[1]);
    if (!minim_symbolp(args[2]) && !minim_falsep(args[2]))
        bad_type_exn("make-record-type-descriptor", "rtd? or #f", args[2]);
    if (!minim_boolp(args[3]))
        bad_type_exn("make-record-type-descriptor", "boolean?", args[3]);
    if (!minim_boolp(args[4]))
        bad_type_exn("make-record-type-descriptor", "boolean?", args[4]);
    if (!minim_vectorp(args[5]))
        bad_type_exn("make-record-type-descriptor", "vector?", args[5]);

    // check number of fields
    fieldc = minim_vector_len(args[5]);
    if (fieldc + record_rtd_min_size > RECORD_FIELD_MAX) {
        fprintf(stderr, "make-record-type-descriptor: too many fields provided\n");
        fprintf(stderr, " record type: %s\n", minim_symbol(args[0]));
        fprintf(stderr, " field count: %d\n", fieldc);
        exit(1);
    }

    // check if parent is sealed
    if (!minim_falsep(args[1]) && !minim_falsep(record_rtd_sealed(args[1]))) {
        fprintf(stderr, "make-record-type-descriptor: cannot extend sealed record type\n");
        fprintf(stderr, " record type: %s\n", minim_symbol(args[0]));
        fprintf(stderr, " parent: %s\n", minim_symbol(record_rtd_name(args[1])));
        exit(1);
    }

    // allocate record type
    rtd = Mrecord(minim_base_rtd, fieldc + record_rtd_min_size);
    record_rtd_name(rtd) = args[0];
    record_rtd_parent(rtd) = args[1];
    record_rtd_uid(rtd) = args[2];
    record_rtd_sealed(rtd) = args[3];
    record_rtd_opaque(rtd) = args[4];
    record_rtd_protocol(rtd) = minim_false;

    // validate fields
    for (i = 0; i < fieldc; ++i) {
        // need to validate field descriptors
        //
        //  '(immutable <name>)
        //  '(mutable <name>)
        //
        field = minim_vector_ref(args[5], i);
        
        // should be a list
        if (!minim_consp(field))
            make_rtd_field_exn("expected a list", field, i);

        // check type first (either 'immutable or 'mutable)
        field_it = minim_car(field);
        if (!minim_symbolp(field_it))
            make_rtd_field_exn("expected 'immutable or 'mutable", field, i);

        if (!minim_eqp(field_it, intern("immutable")) &&
            !minim_eqp(field_it, intern("mutable"))) {
            make_rtd_field_exn("expected 'immutable or 'mutable", field, i);
        }

        // expecting exactly one more entry
        field_it = minim_cdr(field);
        if (!minim_consp(field_it))
            make_rtd_field_exn("expected a list", field, i);
        if (!minim_symbolp(minim_car(field_it)))
            make_rtd_field_exn("expected an identitifer after field type", field, i);
        if (!minim_nullp(minim_cdr(field_it)))
            make_rtd_field_exn("too many entries in field descriptor", field, i);

        // field descriptor is valid
        minim_record_ref(rtd, i + record_rtd_min_size) = field;
    }

    return rtd;
}

mobj record_type_name_proc(int argc, mobj *args) {
    // (-> rtd symbol)
    if (!record_rtdp(args[0]))
        bad_type_exn("record-type-name", "record-type-descriptor?", args[0]);
    
    return record_rtd_name(args[0]);
}

mobj record_type_parent_proc(int argc, mobj *args) {
    // (-> rtd (or symbol #f))
    if (!record_rtdp(args[0]))
        bad_type_exn("record-type-parent", "record-type-descriptor?", args[0]);
    
    return record_rtd_parent(args[0]);
}

mobj record_type_uid_proc(int argc, mobj *args) {
    // (-> rtd (or symbol #f))
    if (!record_rtdp(args[0]))
        bad_type_exn("record-type-uid", "record-type-descriptor?", args[0]);
    
    return record_rtd_uid(args[0]);
}

mobj record_type_opaque_proc(int argc, mobj *args) {
    // (-> rtd boolean?)
    if (!record_rtdp(args[0]))
        bad_type_exn("record-type-opaque", "record-type-descriptor?", args[0]);
    
    return record_rtd_opaque(args[0]);
}

mobj record_type_sealed_proc(int argc, mobj *args) {
    // (-> rtd boolean?)
    if (!record_rtdp(args[0]))
        bad_type_exn("record-type-sealed", "record-type-descriptor?", args[0]);
    
    return record_rtd_sealed(args[0]);
}

mobj record_type_fields_proc(int argc, mobj *args) {
    // (-> rtd (vector symbol))
    mobj fields;
    int fieldc, i;
    
    if (!record_rtdp(args[0]))
        bad_type_exn("record-type-fields", "record-type-descriptor?", args[0]);
    
    fieldc = minim_record_count(args[0]) - record_rtd_min_size;
    if (fieldc == 0)
        return minim_empty_vec;
    
    fields = Mvector(fieldc, NULL);
    for (i = 0; i < fieldc; ++i) {
        minim_vector_ref(fields, i) = minim_cadr(record_rtd_field(args[0], i));
    }

    return fields;
}

mobj record_type_field_mutable_proc(int argc, mobj *args) {
    // (-> rtd (vector symbol))
    mobj rtd;
    int fieldc, idx;
    
    if (!record_rtdp(args[0]))
        bad_type_exn("record-type-field-mutable", "record-type-descriptor?", args[0]);
    if (!minim_fixnump(args[1]))   // TODO: positive-integer?
        bad_type_exn("record-type-field-mutable", "integer?", args[0]);

    rtd = args[0];
    idx = minim_fixnum(args[1]);
    
    fieldc = minim_record_count(rtd) - record_rtd_min_size;
    if (idx < 0 || fieldc < idx) {
        fprintf(stderr, "record-type-field-mutable: index out of bounds\n");
        fprintf(stderr, " field count: %d\n", fieldc);
        fprintf(stderr, " index: %d\n", idx);
        exit(1);
    }
    
    return (minim_car(record_rtd_field(rtd, idx)) == intern("mutable")) ? minim_true : minim_false;
}

mobj Mrecord_proc(int argc, mobj *args) {
    // (-> rtd any ...)
    mobj rtd, rec;
    int fieldc, i;

    if (!record_rtdp(args[0]))
        bad_type_exn("Mrecord", "record-type-descriptor?", args[0]);
    
    rtd = args[0];
    fieldc = minim_record_count(rtd) - record_rtd_min_size;
    while (record_rtd_parent(rtd) != minim_false) {
        rtd = record_rtd_parent(rtd);
        fieldc += minim_record_count(rtd) - record_rtd_min_size;
    }

    if (argc - 1 != fieldc) {
        fprintf(stderr, "$make-record: arity mismatch\n");
        fprintf(stderr, " record type: ");
        write_object(stderr, args[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, " field count: %d\n", fieldc);
        fprintf(stderr, " fields provided: %d\n", argc - 1);
        exit(1);
    }

    rec = Mrecord(args[0], fieldc);
    for (i = 0; i < fieldc; ++i) {
        minim_record_ref(rec, i) = args[i + 1];
    }

    return rec;
}

mobj record_rtd_proc(int argc, mobj *args) {
    // (-> record rtd)
    if (!record_valuep(args[0]))
        bad_type_exn("$record-rtd", "record?", args[0]);
    return minim_record_rtd(args[0]);
}

mobj record_ref_proc(int argc, mobj *args) {
    // (-> record idx any)
    int fieldc, idx;

    if (!record_valuep(args[0]))
        bad_type_exn("$record-ref", "record?", args[0]);
    if (!minim_fixnump(args[1]))   // TODO: positive-integer?
        bad_type_exn("$record-ref", "integer?", args[0]);

    fieldc = minim_record_count(args[0]);
    idx = minim_fixnum(args[1]);
    if (idx < 0 || fieldc < idx) {
        fprintf(stderr, "$record-ref: index out of bounds\n");
        fprintf(stderr, " field count: %d\n", fieldc);
        fprintf(stderr, " index: %d\n", idx);
        exit(1);
    }

    return minim_record_ref(args[0], idx);
}

mobj record_set_proc(int argc, mobj *args) {
    // (-> record idx any void)
    int fieldc, idx;

    if (!record_valuep(args[0]))
        bad_type_exn("$record-ref", "record?", args[0]);
    if (!minim_fixnump(args[1]))   // TODO: positive-integer?
        bad_type_exn("$record-ref", "integer?", args[0]);

    fieldc = minim_record_count(args[0]);
    idx = minim_fixnum(args[1]);
    if (idx < 0 || fieldc < idx) {
        fprintf(stderr, "$record-ref: index out of bounds\n");
        fprintf(stderr, " field count: %d\n", fieldc);
        fprintf(stderr, " index: %d\n", idx);
        exit(1);
    }

    minim_record_ref(args[0], idx) = args[2];
    return minim_void;
}

mobj default_record_equal_procedure_proc(int argc, mobj *args) {
    // (-> any any proc boolean)
    if (record_valuep(args[0]) && record_valuep(args[1])) {
        return minim_eqp(args[0], args[1]) ? minim_true : minim_false;
    } else {
        return minim_equalp(args[0], args[1]) ? minim_true : minim_false;
    }
}

mobj default_record_hash_procedure_proc(int argc, mobj *args) {
    // (-> any proc boolean)
    if (record_valuep(args[0])) {
        return eq_hash_proc(1, args);
    } else {
        return equal_hash_proc(1, args);
    }
}

mobj default_record_write_procedure_proc(int argc, mobj *args) {
    // (-> any proc boolean)
    mobj o, quote, display;
    FILE *out;

    o = args[0];
    out = minim_port(args[1]);
    quote = minim_car(args[2]);
    display = minim_cdr(args[2]);
    if (record_valuep(o)) {
        fprintf(out, "#<%s>", minim_symbol(minim_record_ref(minim_record_rtd(o), 0)));
    } else {
        write_object2(out, o, minim_fixnum(quote), minim_fixnum(display));
    }

    return minim_void;
}

mobj current_record_equal_procedure_proc(int argc, mobj *args) {
    // (-> proc)
    // (-> proc void)
    if (argc == 0) {
        return record_equal_proc(current_thread());
    } else {
        if (!minim_procp(args[0]))
            bad_type_exn("current-record-equal-procedure", "procedure?", args[0]);
        record_equal_proc(current_thread()) = args[0];
        return minim_void;
    }
}

mobj current_record_hash_procedure_proc(int argc, mobj *args) {
    // (-> proc)
    // (-> proc void)
    if (argc == 0) {
        return record_hash_proc(current_thread());
    } else {
        if (!minim_procp(args[0]))
            bad_type_exn("current-record-hash-procedure", "procedure?", args[0]);
        record_hash_proc(current_thread()) = args[0];
        return minim_void;
    }
}

mobj current_record_write_procedure_proc(int argc, mobj *args) {
    // (-> proc)
    // (-> proc void)
    if (argc == 0) {
        return record_write_proc(current_thread());
    } else {
        if (!minim_procp(args[0]))
            bad_type_exn("current-record-write-procedure", "procedure?", args[0]);
        record_write_proc(current_thread()) = args[0];
        return minim_void;
    }
}
