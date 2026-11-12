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

static minim_obj read_port_again(void *port) {
    return minim_read(*(minim_obj *)port);
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

    // Case is insignificant in `#` syntax and special numbers.
    CHECK(reads_as("#T", "#t"));
    CHECK(reads_as("#FALSE", "#f"));
    CHECK(reads_as("#U8(1)", "#u8(1)"));
    CHECK(reads_as("+NAN.0", "+nan.0"));
    CHECK(reads_as("-Inf.0", "-inf.0"));
}

// Whether `s` reads as a symbol, as a number's look-alike does.
static bool reads_symbol(const char *s) {
    const char *p = s;
    return minim_is_symbol(minim_read_string(&p));
}

static void test_numbers(void) {
    // Decimals and their special values are flonums.
    CHECK(reads_as("1.5", "1.5"));
    CHECK(reads_as(".5", "0.5"));
    CHECK(reads_as("1.", "1.0"));
    CHECK(reads_as("-0.0", "-0.0"));
    CHECK(reads_as("1e10", "10000000000.0"));
    CHECK(reads_as("1E10", "10000000000.0"));
    CHECK(reads_as("-2.5e-3", "-0.0025"));
    CHECK(reads_as("+.5e+1", "5.0"));
    CHECK(reads_as("+inf.0", "+inf.0"));
    CHECK(reads_as("-inf.0", "-inf.0"));
    CHECK(reads_as("+nan.0", "+nan.0"));
    CHECK(reads_as("-nan.0", "+nan.0"));

    // Prefixes, in either order and either case.
    CHECK(reads_as("#x1F", "31"));
    CHECK(reads_as("#XfF", "255"));
    CHECK(reads_as("#x-ff", "-255"));
    CHECK(reads_as("#b101", "5"));
    CHECK(reads_as("#o17", "15"));
    CHECK(reads_as("#d10", "10"));
    CHECK(reads_as("#e1.0", "1"));
    CHECK(reads_as("#e1e3", "1000"));
    CHECK(reads_as("#e-0.0", "0"));
    CHECK(reads_as("#i3", "3.0"));
    CHECK(reads_as("#x#e10", "16"));
    CHECK(reads_as("#e#x10", "16"));
    CHECK(reads_as("#i#xff", "255.0"));
    CHECK(reads_as("#x+inf.0", "+inf.0"));

    // Each flonum the printer writes reads back as itself.
    static const char *const printed[] = {
        "0.1", "100.0", "123.456", "100000000000000000000.0", "1e21", "0.000001", "1e-7",
        "1.5e-7", "5e-324", "1.7976931348623157e308", "0.30000000000000004",
    };
    for (size_t i = 0; i < sizeof(printed) / sizeof(printed[0]); i++)
        CHECK(reads_as(printed[i], printed[i]));

    // Look-alikes stay symbols.
    static const char *const symbols[] = {"...", "+", "-", "1+", "+i", "1e", "e1", "1.2.3", "-e5",
                                          "inf.0", "+inf.1"};
    for (size_t i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++)
        CHECK(reads_symbol(symbols[i]));
}

// Whether the first datum in `s`, read from a port and from a C string, is the
// symbol named `name`.
static bool reads_symbol_named(const char *s, const char *name) {
    minim_obj sym = minim_intern_cstr(name);
    minim_obj x = MINIM_NIL;
    MINIM_FRAME(&sym, &x);
    x = minim_read(input(s));
    bool ok = x == sym;
    const char *p = s;
    x = minim_read_string(&p);
    MINIM_RETURN(ok && x == sym);
}

