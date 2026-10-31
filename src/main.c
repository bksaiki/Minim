/**
 * @file
 * Command-line entry point: a read-print loop.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "minim.h"

static minim_obj read_stdin(void *arg) {
    (void)arg;
    return minim_read(stdin);
}

int main(void) {
    minim_init();
    bool interactive = isatty(fileno(stdin));
    for (;;) {
        if (interactive)
            fputs("> ", stderr);

        minim_obj x;
        if (!minim_catch(read_stdin, NULL, &x)) {
            minim_write_error(stderr);
            continue;
        }
        if (x == MINIM_EOF)
            break;
        minim_write(stdout, x);
        putchar('\n');
    }

    if (interactive)
        fputc('\n', stderr);
    minim_shutdown();
    return 0;
}
