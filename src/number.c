/**
 * @file
 * Numbers: parsing, and fixnum arithmetic, comparison, and conversion.
 */

#include "number.h"

#include <math.h>
#include <stdlib.h>

#include "heap.h"
#include "prims.h"

// Value of the digit `c` in `radix`, or -1 if it isn't one.
static int digit_value(uint32_t c, unsigned radix) {
    int d = c >= '0' && c <= '9'   ? (int)(c - '0')
          : c >= 'a' && c <= 'z' ? (int)(c - 'a' + 10)
          : c >= 'A' && c <= 'Z' ? (int)(c - 'A' + 10)
                                   : -1;
    return d >= 0 && (unsigned)d < radix ? d : -1;
}

// Whether the `n` chars `s` are the ASCII string `t`.
static bool chars_are(const uint32_t *s, size_t n, const char *t) {
    size_t i = 0;
    for (; i < n && t[i] != '\0'; i++) {
        if (s[i] != (unsigned char)t[i])
            return false;
    }
    return i == n && t[i] == '\0';
}

// The double the `n` chars `s`, an ASCII decimal, denote, correctly rounded.
static double parse_decimal(const uint32_t *s, size_t n) {
    char *buf = malloc(n + 1);
    if (buf == NULL)
        minim_out_of_memory();
    for (size_t i = 0; i < n; i++)
        buf[i] = (char)s[i];
    buf[n] = '\0';
    double d = strtod(buf, NULL);
    free(buf);
    return d;
}

// Length of the decimal's digits, `.`, and exponent at the start of the `n`
// chars `s`, which have no sign; 0 if there are no digits.
static size_t decimal_length(const uint32_t *s, size_t n, bool *integer) {
    size_t i = 0;
    size_t digits = 0;
    for (; i < n && digit_value(s[i], 10) >= 0; i++)
        digits++;
    *integer = true;
    if (i < n && s[i] == '.') {
        *integer = false;
        for (i++; i < n && digit_value(s[i], 10) >= 0; i++)
            digits++;
    }
    if (digits == 0)
        return 0;
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        size_t j = i + 1;
        if (j < n && (s[j] == '+' || s[j] == '-'))
            j++;
        size_t start = j;
        while (j < n && digit_value(s[j], 10) >= 0)
            j++;
        if (j > start) {
            *integer = false;
            i = j;
        }
    }
    return i;
}

minim_parse minim_parse_number(const uint32_t *s, size_t n, unsigned radix, minim_obj *x) {
    // A radix and an exactness prefix, each at most once, in either order.
    uint32_t exactness = 0;
    bool radix_given = false;
    for (; n >= 2 && s[0] == '#'; s += 2, n -= 2) {
        uint32_t c = s[1] | 0x20;  // lowercase, for letters
        if ((c == 'e' || c == 'i') && exactness == 0) {
            exactness = c;
        } else if ((c == 'x' || c == 'o' || c == 'b' || c == 'd') && !radix_given) {
            radix = c == 'x' ? 16 : c == 'o' ? 8 : c == 'b' ? 2 : 10;
            radix_given = true;
        } else {
            return MINIM_PARSE_NOT_NUMBER;
        }
    }

    bool negative = n > 0 && s[0] == '-';
    size_t sign = n > 0 && (s[0] == '+' || s[0] == '-');
    double d;
    if (sign && chars_are(s + 1, n - 1, "inf.0")) {
        d = negative ? -INFINITY : INFINITY;
    } else if (sign && chars_are(s + 1, n - 1, "nan.0")) {
        d = NAN;
    } else {
        bool integer = true;
        size_t len = radix == 10 ? decimal_length(s + sign, n - sign, &integer) : 0;
        if (radix != 10) {
            while (sign + len < n && digit_value(s[sign + len], radix) >= 0)
                len++;
        }
        if (len == 0 || sign + len != n)
            return MINIM_PARSE_NOT_NUMBER;

        if (integer && exactness != 'i') {
            uint64_t limit = (uint64_t)MINIM_FIXNUM_MAX + (negative ? 1 : 0);
            uint64_t magnitude = 0;
            for (size_t i = sign; i < n; i++) {
                uint64_t digit = (uint64_t)digit_value(s[i], radix);
                if (magnitude > (limit - digit) / radix)
                    return MINIM_PARSE_OUT_OF_RANGE;
                magnitude = radix * magnitude + digit;
            }
            // |MINIM_FIXNUM_MIN| fits in intptr_t, so negation can't overflow.
            *x = minim_fixnum(negative ? -(intptr_t)magnitude : (intptr_t)magnitude);
            return MINIM_PARSE_OK;
        }
        if (radix == 10) {
            d = parse_decimal(s, n);
        } else {  // an inexact integer in another radix
            d = 0;
            for (size_t i = sign; i < n; i++)
                d = radix * d + digit_value(s[i], radix);
            d = negative ? -d : d;
        }
    }

    if (exactness == 'e') {
        if (!isfinite(d) || d != floor(d))
            return MINIM_PARSE_NOT_EXACT;
        if (d < -0x1p60 || d >= 0x1p60)
            return MINIM_PARSE_OUT_OF_RANGE;
        *x = minim_fixnum((intptr_t)d);
        return MINIM_PARSE_OK;
    }
    *x = minim_make_flonum(d);
    return MINIM_PARSE_OK;
}

