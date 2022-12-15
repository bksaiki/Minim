/*
    Header file for the bootstrap interpreter.
    Should not be referenced outside this directory

    Entry point of the REPL
*/

#include "../build/config.h"
#include "boot.h"

//
//  REPL
//

static int opt_load_library = 1;    // load bootstrap library
static int interactive = 0;         // REPL

static void unknown_flag_exn(const char *flag) {
    fprintf(stderr, "unknown flag: %s\n", flag);
    exit(1);
}

static int handle_flags(int argc, char **argv) {
    int i;

    for (i = 1; i < argc; ++i) {
        if (argv[i][0] != '-')
            return i;

        if (argv[i][1] == '-') {
            if (argv[i][2] == '\0') {
                return i + 1;
            } else if (strcmp(argv[i], "--interactive") == 0) {
                interactive = 1;
            } else if (strcmp(argv[i], "--no-libs") == 0) {
                opt_load_library = 0;
            } else {
                unknown_flag_exn(argv[i]);
            }
        } else if (argv[i][1] == 'i') {
            interactive = 1;
        } else {
            unknown_flag_exn(argv[i]);
        }
    }

    return i;
}

static void load_library() {
    char *old_cwd = get_current_dir();
    set_current_dir(BOOT_DIR);
    load_file("boot.min");
    set_current_dir(old_cwd);
}

int main(int argc, char **argv) {
    volatile int stack_top;
    minim_object *expr, *evaled;
    int argi;

    argi = handle_flags(argc, argv);
    printf("Minim v%s\n", MINIM_BOOT_VERSION);

    GC_init(((void*) &stack_top));
    minim_boot_init();
    if (opt_load_library)
        load_library();

    if (argi < argc) {
        if (!interactive && opt_load_library) {
            eval_expr(make_pair(intern_symbol(symbols, "import"),
                  make_pair(make_string(argv[argi]), 
                  minim_null)),
                  global_env);
        } else {
            eval_expr(make_pair(intern_symbol(symbols, "load"),
                  make_pair(make_string(argv[argi]), 
                  minim_null)),
                  global_env);
        }
    }
        
    if (argc == 1 || interactive) {
        while (1) {
            printf("> ");
            expr = read_object(stdin);
            if (expr == NULL) break;

            evaled = eval_expr(expr, global_env);
            if (!minim_is_void(evaled)) {
                if (minim_is_values(evaled)) {
                    for (int i = 0; i < values_buffer_count; ++i) {
                        write_object(stdout, values_buffer_ref(i));
                        printf("\n");
                    }
                } else {
                    write_object(stdout, evaled);
                    printf("\n");
                }
            }
        }
    }

    GC_finalize();
    return 0;
}
