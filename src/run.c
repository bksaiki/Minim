// run.c: compiled runtime support

#include <setjmp.h>

#include "minim.h"

#define STACK_SIZE  (1024 * 1024)

static jmp_buf jbuf;

static mobj env_get_entry(mobj env, mobj k) {
    while (!minim_nullp(env)) {
        mobj bind = minim_car(env);
        for_each(bind,
            if (minim_caar(bind) == k)
                return minim_car(bind);
        );

        env = minim_cdr(env);
    }

    return NULL;
}

mobj make_env() {
    return Mlist1(minim_null);
}

mobj env_extend(mobj env) {
    return Mcons(minim_null, env);
}

mobj env_get(mobj env, mobj k) {
    mobj maybe = env_get_entry(env, k);
    if (!maybe) error1("env_get", "unbound variable", k);
    return minim_cdr(maybe);
}

mobj env_define(mobj env, mobj k, mobj v) {
    minim_car(env) = Mcons(Mcons(k, v), minim_car(env));
    return minim_void;
}

mobj env_set(mobj env, mobj k, mobj v) {
    mobj maybe = env_get_entry(env, k);
    if (!maybe) error1("env_set", "unbound variable", k);
    minim_cdr(maybe) = v;
    return minim_void;
}

static mobj alloc_stack() {
    void *page = GC_alloc(STACK_SIZE);
    if (page == NULL)
        error("alloc_stack", "failed to allocate stack");
    return (mobj) page;
}

static void underflow_handler() {
    longjmp(jbuf, 1);
}

static void enter_with(void *fn, void *frame, mobj env) {
    // stash all callee-preserved registers
    // move `env` to `%r9`
    // set [%rbp] to frame
    // set [%rbp] to `underflow_handler`
    // set [%rbp-8] to `%rbp`

    __asm__ (
        "movq %0, %%r9\n\t"
        "movq %2, (%%rbp)\n\t"
        "movq %%rbp, -8(%%rbp)\n\t"
        // "movq %2, (%1)\n\t"
        // "movq %%rbp, -8(%1)\n\t"
        // "movq %1, %%rbp\n\t"
        "jmp *%3\n\t"
        :
        : "r" (env), "r" (frame), "r" (underflow_handler), "r" (fn)
        : "%rbx", "%r12", "%r13", "%r14", "%r15"
    );
}

void runtime_init() {
    M_glob.stacks = Mcons(alloc_stack(), minim_null);
}

void call0(void *fn) {
    mobj env = make_env();
    mobj stack = minim_car(M_glob.stacks);
    if (setjmp(jbuf) == 0) {
        enter_with(fn, PTR_ADD(stack, STACK_SIZE - 16), env);
    }

    write_object(Mport(stdout, 0x0), env);
    fprintf(stdout, "\n");
}

mobj do_rest_arg() {
    // fp is %rbp
    // arity is %r14
    // argc is %rsi
    // nth argument is at fp-8(n+2)
    error("do_rest_arg", "unimplemented");
}
