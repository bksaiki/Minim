/**
 * @file
 * Writes the top-level forms of the prelude's sources, in order, expanded into
 * core forms, one per line, for src/prelude.boot.
 * Usage: boot_gen boot.scm out source...
 */

#include <stdio.h>

#include "minim.h"
#include "read.h"

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

int main(int argc, char **argv) {
    if (argc < 4) {
        fputs("usage: boot_gen boot.scm out source...\n", stderr);
        return 2;
    }
    job j = {argv[1], argv + 3, argc - 3, fopen(argv[2], "w")};
    if (j.out == NULL) {
        perror(argv[2]);
        return 1;
    }
    minim_init();
    minim_obj result;
    bool ok = minim_catch(generate, &j, &result);
    if (!ok)
        minim_write_error(stderr);
    fclose(j.out);
    minim_shutdown();
    return ok ? 0 : 1;
}
