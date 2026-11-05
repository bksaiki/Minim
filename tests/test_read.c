/**
 * @file
 * Tests for the reader.
 */

#include <string.h>

#include "minim.h"
#include "read.h"
#include "test.h"

// Whether the first datum in `s`, read from a port and from a C string,
// prints as `expect`.
static bool reads_as(const char *s, const char *expect) {
    if (!prints_as(minim_read(input(s)), expect))
        return false;

    const char *p = s;
    return prints_as(minim_read_string(&p), expect);
}

static minim_obj read_port(void *s) {
    return minim_read(input(s));
}

static minim_obj read_string(void *s) {
    const char *p = s;
    return minim_read_string(&p);
}

// Whether reading `s` with `read` raises the error `msg`.
static bool raises(minim_obj (*read)(void *), void *arg, const char *s, const char *msg) {
    minim_obj x;
    if (minim_catch(read, arg, &x)) {
        fprintf(stderr, "`%s` read without error\n", s);
        return false;
    }

    if (last_error_is(msg))
        return true;
    fprintf(stderr, "  in `%s`\n", s);
    return false;
}

// Whether reading `s`, from a port and from a C string, raises the error `msg`.
static bool read_fails_with(const char *s, const char *msg) {
    return raises(read_port, (void *)s, s, msg) && raises(read_string, (void *)s, s, msg);
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
    CHECK(reads_as("\"\\a\\b\\r\\|\"", "\"\\a\\b\\r|\""));
}

// A line continuation drops `\`, intraline white space, a line ending, and
// intraline white space.
static void test_continuations(void) {
    CHECK(reads_as("\"a\\\nb\"", "\"ab\""));
    CHECK(reads_as("\"a\\ \t\n \tb\"", "\"ab\""));
    CHECK(reads_as("\"a\\\r\nb\"", "\"ab\""));
    CHECK(reads_as("\"a\\\rb\"", "\"ab\""));
    CHECK(reads_as("\"a\\ \n \n b\"", "\"a\\n b\""));
    CHECK(reads_as("\"a\\\n \u03bb\"", "\"a\u03bb\""));
    CHECK(read_fails_with("\"a\\ b\"", "bad string escape"));
}

static void test_chars(void) {
    CHECK(reads_as("#\\a", "#\\a"));
    CHECK(reads_as("#\\alarm", "#\\alarm"));
    CHECK(reads_as("#\\backspace", "#\\backspace"));
    CHECK(reads_as("#\\delete", "#\\delete"));
    CHECK(reads_as("#\\escape", "#\\escape"));
    CHECK(reads_as("#\\newline", "#\\newline"));
    CHECK(reads_as("#\\null", "#\\null"));
    CHECK(reads_as("#\\return", "#\\return"));
    CHECK(reads_as("#\\space", "#\\space"));
    CHECK(reads_as("#\\tab", "#\\tab"));
    CHECK(reads_as("#\\nul", "#\\null"));
    CHECK(reads_as("#\\x7", "#\\alarm"));
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
    CHECK(reads_as("`x", "`x"));
    CHECK(reads_as("`(a ,b ,@c)", "`(a ,b ,@c)"));
    CHECK(reads_as("`(a ,(b ,@c))", "`(a ,(b ,@c))"));
    CHECK(reads_as("`(1 . ,x)", "`(1 . ,x)"));
    CHECK(reads_as("`#(1 ,x)", "`#(1 ,x)"));
    CHECK(reads_as(", x", ",x"));
    CHECK(reads_as(",; comment\nx", ",x"));
    CHECK(reads_as(",caf\u00e9", ",caf\u00e9"));
    CHECK(reads_as(", @x", "(unquote @x)"));
    CHECK(reads_as("'(1 2)", "'(1 2)"));
    CHECK(reads_as("#()", "#()"));
    CHECK(reads_as("#(1 \"a\" #\\b #(2))", "#(1 \"a\" #\\b #(2))"));
}

