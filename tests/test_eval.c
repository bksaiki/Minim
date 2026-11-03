/**
 * @file
 * Tests for the evaluator.
 */

#include <stdlib.h>

#include "eval.h"
#include "gc.h"
#include "minim.h"
#include "stack.h"
#include "symbol.h"
#include "test.h"

// The list of the values of the first datum of `src`.
static minim_obj eval_values(void *src) {
    minim_obj x = minim_read(input(src));
    return minim_eval_values(x);
}

// Whether the expression `src` returns values whose list prints as `expect`.
static bool values_are(const char *src, const char *expect) {
    minim_obj val;
    bool ok = minim_catch(eval_values, (void *)src, &val);
    if (!ok) {
        fprintf(stderr, "`%s` raised ", src);
        minim_write_error(stderr);
        return false;
    }
    return prints_as(val, expect);
}

// minim_eval takes exactly one value; minim_eval_values any number.
static void test_values(void) {
    size_t before = minim_stack_height();
    CHECK(fails_with("(values)", "wrong number of values"));
    CHECK(fails_with("(values 1 2)", "wrong number of values"));
    CHECK_EQ(minim_stack_height(), before);
    CHECK(evals_to("(values 1)", "1"));
    CHECK(values_are("(values)", "()"));
    CHECK(values_are("(values 1 2)", "(1 2)"));
    CHECK(values_are("5", "(5)"));
    CHECK(values_are("(call-with-values (lambda () (values 1 2)) values)", "(1 2)"));
    CHECK_EQ(minim_stack_height(), before);
}

// The core forms binding multiple values, without the expander, so the
// evaluator's own checks run.
static void test_binding_values(void) {
    minim_obj expand = minim_symbol_value(minim_sym_expand);
    MINIM_FRAME(&expand);
    minim_set_symbol_value(minim_sym_expand, MINIM_UNBOUND);
    size_t before = minim_stack_height();

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
    CHECK_EQ(minim_stack_height(), before);

    minim_set_symbol_value(minim_sym_expand, expand);
    MINIM_RETURN_VOID;
}

// Frames: references and `set!` across them, rest parameters, and `let-values`
// frames, whose variables are unbound until their clause's values arrive.
static void test_frames(void) {
    CHECK(evals_to("((((lambda (a b) (lambda (c) (lambda (d) (list a b c d)))) 1 2) 3) 4)",
                   "(1 2 3 4)"));
    CHECK(evals_to("((lambda (a) ((lambda (b) (set! a (+ a b))) 10) a) 1)", "11"));
    CHECK(evals_to("((lambda (a . r) (list a r)) 1 2 3)", "(1 (2 3))"));
    CHECK(evals_to("((lambda (a . r) (list a r)) 1)", "(1 ())"));
    CHECK(evals_to("((lambda r r) 1 2)", "(1 2)"));
    CHECK(evals_to("((lambda r r))", "()"));
    CHECK(evals_to("(apply (lambda (a . r) (list a r)) 1 '(2 3))", "(1 (2 3))"));
    CHECK(evals_to("(let-values (((a b) (values 1 2)) ((c . d) (values 3 4 5)) (e (values)))"
                   " (list a b c d e))",
                   "(1 2 3 (4 5) ())"));
    CHECK(evals_to("(let-values (((a) 1)) (let-values (((a) 2) ((b) a)) (list a b)))",
                   "(2 1)"));
    CHECK(evals_to("(letrec-values (((f) (lambda () g)) ((g) 5)) (f))", "5"));
    CHECK(fails_with("(letrec-values (((a) b) ((b) 1)) a)", "unbound variable"));
    CHECK(fails_with("(letrec-values (((a) (set! b 2)) ((b) 1)) a)", "unbound variable"));
}

// Collects, then returns its second argument.
static minim_obj collect_then_second(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    minim_gc_collect_major();
    return args[1];
}

static void test_primitive_allocates(void) {
    minim_obj sym = minim_intern_cstr("collect-then-second");
    MINIM_FRAME(&sym);
    minim_obj p = minim_make_primitive(sym, collect_then_second, 2, 2);
    minim_define("collect-then-second", p);
    CHECK(evals_to("(collect-then-second 1 (cons 2 '(3)))", "(2 3)"));
    MINIM_RETURN_VOID;
}

