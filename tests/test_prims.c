/**
 * @file
 * Tests for primitives that golden tests can't reach.
 */

#include "minim.h"
#include "test.h"

// The primitive named `name`.
static minim_obj primitive(const char *name) {
    return minim_symbol_value(minim_intern_cstr(name));
}

// Calls the primitive `p` with `a`; `p` must not allocate.
static minim_obj call1(minim_obj p, minim_obj a) {
    return minim_primitive_fn(p)(&a, 1);
}

// Calls the primitive `p` with `a` and `b`; `p` must not allocate.
static minim_obj call2(minim_obj p, minim_obj a, minim_obj b) {
    minim_obj args[] = {a, b};
    return minim_primitive_fn(p)(args, 2);
}

// Returns `x` nested `depth` times in the car: `((... (x) ...))`.
static minim_obj nest(minim_obj x, size_t depth) {
    MINIM_FRAME(&x);
    for (size_t i = 0; i < depth; i++)
        x = minim_cons(x, MINIM_NIL);
    MINIM_RETURN(x);
}

// `equal?` uses an explicit stack, so depth that would overflow the C stack
// is fine.
static void test_equal_deep(void) {
    const size_t depth = gc_stressed() ? 1000 : 1000000;
    minim_obj a = nest(minim_fixnum(1), depth);
    minim_obj b = MINIM_NIL;
    minim_obj c = MINIM_NIL;
    MINIM_FRAME(&a, &b, &c);
    b = nest(minim_fixnum(1), depth);
    c = nest(minim_fixnum(2), depth);

    minim_obj equal = primitive("equal?");
    CHECK_EQ(call2(equal, a, b), MINIM_TRUE);
    CHECK_EQ(call2(equal, a, c), MINIM_FALSE);
    MINIM_RETURN_VOID;
}

// Only read and file errors, made in C, satisfy `read-error?` and `file-error?`.
static void test_error_kinds(void) {
    minim_obj msg = minim_string_from_cstr("bad");
    minim_obj e = MINIM_NIL;
    MINIM_FRAME(&msg, &e);
    for (int kind = MINIM_ERROR_GENERAL; kind <= MINIM_ERROR_FILE; kind++) {
        e = minim_make_error_object(kind, msg, MINIM_NIL);
        CHECK_EQ(minim_error_object_kind(e), kind);
        minim_obj p = primitive("error-object?");
        CHECK_EQ(call1(p, e), MINIM_TRUE);
        p = primitive("read-error?");
        CHECK_EQ(call1(p, e), kind == MINIM_ERROR_READ ? MINIM_TRUE : MINIM_FALSE);
        p = primitive("file-error?");
        CHECK_EQ(call1(p, e), kind == MINIM_ERROR_FILE ? MINIM_TRUE : MINIM_FALSE);
    }
    MINIM_RETURN_VOID;
}

int main(void) {
    minim_init();
    test_equal_deep();
    test_error_kinds();
    minim_shutdown();
    return test_result();
}
