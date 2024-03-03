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

mobj recordp_proc(mobj x) {
    // (-> any boolean)
    return true_recordp(x) ? minim_true : minim_false;
}

mobj record_rtdp_proc(mobj x) {
    // (-> any boolean)
    return record_rtdp(x) ? minim_true : minim_false;
}

mobj record_valuep_proc(mobj x) {
    // (-> any boolean)
    return record_valuep(x) ? minim_true : minim_false;
}

mobj make_rtd(mobj name, mobj parent, mobj uid, mobj sealedp, mobj opaquep, mobj fields) {
    // (-> symbol (or rtd #f) (or symbol #f) boolean boolean vector rtd)
    mobj rtd = Mrecord(minim_base_rtd, record_rtd_min_size + minim_vector_len(fields));
    record_rtd_name(rtd) = name;
    record_rtd_parent(rtd) = parent;
    record_rtd_uid(rtd) = uid;
    record_rtd_sealed(rtd) = sealedp;
    record_rtd_opaque(rtd) = opaquep;
    record_rtd_protocol(rtd) = minim_false;

    for (long i = 0; i < minim_vector_len(fields); i++)
        record_rtd_field(rtd, i) = minim_vector_ref(fields, i);
    return rtd;
}

mobj rtd_name(mobj rtd) {
    // (-> rtd symbol)
    return record_rtd_name(rtd);
}

mobj rtd_parent(mobj rtd) {
    // (-> rtd (or rtd #f))
    return record_rtd_parent(rtd);
}

mobj rtd_uid(mobj rtd) {
    // (-> rtd (or symbol #f))
    return record_rtd_uid(rtd);
}

mobj rtd_sealedp(mobj rtd) {
    // (-> rtd boolean)
    return record_rtd_sealed(rtd);
}

mobj rtd_opaquep(mobj rtd) {
    // (-> rtd boolean)
    return record_rtd_opaque(rtd);
}

mobj rtd_length(mobj rtd) {
    // (-> rtd integer)
    return Mfixnum(record_rtd_fieldc(rtd));
}

mobj rtd_fields(mobj rtd) {
    // (-> rtd (vectorof symbol))
    mobj fields = Mvector(record_rtd_fieldc(rtd), NULL);
    for (long i = 0; i < record_rtd_fieldc(rtd); i++)
        minim_vector_ref(fields, i) = minim_cadr(record_rtd_field(rtd, i));
    return fields;
}

mobj rtd_field_mutablep(mobj rtd, mobj idx) {
    // (-> rtd integer boolean)
    mobj fieldi = record_rtd_field(rtd, minim_fixnum(idx));
    return minim_car(fieldi) == intern("mutable") ? minim_true : minim_false;
}

mobj make_record(mobj rtd, mobj fields) {
    // (-> rtd list record)
    mobj rec;
    long len, i;

    len = list_length(fields);
    rec = Mrecord(rtd, len);
    for (i = 0; i < len; i++) {
        minim_record_ref(rec, i) = minim_car(fields);
        fields = minim_cdr(fields);
    }

    return rec;
}

mobj record_rtd_proc(mobj rec) {
    // (-> record rtd)
    return minim_record_rtd(rec);
}

mobj record_ref_proc(mobj rec, mobj idx) {
    // (-> record idx any)
    return minim_record_ref(rec, minim_fixnum(idx));
}

mobj record_set_proc(mobj rec, mobj idx, mobj x) {
    // (-> record idx any void)
    minim_record_ref(rec, minim_fixnum(idx)) = x;
    return minim_void;
}

// mobj Mrecord_proc(int argc, mobj *args) {
//     // (-> rtd any ...)
//     mobj rtd, rec;
//     int fieldc, i;

//     if (!record_rtdp(args[0]))
//         bad_type_exn("Mrecord", "record-type-descriptor?", args[0]);
    
//     rtd = args[0];
//     fieldc = minim_record_count(rtd) - record_rtd_min_size;
//     while (record_rtd_parent(rtd) != minim_false) {
//         rtd = record_rtd_parent(rtd);
//         fieldc += minim_record_count(rtd) - record_rtd_min_size;
//     }

//     if (argc - 1 != fieldc) {
//         fprintf(stderr, "$make-record: arity mismatch\n");
//         fprintf(stderr, " record type: ");
//         write_object(stderr, args[0]);
//         fprintf(stderr, "\n");
//         fprintf(stderr, " field count: %d\n", fieldc);
//         fprintf(stderr, " fields provided: %d\n", argc - 1);
//         exit(1);
//     }

//     rec = Mrecord(args[0], fieldc);
//     for (i = 0; i < fieldc; ++i) {
//         minim_record_ref(rec, i) = args[i + 1];
//     }

//     return rec;
// }

// mobj record_rtd_proc(int argc, mobj *args) {
//     // (-> record rtd)
//     if (!record_valuep(args[0]))
//         bad_type_exn("$record-rtd", "record?", args[0]);
//     return minim_record_rtd(args[0]);
// }

// mobj record_ref_proc(int argc, mobj *args) {
//     // (-> record idx any)
//     int fieldc, idx;

//     if (!record_valuep(args[0]))
//         bad_type_exn("$record-ref", "record?", args[0]);
//     if (!minim_fixnump(args[1]))   // TODO: positive-integer?
//         bad_type_exn("$record-ref", "integer?", args[0]);

//     fieldc = minim_record_count(args[0]);
//     idx = minim_fixnum(args[1]);
//     if (idx < 0 || fieldc < idx) {
//         fprintf(stderr, "$record-ref: index out of bounds\n");
//         fprintf(stderr, " field count: %d\n", fieldc);
//         fprintf(stderr, " index: %d\n", idx);
//         exit(1);
//     }

//     return minim_record_ref(args[0], idx);
// }

// mobj record_set_proc(int argc, mobj *args) {
//     // (-> record idx any void)
//     int fieldc, idx;

//     if (!record_valuep(args[0]))
//         bad_type_exn("$record-ref", "record?", args[0]);
//     if (!minim_fixnump(args[1]))   // TODO: positive-integer?
//         bad_type_exn("$record-ref", "integer?", args[0]);

//     fieldc = minim_record_count(args[0]);
//     idx = minim_fixnum(args[1]);
//     if (idx < 0 || fieldc < idx) {
//         fprintf(stderr, "$record-ref: index out of bounds\n");
//         fprintf(stderr, " field count: %d\n", fieldc);
//         fprintf(stderr, " index: %d\n", idx);
//         exit(1);
//     }

//     minim_record_ref(args[0], idx) = args[2];
//     return minim_void;
// }

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