// Fixnum sums never overflow intptr_t, so range checks after each step suffice.
static minim_obj prim_add(minim_obj *args, size_t n) {
    intptr_t sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += fixnum_arg(args[i], "+: not a fixnum");
        fixnum_result(sum, "+: fixnum overflow");
    }
    return minim_fixnum(sum);
}

static minim_obj prim_sub(minim_obj *args, size_t n) {
    intptr_t v = fixnum_arg(args[0], "-: not a fixnum");
    if (n == 1)
        return fixnum_result(-v, "-: fixnum overflow");
    for (size_t i = 1; i < n; i++) {
        v -= fixnum_arg(args[i], "-: not a fixnum");
        fixnum_result(v, "-: fixnum overflow");
    }
    return minim_fixnum(v);
}

static minim_obj prim_mul(minim_obj *args, size_t n) {
    intptr_t product = 1;
    for (size_t i = 0; i < n; i++) {
        intptr_t v = fixnum_arg(args[i], "*: not a fixnum");
        // Compare magnitudes, which fit in uint64_t, before multiplying.
        uint64_t a = product < 0 ? -(uint64_t)product : (uint64_t)product;
        uint64_t b = v < 0 ? -(uint64_t)v : (uint64_t)v;
        bool negative = (product < 0) != (v < 0);
        uint64_t limit = (uint64_t)MINIM_FIXNUM_MAX + (negative ? 1 : 0);
        if (b != 0 && a > limit / b)
            minim_error("*: fixnum overflow", MINIM_VOID);
        product *= v;
    }
    return minim_fixnum(product);
}

static minim_obj prim_quotient(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    intptr_t a = fixnum_arg(args[0], "quotient: not a fixnum");
    intptr_t b = fixnum_arg(args[1], "quotient: not a fixnum");
    check(b != 0, "quotient: division by zero", args[1]);
    return fixnum_result(a / b, "quotient: fixnum overflow");
}

static minim_obj prim_remainder(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    intptr_t a = fixnum_arg(args[0], "remainder: not a fixnum");
    intptr_t b = fixnum_arg(args[1], "remainder: not a fixnum");
    check(b != 0, "remainder: division by zero", args[1]);
    return minim_fixnum(a % b);
}

// The radix args[i]: 2, 8, 10, or 16, or 10 if absent; raises `msg` otherwise.
static unsigned radix_arg(minim_obj *args, size_t n, size_t i, const char *msg) {
    if (n <= i)
        return 10;
    intptr_t radix = minim_is_fixnum(args[i]) ? minim_fixnum_value(args[i]) : 0;
    check(radix == 2 || radix == 8 || radix == 10 || radix == 16, msg, args[i]);
    return (unsigned)radix;
}

