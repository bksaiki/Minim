/**
 * @file
 * Tests for the evaluator.
 */

#include "gc.h"
#include "heap.h"
#include "minim.h"
#include "test.h"

// A file holding `s`, positioned at its start.
static FILE *input(const char *s) {
    FILE *f = tmpfile();
    fputs(s, f);
    rewind(f);
    return f;
}

// Evaluates every datum in the file `f`; returns the last value.
static minim_obj eval_file(void *f) {
    minim_obj val = MINIM_VOID;
    minim_obj x = MINIM_NIL;
    MINIM_FRAME(&val, &x);
    while ((x = minim_read(f)) != MINIM_EOF)
        val = minim_eval(x);
    MINIM_RETURN(val);
}

// Whether evaluating `src` gives a value that prints as `expect`.
static bool evals_to(const char *src, const char *expect) {
    FILE *f = input(src);
    minim_obj val;
    bool ok = minim_catch(eval_file, f, &val);
    fclose(f);
    if (!ok) {
        fprintf(stderr, "`%s` raised ", src);
        minim_write_error(stderr);
        return false;
    }
    return prints_as(val, expect);
}

// Whether evaluating `src` raises the error `msg`.
static bool fails_with(const char *src, const char *msg) {
    FILE *f = input(src);
    minim_obj val;
    bool ok = minim_catch(eval_file, f, &val);
    fclose(f);
    if (ok) {
        fprintf(stderr, "`%s` evaluated without error\n", src);
        return false;
    }

    const char *actual;
    minim_obj irritant;
    minim_last_error(&actual, &irritant);
    if (strcmp(actual, msg) != 0) {
        fprintf(stderr, "`%s` raised `%s`, expected `%s`\n", src, actual, msg);
        return false;
    }
    return true;
}

static void test_self_evaluating(void) {
    CHECK(evals_to("42", "42"));
    CHECK(evals_to("\"s\"", "\"s\""));
    CHECK(evals_to("#\\a", "#\\a"));
    CHECK(evals_to("#f", "#f"));
    CHECK(evals_to("#(1 x)", "#(1 x)"));
}

static void test_special_forms(void) {
    CHECK(evals_to("'(1 x)", "(1 x)"));
    CHECK(evals_to("(quote x)", "x"));
    CHECK(evals_to("(if #t 1 2)", "1"));
    CHECK(evals_to("(if #f 1 2)", "2"));
    CHECK(evals_to("(if 0 1 2)", "1"));
    CHECK(evals_to("(if #f 1)", "#<void>"));
    CHECK(evals_to("(define x 5) x", "5"));
    CHECK(evals_to("(define x 1) (set! x 2) x", "2"));
    CHECK(evals_to("(define f (lambda (a b) (+ a b))) (f 2 3)", "5"));
    CHECK(evals_to("(define x 1)", "#<void>"));
    CHECK(evals_to("((lambda (x y) (+ x y)) 1 2)", "3"));
    CHECK(evals_to("(lambda (x) x)", "#<procedure>"));
    CHECK(evals_to("(begin 1 2 3)", "3"));
    CHECK(evals_to("(begin)", "#<void>"));
}

static void test_parameters(void) {
    CHECK(evals_to("((lambda (x . r) r) 1 2 3)", "(2 3)"));
    CHECK(evals_to("((lambda (x . r) r) 1)", "()"));
    CHECK(evals_to("((lambda r r))", "()"));
    CHECK(evals_to("((lambda r r) 1 2)", "(1 2)"));
    CHECK(evals_to("(define f (lambda r r)) (f 1 2)", "(1 2)"));
}

static void test_closures(void) {
    const char *counter =
        "(define make-counter"
        "  (lambda ()"
        "    (define n 0)"
        "    (lambda () (set! n (+ n 1)) n)))"
        "(define a (make-counter))"
        "(define b (make-counter))";
    char src[512];
    snprintf(src, sizeof(src), "%s (a) (a) (b) (a)", counter);
    CHECK(evals_to(src, "3"));
    snprintf(src, sizeof(src), "%s (a) (b) (b)", counter);
    CHECK(evals_to(src, "2"));

    CHECK(evals_to("(define f (lambda () (define a 1) (define b 2) (+ a b))) (f)", "3"));
    CHECK(evals_to("(define adder (lambda (n) (lambda (x) (+ x n)))) ((adder 3) 4)", "7"));
    CHECK(evals_to("(define x 1) (define f (lambda (x) (set! x 5) x)) (f 0)", "5"));
    CHECK(evals_to("(define x 1) (define f (lambda (x) (set! x 5) x)) (f 0) x", "1"));
}

