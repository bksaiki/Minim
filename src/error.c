// error.c: exception handling

#include "minim.h"

void fatal_exit() {
    fprintf(stderr, "fatal_error(): uncaught exception, exiting\n");
    exit(1);
}

void error(const char *who, const char *why) {
    fprintf(stderr, "%s: %s\n", who, why);
    fatal_exit();  // TODO: error handling
}

void error1(const char *who, const char *why, mobj x) {
    mobj p = Mport(stderr, PORT_FLAG_OPEN);
    fprintf(stderr, "%s: %s\n", who, why);
    fprintf(stderr, " what: ");
    write_object(p, x);
    fprintf(stderr, "\n");
    fatal_exit();  // TODO: error handling
}

