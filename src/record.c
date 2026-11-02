/**
 * @file
 * Records.
 */

#include "prims.h"

// A new rtd named by the symbol args[0], with the distinct symbols of the list
// args[1] as its fields.  The rtd's fields are set only here.
static minim_obj prim_make_record_type(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_symbol(args[0]), "%make-record-type: not a symbol", args[0]);
    intptr_t len = 0;
    minim_obj x = args[1];
    for (; minim_is_pair(x); x = minim_cdr(x), len++) {
        bool ok = minim_is_symbol(minim_car(x));
        minim_obj y = args[1];
        for (intptr_t i = 0; ok && i < len; i++, y = minim_cdr(y))
            ok = minim_car(y) != minim_car(x);  // a cycle repeats a field
        check(ok, "%make-record-type: bad fields", args[1]);
    }
    check(minim_is_null(x), "%make-record-type: bad fields", args[1]);
    minim_obj rtd = minim_make_record(minim_base_rtd);
    minim_record_set(rtd, MINIM_RTD_NAME, args[0]);
    minim_record_set(rtd, MINIM_RTD_NFIELDS, minim_fixnum(len));
    minim_record_set(rtd, MINIM_RTD_FIELDS, args[1]);
    return rtd;
}

static minim_obj prim_is_record(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_record(args[0]) && minim_record_rtd(args[0]) == args[1]);
}

static minim_obj prim_record_rtd(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_record(args[0]), "%record-rtd: not a record", args[0]);
    return minim_record_rtd(args[0]);
}

// Not the base rtd: an rtd's fields are set only by `%make-record-type`.
static minim_obj prim_make_record(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_record(args[0]) && minim_record_rtd(args[0]) == minim_base_rtd
          && args[0] != minim_base_rtd, "%make-record: not a record type", args[0]);
    return minim_make_record(args[0]);
}

static minim_obj prim_record_ref(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_record(args[0]), "%record-ref: not a record", args[0]);
    return minim_record_ref(args[0], index_arg(args[1], minim_record_length(args[0]),
                                               "%record-ref: bad index"));
}

static minim_obj prim_record_set(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_record(args[0]), "%record-set!: not a record", args[0]);
    check(minim_record_rtd(args[0]) != minim_base_rtd, "%record-set!: immutable record type",
          args[0]);
    size_t i = index_arg(args[1], minim_record_length(args[0]), "%record-set!: bad index");
    minim_record_set(args[0], i, args[2]);
    return MINIM_VOID;
}

const minim_prim_def minim_record_prims[] = {
    {"%make-record-type", prim_make_record_type, 2, 2},
    {"%record?", prim_is_record, 2, 2},
    {"%record-rtd", prim_record_rtd, 1, 1},
    {"%make-record", prim_make_record, 1, 1},
    {"%record-ref", prim_record_ref, 2, 2},
    {"%record-set!", prim_record_set, 3, 3},
    {NULL, NULL, 0, 0},
};