static void test_stream(void) {
    minim_obj port = input("  ; leading comment\n 1 (2)\"s\"x");
    MINIM_FRAME(&port);
    CHECK(prints_as(minim_read(port), "1"));
    CHECK(prints_as(minim_read(port), "(2)"));
    CHECK(prints_as(minim_read(port), "\"s\""));
    CHECK(prints_as(minim_read(port), "x"));
    CHECK_EQ(minim_read(port), MINIM_EOF);
    CHECK_EQ(minim_read(port), MINIM_EOF);

    const char *p = "  ; leading comment\n 1 (2)\"s\"x caf\u00e9";
    CHECK(prints_as(minim_read_string(&p), "1"));
    CHECK(prints_as(minim_read_string(&p), "(2)"));
    CHECK(prints_as(minim_read_string(&p), "\"s\""));
    CHECK(prints_as(minim_read_string(&p), "x"));
    CHECK(prints_as(minim_read_string(&p), "caf\u00e9"));
    CHECK_EQ(minim_read_string(&p), MINIM_EOF);
    CHECK_EQ(minim_read_string(&p), MINIM_EOF);
    MINIM_RETURN_VOID;
}

// `#;` comments out the next datum and `#| ... |#` a region, nesting; both
// are atmosphere, wherever whitespace may be.
static void test_comments(void) {
    CHECK(reads_as("#;1 2", "2"));
    CHECK(reads_as("(1 #;2 3)", "(1 3)"));
    CHECK(reads_as("(1 #;2)", "(1)"));
    CHECK(reads_as("(#;1 #;(2 3))", "()"));
    CHECK(reads_as("#;#;1 2 3", "3"));
    CHECK(reads_as("(a . #;b c)", "(a . c)"));
    CHECK(reads_as("'#;x y", "'y"));
    CHECK(reads_as("#| c |# 5", "5"));
    CHECK(reads_as("(1 #| c |# 2)", "(1 2)"));
    CHECK(reads_as("(1 #||#)", "(1)"));
    CHECK(reads_as("#| a #| nested |# b |# 6", "6"));
    CHECK(reads_as("#|\nline\n|# 7", "7"));
    CHECK(reads_as("#| |# |# 8", "|#"));
    CHECK(reads_as("\"#|\"", "\"#|\""));
    CHECK(read_fails_with("#| open", "eof in comment"));
    CHECK(read_fails_with("#| #| |#", "eof in comment"));
    CHECK(read_fails_with("(1 #;)", "bad datum comment"));
    CHECK(read_fails_with("#;", "bad datum comment"));
}

static void test_errors(void) {
    CHECK(read_fails_with(")", "unexpected )"));
    CHECK(read_fails_with(".", "unexpected ."));
    CHECK(read_fails_with("(1 . )", "bad dotted list"));
    CHECK(read_fails_with("(. 1)", "bad dotted list"));
    CHECK(read_fails_with("(1 . 2 3)", "bad dotted list"));
    CHECK(read_fails_with("(1 2", "eof in list"));
    CHECK(read_fails_with("#(1", "eof in vector"));
    CHECK(read_fails_with("#(1 . 2)", "bad vector"));
    CHECK(read_fails_with("\"abc", "eof in string"));
    CHECK(read_fails_with("\"\\q\"", "bad string escape"));
    CHECK(read_fails_with("\"\\xZZ;\"", "bad string escape"));
    CHECK(read_fails_with("#q", "bad # syntax"));
    CHECK(read_fails_with("#\\bogus", "bad char"));
    CHECK(read_fails_with("'", "eof after quote"));
    CHECK(read_fails_with("`", "eof after quasiquote"));
    CHECK(read_fails_with(",", "eof after unquote"));
    CHECK(read_fails_with(",@", "eof after unquote-splicing"));
    CHECK(read_fails_with("(a ,)", "eof after unquote"));
    CHECK(read_fails_with("1152921504606846976", "fixnum out of range"));
    CHECK(read_fails_with("-1152921504606846977", "fixnum out of range"));
}

int main(void) {
    minim_init();
    test_atoms();
    test_symbols();
    test_strings();
    test_continuations();
    test_chars();
    test_lists();
    test_stream();
    test_comments();
    test_errors();
    minim_shutdown();
    return test_result();
}