// The digits of the number args[0] in the radix args[1]: 2, 8, 10 (default), or
// 16, a flonum's only 10, as the printer writes them.
static minim_obj prim_number_to_string(minim_obj *args, size_t n) {
    unsigned radix = radix_arg(args, n, 1, "number->string: bad radix");
    if (minim_is_flonum(args[0])) {
        check(radix == 10, "number->string: bad radix", args[1]);
        minim_obj port = minim_open_output_string();
        MINIM_FRAME(&port);
        minim_port_write(port, args[0]);
        MINIM_RETURN(minim_get_output_string(port));
    }
    intptr_t v = fixnum_arg(args[0], "number->string: not a number");
    char buf[66];  // a sign, 64 binary digits, and a NUL
    char *p = buf + sizeof(buf);
    *--p = '\0';
    uintptr_t u = v < 0 ? -(uintptr_t)v : (uintptr_t)v;
    do {
        *--p = "0123456789abcdef"[u % (uintptr_t)radix];
        u /= (uintptr_t)radix;
    } while (u != 0);
    if (v < 0)
        *--p = '-';
    return minim_string_from_cstr(p);
}

static minim_obj prim_num_eq(minim_obj *args, size_t n) {
    return compare(args, n, fixnum_arg, eq, "=: not a fixnum");
}

static minim_obj prim_lt(minim_obj *args, size_t n) {
    return compare(args, n, fixnum_arg, lt, "<: not a fixnum");
}

static minim_obj prim_gt(minim_obj *args, size_t n) {
    return compare(args, n, fixnum_arg, gt, ">: not a fixnum");
}

static minim_obj prim_le(minim_obj *args, size_t n) {
    return compare(args, n, fixnum_arg, le, "<=: not a fixnum");
}

static minim_obj prim_ge(minim_obj *args, size_t n) {
    return compare(args, n, fixnum_arg, ge, ">=: not a fixnum");
}

// The number the string args[0] denotes in the radix args[1], or #f if it
// isn't one.
static minim_obj prim_string_to_number(minim_obj *args, size_t n) {
    check(minim_is_string(args[0]), "string->number: not a string", args[0]);
    unsigned radix = radix_arg(args, n, 1, "string->number: bad radix");
    minim_obj x;
    const uint32_t *chars = minim_string_chars(args[0]);
    switch (minim_parse_number(chars, minim_string_length(args[0]), radix, &x)) {
    case MINIM_PARSE_OK:
        return x;
    case MINIM_PARSE_NOT_NUMBER:
        return MINIM_FALSE;
    case MINIM_PARSE_OUT_OF_RANGE:
        minim_error("string->number: fixnum out of range", args[0]);
    default:
        minim_error("string->number: no exact representation", args[0]);
    }
}

static minim_obj prim_is_fixnum(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_fixnum(args[0]));
}

static minim_obj prim_is_zero(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(fixnum_arg(args[0], "zero?: not a fixnum") == 0);
}

const minim_prim_def minim_number_prims[] = {
    {"+", prim_add, 0, MINIM_VARIADIC},
    {"-", prim_sub, 1, MINIM_VARIADIC},
    {"*", prim_mul, 0, MINIM_VARIADIC},
    {"=", prim_num_eq, 1, MINIM_VARIADIC},
    {"<", prim_lt, 1, MINIM_VARIADIC},
    {">", prim_gt, 1, MINIM_VARIADIC},
    {"<=", prim_le, 1, MINIM_VARIADIC},
    {">=", prim_ge, 1, MINIM_VARIADIC},
    {"quotient", prim_quotient, 2, 2},
    {"remainder", prim_remainder, 2, 2},
    {"number->string", prim_number_to_string, 1, 2},
    {"string->number", prim_string_to_number, 1, 2},
    {"fixnum?", prim_is_fixnum, 1, 1},
    {"zero?", prim_is_zero, 1, 1},
    {NULL, NULL, 0, 0},
};
