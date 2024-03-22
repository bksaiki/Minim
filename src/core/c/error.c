// error.c: error handling

#include "../minim.h"

NORETURN static void do_error2(const char *name, const char *msg, mobj args) {
    minim_thread *th;

    th = current_thread();
    if (minim_nullp(current_continuation(th)) || minim_falsep(c_error_handler(th))) {
        // exception cannot be handled by runtime
        if (name) fprintf(stderr, "Error in %s: %s", name, msg);
        else fprintf(stderr, "Error: %s", msg);
        for (; !minim_nullp(args); args = minim_cdr(args)) {
            fputs("\n ", stderr);
            write_object(stderr, minim_car(args));
        }

        fputc('\n', stderr);
        minim_shutdown(1);
    }
    
    // call back into the Scheme runtime
    current_ac(th) = 3;
    reserve_stack(th, 3);
    set_arg(th, 0, (name ? Mstring(name) : minim_false));
    set_arg(th, 1, Mstring(msg));
    set_arg(th, 2, args);
    current_cp(th) = c_error_handler(th);
    longjmp(*current_reentry(th), 1);
}

//
//  C-side errors
//

void minim_error(const char *name, const char *msg) {
    do_error2(name, msg, minim_null);
}

void minim_error1(const char *name, const char *msg, mobj x) {
    do_error2(name, msg, Mlist1(x));
}

void minim_error2(const char *name, const char *msg, mobj x, mobj y) {
    do_error2(name, msg, Mlist2(x, y));
}

void minim_error3(const char *name, const char *msg, mobj x, mobj y, mobj z) {
    do_error2(name, msg, Mlist3(x, y, z));
}

//
//  Primitives
//

mobj boot_error_proc(mobj who, mobj msg, mobj args) {
    fprintf(stderr, "error caught before error handlers initialized\n");
    fprintf(stderr, "who: ");
    write_object(stderr, who);
    fprintf(stderr, "\nmsg: ");
    write_object(stderr, msg);
    fprintf(stderr, "\nargs: ");
    write_object(stderr, args);
    fprintf(stderr, "\n");
    minim_shutdown(1);
}

mobj c_error_handler_proc() {
    // (-> any (or procedure #f))
    return c_error_handler(current_thread());
}

mobj c_error_handler_set_proc(mobj proc) {
    // (-> procedure void)
    c_error_handler(current_thread()) = proc;
    return minim_void;
}
