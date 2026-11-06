/**
 * @file
 * Minimal test harness.
 */

#ifndef MINIM_TEST_H
#define MINIM_TEST_H

#include <stdio.h>
#include <string.h>

#include "minim.h"

/** Number of failed checks. */
static int test_failures = 0;

/** Records a failure unless `cond` holds. */
#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n",                    \
                    __FILE__, __LINE__, #cond);                             \
            test_failures++;                                                \
        }                                                                   \
    } while (0)

/** Records a failure unless `a == b`, printing both values. */
#define CHECK_EQ(a, b)                                                      \
    do {                                                                    \
        long long a_ = (long long)(a), b_ = (long long)(b);                 \
        if (a_ != b_) {                                                     \
            fprintf(stderr, "%s:%d: CHECK_EQ failed: %s == %s (%lld vs %lld)\n", \
                    __FILE__, __LINE__, #a, #b, a_, b_);                    \
            test_failures++;                                                \
        }                                                                   \
    } while (0)

/**
 * Whether the collector runs at nearly every allocation, as in the `_stress`
 * copies.
 */
static inline bool gc_stressed(void) {
    return minim_gc_get_config().nursery_bytes < 1024;
}

/**
 * Sets the default collector settings, for a test that relies on when
 * collections happen; returns the settings to restore.
 */
static inline minim_gc_config use_default_gc_config(void) {
    minim_gc_config saved = minim_gc_get_config();
    minim_gc_set_config(minim_gc_default_config());
    return saved;
}

/** Exit status for main. */
static inline int test_result(void) {
    return test_failures == 0 ? 0 : 1;
}

/** A temporary file holding `s`, positioned at its start. */
static inline FILE *input_file(const char *s) {
    FILE *f = tmpfile();
    fputs(s, f);
    rewind(f);
    return f;
}

/** Number of type predicates `x` satisfies: one for every value with a type. */
static inline int count_predicates(minim_obj x) {
    return minim_is_fixnum(x) + minim_is_char(x) + minim_is_boolean(x) + minim_is_null(x)
        + minim_is_pair(x) + minim_is_symbol(x) + minim_is_closure(x) + minim_is_vector(x)
        + minim_is_string(x) + minim_is_primitive(x) + minim_is_continuation(x)
        + minim_is_error_object(x) + minim_is_record(x) + minim_is_port(x)
        + minim_is_bytevector(x) + minim_is_flonum(x);
}

/** Whether the last error raised has the message `msg`; reports a mismatch. */
static inline bool last_error_is(const char *msg) {
    const char *actual;
    minim_obj irritant;
    minim_last_error(&actual, &irritant);
    if (strcmp(actual, msg) == 0)
        return true;
    fprintf(stderr, "raised `%s`, expected `%s`\n", actual, msg);
    return false;
}

/** A new record type named `name`, with the `n` fields named `fields`. */
static inline minim_obj make_record_type(const char *name, const char *const *fields, size_t n) {
    minim_obj names = MINIM_NIL;
    minim_obj rtd = MINIM_NIL;
    MINIM_FRAME(&names, &rtd);
    for (size_t i = n; i > 0; i--) {
        minim_obj sym = minim_intern_cstr(fields[i - 1]);
        names = minim_cons(sym, names);
    }
    rtd = minim_make_record(minim_base_rtd);
    minim_record_set(rtd, MINIM_RTD_NFIELDS, minim_fixnum((intptr_t)n));
    minim_record_set(rtd, MINIM_RTD_FIELDS, names);
    minim_obj sym = minim_intern_cstr(name);
    minim_record_set(rtd, MINIM_RTD_NAME, sym);
    MINIM_RETURN(rtd);
}

/** A new input port reading the string `s`. */
static inline minim_obj input(const char *s) {
    return minim_open_input_string(minim_string_from_cstr(s));
}

/** Evaluates every datum of the source text `src`; returns the last value. */
static inline minim_obj eval_source(void *src) {
    minim_obj port = input(src);
    minim_obj val = MINIM_VOID;
    minim_obj x = MINIM_NIL;
    MINIM_FRAME(&port, &val, &x);
    while ((x = minim_read(port)) != MINIM_EOF)
        val = minim_eval(x);
    MINIM_RETURN(val);
}

/** Whether `x` prints as `expect`; reports a mismatch. */
static inline bool prints_as(minim_obj x, const char *expect) {
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

/** Whether evaluating `src` gives a value that prints as `expect`. */
static inline bool evals_to(const char *src, const char *expect) {
    minim_obj val;
    bool ok = minim_catch(eval_source, (void *)src, &val);
    if (!ok) {
        fprintf(stderr, "`%s` raised ", src);
        minim_write_error(stderr);
        return false;
    }
    return prints_as(val, expect);
}

/** Whether evaluating `src` raises the error `msg`. */
static inline bool fails_with(const char *src, const char *msg) {
    minim_obj val;
    bool ok = minim_catch(eval_source, (void *)src, &val);
    if (ok) {
        fprintf(stderr, "`%s` evaluated without error\n", src);
        return false;
    }
    if (last_error_is(msg))
        return true;
    fprintf(stderr, "  in `%s`\n", src);
    return false;
}

#endif
