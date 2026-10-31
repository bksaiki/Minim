/**
 * @file
 * Tests for closures and primitives.
 */

#include "minim.h"
#include "test.h"

// Returns its first argument plus its argument count.
static minim_obj first_plus_n(minim_obj *args, size_t n) {
    return minim_fixnum(minim_fixnum_value(args[0]) + (intptr_t)n);
}

// Number of kind predicates `x` satisfies
static int count_predicates(minim_obj x) {
    return minim_is_fixnum(x) + minim_is_pair(x) + minim_is_symbol(x)
        + minim_is_vector(x) + minim_is_string(x) + minim_is_closure(x)
        + minim_is_primitive(x) + minim_is_char(x) + minim_is_boolean(x)
        + minim_is_null(x);
}

static void test_closure(void) {
    minim_obj code = minim_cons(MINIM_NIL, MINIM_NIL);
    minim_obj env = MINIM_NIL;
    minim_obj c = MINIM_NIL;
    MINIM_FRAME(&code, &env, &c);
    env = minim_make_vector(1, minim_fixnum(7));
    c = minim_make_closure(code, env);

    CHECK(minim_is_closure(c));
    CHECK(minim_is_procedure(c));
    CHECK_EQ(count_predicates(c), 1);
    CHECK_EQ(minim_closure_code(c), code);
    CHECK_EQ(minim_closure_env(c), env);
    CHECK(prints_as(c, "#<procedure>"));
    MINIM_RETURN_VOID;
}

static void test_primitive(void) {
    minim_obj name = minim_intern_cstr("first+n");
    minim_obj p = MINIM_NIL;
    MINIM_FRAME(&name, &p);
    p = minim_make_primitive(name, first_plus_n, 1, MINIM_VARIADIC);

    CHECK(minim_is_primitive(p));
    CHECK(minim_is_procedure(p));
    CHECK_EQ(count_predicates(p), 1);
    CHECK_EQ(minim_primitive_name(p), name);
    CHECK_EQ(minim_primitive_min_arity(p), 1);
    CHECK_EQ(minim_primitive_max_arity(p), MINIM_VARIADIC);
    CHECK(prints_as(p, "#<procedure first+n>"));

    minim_obj args[] = {minim_fixnum(10), minim_fixnum(0)};
    CHECK_EQ(minim_primitive_fn(p)(args, 2), minim_fixnum(12));
    MINIM_RETURN_VOID;
}

static void test_not_procedures(void) {
    minim_obj v = minim_make_vector(0, MINIM_NIL);
    CHECK(!minim_is_procedure(v));
    CHECK(!minim_is_procedure(minim_fixnum(5)));
    CHECK(!minim_is_procedure(MINIM_NIL));
}

static void test_gc(void) {
    minim_obj code = minim_cons(minim_fixnum(1), minim_fixnum(2));
    minim_obj c = MINIM_NIL;
    minim_obj p = MINIM_NIL;
    MINIM_FRAME(&code, &c, &p);
    c = minim_make_closure(code, MINIM_NIL);
    code = MINIM_NIL;  // only `c` keeps the code alive
    p = minim_intern_cstr("f");
    p = minim_make_primitive(p, first_plus_n, 1, 3);

    minim_obj old_c = c;
    minim_obj old_p = p;
    minim_gc_collect();
    CHECK(c != old_c);  // moved
    CHECK(p != old_p);

    code = minim_closure_code(c);
    CHECK_EQ(minim_car(code), minim_fixnum(1));
    CHECK_EQ(minim_cdr(code), minim_fixnum(2));
    CHECK_EQ(minim_closure_env(c), MINIM_NIL);

    minim_obj f = minim_intern_cstr("f");  // may collect, so before reading `p`
    CHECK_EQ(minim_primitive_name(p), f);
    CHECK_EQ(minim_primitive_min_arity(p), 1);
    CHECK_EQ(minim_primitive_max_arity(p), 3);
    minim_obj args[] = {minim_fixnum(1)};
    CHECK_EQ(minim_primitive_fn(p)(args, 1), minim_fixnum(2));
    MINIM_RETURN_VOID;
}

int main(void) {
    minim_init();
    test_closure();
    test_primitive();
    test_not_procedures();
    test_gc();
    minim_shutdown();
    return test_result();
}
