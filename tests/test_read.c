/**
 * @file
 * Tests for the reader.
 */

#include <string.h>

#include "minim.h"
#include "test.h"

// A file holding `s`, positioned at its start.
static FILE *input(const char *s) {
    FILE *f = tmpfile();
    fputs(s, f);
    rewind(f);
    return f;
}

// Whether `x` prints as `expect`; reports a mismatch.
static bool prints_as(minim_obj x, const char *expect) {
    FILE *f = tmpfile();
    minim_write(f, x);
    rewind(f);
    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    if (strcmp(buf, expect) != 0) {
        fprintf(stderr, "printed `%s`, expected `%s`\n", buf, expect);
        return false;
    }
    return true;
}

// Whether the first datum in `s` prints as `expect`.
static bool reads_as(const char *s, const char *expect) {
    FILE *f = input(s);
    minim_obj x = minim_read(f);
    fclose(f);
    return prints_as(x, expect);
}

static minim_obj read_file(void *f) {
    return minim_read(f);
}

// Whether reading `s` raises the error `msg`.
static bool fails_with(const char *s, const char *msg) {
    FILE *f = input(s);
    minim_obj x;
    bool ok = minim_catch(read_file, f, &x);
    fclose(f);
    if (ok) {
        fprintf(stderr, "`%s` read without error\n", s);
        return false;
    }

    const char *actual;
    minim_obj irritant;
    minim_last_error(&actual, &irritant);
    if (strcmp(actual, msg) != 0) {
        fprintf(stderr, "`%s` raised `%s`, expected `%s`\n", s, actual, msg);
        return false;
    }
    return true;
}

static void test_atoms(void) {
    CHECK(reads_as("42", "42"));
    CHECK(reads_as("-7", "-7"));
    CHECK(reads_as("+5", "5"));
    CHECK(reads_as("-0", "0"));
    CHECK(reads_as("1152921504606846975", "1152921504606846975"));
    CHECK(reads_as("-1152921504606846976", "-1152921504606846976"));
    CHECK(reads_as("#t", "#t"));
    CHECK(reads_as("#true", "#t"));
    CHECK(reads_as("#f", "#f"));
    CHECK(reads_as("#false", "#f"));
}

static void test_symbols(void) {
    CHECK(reads_as("foo", "foo"));
    CHECK(reads_as("Foo", "Foo"));
    CHECK(reads_as("caf\u00e9", "caf\u00e9"));
    CHECK(reads_as("+", "+"));
    CHECK(reads_as("-", "-"));
    CHECK(reads_as("...", "..."));
    CHECK(reads_as("-x", "-x"));
}

static void test_strings(void) {
    CHECK(reads_as("\"\"", "\"\""));
    CHECK(reads_as("\"a\\\"b\\\\c\\nd\\te\"", "\"a\\\"b\\\\c\\nd\\te\""));
    CHECK(reads_as("\"\\x41;\\x1F600;\"", "\"A\U0001F600\""));
    CHECK(reads_as("\"caf\u00e9\"", "\"caf\u00e9\""));
}

static void test_chars(void) {
    CHECK(reads_as("#\\a", "#\\a"));
    CHECK(reads_as("#\\space", "#\\space"));
    CHECK(reads_as("#\\newline", "#\\newline"));
    CHECK(reads_as("#\\tab", "#\\tab"));
    CHECK(reads_as("#\\nul", "#\\nul"));
    CHECK(reads_as("#\\x41", "#\\A"));
    CHECK(reads_as("#\\x", "#\\x"));
    CHECK(reads_as("#\\(", "#\\("));
    CHECK(reads_as("#\\\u00e9", "#\\\u00e9"));
}

static void test_lists(void) {
    CHECK(reads_as("()", "()"));
    CHECK(reads_as("(1 2 3)", "(1 2 3)"));
    CHECK(reads_as("(1 . 2)", "(1 . 2)"));
    CHECK(reads_as("(1 (2 3) . 4)", "(1 (2 3) . 4)"));
    CHECK(reads_as("( a\n\tb )", "(a b)"));
    CHECK(reads_as("(a ; comment\n b)", "(a b)"));
    CHECK(reads_as("'x", "'x"));
    CHECK(reads_as("'(1 2)", "'(1 2)"));
    CHECK(reads_as("#()", "#()"));
    CHECK(reads_as("#(1 \"a\" #\\b #(2))", "#(1 \"a\" #\\b #(2))"));
}

static void test_stream(void) {
    FILE *f = input("  ; leading comment\n 1 (2)\"s\"x");
    CHECK(prints_as(minim_read(f), "1"));
    CHECK(prints_as(minim_read(f), "(2)"));
    CHECK(prints_as(minim_read(f), "\"s\""));
    CHECK(prints_as(minim_read(f), "x"));
    CHECK_EQ(minim_read(f), MINIM_EOF);
    CHECK_EQ(minim_read(f), MINIM_EOF);
    fclose(f);
}

static void test_errors(void) {
    CHECK(fails_with(")", "unexpected )"));
    CHECK(fails_with(".", "unexpected ."));
    CHECK(fails_with("(1 . )", "bad dotted list"));
    CHECK(fails_with("(. 1)", "bad dotted list"));
    CHECK(fails_with("(1 . 2 3)", "bad dotted list"));
    CHECK(fails_with("(1 2", "eof in list"));
    CHECK(fails_with("#(1", "eof in vector"));
    CHECK(fails_with("#(1 . 2)", "bad vector"));
    CHECK(fails_with("\"abc", "eof in string"));
    CHECK(fails_with("\"\\q\"", "bad string escape"));
    CHECK(fails_with("\"\\xZZ;\"", "bad string escape"));
    CHECK(fails_with("#q", "bad # syntax"));
    CHECK(fails_with("#\\bogus", "bad char"));
    CHECK(fails_with("'", "eof after quote"));
    CHECK(fails_with("1152921504606846976", "fixnum out of range"));
    CHECK(fails_with("-1152921504606846977", "fixnum out of range"));
}

int main(void) {
    minim_init();
    test_atoms();
    test_symbols();
    test_strings();
    test_chars();
    test_lists();
    test_stream();
    test_errors();
    minim_shutdown();
    return test_result();
}