// minim_define gives evaluated code a global, which a second call replaces.
static void test_define(void) {
    minim_define("answer", minim_fixnum(42));
    CHECK(evals_to("answer", "42"));
    CHECK(evals_to("(define (get-answer) answer) (get-answer)", "42"));
    minim_define("answer", minim_fixnum(43));
    CHECK(evals_to("(get-answer)", "43"));

    // A definition binds the REPL's label, not the interned symbol of its name.
    CHECK(evals_to("(define defined-at-the-repl 1) defined-at-the-repl", "1"));
    CHECK_EQ(minim_symbol_value(minim_intern_cstr("defined-at-the-repl")), MINIM_UNBOUND);
}

static minim_obj stack_height(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    return minim_fixnum((intptr_t)minim_stack_height());
}

// The value of `(loop n)` after the definitions in `tmpl`, an integer.
static intptr_t loop_result(const char *tmpl, int n) {
    char src[512];
    snprintf(src, sizeof(src), tmpl, n);
    minim_obj val;
    bool ok = minim_catch(eval_source, src, &val);
    if (!ok) {
        minim_write_error(stderr);
        return -1;
    }
    return minim_fixnum_value(val);
}

// Each loop returns the value stack's height at its last iteration, which
// must be what it is with no iterations, and leaves the heap small.
static void test_tail_calls(void) {
    minim_obj sym = minim_intern_cstr("stack-height");
    MINIM_FRAME(&sym);
    minim_obj p = minim_make_primitive(sym, stack_height, 0, 0);
    minim_define("stack-height", p);
    minim_define("%eval-expanded", minim_symbol_value(minim_intern_cstr("%eval-expanded")));

    const char *loops[] = {
        "(define loop (lambda (n) (if (= n 0) (stack-height) (loop (- n 1))))) (loop %d)",
        // `apply` in tail position, and the last subform of `and`, `or`, and
        // `when`, the body of a named `let` or a `let-values`, the consumer of
        // `call-with-values`,
        "(define loop (lambda (n) (if (= n 0) (stack-height) (apply loop (list (- n 1))))))"
        " (loop %d)",
        "(define loop (lambda (n) (and #t (if (= n 0) (stack-height) (loop (- n 1))))))"
        " (loop %d)",
        "(define loop (lambda (n) (or #f (if (= n 0) (stack-height) (loop (- n 1))))))"
        " (loop %d)",
        "(define loop (lambda (n) (if (= n 0) (stack-height) (when #t (loop (- n 1))))))"
        " (loop %d)",
        "(define (loop n) (let lp ((n n)) (cond ((= n 0) (stack-height)) (else (lp (- n 1))))))"
        " (loop %d)",
        "(define loop (lambda (n) (if (= n 0) (stack-height)"
        " (let-values (((m) (- n 1))) (loop m))))) (loop %d)",
        "(define loop (lambda (n) (if (= n 0) (stack-height)"
        " (call-with-values (lambda () (values (- n 1) 0)) (lambda (m z) (loop m))))))"
        " (loop %d)",
        // a call through `call/cc`, a loop that re-enters a continuation, a
        // call from a `guard` clause,
        "(define loop (lambda (n) (if (= n 0) (stack-height)"
        " (call/cc (lambda (k) (loop (- n 1))))))) (loop %d)",
        "(define (loop n) (let ((i 0) (k #f)) (call/cc (lambda (c) (set! k c)))"
        " (set! i (+ i 1)) (if (< i n) (k #f) (stack-height)))) (loop %d)",
        "(define (loop n) (if (= n 0) (stack-height)"
        " (guard (e (#t (loop (- n 1)))) (car e)))) (loop %d)",
        // and a call through `%eval-expanded`
        "(define (loop n) (if (= n 0) (stack-height)"
        " (%%eval-expanded (list loop (- n 1))))) (loop %d)",
    };
    for (size_t i = 0; i < sizeof(loops) / sizeof(loops[0]); i++) {
        intptr_t base = loop_result(loops[i], 0);
        CHECK(base >= 0);
        CHECK_EQ(loop_result(loops[i], gc_stressed() ? 2000 : 100000), base);
        // Without tail calls, every iteration's frame would still be live.
        CHECK(minim_gc.size <= 8 * 1024 * 1024);
    }
    MINIM_RETURN_VOID;
}