static void test_symbols(void) {
    CHECK(reads_as("foo", "foo"));
    CHECK(reads_as("Foo", "Foo"));
    CHECK(reads_as("caf\u00e9", "caf\u00e9"));
    CHECK(reads_as("+", "+"));
    CHECK(reads_as("-", "-"));
    CHECK(reads_as("...", "..."));
    CHECK(reads_as("-x", "-x"));

    // `|...|` names a symbol exactly, with the string escapes.
    CHECK(reads_symbol_named("|foo|", "foo"));
    CHECK(reads_symbol_named("|a b|", "a b"));
    CHECK(reads_symbol_named("||", ""));
    CHECK(reads_symbol_named("|H\\x65;llo|", "Hello"));
    CHECK(reads_symbol_named("|a\\|b|", "a|b"));
    CHECK(reads_symbol_named("|a\\\\b|", "a\\b"));
    CHECK(reads_symbol_named("|1|", "1"));
    CHECK(reads_symbol_named("|.|", "."));
    CHECK(reads_as("(|a|)", "(a)"));
}

// `|` is a delimiter, so `a|b|c` is three symbols.
static void test_bar_delimits(void) {
    minim_obj port = input("a|b|c");
    minim_obj a = MINIM_NIL;
    minim_obj b = MINIM_NIL;
    minim_obj c = MINIM_NIL;
    minim_obj x = MINIM_NIL;
    MINIM_FRAME(&port, &a, &b, &c, &x);
    a = minim_intern_cstr("a");
    b = minim_intern_cstr("b");
    c = minim_intern_cstr("c");
    x = minim_read(port);
    CHECK_EQ(x, a);
    x = minim_read(port);
    CHECK_EQ(x, b);
    x = minim_read(port);
    CHECK_EQ(x, c);
    CHECK_EQ(minim_read(port), MINIM_EOF);
    MINIM_RETURN_VOID;
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
    CHECK(reads_as("#u8()", "#u8()"));
    CHECK(reads_as("#u8(0 1 255)", "#u8(0 1 255)"));
    CHECK(reads_as("(#u8(1) #u8(2))", "(#u8(1) #u8(2))"));
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
    CHECK(read_fails_with("#| |# |# 8", "eof in symbol"));
    CHECK(reads_as("\"#|\"", "\"#|\""));
    CHECK(read_fails_with("#| open", "eof in comment"));
    CHECK(read_fails_with("#| #| |#", "eof in comment"));
    CHECK(read_fails_with("(1 #;)", "bad datum comment"));
    CHECK(read_fails_with("#;", "bad datum comment"));
}

// `#!fold-case` folds the identifiers and char names after it, until
// `#!no-fold-case`: for the rest of a port, or of one C string read.
static void test_fold_case(void) {
    CHECK(reads_symbol_named("#!fold-case ABC", "abc"));
    CHECK(reads_symbol_named("#!fold-case #!no-fold-case ABC", "ABC"));
    CHECK(reads_symbol_named("#!fold-case STRA\u00dfE", "strasse"));
    CHECK(reads_symbol_named("#!fold-case |ABC|", "ABC"));
    CHECK(reads_as("#!fold-case #\\SPACE", "#\\space"));
    CHECK(reads_as("#!fold-case #\\A", "#\\A"));
    CHECK(reads_as("#!fold-case \"ABC\"", "\"ABC\""));
    CHECK(reads_as("(A #!fold-case B)", "(A b)"));

    minim_obj port = input("#!fold-case ABC DEF");
    MINIM_FRAME(&port);
    CHECK(prints_as(minim_read(port), "abc"));
    CHECK(prints_as(minim_read(port), "def"));

    const char *p = "#!fold-case ABC DEF";
    CHECK(prints_as(minim_read_string(&p), "abc"));
    CHECK(prints_as(minim_read_string(&p), "DEF"));
    MINIM_RETURN_VOID;
}

