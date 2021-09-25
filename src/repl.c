#include <setjmp.h>
#include <signal.h>
#include <string.h>

#include "minim.h"
#include "run.h"
#include "repl.h"

static jmp_buf top_of_repl;

static void flush_stdin()
{
    char c;

    c = getc(stdin);
    while (c != '\n' && c != EOF)
        c = getc(stdin);
}

static void int_handler(int sig)
{
    printf(" ; user break\n");
    fflush(stdout);
    flush_stdin();
    longjmp(top_of_repl, 0);
}

static bool is_int8(MinimObject *thing)
{
    MinimObject *limit;

    if (!MINIM_OBJ_NUMBERP(thing) || !minim_exact_nonneg_intp(thing))
        return false;

    limit = uint_to_minim_number(256);
    return minim_number_cmp(thing, limit) < 0;
}

int minim_repl(char **argv, uint32_t flags)
{
    PrintParams pp;
    MinimEnv *top_env;
    MinimModule *module;
    MinimModuleCache *imports;
    MinimObject *exit_handler, *err_handler;
    jmp_buf *exit_buf, *error_buf;

    printf("Minim v%s \n", MINIM_VERSION_STR);
    fflush(stdout);

    // set up builtins
    init_env(&top_env, NULL, NULL);
    minim_load_builtins(top_env);

    // set up cache
    init_minim_module_cache(&imports);
    init_minim_module(&module, imports);
    init_env(&module->env, top_env, NULL);
    module->env->module = module;
    module->env->current_dir = get_current_dir();

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

    MINIM_PORT_NAME(minim_input_port) = "REPL";
    GC_register_root(minim_error_port);
    GC_register_root(minim_output_port);
    GC_register_root(minim_input_port);

    // Set up handlers
    set_default_print_params(&pp);
    exit_buf = GC_alloc_atomic(sizeof(jmp_buf));
    exit_handler = minim_jmp(exit_buf, NULL);
    if (setjmp(*exit_buf) == 0)
    {
        error_buf = GC_alloc_atomic(sizeof(jmp_buf));
        err_handler = minim_jmp(error_buf, NULL);
        if (setjmp(*error_buf) != 0)    // error on loading library
        {
            print_minim_object(MINIM_JUMP_VAL(err_handler), module->env, &pp);
            printf("\n");
            fflush(stdout);
            return 0;
        }

        minim_exit_handler = exit_handler;
        minim_error_handler = err_handler;
        if (!(flags & MINIM_FLAG_LOAD_LIBS))
            minim_load_library(module->env);

        setjmp(top_of_repl);
        signal(SIGINT, int_handler);

        // enter REPL
        while (1)
        {   
            MinimObject *obj;
            SyntaxNode *ast, *err;

            // get user input
            while (1)
            {
                printf("> ");
                fflush(stdout);

                ast = minim_parse_port(minim_input_port, &err, READ_FLAG_WAIT);
                if (err)
                {
                    printf("; bad syntax: %s\n", err->sym);
                    printf(";   in REPL:%zu:%zu", MINIM_PORT_ROW(minim_input_port),
                                                  MINIM_PORT_COL(minim_input_port));
                    fflush(stdout);
                    continue;
                }

                break;
            }

            MINIM_PORT_MODE(minim_input_port) |= MINIM_PORT_MODE_READY;
            err_handler = minim_jmp(error_buf, NULL);
            if (setjmp(*error_buf) == 0)
            {
                minim_error_handler = err_handler;
                obj = eval_ast(module->env, ast);
                if (!minim_voidp(obj))
                {
                    print_minim_object(obj, module->env, &pp);
                    printf("\n");
                    fflush(stdout);
                }
            }       // error thrown
            else
            {
                print_minim_object(MINIM_JUMP_VAL(err_handler), module->env, &pp);
                printf("\n");
                fflush(stdout);
            }
        }

        signal(SIGINT, SIG_DFL);
        return 0;
    }
    else    // thrown
    {
        MinimObject *thrown = MINIM_JUMP_VAL(exit_handler);

        signal(SIGINT, SIG_DFL);
        return is_int8(thrown) ? MINIM_NUMBER_TO_UINT(thrown): 0;
    }
}
