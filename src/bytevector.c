/**
 * @file
 * Bytevectors.
 */

#include "char.h"
#include "prims.h"

static minim_obj prim_make_bytevector(minim_obj *args, size_t n) {
    check(minim_is_fixnum(args[0]) && minim_fixnum_value(args[0]) >= 0,
          "make-bytevector: bad length", args[0]);
    uint8_t b = n == 2 ? byte_arg(args[1], "make-bytevector: not a byte") : 0;
    return minim_make_bytevector((size_t)minim_fixnum_value(args[0]), b);
}

static minim_obj prim_is_bytevector(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_bytevector(args[0]));
}

static minim_obj prim_bytevector_length(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_bytevector(args[0]), "bytevector-length: not a bytevector", args[0]);
    return minim_fixnum((intptr_t)minim_bytevector_length(args[0]));
}

static minim_obj prim_bytevector_u8_ref(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_bytevector(args[0]), "bytevector-u8-ref: not a bytevector", args[0]);
    size_t i = index_arg(args[1], minim_bytevector_length(args[0]),
                         "bytevector-u8-ref: bad index");
    return minim_fixnum(minim_bytevector_data(args[0])[i]);
}

static minim_obj prim_bytevector_u8_set(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_bytevector(args[0]), "bytevector-u8-set!: not a bytevector", args[0]);
    size_t i = index_arg(args[1], minim_bytevector_length(args[0]),
                         "bytevector-u8-set!: bad index");
    minim_bytevector_data(args[0])[i] = byte_arg(args[2], "bytevector-u8-set!: not a byte");
    return MINIM_VOID;
}

// The string the bytes of args[0] in the range args[1] and args[2] encode in
// UTF-8; a malformed sequence decodes to U+FFFD.
static minim_obj prim_utf8_to_string(minim_obj *args, size_t n) {
    check(minim_is_bytevector(args[0]), "utf8->string: not a bytevector", args[0]);
    range r = range_args(args, n, 1, minim_bytevector_length(args[0]),
                         "utf8->string: bad index");
    size_t len = r.end - r.start;
    const uint8_t *bytes = minim_bytevector_data(args[0]) + r.start;
    uint32_t c;
    size_t count = 0;
    for (size_t i = 0; i < len; count++)
        i += minim_utf8_decode(bytes + i, len - i, &c);

    minim_obj s = minim_make_string(count, 0);
    bytes = minim_bytevector_data(args[0]) + r.start;  // the allocation may have moved it
    uint32_t *out = minim_string_chars(s);
    for (size_t i = 0; i < len;)
        i += minim_utf8_decode(bytes + i, len - i, out++);
    return s;
}

// The UTF-8 encoding of the chars of the string args[0] in the range args[1]
// and args[2].
static minim_obj prim_string_to_utf8(minim_obj *args, size_t n) {
    check(minim_is_string(args[0]), "string->utf8: not a string", args[0]);
    range r = range_args(args, n, 1, minim_string_length(args[0]), "string->utf8: bad index");
    unsigned char buf[4];
    size_t len = 0;
    for (size_t i = r.start; i < r.end; i++)
        len += minim_utf8_encode(minim_string_ref(args[0], i), buf);
    minim_obj bv = minim_make_bytevector(len, 0);
    uint8_t *out = minim_bytevector_data(bv);
    for (size_t i = r.start; i < r.end; i++)
        out += minim_utf8_encode(minim_string_ref(args[0], i), out);
    return bv;
}

const minim_prim_def minim_bytevector_prims[] = {
    {"make-bytevector", prim_make_bytevector, 1, 2},
    {"bytevector?", prim_is_bytevector, 1, 1},
    {"bytevector-length", prim_bytevector_length, 1, 1},
    {"bytevector-u8-ref", prim_bytevector_u8_ref, 2, 2},
    {"bytevector-u8-set!", prim_bytevector_u8_set, 3, 3},
    {"utf8->string", prim_utf8_to_string, 1, 3},
    {"string->utf8", prim_string_to_utf8, 1, 3},
    {NULL, NULL, 0, 0},
};
