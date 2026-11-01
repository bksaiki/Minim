/**
 * @file
 * Tests for the value stack's buffer and frozen segments.
 */

#include "gc.h"
#include "minim.h"
#include "stack.h"
#include "test.h"

// Pushes the fixnums `from` up to `to`, exclusive.
static void push_range(intptr_t from, intptr_t to) {
    for (intptr_t i = from; i < to; i++)
        minim_stack_push(minim_fixnum(i));
}

// The value at height `h`, unwinding to just above it.
static minim_obj top_at(size_t h) {
    minim_stack_unwind(h + 1);
    minim_stack_reload(h);
    return *minim_stack_at(h);
}

// Whether the stack holds 0, 1, ... from height `h0` to its top, which it
// unwinds to `h0`.
static bool counts_down_to(size_t h0) {
    bool ok = true;
    for (size_t h = minim_stack_height(); h > h0; h--)
        ok = ok && top_at(h - 1) == minim_fixnum((intptr_t)(h - 1 - h0));
    minim_stack_unwind(h0);
    return ok;
}

// A full buffer freezes on the next push; values keep their heights.
static void test_push_freezes(void) {
    size_t h0 = minim_stack_height();
    push_range(0, MINIM_SEGMENT_SIZE - (intptr_t)minim_gc.values.len);
    CHECK_EQ(minim_gc.values.len, MINIM_SEGMENT_SIZE);
    size_t h = minim_stack_height();
    minim_stack_push(minim_fixnum((intptr_t)(h - h0)));
    CHECK_EQ(minim_gc.values.len, 1);
    CHECK_EQ(minim_gc.values.start, h);
    CHECK(minim_gc.values.below != MINIM_NIL);
    CHECK(counts_down_to(h0));
    CHECK_EQ(minim_stack_height(), h0);
}

// A call that doesn't fit freezes the buffer; one bigger than a segment raises.
static minim_obj reserve_too_many(void *arg) {
    (void)arg;
    minim_stack_reserve(MINIM_SEGMENT_SIZE + 1);
    return MINIM_VOID;
}

static void test_reserve(void) {
    size_t h0 = minim_stack_height();
    push_range(0, 10);
    minim_stack_reserve(MINIM_SEGMENT_SIZE - minim_gc.values.len);
    CHECK_EQ(minim_stack_height(), h0 + 10);
    CHECK(minim_gc.values.len > 0);  // it fit
    minim_stack_reserve(MINIM_SEGMENT_SIZE);
    CHECK_EQ(minim_gc.values.len, 0);  // it didn't
    CHECK_EQ(minim_stack_height(), h0 + 10);

    minim_obj r;
    CHECK(!minim_catch(reserve_too_many, NULL, &r));
    const char *msg;
    minim_obj irritant;
    minim_last_error(&msg, &irritant);
    CHECK(strcmp(msg, "too many arguments") == 0);
    CHECK(counts_down_to(h0));
}

// Relocating freezes what is below; including copies one value back down.
static void test_relocate_include(void) {
    size_t h0 = minim_stack_height();
    push_range(0, 6);
    minim_stack_relocate(h0 + 3);
    CHECK_EQ(minim_gc.values.start, h0 + 3);
    CHECK_EQ(*minim_stack_at(h0 + 3), minim_fixnum(3));
    minim_stack_include(h0 + 2);
    CHECK_EQ(minim_gc.values.start, h0 + 2);
    CHECK_EQ(*minim_stack_at(h0 + 2), minim_fixnum(2));
    CHECK_EQ(*minim_stack_at(h0 + 5), minim_fixnum(5));
    CHECK(counts_down_to(h0));
}

// Unwinding passes whole segments; frozen objects survive collections.
static void test_unwind_and_collect(void) {
    size_t h0 = minim_stack_height();
    for (int i = 0; i < 3; i++) {
        minim_obj p = minim_cons(minim_fixnum(i), MINIM_NIL);
        minim_stack_push(p);
        push_range(1, MINIM_SEGMENT_SIZE);  // a segment's worth
    }
    minim_gc_collect_major();
    minim_obj p = top_at(h0 + MINIM_SEGMENT_SIZE);  // past the third segment, into the second
    CHECK(minim_is_pair(p) && minim_car(p) == minim_fixnum(1));
    p = top_at(h0);
    CHECK(minim_is_pair(p) && minim_car(p) == minim_fixnum(0));
    minim_stack_unwind(h0);
    CHECK_EQ(minim_stack_height(), h0);

    // Down past every segment at once, as an error does.
    push_range(0, 3 * MINIM_SEGMENT_SIZE);
    minim_stack_unwind(h0);
    CHECK_EQ(minim_stack_height(), h0);
}

// `spill` holds the values from a height, then a list's elements.
static void test_spill(void) {
    size_t h0 = minim_stack_height();
    minim_obj lst = minim_cons(minim_fixnum(3), MINIM_NIL);
    MINIM_FRAME(&lst);
    push_range(1, 3);
    CHECK_EQ(minim_stack_spill(h0, lst), 3);
    minim_gc_collect_major();
    for (intptr_t i = 0; i < 3; i++)
        CHECK_EQ(minim_gc.spill.items[i], minim_fixnum(i + 1));
    minim_gc.spill.len = 0;
    minim_stack_unwind(h0);
    MINIM_RETURN_VOID;
}

int main(void) {
    minim_init();
    test_push_freezes();
    test_reserve();
    test_relocate_include();
    test_unwind_and_collect();
    test_spill();
    minim_shutdown();
    return test_result();
}
