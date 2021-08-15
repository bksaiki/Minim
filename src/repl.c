#include <setjmp.h>
#include <signal.h>
#include <string.h>

#include "minim.h"
#include "read.h"
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
    Buffer *path;
    uint8_t last_readf;

    printf("Minim v%s \n", MINIM_VERSION_STR);
    fflush(stdout);

    init_env(&top_env, NULL, NULL);
    top_env->current_dir = "REPL";
    minim_load_builtins(top_env);

    path = get_directory(argv[0]);
    init_minim_module_cache(&imports);
    init_minim_module(&module, imports);
    init_env(&module->env, top_env, NULL);
    module->env->module = module;
    module->env->current_dir = get_buffer(path);

    set_default_print_params(&pp);
    last_readf = READ_TABLE_FLAG_EOF;
    if (!(flags & MINIM_FLAG_LOAD_LIBS))
        minim_load_library(module->env);

    setjmp(top_of_repl);
    signal(SIGINT, int_handler);

    exit_buf = GC_alloc_atomic(sizeof(jmp_buf));
    exit_handler = minim_jmp(exit_buf, NULL);
    if (setjmp(*exit_buf) == 0)
    {
        minim_exit_handler = exit_handler;
        while (1)
        {   
            ReadTable rt;
            SyntaxNode *ast, *err;
            MinimObject *obj;

            rt.idx = 0;
            rt.row = 1;
            rt.col = 0;
            rt.eof = '\n';
            rt.flags = READ_TABLE_FLAG_WAIT | last_readf;

            if (rt.flags & READ_TABLE_FLAG_EOF)
            {
                printf("> ");
                fflush(stdout);
                rt.flags ^= READ_TABLE_FLAG_EOF;
            }
            
            if (minim_parse_port(stdin, "repl", &ast, &err, &rt))
            {
                if (rt.flags & READ_TABLE_FLAG_BAD)
                    ast = err;

                if (ast)
                {
                    printf("; bad syntax: %s\n", ast->sym);
                    fflush(stdout);
                }
                
                last_readf = rt.flags & READ_TABLE_FLAG_EOF;
                continue;
            }

            last_readf = rt.flags;
            error_buf = GC_alloc_atomic(sizeof(jmp_buf));
            err_handler = minim_jmp(error_buf, NULL);
            if (setjmp(*error_buf) == 0)
            {
                minim_error_handler = err_handler;
                eval_ast(module->env, ast, &obj);
                if (MINIM_OBJ_ERRORP(obj))
                {    
                    print_minim_object(obj, module->env, &pp);
                    printf("\n");
                    fflush(stdout);
                }
                else if (MINIM_OBJ_EXITP(obj))
                {
                    break;
                }
                else if (!minim_voidp(obj))
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
