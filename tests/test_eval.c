/**
 * @file
 * Tests for the evaluator.
 */

#include "gc.h"
#include "minim.h"
#include "symbol.h"
#include "test.h"

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

static minim_obj eval_values(void *f) {
    return minim_eval_values(minim_read(f));
}

// Whether the expression `src` returns values whose list prints as `expect`.
static bool values_are(const char *src, const char *expect) {
    FILE *f = input(src);
    minim_obj val;
    bool ok = minim_catch(eval_values, f, &val);
    fclose(f);
    if (!ok) {
        fprintf(stderr, "`%s` raised ", src);
        minim_write_error(stderr);
        return false;
    }
    return prints_as(val, expect);
}

// minim_eval takes exactly one value; minim_eval_values any number.
static void test_values(void) {
    size_t before = minim_gc.values.len;
    CHECK(fails_with("(values)", "wrong number of values"));
    CHECK(fails_with("(values 1 2)", "wrong number of values"));
    CHECK_EQ(minim_gc.values.len, before);
    CHECK(evals_to("(values 1)", "1"));
    CHECK(values_are("(values)", "()"));
    CHECK(values_are("(values 1 2)", "(1 2)"));
    CHECK(values_are("5", "(5)"));
    CHECK(values_are("(call-with-values (lambda () (values 1 2)) values)", "(1 2)"));
    CHECK_EQ(minim_gc.values.len, before);
}

// The core forms binding multiple values, without the expander, so the
// evaluator's own checks run.
static void test_binding_values(void) {
    minim_obj expand = minim_symbol_value(minim_sym_expand);
    MINIM_FRAME(&expand);
    minim_set_symbol_value(minim_sym_expand, MINIM_UNBOUND);
    size_t before = minim_gc.values.len;

    CHECK(evals_to("(define-values (a b) (values 1 2)) (cons a b)", "(1 . 2)"));
    CHECK(evals_to("(define-values (a . r) (values 1 2 3)) (cons a r)", "(1 2 3)"));
    CHECK(evals_to("(define-values r (values 1 2)) r", "(1 2)"));
    CHECK(evals_to("(define-values (a) 5) a", "5"));
    CHECK(evals_to("(define-values () (values))", "#<void>"));

    CHECK(evals_to("(let-values (((a b) (values 1 2)) ((c) 3)) (list a b c))", "(1 2 3)"));
    CHECK(evals_to("(let-values (((a . r) (values 1 2 3)) (s (values))) (list a r s))",
                   "(1 (2 3) ())"));
    CHECK(evals_to("(let-values () 1 2)", "2"));
    CHECK(evals_to("(define a 1) (let-values (((a) 2) ((b) a)) (list a b))", "(2 1)"));
    CHECK(evals_to("(letrec-values (((a) 1) ((b) (+ a 1))) (list a b))", "(1 2)"));
    CHECK(evals_to("(letrec-values (((ev? od?) (values (lambda (n) (if (= n 0) #t (od? (- n 1))))"
                   " (lambda (n) (if (= n 0) #f (ev? (- n 1)))))))"
                   " (ev? 10))", "#t"));

    CHECK(fails_with("(define-values (a b) (values 1))", "wrong number of values"));
    CHECK(fails_with("(define-values (a . r) (values))", "wrong number of values"));
    CHECK(fails_with("(let-values (((a) (values 1 2))) a)", "wrong number of values"));
    CHECK(fails_with("(define-values (a 1) 2)", "bad syntax"));
    CHECK(fails_with("(define-values (a))", "bad syntax"));
    // Bodies' definitions are the expander's to turn into letrec-values.
    CHECK(fails_with("((lambda () (define a 1) a))", "bad syntax"));
    CHECK(fails_with("((lambda () (define-values (a) 1) a))", "bad syntax"));
    CHECK(fails_with("(let-values () (define a 1) a)", "bad syntax"));
    CHECK(fails_with("(let-values (((a) 1)))", "bad syntax"));
    CHECK(fails_with("(let-values ((a)) 1)", "bad syntax"));
    CHECK(fails_with("(let-values (((1) 1)) 1)", "bad syntax"));
    CHECK(fails_with("(letrec-values (((a) 1) . b) 1)", "bad syntax"));
    CHECK_EQ(minim_gc.values.len, before);

    minim_set_symbol_value(minim_sym_expand, expand);
    MINIM_RETURN_VOID;
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
    CHECK(fails_with("(define (1) 1)", "bad syntax"));
    CHECK(fails_with("(set! 1 2)", "bad syntax"));
    CHECK(fails_with("(lambda (1) 1)", "bad syntax"));
    CHECK(fails_with("(lambda (x))", "bad syntax"));
    CHECK(fails_with("(+ 1 . 2)", "bad syntax"));
}