// `#n=` labels a datum, which `#n#` refers to, within one outermost datum.
static void test_labels(void) {
    minim_obj x = MINIM_NIL;
    minim_obj port = MINIM_NIL;
    MINIM_FRAME(&x, &port);
    const char *p = "#0=(1 . #0#)";
    x = minim_read_string(&p);
    CHECK_EQ(minim_cdr(x), x);
    p = "(#0=(1 2) #0#)";
    x = minim_read_string(&p);
    CHECK_EQ(minim_car(x), minim_car(minim_cdr(x)));
    p = "(#0=\"a\" #0#)";
    x = minim_read_string(&p);
    CHECK_EQ(minim_car(x), minim_car(minim_cdr(x)));
    p = "#0=#(1 #0#)";
    x = minim_read_string(&p);
    CHECK_EQ(minim_vector_ref(x, 1), x);
    p = "#0=(#1=(a . #1#) . #0#)";
    x = minim_read_string(&p);
    CHECK_EQ(minim_cdr(x), x);
    CHECK_EQ(minim_cdr(minim_car(x)), minim_car(x));

    // Labels don't carry over to the next datum.
    port = input("#0=(1) #0#");
    CHECK(prints_as(minim_read(port), "(1)"));
    CHECK(raises(read_port_again, &port, "#0#", "undefined datum label"));
    MINIM_RETURN_VOID;
}

// What `write` prints of cyclic data reads back `equal?` to it.
static void test_label_round_trips(void) {
    static const char *const data[] = {
        "(let ((c (list 1 2))) (set-cdr! (cdr c) c) c)",
        "(let ((p (list #f))) (set-car! p p) p)",
        "(let ((v (vector 1 #f))) (vector-set! v 1 v) v)",
        "(let ((c (list 1 2))) (set-cdr! (cdr c) (cdr c)) c)",
        "(let ((q (list 'quote 1))) (set-cdr! (cdr q) (cdr q)) q)",
    };
    char src[512];
    for (size_t i = 0; i < sizeof(data) / sizeof(*data); i++) {
        snprintf(src, sizeof(src),
                 "(let ((x %s) (out (open-output-string)))"
                 "  (write x out)"
                 "  (equal? x (read (open-input-string (get-output-string out)))))",
                 data[i]);
        CHECK(evals_to(src, "#t"));
    }
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
    CHECK(read_fails_with("#u8(1", "eof in bytevector"));
    CHECK(read_fails_with("#u8(1 . 2)", "bad bytevector"));
    CHECK(read_fails_with("#u8(256)", "bad byte"));
    CHECK(read_fails_with("#u8(-1)", "bad byte"));
    CHECK(read_fails_with("#u8(a)", "bad byte"));
    CHECK(read_fails_with("#u8 (1)", "bad # syntax"));
    CHECK(read_fails_with("#u9(1)", "bad # syntax"));
    CHECK(read_fails_with("\"abc", "eof in string"));
    CHECK(read_fails_with("|abc", "eof in symbol"));
    CHECK(read_fails_with("#!fold-casex", "bad # syntax"));
    CHECK(read_fails_with("#!foo", "bad # syntax"));
    CHECK(read_fails_with("#0#", "undefined datum label"));
    CHECK(read_fails_with("(#0=1 #1#)", "undefined datum label"));
    CHECK(read_fails_with("#0=#0#", "bad datum label"));
    CHECK(read_fails_with("(#0=1 #0=2)", "duplicate datum label"));
    CHECK(read_fails_with("#0x", "bad # syntax"));
    CHECK(read_fails_with("#0=", "bad datum label"));
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
    CHECK(read_fails_with("#e1e19", "fixnum out of range"));
    CHECK(read_fails_with("#x1000000000000000", "fixnum out of range"));
    CHECK(read_fails_with("#e1.5", "no exact representation"));
    CHECK(read_fails_with("#e+inf.0", "no exact representation"));
    CHECK(read_fails_with("#x#x1", "bad # syntax"));
    CHECK(read_fails_with("#e#i1", "bad # syntax"));
    CHECK(read_fails_with("#x1.5", "bad # syntax"));
    CHECK(read_fails_with("#b2", "bad # syntax"));
}

int main(void) {
    minim_init();
    test_atoms();
    test_numbers();
    test_symbols();
    test_bar_delimits();
    test_strings();
    test_continuations();
    test_chars();
    test_lists();
    test_stream();
    test_comments();
    test_fold_case();
    test_labels();
    test_label_round_trips();
    test_errors();
    minim_shutdown();
    return test_result();
}
