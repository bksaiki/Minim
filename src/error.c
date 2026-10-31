// error.c: minimal error handling

#include "minim.h"

NORETURN static void do_error(const char *name, const char *msg, obj args) {
    // TODO: no Scheme-side error handling
    if (name) {
        fprintf(stderr, "Error in %s: %s", name, msg);
    } else {
        fprintf(stderr, "Error: %s", msg);
    }

    for (; !Mnullp(args); args = Mcdr(args)) {
        fputs("\n ", stderr);
        write_obj(stderr, Mcar(args));
    }

    fputc('\n', stderr);
    minim_shutdown(1);
}

void minim_error(const char *name, const char *msg) {
    do_error(name, msg, Mnull);
}

void minim_error1(const char *name, const char *msg, obj x) {
    do_error(name, msg, Mlist1(x));
}

void minim_error2(const char *name, const char *msg, obj x, obj y) {
    do_error(name, msg, Mlist2(x, y));
}

void minim_error3(const char *name, const char *msg, obj x, obj y, obj z) {
    do_error(name, msg, Mlist3(x, y, z));
}

obj Mkernel_error(obj who, obj msg, obj args) {
    fprintf(stderr, "error caught before error handlers initialized\n");
    fprintf(stderr, "who: ");
    writeln_object(stderr, who);
    fprintf(stderr, "msg: ");
    writeln_object(stderr, msg);
    fprintf(stderr, "args: ");
    writeln_object(stderr, args);
    minim_shutdown(1);
}
