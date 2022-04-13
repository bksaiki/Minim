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

char *compile_string(MinimEnv *env, char *str, size_t len)
{
    PrintParams pp;
    MinimObject *exit_handler, *port, *stx, *obj, *err;
    jmp_buf *exit_buf;
    FILE *tmp;

    // set up string as file
    tmp = tmpfile();
    fputs(str, tmp);
    rewind(tmp);

    port = minim_file_port(tmp, MINIM_PORT_MODE_READ |
                                MINIM_PORT_MODE_READY |
                                MINIM_PORT_MODE_OPEN);
    MINIM_PORT_NAME(port) = "string";

    // setup environment
    set_default_print_params(&pp);
    exit_buf = GC_alloc_atomic(sizeof(jmp_buf));
    exit_handler = minim_jmp(exit_buf, NULL);
    if (setjmp(*exit_buf) == 0)
    {
        minim_error_handler = exit_handler;
        minim_exit_handler = exit_handler;
        stx = minim_cons(minim_ast(intern("begin"), NULL), minim_null);
        while (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
        {
            stx = minim_cons(minim_parse_port(port, &err, 0), stx);
            if (err)
            {
                const char *msg = "parsing failed";
                char *out = GC_alloc_atomic((strlen(msg) + 1) * sizeof(char));
                strcpy(out, msg);
                return out;
            }
        }

        stx = minim_ast(minim_list_reverse(stx), NULL);
        check_syntax(env, stx);
        expand_module_level(env, stx);
        compile_expr(env, stx);

        obj = env_get_sym(env, MINIM_SYMBOL(intern("top")));
        if (obj == NULL) {
            const char *msg = "FAILED! could not find top-level function";
            char *out = GC_alloc_atomic((strlen(msg) + 1) * sizeof(char));
            strcpy(out, msg);
            return out;
        }
    }
    else
    {
        obj = MINIM_JUMP_VAL(exit_handler);
        if (MINIM_OBJ_ERRORP(obj))
            return print_to_string(obj, env, &pp);
    }

    return NULL;
}

bool compile_test(char *input, char *expected)
{
    MinimEnv *env;
    char *str;

    // read and compile first expression
    env = init_env(NULL);
    str = compile_string(env, input, strlen(input));
    if (str == NULL || strcmp(str, expected) != 0) {
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
