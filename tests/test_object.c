/**
 * @file
 * Tests for the constructors and accessors of closures, primitives, records,
 * bytevectors, and flonums.
 */

#include "minim.h"
#include "test.h"

// Returns its first argument plus its argument count.
static minim_obj first_plus_n(minim_obj *args, size_t n) {
    return minim_fixnum(minim_fixnum_value(args[0]) + (intptr_t)n);
}

static void test_closure(void) {
    minim_obj code = minim_cons(MINIM_NIL, MINIM_NIL);
    minim_obj env = MINIM_NIL;
    minim_obj c = MINIM_NIL;
    minim_obj name = MINIM_NIL;
    MINIM_FRAME(&code, &env, &c, &name);
    env = minim_make_vector(1, minim_fixnum(7));
    c = minim_make_closure(code, env);
    CHECK_EQ(minim_closure_name(c), MINIM_FALSE);
    name = minim_intern_cstr("f");
    minim_set_closure_name(c, name);

    CHECK(minim_is_closure(c));
    CHECK(minim_is_procedure(c));
    CHECK_EQ(count_predicates(c), 1);
    CHECK_EQ(minim_closure_code(c), code);
    CHECK_EQ(minim_closure_env(c), env);
    CHECK_EQ(minim_closure_name(c), name);
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

    minim_obj args[] = {minim_fixnum(10), minim_fixnum(0)};
    CHECK_EQ(minim_primitive_fn(p)(args, 2), minim_fixnum(12));
    MINIM_RETURN_VOID;
}

// A record of a type holds its fields, #f until set.
static void test_record(void) {
    static const char *const fields[] = {"x", "y"};
    minim_obj rtd = make_record_type("point", fields, 2);
    minim_obj r = MINIM_NIL;
    MINIM_FRAME(&rtd, &r);
    r = minim_make_record(rtd);

    CHECK(minim_is_record(r));
    CHECK(!minim_is_procedure(r));
    CHECK_EQ(count_predicates(r), 1);
    CHECK_EQ(minim_record_rtd(r), rtd);
    CHECK_EQ(minim_record_length(r), 2);
    CHECK_EQ(minim_record_ref(r, 0), MINIM_FALSE);
    minim_record_set(r, 1, minim_fixnum(5));
    CHECK_EQ(minim_record_ref(r, 1), minim_fixnum(5));

    CHECK(minim_is_record(rtd));
    CHECK_EQ(minim_record_rtd(rtd), minim_base_rtd);
    CHECK_EQ(minim_record_rtd(minim_base_rtd), minim_base_rtd);
    MINIM_RETURN_VOID;
}

static void test_bytevector(void) {
    minim_obj bv = minim_make_bytevector(3, 7);
    CHECK(minim_is_bytevector(bv));
    CHECK(!minim_is_vector(bv) && !minim_is_string(bv) && !minim_is_procedure(bv));
    CHECK_EQ(minim_bytevector_length(bv), 3);
    minim_bytevector_data(bv)[1] = 255;
    CHECK_EQ(minim_bytevector_data(bv)[0], 7);
    CHECK_EQ(minim_bytevector_data(bv)[1], 255);
    CHECK_EQ(minim_bytevector_length(minim_make_bytevector(0, 0)), 0);
    CHECK(!minim_is_bytevector(minim_make_vector(1, MINIM_NIL)));
}

static void test_flonum(void) {
    minim_obj f = minim_make_flonum(-2.5);
    CHECK(minim_is_flonum(f));
    CHECK_EQ(minim_tag(f), MINIM_TAG_FLONUM);
    CHECK_EQ(count_predicates(f), 1);
    CHECK(minim_flonum_value(f) == -2.5);
    CHECK(!minim_is_flonum(minim_fixnum(2)));
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
    minim_gc_collect_major();
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
    test_record();
    test_bytevector();
    test_flonum();
    test_not_procedures();
    test_gc();
    minim_shutdown();
    return test_result();
}
