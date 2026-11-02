/**
 * @file
 * Tests for primitives and prelude procedures on inputs too large for golden
 * tests.
 */

#include "minim.h"
#include "test.h"

// The primitive named `name`.
static minim_obj primitive(const char *name) {
    return minim_symbol_value(minim_intern_cstr(name));
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

// `map` in the prelude recurses on the heap, not the C stack.
static void test_map_long_list(void) {
    const long n = gc_stressed() ? 1000 : 100000;
    char src[256];
    snprintf(src, sizeof(src),
             "(define build (lambda (k acc) (if (= k 0) acc (build (- k 1) (cons k acc)))))"
             "(fold + 0 (map (lambda (x) (* 2 x)) (build %ld '())))", n);
    char expect[32];
    snprintf(expect, sizeof(expect), "%ld", n * (n + 1));  // 2 * (1 + ... + n)
    CHECK(evals_to(src, expect));
}

int main(void) {
    minim_init();
    test_equal_deep();
    test_map_long_list();
    minim_shutdown();
    return test_result();
}
