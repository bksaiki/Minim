/**
 * @file
 * Tests for errors and catches.
 */

#include <string.h>

#include "minim.h"
#include "test.h"

static minim_obj return_arg(void *arg) {
    return *(minim_obj *)arg;
}

static minim_obj raise(void *arg) {
    minim_error("boom", *(minim_obj *)arg);
}

// Raises from inside a frame, leaving roots pushed.
static minim_obj raise_in_frame(void *arg) {
    minim_obj x = minim_cons(minim_fixnum(1), MINIM_NIL);
    MINIM_FRAME(&x);
    MINIM_PROTECT(&x, &x);
    minim_error("in frame", *(minim_obj *)arg);
    MINIM_RETURN(x);
}

// Catches an inner error, then raises one of its own.
static minim_obj nested(void *arg) {
    minim_obj r;
    bool *inner_caught = arg;
    minim_obj irritant = minim_fixnum(1);
    *inner_caught = !minim_catch(raise, &irritant, &r);
    minim_error("outer", minim_fixnum(2));
}

static void test_success(void) {
    minim_obj x = minim_fixnum(42);
    minim_obj r = MINIM_NIL;
    CHECK(minim_catch(return_arg, &x, &r));
    CHECK_EQ(r, minim_fixnum(42));
}

static void test_error(void) {
    minim_obj x = minim_fixnum(7);
    minim_obj r = MINIM_NIL;
    CHECK(!minim_catch(raise, &x, &r));
    CHECK_EQ(r, MINIM_NIL);  // untouched

    const char *msg;
    minim_obj irritant;
    minim_last_error(&msg, &irritant);
    CHECK(strcmp(msg, "boom") == 0);
    CHECK_EQ(irritant, minim_fixnum(7));
}

static void test_root_stack_restored(void) {
    size_t before = minim_gc_mark();
    minim_obj x = MINIM_VOID;
    minim_obj r;
    CHECK(!minim_catch(raise_in_frame, &x, &r));
    CHECK_EQ(minim_gc_mark(), before);
}

static void test_nested(void) {
    bool inner_caught = false;
    minim_obj r;
    CHECK(!minim_catch(nested, &inner_caught, &r));
    CHECK(inner_caught);

    const char *msg;
    minim_obj irritant;
    minim_last_error(&msg, &irritant);
    CHECK(strcmp(msg, "outer") == 0);
    CHECK_EQ(irritant, minim_fixnum(2));

    // No catch is left installed.
    minim_obj x = minim_fixnum(3);
    CHECK(minim_catch(return_arg, &x, &r));
}

static void test_irritant_survives_gc(void) {
    minim_obj x = minim_cons(minim_fixnum(1), minim_fixnum(2));
    minim_obj r;
    CHECK(!minim_catch(raise, &x, &r));
    minim_gc_collect_major();

    const char *msg;
    minim_obj irritant;
    minim_last_error(&msg, &irritant);
    CHECK(minim_is_pair(irritant));
    CHECK_EQ(minim_car(irritant), minim_fixnum(1));
    CHECK_EQ(minim_cdr(irritant), minim_fixnum(2));
}

int main(void) {
    minim_init();
    test_success();
    test_error();
    test_root_stack_restored();
    test_nested();
    test_irritant_survives_gc();
    minim_shutdown();
    return test_result();
}
