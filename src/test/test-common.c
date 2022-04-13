#include "test-common.h"
#include "../core/minimpriv.h"
#include "../compiler/compile.h"

void setup_test_env()
{
    // set up globals
    init_global_state(GLOBAL_FLAG_DEFAULT);
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

bool compile_test(char *input, char *expected)
{
    MinimObject *stx, *obj;
    MinimEnv *env;
    char *str;

    // read and compile first expression
    env = init_env(NULL);
    stx = compile_string(env, input, strlen(input));
    if (stx == NULL)    return false;

    compile_expr(env, stx);
    obj = env_get_sym(env, MINIM_SYMBOL(intern("top")));
    if (obj == NULL) {
        printf("FAILED! could not find top-level function\n");
        return false;
    }

    str = "bad";
    if (strcmp(str, expected) == 0) {
        printf("FAILED! input: %s, expected: %s, got: %s\n", input, expected, str);
        return false;
    }

    return true;
}

bool run_test(char *input, char *expected)
{
    MinimEnv *env;
    char *str;
    bool s;

    env = init_env(NULL);
    str = eval_string(env, input, INT_MAX);
    if (!str)   return false;

    s = (strcmp(str, expected) == 0);
    if (!s) printf("FAILED! input: %s, expected: %s, got: %s\n", input, expected, str);

    return s;
}

bool evaluate(char *input)
{
    MinimEnv *env = init_env(NULL);
    return eval_string(env, input, INT_MAX);
}
