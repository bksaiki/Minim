/*
    Header file for the bootstrap interpreter.
    Should not be referenced outside this directory

    Entry point of the REPL
*/

#include "../build/config.h"
#include "../boot.h"

//
//  REPL
//

static int opt_load_library = 1;    // load bootstrap library
static int interactive = 0;         // REPL
static int quiet = 0;               // No header

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
            } else if (strcmp(argv[i], "--quiet") == 0) {
                quiet = 1;
            } else {
                unknown_flag_exn(argv[i]);
            }
        } else if (argv[i][1] == 'i') {
            interactive = 1;
        } else if (argv[i][1] == 'q') {
            quiet = 1;
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
    minim_thread *th;
    int argi, i;

    stack_top = 0;
    argi = handle_flags(argc, argv);
    if (!quiet) printf("Minim v%s\n", MINIM_VERSION);

    GC_init(((void*) &stack_top));
    minim_boot_init();
    th = current_thread();

    if (opt_load_library)
        load_library();

    for (i = argc - 1; i >= argi; --i)
        command_line(th) = make_pair(make_string(argv[i]), command_line(th));

    if (argi < argc) {
        if (!interactive && opt_load_library) {
            eval_expr(
                make_pair(intern("import"),
                    make_pair(make_string(argv[argi]), 
                    minim_null)),
                global_env(th));
        } else {
            eval_expr(
                make_pair(intern("load"),
                    make_pair(make_string(argv[argi]), 
                    minim_null)),
                global_env(th));
        }
    }
        
    if (argc == 1 || interactive) {
        while (1) {
            printf("> ");
            expr = read_object(stdin);
            if (expr == NULL) break;

            evaled = eval_expr(expr, global_env(th));
            if (!minim_is_void(evaled)) {
                if (minim_is_values(evaled)) {
                    for (int i = 0; i < values_buffer_count(th); ++i) {
                        write_object(stdout, values_buffer_ref(th, i));
                        printf("\n");
                    }
                } else {
                    write_object(stdout, evaled);
                    printf("\n");
                }
            }
        }
    }

    minim_shutdown(stack_top);
}
