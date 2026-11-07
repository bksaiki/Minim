/**
 * @file
 * Tests for the printer.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gc.h"
#include "minim.h"
#include "read.h"
#include "test.h"

static minim_obj list2(minim_obj a, minim_obj b) {
    MINIM_FRAME(&a, &b);
    minim_obj tail = minim_cons(b, MINIM_NIL);
    MINIM_RETURN(minim_cons(a, tail));
}

static minim_obj list3(minim_obj a, minim_obj b, minim_obj c) {
    MINIM_FRAME(&a);
    minim_obj tail = list2(b, c);
    MINIM_RETURN(minim_cons(a, tail));
}

static void test_immediates(void) {
    CHECK(prints_as(minim_fixnum(0), "0"));
    CHECK(prints_as(minim_fixnum(-42), "-42"));
    CHECK(prints_as(minim_fixnum(MINIM_FIXNUM_MAX), "1152921504606846975"));
    CHECK(prints_as(MINIM_TRUE, "#t"));
    CHECK(prints_as(MINIM_FALSE, "#f"));
    CHECK(prints_as(MINIM_NIL, "()"));
    CHECK(prints_as(MINIM_VOID, "#<void>"));
    CHECK(prints_as(MINIM_EOF, "#<eof>"));
    CHECK(prints_as(MINIM_UNBOUND, "#<unbound>"));
}

// Flonums print in the fewest digits that read back as them.
static void test_flonums(void) {
    static const struct {
        double d;
        const char *s;
    } cases[] = {
        {1.0, "1.0"},
        {0.0, "0.0"},
        {-0.0, "-0.0"},
        {0.1, "0.1"},
        {-2.5, "-2.5"},
        {100.0, "100.0"},
        {123.456, "123.456"},
        {1e20, "100000000000000000000.0"},
        {1e21, "1e21"},
        {1e-6, "0.000001"},
        {1e-7, "1e-7"},
        {1.5e-7, "1.5e-7"},
        {5e-324, "5e-324"},
        {1.7976931348623157e308, "1.7976931348623157e308"},
        {0.1 + 0.2, "0.30000000000000004"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        CHECK(prints_as(minim_make_flonum(cases[i].d), cases[i].s));
    CHECK(prints_as(minim_make_flonum(INFINITY), "+inf.0"));
    CHECK(prints_as(minim_make_flonum(-INFINITY), "-inf.0"));
    CHECK(prints_as(minim_make_flonum(NAN), "+nan.0"));
}

static void test_chars(void) {
    CHECK(prints_as(minim_char('a'), "#\\a"));
    CHECK(prints_as(minim_char(' '), "#\\space"));
    CHECK(prints_as(minim_char('\n'), "#\\newline"));
    CHECK(prints_as(minim_char('\t'), "#\\tab"));
    CHECK(prints_as(minim_char(0), "#\\null"));
    CHECK(prints_as(minim_char(0x07), "#\\alarm"));
    CHECK(prints_as(minim_char(0x08), "#\\backspace"));
    CHECK(prints_as(minim_char('\r'), "#\\return"));
    CHECK(prints_as(minim_char(0x1B), "#\\escape"));
    CHECK(prints_as(minim_char(0x7F), "#\\delete"));
    CHECK(prints_as(minim_char(0x01), "#\\x1"));
    CHECK(prints_as(minim_char(0xE9), "#\\\u00e9"));
    CHECK(prints_as(minim_char(0x1F600), "#\\\U0001F600"));
}

static void test_strings(void) {
    CHECK(prints_as(minim_string_from_cstr(""), "\"\""));
    CHECK(prints_as(minim_string_from_cstr("a\"b\\c\nd\te"), "\"a\\\"b\\\\c\\nd\\te\""));
    CHECK(prints_as(minim_string_from_cstr("\x01"), "\"\\x1;\""));
    CHECK(prints_as(minim_string_from_cstr("\a\b\r"), "\"\\a\\b\\r\""));
    CHECK(prints_as(minim_string_from_cstr("caf\u00e9 \U0001F600"), "\"caf\u00e9 \U0001F600\""));
}

// Whether the symbol named `name` is written as `expect` and reads back as
// itself.
static bool writes_symbol(const char *name, const char *expect) {
    minim_obj sym = minim_intern_cstr(name);
    minim_obj x = MINIM_NIL;
    MINIM_FRAME(&sym, &x);
    bool ok = prints_as(sym, expect);
    const char *p = expect;
    x = minim_read_string(&p);
    if (x != sym) {
        fprintf(stderr, "`%s` didn't read back as itself\n", expect);
        ok = false;
    }
    MINIM_RETURN(ok);
}

static void test_symbols(void) {
    // Names that read back as themselves are written bare.
    static const char *const bare[] = {
        "foo", "caf\u00e9", "\u03bb", "a", "+", "-", "...", "->x", "1+", "a#b", "a,b", "a'b",
    };
    for (size_t i = 0; i < sizeof(bare) / sizeof(*bare); i++)
        CHECK(writes_symbol(bare[i], bare[i]));

    // Others are written in bars.
    static const struct {
        const char *name;
        const char *written;
    } barred[] = {
        {"", "||"},
        {".", "|.|"},
        {"1", "|1|"},
        {"+3", "|+3|"},
        {"-.4", "|-.4|"},
        {"+inf.0", "|+inf.0|"},
        {"1152921504606846976", "|1152921504606846976|"},
        {"+i", "|+i|"},
        {"-i", "|-i|"},
        {"a b", "|a b|"},
        {"a(b", "|a(b|"},
        {"a\"b", "|a\"b|"},
        {"a;b", "|a;b|"},
        {"a|b", "|a\\|b|"},
        {"a\\b", "|a\\\\b|"},
        {"#a", "|#a|"},
        {"'a", "|'a|"},
        {"`a", "|`a|"},
        {",a", "|,a|"},
        {"a\nb", "|a\\nb|"},
        {"a\x01" "b", "|a\\x1;b|"},
    };
    for (size_t i = 0; i < sizeof(barred) / sizeof(*barred); i++)
        CHECK(writes_symbol(barred[i].name, barred[i].written));
}

static void test_pairs(void) {
    minim_obj x = list3(minim_fixnum(1), minim_fixnum(2), minim_fixnum(3));
    MINIM_FRAME(&x);
    CHECK(prints_as(x, "(1 2 3)"));

    x = minim_cons(minim_fixnum(1), minim_fixnum(2));
    CHECK(prints_as(x, "(1 . 2)"));

    x = minim_cons(minim_fixnum(2), minim_fixnum(3));
    x = minim_cons(minim_fixnum(1), x);
    CHECK(prints_as(x, "(1 2 . 3)"));

    x = list2(minim_fixnum(1), MINIM_NIL);
    x = list2(x, minim_fixnum(2));
    CHECK(prints_as(x, "((1 ()) 2)"));
    MINIM_RETURN_VOID;
}

static void test_quote(void) {
    minim_obj quote = minim_intern_cstr("quote");
    minim_obj x = MINIM_NIL;
    MINIM_FRAME(&quote, &x);
    x = minim_intern_cstr("x");
    x = list2(quote, x);
    CHECK(prints_as(x, "'x"));

    x = list2(quote, x);
    CHECK(prints_as(x, "''x"));

    x = list3(quote, minim_fixnum(1), minim_fixnum(2));
    CHECK(prints_as(x, "(quote 1 2)"));

    x = minim_cons(quote, MINIM_NIL);
    CHECK(prints_as(x, "(quote)"));
    MINIM_RETURN_VOID;
}

// `quasiquote` with two operands isn't abbreviated.
static void test_quasiquote(void) {
    minim_obj x = list3(minim_intern_cstr("quasiquote"), minim_fixnum(1), minim_fixnum(2));
    CHECK(prints_as(x, "(quasiquote 1 2)"));
}

static void test_vectors(void) {
    minim_obj v = minim_make_vector(0, MINIM_NIL);
    minim_obj inner = MINIM_NIL;
    MINIM_FRAME(&v, &inner);
    CHECK(prints_as(v, "#()"));

    v = minim_make_vector(3, MINIM_NIL);
    inner = minim_make_vector(1, minim_fixnum(2));
    minim_vector_set(v, 0, minim_fixnum(1));
    minim_vector_set(v, 1, inner);
    inner = minim_string_from_cstr("s");
    minim_vector_set(v, 2, inner);
    CHECK(prints_as(v, "#(1 #(2) \"s\")"));

    // Printing allocates nothing.
    size_t before = minim_gc.allocated;
    CHECK(prints_as(v, "#(1 #(2) \"s\")"));
    CHECK_EQ(minim_gc.allocated, before);
    MINIM_RETURN_VOID;
}

static void test_bytevectors(void) {
    minim_obj bv = minim_make_bytevector(0, 0);
    MINIM_FRAME(&bv);
    CHECK(prints_as(bv, "#u8()"));
    bv = minim_make_bytevector(3, 0);
    minim_bytevector_data(bv)[1] = 1;
    minim_bytevector_data(bv)[2] = 255;
    CHECK(prints_as(bv, "#u8(0 1 255)"));
    MINIM_RETURN_VOID;
}

// Returns void.
static minim_obj nothing(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    return MINIM_VOID;
}

// A procedure prints with its name, if it has one.
static void test_procedures(void) {
    minim_obj p = minim_make_closure(MINIM_NIL, MINIM_NIL);
    minim_obj name = MINIM_NIL;
    MINIM_FRAME(&p, &name);
    CHECK(prints_as(p, "#<procedure>"));
    name = minim_intern_cstr("f");
    minim_set_closure_name(p, name);
    CHECK(prints_as(p, "#<procedure f>"));
    p = minim_intern_cstr("nothing");
    p = minim_make_primitive(p, nothing, 0, 0);
    CHECK(prints_as(p, "#<procedure nothing>"));
    MINIM_RETURN_VOID;
}

// A record prints its type's name, without angle brackets; an rtd, as a record type.
static void test_records(void) {
    minim_obj rtd = make_record_type("<point>", NULL, 0);
    minim_obj r = MINIM_NIL;
    MINIM_FRAME(&rtd, &r);
    CHECK(prints_as(rtd, "#<record-type point>"));
    r = minim_make_record(rtd);
    CHECK(prints_as(r, "#<point>"));
    rtd = make_record_type("<", NULL, 0);
    CHECK(prints_as(rtd, "#<record-type <>"));
    CHECK(prints_as(minim_base_rtd, "#<record-type record-type>"));
    MINIM_RETURN_VOID;
}

// Whether writing `src`'s value to a string port gives what writing it to a
// file does: #t or #f.
static minim_obj port_writes_like_file(const char *src) {
    minim_obj x = eval_source((void *)src);
    minim_obj s = MINIM_NIL;
    MINIM_FRAME(&x, &s);
    s = minim_open_output_string();
    minim_port_write(s, x);
    s = minim_get_output_string(s);
    FILE *f = tmpfile();
    minim_write(f, x);
    size_t len = (size_t)ftell(f);
    char *bytes = calloc(len + 1, 1);
    rewind(f);
    bool ok = fread(bytes, 1, len, f) == len;
    fclose(f);
    minim_obj expect = minim_string_from_cstr(bytes);
    free(bytes);
    ok = ok && minim_string_length(expect) == minim_string_length(s)
         && memcmp(minim_string_chars(expect), minim_string_chars(s),
                   minim_string_length(s) * sizeof(uint32_t)) == 0;
    if (!ok)
        fprintf(stderr, "`%s` wrote differently to a port\n", src);
    MINIM_RETURN(ok ? MINIM_TRUE : MINIM_FALSE);
}

// Writing to a string port matches writing to a file, even when growing its
// buffer collects.
static void test_port(void) {
    const char *srcs[] = {
        "'(1 \"two\\n\" #\\3 #(a (b . c)) caf\u00e9 '(q ,u ,@s))",
        "(make-string 1000 #\\\u03bb)",
        "(let loop ((i 0) (l '())) (if (= i 300) l (loop (+ i 1) (cons (vector i) l))))",
        "(list car (open-output-string) (eof-object))",
    };
    for (size_t i = 0; i < sizeof(srcs) / sizeof(srcs[0]); i++)
        CHECK_EQ(port_writes_like_file(srcs[i]), MINIM_TRUE);
}

int main(void) {
    minim_init();
    test_immediates();
    test_flonums();
    test_chars();
    test_strings();
    test_symbols();
    test_pairs();
    test_quote();
    test_quasiquote();
    test_vectors();
    test_bytevectors();
    test_procedures();
    test_records();
    test_port();
    minim_shutdown();
    return test_result();
}
