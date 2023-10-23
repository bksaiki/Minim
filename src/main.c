// main.c: entry point of the executable

#include "minim.h"

int main(int argc, char **argv) {
    GC_INIT();

    printf("Minim v%s\n", MINIM_VERSION);
    minim_init();

    GC_deinit();
    return 0;
}
