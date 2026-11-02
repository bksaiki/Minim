/**
 * @file
 * Command-line entry point: runs a script, a program, or a read-eval-print loop.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "eval.h"
#include "minim.h"

static const char usage[] = "usage: minim [--program] [file]\n";

static const char help[] =
    "Runs file as a script in the interaction environment, or else a REPL.\n"
    "\n"
    "  --program  run file as an R7RS program, which sees only what it imports\n"
    "  --help     print this message\n"
    "  --version  print the version\n";

static minim_obj read_stdin(void *arg) {
    MINIM_UNUSED(arg);
    return minim_read(stdin);
}

static minim_obj eval(void *expr) {
    return minim_eval_values(*(minim_obj *)expr);
}

// The form `(name "path")`.
static minim_obj call_form(const char *name, const char *path) {
    minim_obj expr = minim_string_from_cstr(path);
    MINIM_FRAME(&expr);
    expr = minim_cons(expr, MINIM_NIL);
    minim_obj sym = minim_intern_cstr(name);
    expr = minim_cons(sym, expr);
    MINIM_RETURN(expr);
}

static minim_obj load(void *path) {
    return minim_eval(call_form("load", path));
}

// Runs the program in the file `path`, expanded by `%expand-program`.
static minim_obj run_program(void *path) {
    return minim_eval_expanded(minim_eval_expanded(call_form("%expand-program", path)));
}

// Runs the file `path` with `run`; returns the exit status.
static int run_file(minim_obj (*run)(void *), const char *path) {
    minim_obj x;
    if (!minim_catch(run, (void *)path, &x)) {
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
        for (; minim_is_pair(x); x = minim_cdr(x)) {
            if (minim_car(x) != MINIM_VOID) {
                minim_write(stdout, minim_car(x));
                putchar('\n');
            }
        }
    }

    if (interactive)
        fputc('\n', stderr);
    return 0;
}

// Prints `message` and the usage to stderr; returns the exit status.
static int usage_error(const char *message, const char *arg) {
    fprintf(stderr, "minim: %s %s\n%s", message, arg, usage);
    return 2;
}

int main(int argc, char **argv) {
    const char *file = NULL;
    bool program = false;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0) {
            printf("%s%s", usage, help);
            return 0;
        } else if (strcmp(arg, "--version") == 0) {
            puts("minim " MINIM_VERSION);
            return 0;
        } else if (strcmp(arg, "--program") == 0) {
            program = true;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return usage_error("unknown option", arg);
        } else if (file != NULL) {
            return usage_error("unexpected argument", arg);
        } else {
            file = arg;
        }
    }
    if (program && file == NULL)
        return usage_error("missing file after", "--program");

    minim_init();
    int status = file == NULL ? run_repl() : run_file(program ? run_program : load, file);
    minim_shutdown();
    return status;
}
