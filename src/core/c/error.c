// error.c: error handling

#include "../minim.h"

NORETURN static void do_error2(const char *name, const char *msg, mobj args) {
    mobj tc = current_tc();
    if (minim_nullp(tc_ccont(tc)) || minim_falsep(tc_c_error_handler(tc))) {
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
    reserve_stack(tc, 3);
    tc_ac(tc) = 3;
    tc_cp(tc) = tc_c_error_handler(tc);
    set_arg(tc, 0, (name ? Mstring(name) : minim_false));
    set_arg(tc, 1, Mstring(msg));
    set_arg(tc, 2, args);
    
    longjmp(*tc_reentry(tc), 1);
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
    return tc_c_error_handler(current_tc());
}

mobj c_error_handler_set_proc(mobj proc) {
    // (-> procedure void)
    tc_c_error_handler(current_tc()) = proc;
    return minim_void;
}

mobj error_handler_proc() {
    // (-> any (or procedure #f))
    return tc_error_handler(current_tc());
}

mobj error_handler_set_proc(mobj proc) {
    // (-> procedure void)
    tc_error_handler(current_tc()) = proc;
    return minim_void;
}
