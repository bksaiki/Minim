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

/** Whether the collection policy collects at nearly every allocation, as the
 * `_stress` tests' does; slow tests then do less work. */
static inline bool gc_stressed(void) {
    return minim_gc_get_policy().nursery_bytes < 1024;
}

/**
 * Sets the default collection policy, for a test that relies on when
 * collections happen; returns the policy to restore.
 */
static inline minim_gc_policy default_gc_policy(void) {
    minim_gc_policy saved = minim_gc_get_policy();
    minim_gc_set_policy((minim_gc_policy){
        .nursery_bytes = 1024 * 1024,
        .radix = 0,
        .major_growth = 2,
        .major_min_bytes = 1024 * 1024,
    });
    return saved;
}

/** Exit status for main. */
static inline int test_result(void) {
    return test_failures == 0 ? 0 : 1;
}

/** A temporary file holding `s`, positioned at its start. */
static inline FILE *input(const char *s) {
    FILE *f = tmpfile();
    fputs(s, f);
    rewind(f);
    return f;
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

#endif
