/**
 * @file
 * Tests for primitives that golden tests can't reach.
 */

#include "minim.h"
#include "test.h"

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
#ifdef MINIM_GC_STRESS
    const size_t depth = 1000;  // every allocation collects
#else
    const size_t depth = 1000000;
#endif
    minim_obj a = nest(minim_fixnum(1), depth);
    minim_obj b = MINIM_NIL;
    minim_obj c = MINIM_NIL;
    MINIM_FRAME(&a, &b, &c);
    b = nest(minim_fixnum(1), depth);
    c = nest(minim_fixnum(2), depth);

    minim_obj equal = minim_symbol_value(minim_intern_cstr("equal?"));
    CHECK_EQ(call2(equal, a, b), MINIM_TRUE);
    CHECK_EQ(call2(equal, a, c), MINIM_FALSE);
    MINIM_RETURN_VOID;
}

int main(void) {
    minim_init();
    test_equal_deep();
    minim_shutdown();
    return test_result();
}
