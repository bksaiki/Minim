/*
    Header file for the bootstrap interpreter.
    Should not be referenced outside this directory

    Entry point of the REPL
*/

#include "build/config.h"
#include "boot.h"

//
//  REPL
//

static int opt_load_library;

static void handle_flags(int argc, char **argv) {
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "--no-libs") == 0) {
            // Ignores library definitions
            opt_load_library = 0;
            ++i;
        } else {
            fprintf(stderr, "unknown flag: %s\n", argv[i]);
            exit(1);
        }
    }
}

static void load_library() {
    FILE *prelude;
    minim_object *o;
    char *old_cwd;

    old_cwd = get_current_dir();
    prelude = fopen(BOOT_DIR "boot.min", "r");
    set_current_dir(BOOT_DIR);

    if (prelude == NULL) {
        fprintf(stderr, "entry file does not exists\n");
        exit(1);
    }
    
    while (!feof(prelude) || !ferror(prelude)) {
        o = read_object(prelude);
        if (o == NULL) break;

        o = eval_expr(o, global_env);
        if (!minim_is_void(o)) {
            write_object(stdout, o);
            printf("\n");
        }
    }

    fclose(prelude);
    set_current_dir(old_cwd);
}

int main(int argc, char **argv) {
    volatile int stack_top;
    minim_object *expr, *evaled;

    printf("Minim v%s\n", MINIM_BOOT_VERSION);

    GC_init(((void*) &stack_top));
    minim_boot_init();

    opt_load_library = 1;
    handle_flags(argc, argv);

    if (opt_load_library)
        load_library();
    
    while (1) {
        printf("> ");
        expr = read_object(stdin);
        if (expr == NULL) break;

        evaled = eval_expr(expr, global_env);
        if (!minim_is_void(evaled)) {
            write_object(stdout, evaled);
            printf("\n");
        }
    }

    GC_finalize();
    return 0;
}