// `prefix`, then " 0" `n` times, then `suffix`, in a new malloc'd string.
static char *repeat_zeros(const char *prefix, size_t n, const char *suffix) {
    size_t len = strlen(prefix) + 2 * n + strlen(suffix);
    char *s = malloc(len + 1);
    memcpy(s, prefix, strlen(prefix));
    char *p = s + strlen(prefix);
    for (size_t i = 0; i < n; i++, p += 2)
        memcpy(p, " 0", 2);
    strcpy(p, suffix);
    return s;
}

// Forms with more elements than a call may take arguments: a quasiquoted
// vector, and a macro use whose ellipsis instantiates that many.
static void test_large_forms(void) {
    const size_t n = gc_stressed() ? 100 : 10000;
    char expect[32];
    char *src = repeat_zeros("(define x 1) (vector-length `#(,x", n, "))");
    snprintf(expect, sizeof(expect), "%zu", n + 1);
    CHECK(evals_to(src, expect));
    free(src);
    src = repeat_zeros("(define-syntax count (syntax-rules () ((_ e ...) (vector-length #(e ...)))))"
                       " (count", n, ")");
    snprintf(expect, sizeof(expect), "%zu", n);
    CHECK(evals_to(src, expect));
    free(src);
}

// Non-tail recursion deep enough to freeze and reload many segments, with one
// or several values returned at every depth, and an error from the bottom.
static void test_deep_recursion(void) {
    size_t before = minim_stack_height();
    const int n = gc_stressed() ? 6000 : 100000;  // about two values a level
    char src[512];
    char expect[64];
    snprintf(src, sizeof(src),
             "(define (sum n) (if (= n 0) 0 (+ n (sum (- n 1))))) (sum %d)", n);
    snprintf(expect, sizeof(expect), "%lld", (long long)n * (n + 1) / 2);
    CHECK(evals_to(src, expect));
    snprintf(src, sizeof(src),
             "(define (sum2 n) (if (= n 0) 0"
             " (+ (call-with-values (lambda () (values n 1)) -) (sum2 (- n 1)))))"
             " (sum2 %d)", n);
    snprintf(expect, sizeof(expect), "%lld", (long long)n * (n - 1) / 2);
    CHECK(evals_to(src, expect));
    snprintf(src, sizeof(src),
             "(define (bad n) (if (= n 0) (car 5) (+ 1 (bad (- n 1))))) (bad %d)", n);
    CHECK(fails_with(src, "car: not a pair"));
    CHECK_EQ(minim_stack_height(), before);
    CHECK_EQ(minim_gc.values.below, MINIM_NIL);
}

// A call written with more operands than fit raises; `apply` takes any number.
static void test_many_arguments(void) {
    if (!gc_stressed()) {  // the cap is the point, and minutes under stress
        char *src = repeat_zeros("(+", MINIM_MAX_ARGS, ")");
        CHECK(evals_to(src, "0"));
        free(src);
        src = repeat_zeros("(+", MINIM_MAX_ARGS + 1, ")");
        CHECK(fails_with(src, "too many arguments"));
        free(src);
    }

    const int n = gc_stressed() ? 10000 : 100000;  // more than MINIM_MAX_ARGS
    char buf[1024];
    char expect[64];
    snprintf(buf, sizeof(buf),
             "(define (iota n) (let loop ((i n) (acc '()))"
             " (if (= i 0) acc (loop (- i 1) (cons i acc)))))"
             " (define l (iota %d))"
             " (define (count . r) (length r))"
             " (define (f a b . r) (list a b (length r)))"
             " (list (apply count l) (apply f 1 2 l) (apply + 1 2 l) (length (apply list l)))",
             n);
    snprintf(expect, sizeof(expect), "(%d (1 2 %d) %lld %d)", n, n,
             (long long)n * (n + 1) / 2 + 3, n);
    CHECK(evals_to(buf, expect));
    CHECK(fails_with("(apply values l)", "too many arguments"));
    CHECK(fails_with("(apply car l)", "arity mismatch"));
    CHECK_EQ(minim_gc.spill.len, 0);
}

