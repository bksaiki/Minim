// main.c: entry point of the executable

#include "minim.h"

int main(int argc, char **argv) {
    mobj o, op, ip, es;
    mthread *th;
    int version = 1;
    int repl = (argc == 1);
    int boot = 0;
    int argi;

    for (argi = 1; argi < argc; argi++) {
        if (argv[argi][0] != '-')
            break;

        if (strcmp(argv[argi], "--version") == 0) {
            version = 1;
            repl = 0;
        } else if (strcmp(argv[argi], "--boot") == 0) {
            version = 0;
            boot = 1;
        } else {
            fprintf(stderr, "unknown flag %s\n", argv[argi]);
            exit(1);
        }
    }

    if (version) {
        printf("Minim v%s\n", MINIM_VERSION);
    }

    GC_init();
    GC_pause();
    minim_init();

    th = get_thread();

    // Bootstrapping
    if (boot) {
        // expected two arguments <input> <output>
        if (argi + 2 != argc) {
            fprintf(stderr, "command-line arguments expected\n");
            fprintf(stderr, " expected 2 arguments, given: %d\n", argc - argi);
            exit(1);
        }

        ip = open_input_file(argv[argi]);
        op = open_output_file(argv[argi + 1]);
        es = minim_null;

        while (1) {
            // read in an expression
            o = read_object(ip);
            if (minim_eofp(o))
                break;

            // expand
            o = expand_top(o);
            write_object(op, o);
            fputc('\n', minim_port(op));
            es = Mcons(o, es);
        }

        es = list_reverse(es);
        compile_module(op, Mstring(argv[argi]), es);

        close_port(ip);
        close_port(op);
    }
    
    // REPL
    if (repl) {
        fputs("[cwd ", stdout);
        write_object(th_output_port(th), th_working_dir(th));
        fputs("]\n", stdout);

        while (1) {
            fputs("> ", stdout);
            o = read_object(th_input_port(th));
            if (minim_eofp(o))
                break;

            write_object(th_output_port(th), o);
            fputc('\n', stdout);
        }
    }

    GC_shutdown();
    return 0;
}
