#include "test-common.h"

void setup_test_env()
{
    // set up globals
    init_global_state();
    init_builtins();
    
    // set up ports
    minim_error_port = minim_file_port(stderr, MINIM_PORT_MODE_WRITE |
                                               MINIM_PORT_MODE_OPEN |
                                               MINIM_PORT_MODE_READY);
    minim_output_port = minim_file_port(stdout, MINIM_PORT_MODE_WRITE |
                                                MINIM_PORT_MODE_OPEN |
                                                MINIM_PORT_MODE_READY);
    minim_input_port = minim_file_port(stdin, MINIM_PORT_MODE_READ |
                                              MINIM_PORT_MODE_OPEN |
                                              MINIM_PORT_MODE_READY |
                                              MINIM_PORT_MODE_ALT_EOF);

    MINIM_PORT_NAME(minim_input_port) = "stdin";
    GC_register_root(minim_error_port);
    GC_register_root(minim_output_port);
    GC_register_root(minim_input_port);
}

bool run_test(char *input, char *expected)
{
    char *str;
    bool s;

    str = eval_string(input, INT_MAX);
    if (!str)   return false;

    s = (strcmp(str, expected) == 0);
    if (!s) printf("FAILED! input: %s, expected: %s, got: %s\n", input, expected, str);

    return s;
}

bool evaluate(char *input)
{
    return eval_string(input, INT_MAX);
}