// A handled error, deep in a recursion or in a long `apply`, leaves the value
// stack, winders, and spill as they were; an unhandled one, the value stack,
// and runs the afters.
static void test_handled_errors(void) {
    size_t before = minim_stack_height();
    char buf[512];
    snprintf(buf, sizeof(buf),
             "(define (catch thunk) (call/cc (lambda (k) (with-exception-handler"
             " (lambda (e) (k (error-object-message e))) thunk))))"
             " (define (deep n) (if (= n 0) (car 'x) (+ 1 (deep (- n 1)))))"
             " (define (iota n) (let loop ((i n) (acc '()))"
             " (if (= i 0) acc (loop (- i 1) (cons i acc)))))"
             " (list (catch (lambda () (deep %d))) (catch (lambda () (apply + 'x (iota %d)))))",
             gc_stressed() ? 1000 : 100000, 2 * MINIM_SEGMENT_SIZE);
    CHECK(evals_to(buf, "(\"car: not a pair\" \"+: not a fixnum\")"));
    CHECK_EQ(minim_stack_height(), before);
    CHECK_EQ(minim_gc.winders, MINIM_NIL);
    CHECK_EQ(minim_gc.handlers, MINIM_NIL);
    CHECK_EQ(minim_gc.spill.len, 0);

    CHECK(fails_with("(+ 1 (apply + 1 '(2 . 3)))", "apply: not a list"));
    CHECK_EQ(minim_stack_height(), before);

    CHECK(fails_with("(define out #f) (dynamic-wind (lambda () #f) (lambda () (deep 10))"
                     " (lambda () (set! out #t)))",
                     "car: not a pair"));
    CHECK_EQ(minim_stack_height(), before);
    CHECK_EQ(minim_gc.winders, MINIM_NIL);
    CHECK(evals_to("out", "#t"));
}

// Each top-level form, including each one `load` runs, goes through %expand
// when it is reached, so a %expand one form sets applies to the next.  The
// REPL can't set it: its `define` would shadow the system's.
static void test_expand_hook(void) {
    minim_obj saved = minim_symbol_value(minim_sym_expand);
    MINIM_FRAME(&saved);
    minim_obj x = minim_read(input("(define %expand (lambda (form)"
                                   " (if (if (pair? form) (eq? (car form) 'twice) #f)"
                                   " (list 'begin (car (cdr form)) (car (cdr form))) form)))"));
    minim_eval_expanded(x);
    CHECK(evals_to("(define n 0) (twice (set! n (+ n 1))) n", "2"));

    const char *path = gc_stressed() ? "expand_hook_stress.scm" : "expand_hook.scm";
    FILE *f = fopen(path, "w");
    fputs("(twice (set! n (+ n 1)))\n"
          "(define %expand (lambda (form)"
          " (if (if (pair? form) (eq? (car form) 'thrice) #f)"
          " (list 'begin (car (cdr form)) (car (cdr form)) (car (cdr form))) form)))\n"
          "(thrice (set! n (+ n 1)))\n", f);
    fclose(f);
    char src[64];
    snprintf(src, sizeof(src), "(load \"%s\") n", path);
    CHECK(evals_to(src, "7"));
    CHECK(evals_to("(thrice (set! n (+ n 1))) n", "10"));
    remove(path);
    minim_set_symbol_value(minim_sym_expand, saved);
    MINIM_RETURN_VOID;
}

int main(void) {
    minim_init();
    test_frames();
    test_primitive_allocates();
    test_define();
    test_values();
    test_binding_values();
    test_tail_calls();
    test_large_forms();
    test_deep_recursion();
    test_many_arguments();  // after test_tail_calls, which checks the value stack's capacity
    test_handled_errors();
    test_expand_hook();
    minim_shutdown();
    return test_result();
}
