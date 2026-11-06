/**
 * @file
 * Tests for ports.
 */

#include <string.h>

#include "minim.h"
#include "port.h"
#include "test.h"

// A string port's buffer and name survive collections, young and old.
static void test_string_port_gc(void) {
    minim_obj p = eval_source("(open-input-string \"hello\")");
    MINIM_FRAME(&p);
    CHECK(minim_is_port(p));
    CHECK_EQ(count_predicates(p), 1);
    minim_gc_collect();
    CHECK_EQ(minim_port_read_char(p), 'h');
    CHECK(prints_as(minim_port_name(p), "\"string\""));
    minim_gc_collect_major();
    CHECK_EQ(minim_port_read_char(p), 'e');
    CHECK(prints_as(minim_port_name(p), "\"string\""));
    MINIM_RETURN_VOID;
}

// A pushed-back end of input is read once, though more may follow it, as on a
// terminal; for chars and bytes.
static void test_lookahead_end(void) {
    minim_obj p = eval_source("(open-input-string \"ab\")");
    minim_obj read_u8 = MINIM_NIL;
    MINIM_FRAME(&p, &read_u8);
    minim_port_unread_char(p, -1);
    CHECK_EQ(minim_port_read_char(p), -1);
    CHECK_EQ(minim_port_read_char(p), 'a');

    p = eval_source("(open-input-bytevector #u8(7))");
    read_u8 = minim_symbol_value(minim_intern_cstr("%read-u8"));
    minim_port_unread_char(p, -1);
    CHECK_EQ(minim_primitive_fn(read_u8)(&p, 1), MINIM_EOF);
    CHECK_EQ(minim_primitive_fn(read_u8)(&p, 1), minim_fixnum(7));
    MINIM_RETURN_VOID;
}

// A file port's FILE * isn't traced, so collections leave it alone.
static void test_file_untraced(void) {
    minim_gc_collect_major();
    CHECK(minim_port_file(minim_standard_port(0)) == stdin);
    CHECK(minim_port_file(minim_standard_port(1)) == stdout);
    CHECK(minim_port_file(minim_standard_port(2)) == stderr);
    CHECK(prints_as(minim_port_name(minim_standard_port(2)), "\"stderr\""));
}

// The flags, in the header, are copied with it.
static void test_flags_copied(void) {
    minim_obj p = eval_source("(open-output-string)");
    minim_obj q = MINIM_NIL;
    MINIM_FRAME(&p, &q);
    q = eval_source("(let ((p (open-input-string \"\"))) (close-port p) p)");
    minim_gc_collect();
    minim_gc_collect_major();
    CHECK_EQ(minim_port_flags(p), MINIM_PORT_OUTPUT);
    CHECK_EQ(minim_port_flags(q), MINIM_PORT_INPUT | MINIM_PORT_CLOSED);
    CHECK_EQ(minim_port_flags(minim_standard_port(0)), MINIM_PORT_INPUT | MINIM_PORT_FILE);
    MINIM_RETURN_VOID;
}

// Whether the file `path` holds exactly `expect`.
static bool file_is(const char *path, const char *expect) {
    char buf[64] = {0};
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return false;
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    return n == strlen(expect) && memcmp(buf, expect, n) == 0;
}

// A new output port over `path`, with `text` written to it, unflushed.
static minim_obj open_written(const char *path, const char *text) {
    minim_obj p = minim_open_output_file(minim_string_from_cstr(path));
    fputs(text, minim_port_file(p));
    return p;
}

// A dropped file port is closed, so flushed, by the collection that finds it
// dead, young or old.
static void test_dropped_closed(void) {
    open_written("test_port_young.txt", "young");
    minim_gc_collect_minor();
    CHECK(file_is("test_port_young.txt", "young"));

    minim_obj p = open_written("test_port_old.txt", "old");
    MINIM_FRAME(&p);
    minim_gc_collect_major();  // promoted
    p = MINIM_FALSE;
    CHECK(file_is("test_port_old.txt", ""));
    minim_gc_collect_major();
    CHECK(file_is("test_port_old.txt", "old"));
    remove("test_port_young.txt");
    remove("test_port_old.txt");
    MINIM_RETURN_VOID;
}

// A live file port stays open across collections, young and promoted; one
// closed explicitly isn't closed again when collected.
static void test_live_open(void) {
    minim_obj p = open_written("test_port_live.txt", "a");
    MINIM_FRAME(&p);
    minim_gc_collect_minor();
    minim_gc_collect_major();
    CHECK_EQ(minim_port_flags(p), MINIM_PORT_OUTPUT | MINIM_PORT_FILE);
    fputs("b", minim_port_file(p));
    minim_port_close(p);
    CHECK(file_is("test_port_live.txt", "ab"));
    p = MINIM_FALSE;
    minim_gc_collect_major();  // ASan reports a second fclose
    remove("test_port_live.txt");
    MINIM_RETURN_VOID;
}

// A file port still open at minim_shutdown is flushed.
static void test_shutdown_flushes(void) {
    static minim_obj p;
    minim_gc_add_global(&p);
    p = open_written("test_port_shutdown.txt", "kept");
    minim_shutdown();
    CHECK(file_is("test_port_shutdown.txt", "kept"));
    remove("test_port_shutdown.txt");
}

int main(void) {
    minim_init();
    test_string_port_gc();
    test_lookahead_end();
    test_file_untraced();
    test_flags_copied();
    test_dropped_closed();
    test_live_open();
    test_shutdown_flushes();  // shuts down
    return test_result();
}
