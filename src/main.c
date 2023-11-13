// main.c: entry point of the executable

#include "minim.h"

static mobj boot_compile(const char *input) {
    mobj ip, o, es;

    ip = open_input_file(input);
    es = minim_null;

    while (1) {
        // read in an expression and expand
        o = read_object(ip);
        if (minim_eofp(o)) {
            close_port(ip);
            break;
        }

        es = Mcons(expand_top(o), es);
    }

    es = list_reverse(es);
    return compile_module(Mstring(input), es);
}

static void make_bootfile(const char *input, const char *output) {
    mobj cstate, op;

    // compile
    cstate = boot_compile(input);

    // write
    op = open_output_file(output);
    write_object(op, cstate);
    close_port(op);
}

static void install_bootfile(const char *input) {
    mobj ip, cstate;
    void *fn;

    ip = open_input_file(input);
    cstate = read_object(ip);
    if (!minim_eofp(read_object(ip)))
        error("install_bootfile", "found additional data after bootfile");

    fn = install_module(cstate);
    printf("[installed %s at %p]\n", input, fn);
    call0(fn);
}

int main(int argc, char **argv) {
    int version = (argc == 1);
    int argi;

    GC_init();
    minim_init();

    // Process flags
    for (argi = 1; argi < argc; argi++) {
        // first matching
        if (argv[argi][0] != '-') {
            break;
        } else if (strcmp(argv[argi], "--") == 0) {
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

            make_bootfile(argv[argi + 1], argv[argi + 2]);
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

    // compile and install files
    for (; argi < argc; ++argi) {
        // compile
        mobj cstate = boot_compile(argv[argi]);

        // install
        void *fn = install_module(cstate);
        printf("[installed %s at %p]\n", argv[argi], fn);

        // run
        call0(fn);
    }

    GC_shutdown();
    return 0;
}
