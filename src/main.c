// main.c: entry point of the executable

#include "minim.h"

static void boot_compile(const char *input, const char *output) {
    mobj op, ip, o, es, cstate;

    ip = open_input_file(input);
    op = open_output_file(output);
    es = minim_null;

    while (1) {
        // read in an expression and expand
        o = read_object(ip);
        if (minim_eofp(o)) break;
        es = Mcons(expand_top(o), es);
    }

    es = list_reverse(es);
    cstate = compile_module(op, Mstring(input), es);
    write_object(op, cstate);

    close_port(ip);
    close_port(op);
}

static void install_bootfile(const char *input) {
    mobj ip, cstate;

    ip = open_input_file(input);
    cstate = read_object(ip);
    if (!minim_eofp(read_object(ip)))
        error("install_bootfile", "found additional data after bootfile");

    install_module(cstate);
}

int main(int argc, char **argv) {
    int version = (argc == 1);
    int repl = (argc == 1);
    int argi;

    GC_init();
    minim_init();

    // Process flags
    for (argi = 1; argi < argc; argi++) {
        // first matching
        if (strcmp(argv[argi], "--") == 0) {
            break;
        } else if (strcmp(argv[argi], "--version") == 0) {
            version = 1;
        } else if (strcmp(argv[argi], "--bootfile") == 0) {
            // install bootfile
            if (argi + 1 >= argc || strcmp(argv[argi+1], "--") == 0) {
                fprintf(stderr, "expected arguents to --bootfile\n");
                fprintf(stderr, "USAGE: minim --bootfile <path>\n");
            }

            install_bootfile(argv[argi + 1]);
            argi += 1;
        } else if (strcmp(argv[argi], "--compile") == 0) {
            // bootstrap compiler
            if (argi + 2 >= argc ||
                strcmp(argv[argi+1], "--") == 0 ||
                strcmp(argv[argi+2], "--") == 0) {
                fprintf(stderr, "expected arguents to --compile\n");
                fprintf(stderr, "USAGE: minim --compile <source> <output>\n");
            }

            boot_compile(argv[argi + 1], argv[argi + 2]);
            argi += 2;
        } else {
            fprintf(stderr, "unknown flag %s\n", argv[argi]);
            exit(1);
        }
    }

    // version string
    if (version) {
        printf("Minim v%s\n", MINIM_VERSION);
    }

    // REPL
    if (repl) {
        mthread *th = get_thread();
        mobj o;

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
