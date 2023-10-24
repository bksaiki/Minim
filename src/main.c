// main.c: entry point of the executable

#include "minim.h"

int main(int argc, char **argv) {
    mobj o;
    mthread *th;

    GC_INIT();

    printf("Minim v%s\n", MINIM_VERSION);
    minim_init();

    th = get_thread();
    while (1) {
        fputs("> ", stdout);
        o = read_object(get_input_port(th));
        if (minim_eofp(o))
            break;

        write_object(get_output_port(th), o);
        fputc('\n', stdout);
    }

    GC_deinit();
    return 0;
}
