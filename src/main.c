/**
 * @file
 * Command-line entry point: runs a file, or a read-eval-print loop.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "minim.h"

static minim_obj read_stdin(void *arg) {
    (void)arg;
    return minim_read(stdin);
}

static minim_obj eval(void *expr) {
    return minim_eval(*(minim_obj *)expr);
}

// Evaluates `(load path)`.
static minim_obj load(void *path) {
    minim_obj expr = minim_string_from_cstr(path);
    MINIM_FRAME(&expr);
    expr = minim_cons(expr, MINIM_NIL);
    minim_obj sym = minim_intern_cstr("load");
    expr = minim_cons(sym, expr);
    MINIM_RETURN(minim_eval(expr));
}

// Runs the file `path`; returns the exit status.
static int run_file(const char *path) {
    minim_obj x;
    if (!minim_catch(load, (void *)path, &x)) {
        minim_write_error(stderr);
        return 1;
    }
    return 0;
}

static int run_repl(void) {
    bool interactive = isatty(fileno(stdin));
    for (;;) {
        if (interactive)
            fputs("> ", stderr);

        minim_obj x;
        if (!minim_catch(read_stdin, NULL, &x)) {
            minim_write_error(stderr);
            continue;
        }
        if (x == MINIM_EOF)
            break;
        if (!minim_catch(eval, &x, &x)) {
            minim_write_error(stderr);
            continue;
        }
        if (x != MINIM_VOID) {
            minim_write(stdout, x);
            putchar('\n');
        }
    }

    if (interactive)
        fputc('\n', stderr);
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fputs("usage: minim [file]\n", stderr);
        return 2;
    }

    minim_init();
    int status = argc == 2 ? run_file(argv[1]) : run_repl();
    minim_shutdown();
    return status;
}
