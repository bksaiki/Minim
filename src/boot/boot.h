/*
    Header file for the bootstrap interpreter.
    Should not be referenced outside this directory
*/

#define MINIM_BOOT_VERSION      "0.1.0"

// Limits

#define SYMBOL_MAX_LEN      4096

// Testing interface

void minim_boot_init();
minim_object *eval_expr(minim_object *expr, minim_object *env);
minim_object *make_env();