static void test_value_stack_restored(void) {
    size_t before = minim_gc.values.len;
    CHECK(fails_with("(+ 1 (+ 2 (car 5)))", "car: not a pair"));
    CHECK_EQ(minim_gc.values.len, before);
}

// Collects, then returns its second argument.
static minim_obj collect_then_second(minim_obj *args, size_t n) {
    (void)n;
    minim_gc_collect_major();
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
    size_t before = minim_gc.values.len;
    CHECK(fails_with("(+ 1 (apply + 1 '(2 . 3)))", "apply: not a list"));
    CHECK_EQ(minim_gc.values.len, before);
}

// map in the prelude recurses on the heap, not the C stack.
static void test_map_long_list(void) {
    const long n = gc_stressed() ? 1000 : 100000;
    char src[256];
    snprintf(src, sizeof(src),
             "(define build (lambda (k acc) (if (= k 0) acc (build (- k 1) (cons k acc)))))"
             "(fold + 0 (map (lambda (x) (* 2 x)) (build %ld '())))", n);
    char expect[32];
    snprintf(expect, sizeof(expect), "%ld", n * (n + 1));  // 2 * (1 + ... + n)
    CHECK(evals_to(src, expect));
}

static void test_tail_calls(void) {
    size_t cap = minim_gc.values.cap;  // already grown by the prelude
    char src[256];
    snprintf(src, sizeof(src),
             "(define loop (lambda (n) (if (= n 0) 'done (loop (- n 1))))) (loop %d)",
             gc_stressed() ? 10000 : 100000);
    CHECK(evals_to(src, "done"));
    // Without tail calls, every iteration's frame would still be live.
    CHECK(minim_gc.size <= 8 * 1024 * 1024);
    CHECK_EQ(minim_gc.values.cap, cap);

    // `apply` in tail position is a tail call too.
    snprintf(src, sizeof(src),
             "(define loop (lambda (n) (if (= n 0) 'done (apply loop (list (- n 1)))))) (loop %d)",
             gc_stressed() ? 2000 : 100000);
    CHECK(evals_to(src, "done"));
    CHECK(minim_gc.size <= 8 * 1024 * 1024);
    CHECK_EQ(minim_gc.values.cap, cap);

    // So are the last subform of `and`, `or`, and `when`, the body of a named
    // `let` or a `let-values`, and the consumer of `call-with-values`.
    const char *loops[] = {
        "(define loop (lambda (n) (and #t (if (= n 0) 'done (loop (- n 1)))))) (loop %d)",
        "(define loop (lambda (n) (or #f (if (= n 0) 'done (loop (- n 1)))))) (loop %d)",
        "(define loop (lambda (n) (if (= n 0) 'done (when #t (loop (- n 1)))))) (loop %d)",
        "(let loop ((n %d)) (cond ((= n 0) 'done) (else (loop (- n 1)))))",
        "(define loop (lambda (n) (if (= n 0) 'done (let-values (((m) (- n 1))) (loop m)))))"
        " (loop %d)",
        "(define loop (lambda (n) (if (= n 0) 'done"
        " (call-with-values (lambda () (values (- n 1) 0)) (lambda (m z) (loop m)))))) (loop %d)",
    };
    for (size_t i = 0; i < sizeof(loops) / sizeof(loops[0]); i++) {
        snprintf(src, sizeof(src), loops[i], gc_stressed() ? 2000 : 100000);
        CHECK(evals_to(src, "done"));
        CHECK(minim_gc.size <= 8 * 1024 * 1024);
        CHECK_EQ(minim_gc.values.cap, cap);
    }
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
    test_values();
    test_binding_values();
    test_tail_calls();
    test_map_long_list();  // after test_tail_calls, which checks the value stack's capacity
    minim_shutdown();
    return test_result();
}
