/*
    Header file for the bootstrap interpreter.
    Should not be referenced outside this directory

    Entry point of the REPL
*/

#include "boot.h"

//
//  REPL
//

int main(int argv, char **argc) {
    volatile int stack_top;
    minim_object *expr, *evaled;

    printf("Minim v%s\n", MINIM_BOOT_VERSION);

    GC_init(((void*) &stack_top));
    minim_boot_init();
    
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
