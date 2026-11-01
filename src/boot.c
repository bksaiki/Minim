/**
 * @file
 * Writes a prelude source's top-level forms expanded into core forms, one per
 * line, for src/prelude.boot.  Usage: boot_gen boot.scm prelude.scm out.
 */

#include <stdio.h>

#include "minim.h"
#include "read.h"

typedef struct job {
    const char *boot_scm;  ///< defines %boot-form
    const char *prelude;   ///< the forms to expand
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

    x = minim_string_from_cstr(j->prelude);
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
    MINIM_RETURN(MINIM_VOID);
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fputs("usage: boot_gen boot.scm prelude.scm out\n", stderr);
        return 2;
    }
    job j = {argv[1], argv[2], fopen(argv[3], "w")};
    if (j.out == NULL) {
        perror(argv[3]);
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
