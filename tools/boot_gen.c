/**
 * @file
 * Writes the top-level forms of the prelude's sources, in order, expanded into
 * core forms, one per line, for scheme/prelude.boot.
 */

#include <stdio.h>
#include <string.h>

#include "minim.h"
#include "read.h"

static const char usage[] = "usage: boot_gen --boot-scm file [-o file] source...\n";

static const char help[] =
    "Writes the sources' top-level forms, in order, expanded into core forms, one per line.\n"
    "\n"
    "  --boot-scm file  the Scheme file defining %boot-form (tools/boot.scm)\n"
    "  -o file          write to file instead of stdout\n"
    "  --help           print this message\n";

typedef struct job {
    const char *boot_scm;  ///< defines %boot-form
    char **sources;        ///< the files whose forms to expand, in load order
    int nsources;
    FILE *out;
} job;

// Writes the expansion of the job's forms to its file.
static minim_obj generate(void *arg) {
    const job *j = arg;
    minim_obj x = minim_string_from_cstr(j->boot_scm);
    minim_obj forms = MINIM_NIL;
    minim_obj sym = MINIM_NIL;
    MINIM_FRAME(&x, &forms, &sym);
    x = minim_cons(x, MINIM_NIL);
    sym = minim_intern_cstr("load");
    x = minim_cons(sym, x);
    minim_eval(x);

    for (int i = 0; i < j->nsources; i++) {
        x = minim_string_from_cstr(j->sources[i]);
        forms = minim_read_file(x);
        for (; minim_is_pair(forms); forms = minim_cdr(forms)) {
            // (%boot-form 'form)
            x = minim_cons(minim_car(forms), MINIM_NIL);
            sym = minim_intern_cstr("quote");
            x = minim_cons(sym, x);
            x = minim_cons(x, MINIM_NIL);
            sym = minim_intern_cstr("%boot-form");
            x = minim_cons(sym, x);
            for (x = minim_eval(x); minim_is_pair(x); x = minim_cdr(x)) {
                minim_write(j->out, minim_car(x));
                fputc('\n', j->out);
            }
        }
    }
    MINIM_RETURN(MINIM_VOID);
}

// Prints `message` and the usage to stderr; returns the exit status.
static int usage_error(const char *message, const char *arg) {
    fprintf(stderr, "boot_gen: %s %s\n%s", message, arg, usage);
    return 2;
}

int main(int argc, char **argv) {
    const char *out = NULL;
    job j = {NULL, argv + argc, 0, stdout};
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0) {
            printf("%s%s", usage, help);
            return 0;
        } else if (strcmp(arg, "--boot-scm") == 0 || strcmp(arg, "-o") == 0) {
            if (i + 1 == argc)
                return usage_error("missing file after", arg);
            if (strcmp(arg, "-o") == 0)
                out = argv[++i];
            else
                j.boot_scm = argv[++i];
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return usage_error("unknown option", arg);
        } else {
            j.sources = argv + i;  // the rest are sources
            j.nsources = argc - i;
            break;
        }
    }
    if (j.boot_scm == NULL)
        return usage_error("missing option", "--boot-scm");
    if (j.nsources == 0)
        return usage_error("missing", "sources");

    if (out != NULL && (j.out = fopen(out, "w")) == NULL) {
        perror(out);
        return 1;
    }
    minim_init();
    minim_obj result;
    bool ok = minim_catch(generate, &j, &result);
    if (!ok)
        minim_write_error(stderr);
    if (out != NULL)
        fclose(j.out);
    minim_shutdown();
    return ok ? 0 : 1;
}