static void test_primitives(void) {
    CHECK(evals_to("(+)", "0"));
    CHECK(evals_to("(+ 1 2 3)", "6"));
    CHECK(evals_to("(- 5)", "-5"));
    CHECK(evals_to("(- 10 3 2)", "5"));
    CHECK(evals_to("(* 3 -4)", "-12"));
    CHECK(evals_to("(= 1 1 1)", "#t"));
    CHECK(evals_to("(= 1 1 2)", "#f"));
    CHECK(evals_to("(< 1 2 3)", "#t"));
    CHECK(evals_to("(< 1 3 2)", "#f"));
    CHECK(evals_to("(cons 1 2)", "(1 . 2)"));
    CHECK(evals_to("(car '(1 2))", "1"));
    CHECK(evals_to("(cdr '(1 2))", "(2)"));
    CHECK(evals_to("car", "#<procedure car>"));
    CHECK(evals_to("(* 1152921504606846975 -1)", "-1152921504606846975"));
    CHECK(evals_to("(- -1152921504606846975 1)", "-1152921504606846976"));
}

static void test_errors(void) {
    CHECK(fails_with("y", "unbound variable"));
    CHECK(fails_with("(set! y 1)", "unbound variable"));
    CHECK(fails_with("(5)", "not a procedure"));
    CHECK(fails_with("((lambda (x) x))", "arity mismatch"));
    CHECK(fails_with("((lambda (x . r) x))", "arity mismatch"));
    CHECK(fails_with("(car)", "arity mismatch"));
    CHECK(fails_with("(car 5)", "car: not a pair"));
    CHECK(fails_with("(+ 1 #t)", "+: not a fixnum"));
    CHECK(fails_with("(+ 1152921504606846975 1)", "+: fixnum overflow"));
    CHECK(fails_with("(* 1152921504606846975 2)", "*: fixnum overflow"));
    CHECK(fails_with("(- -1152921504606846976)", "-: fixnum overflow"));
    CHECK(fails_with("()", "bad syntax"));
    CHECK(fails_with("(if)", "bad syntax"));
    CHECK(fails_with("(quote 1 2)", "bad syntax"));
    CHECK(fails_with("(define)", "bad syntax"));
    CHECK(fails_with("(define x)", "bad syntax"));
    CHECK(fails_with("(define x 1 2)", "bad syntax"));
    CHECK(fails_with("(define (f) 1)", "bad syntax"));
    CHECK(fails_with("(set! 1 2)", "bad syntax"));
    CHECK(fails_with("(lambda (1) 1)", "bad syntax"));
    CHECK(fails_with("(lambda (x))", "bad syntax"));
    CHECK(fails_with("(+ 1 . 2)", "bad syntax"));
}

static void test_value_stack_restored(void) {
    size_t before = minim_values.len;
    CHECK(fails_with("(+ 1 (+ 2 (car 5)))", "car: not a pair"));
    CHECK_EQ(minim_values.len, before);
}

// Collects, then returns its second argument.
static minim_obj collect_then_second(minim_obj *args, size_t n) {
    (void)n;
    minim_gc_collect();
    return args[1];
}

static void test_primitive_allocates(void) {
    minim_obj sym = minim_intern_cstr("collect-then-second");
    MINIM_FRAME(&sym);
    minim_obj p = minim_make_primitive(sym, collect_then_second, 2, 2);
    minim_set_symbol_value(sym, p);
    CHECK(evals_to("(collect-then-second 1 (cons 2 '(3)))", "(2 3)"));
    MINIM_RETURN_VOID;
}

static void test_apply_restores_value_stack(void) {
    size_t before = minim_values.len;
    CHECK(fails_with("(+ 1 (apply + 1 '(2 . 3)))", "apply: not a list"));
    CHECK_EQ(minim_values.len, before);
}

static void test_tail_calls(void) {
#ifdef MINIM_GC_STRESS
    const char *src = "(define loop (lambda (n) (if (= n 0) 'done (loop (- n 1))))) (loop 10000)";
#else
    const char *src = "(define loop (lambda (n) (if (= n 0) 'done (loop (- n 1))))) (loop 100000)";
#endif
    CHECK(evals_to(src, "done"));
    // Without tail calls, every iteration's frame would still be live.
    CHECK(minim_heap.size <= 8 * 1024 * 1024);
    CHECK(minim_values.cap <= 64);

    // `apply` in tail position is a tail call too.
#ifdef MINIM_GC_STRESS
    src = "(define loop (lambda (n) (if (= n 0) 'done (apply loop (list (- n 1)))))) (loop 2000)";
#else
    src = "(define loop (lambda (n) (if (= n 0) 'done (apply loop (list (- n 1)))))) (loop 100000)";
#endif
    CHECK(evals_to(src, "done"));
    CHECK(minim_heap.size <= 8 * 1024 * 1024);
    CHECK(minim_values.cap <= 64);
}

int main(void) {
    minim_init();
    test_self_evaluating();
    test_special_forms();
    test_parameters();
    test_closures();
    test_primitives();
    test_errors();
    test_value_stack_restored();
    test_apply_restores_value_stack();
    test_primitive_allocates();
    test_tail_calls();
    minim_shutdown();
    return test_result();
}
