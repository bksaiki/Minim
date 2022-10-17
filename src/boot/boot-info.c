/*
    Prints information about the bootstrap interpretr
*/

#include "../minim.h"
#include "boot.h"

char *type_strs[MINIM_LAST_TYPE] = {
    "null",
    "true",
    "false",
    "eof",
    "void",

    "symbol",
    "fixnum",
    "char",
    "string",
    "pair",
    "prim_proc",
    "closure_proc",
    "input_port",
    "output_port"
};

int type_sizes[MINIM_LAST_TYPE] = {
    sizeof(minim_object),
    sizeof(minim_object),
    sizeof(minim_object),
    sizeof(minim_object),
    sizeof(minim_object),

    sizeof(minim_symbol_object),
    sizeof(minim_fixnum_object),
    sizeof(minim_char_object),
    sizeof(minim_string_object),
    sizeof(minim_pair_object),
    sizeof(minim_prim_proc_object),
    sizeof(minim_closure_proc_object),
    sizeof(minim_input_port_object),
    sizeof(minim_output_port_object)
};

int main(int argv, char **argc) {
    printf("Minim Bootstrap Interpreter Information\n");
    printf("Version: %s\n", MINIM_BOOT_VERSION);
    printf("Type count: %d\n", MINIM_LAST_TYPE);

    for (int i = 0; i < MINIM_LAST_TYPE; ++i) {
        printf(" %s: %d bytes\n", type_strs[i], type_sizes[i]);
    }


    return 0;
}
