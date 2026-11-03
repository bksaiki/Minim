/**
 * @file
 * Tests for ports.
 */

#include "minim.h"
#include "test.h"

// The value of evaluating `src`.
static minim_obj eval_str(const char *src) {
    FILE *f = input(src);
    minim_obj x = eval_file(f);
    fclose(f);
    return x;
}

// Whether the string `s` is `expect`, all ASCII.
static bool string_is(minim_obj s, const char *expect) {
    size_t n = strlen(expect);
    if (!minim_is_string(s) || minim_string_length(s) != n)
        return false;
    for (size_t i = 0; i < n; i++) {
        if (minim_string_ref(s, i) != (unsigned char)expect[i])
            return false;
    }
    return true;
}

// A string port's buffer and name survive collections, young and old.
static void test_string_port_gc(void) {
    minim_obj p = eval_str("(open-input-string \"hello\")");
    MINIM_FRAME(&p);
    CHECK(minim_is_port(p));
    CHECK_EQ(count_predicates(p), 1);
    minim_gc_collect();
    CHECK(string_is(minim_port_buffer(p), "hello"));
    CHECK(string_is(minim_port_name(p), "string"));
    minim_gc_collect_major();
    CHECK(string_is(minim_port_buffer(p), "hello"));
    CHECK(string_is(minim_port_name(p), "string"));
    MINIM_RETURN_VOID;
}

// A file port's FILE * isn't traced, so collections leave it alone.
static void test_file_untraced(void) {
    minim_gc_collect_major();
    CHECK(minim_port_file(minim_standard_port(0)) == stdin);
    CHECK(minim_port_file(minim_standard_port(1)) == stdout);
    CHECK(minim_port_file(minim_standard_port(2)) == stderr);
    CHECK(string_is(minim_port_name(minim_standard_port(2)), "stderr"));
}

// The flags, in the header, are copied with it.
static void test_flags_copied(void) {
    minim_obj p = eval_str("(open-output-string)");
    minim_obj q = MINIM_NIL;
    MINIM_FRAME(&p, &q);
    q = eval_str("(let ((p (open-input-string \"\"))) (close-port p) p)");
    minim_gc_collect();
    minim_gc_collect_major();
    CHECK_EQ(minim_port_flags(p), MINIM_PORT_OUTPUT);
    CHECK_EQ(minim_port_flags(q), MINIM_PORT_INPUT | MINIM_PORT_CLOSED);
    CHECK_EQ(minim_port_flags(minim_standard_port(0)), MINIM_PORT_INPUT | MINIM_PORT_FILE);
    MINIM_RETURN_VOID;
}

int main(void) {
    minim_init();
    test_string_port_gc();
    test_file_untraced();
    test_flags_copied();
    minim_shutdown();
    return test_result();
}
