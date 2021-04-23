#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minim.h"
#include "read.h"
#include "repl.h"

static void flush_stdin()
{
    char c;
    while ((c = getc(stdin)) != '\n' && c != EOF);
}

static void int_handler(int sig)
{
    printf(" !! User break\n");
    fflush(stdout);
    flush_stdin();
}

int minim_repl(uint32_t flags)
{
    MinimEnv *env;
    SyntaxNode *ast;
    MinimObject *obj;
    PrintParams pp;

    printf("Minim v%s \n", MINIM_VERSION_STR);

    init_env(&env, NULL);
    minim_load_builtins(env);
    set_default_print_params(&pp);
    signal(SIGINT, int_handler);

    if (!(flags & MINIM_FLAG_LOAD_LIBS))
        minim_load_library(env);

    while (1)
    {   
        printf("> ");
        if (minim_parse_port(stdin, "repl", &ast, '\n', true))
        {
            if (ast)
            {
                printf("bad syntax: %s\n", ast->sym);
                free_syntax_node(ast);
            }
            
            flush_stdin();
            continue;
        }

       if (ast->childc == 1 && ast->children[0]->sym &&
            strcmp(ast->children[0]->sym, "exit") == 0)
            break;
        
        eval_ast(env, ast, &obj);
        if (obj->type == MINIM_OBJ_ERR)
        {    
            print_minim_object(obj, env, &pp);
            printf("\n;  in: %s\n", "REPL");
        }
        else if (obj->type != MINIM_OBJ_VOID)
        {
            print_minim_object(obj, env, &pp);
            printf("\n");
        }

        free_minim_object(obj);
        free_syntax_node(ast);
    }

    free_env(env);
    signal(SIGINT, SIG_DFL);
    return 0;
}