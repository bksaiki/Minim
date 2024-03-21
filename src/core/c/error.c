// error.c: error handling

#include "../minim.h"


NORETURN static void do_error2(const char *name, const char *msg, mobj args) {
    minim_thread *th;

    th = current_thread();
    if (minim_falsep(error_handler(th))) {
        // error handler is not set up
        fprintf(stderr, "error occured before handler set up\n");
        if (name) fprintf(stderr, "%s: %s\n", name, msg);
        else fprintf(stderr, "%s\n", msg);
        write_object(stderr, args);
        putc('\n', stderr);
        minim_shutdown(1);
    }

    minim_shutdown(1);
}

//
//  Public API
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

mobj do_error(int argc, mobj *args) {
    // (-> (or #f string symbol)
    //     string?
    //     any ...
    //     any)
    mobj who, msg, alist;

    who = args[0];
    if (!minim_falsep(who) && !minim_symbolp(who) && !minim_stringp(who))
        bad_type_exn("error", "(or #f string? symbol?)", who);

    msg = args[1];
    if (!minim_stringp(msg))
        bad_type_exn("error", "string?", msg);
    
    alist = minim_null;
    for (int i = 2; i < argc; i++)
        alist = Mcons(args[i], alist);
    alist = list_reverse(alist);

    if (minim_falsep(who))
        do_error2(NULL, minim_string(msg), alist);
    else if (minim_stringp(who))
        do_error2(minim_string(who), minim_string(msg), alist);
    else
        do_error2(minim_symbol(who), minim_string(msg), alist);
}

//
//  Primitives
//

mobj boot_error_proc(mobj who, mobj msg, mobj args) {
    fprintf(stderr, "error caught before error handlers initalized\n");
    fprintf(stderr, "who: ");
    write_object(stderr, who);
    fprintf(stderr, "\nmsg: ");
    write_object(stderr, msg);
    fprintf(stderr, "\nargs: ");
    write_object(stderr, args);
    fprintf(stderr, "\n");
    minim_shutdown(1);
}
