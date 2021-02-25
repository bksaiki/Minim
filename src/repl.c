#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minim.h"
#include "read.h"
#include "repl.h"

static void int_handler(int sig)
{
    printf(" !! User break\n");
    fflush(stdout);
    fflush(stdin);
    signal(SIGINT, int_handler);
}

int minim_repl()
{
    MinimEnv *env;
    MinimAst *ast;
    MinimObject *obj;
    Buffer *bf;
    PrintParams pp;
    char *input;

    printf("Minim v%s \n", MINIM_VERSION_STR);

    init_env(&env, NULL);
    minim_load_builtins(env);
    minim_load_library(env);
    set_default_print_params(&pp);
    signal(SIGINT, int_handler);

    while (1)
    {
        ReadResult rr;
        SyntaxLoc *loc;

        init_buffer(&bf);
        init_syntax_loc(&loc, "REPL");
        set_default_read_result(&rr);

        loc->row = 0;
        loc->col = 0;
        
        printf("> ");
        while (!(rr.status & READ_RESULT_EOF) || rr.status & READ_RESULT_INCOMPLETE)
        {
            if (rr.status & READ_RESULT_INCOMPLETE)
                writec_buffer(bf, ' ');
                
            rr.status = READ_RESULT_SUCCESS;
            fread_expr(stdin, bf, loc, &rr, '\n');
        }

        input = get_buffer(bf);
        if (strlen(input) == 0 || strcmp(input, "\377") == 0)
        {
            free_buffer(bf);
            free_syntax_loc(loc);
            continue;
        }
        else if (strcmp(input, "(exit)") == 0)
        {
            free_buffer(bf);
            free_syntax_loc(loc);
            break;
        }

        if (!parse_expr_loc(input, &ast, loc))
        {
            free_buffer(bf);
            free_syntax_loc(loc);
            continue;
        }
        
        eval_ast(env, ast, &obj);
        if (obj->type == MINIM_OBJ_ERR)
        {    
            print_minim_object(obj, env, &pp);
            printf("\n  in: %s\n", "REPL");
        }
        else if (obj->type != MINIM_OBJ_VOID)
        {
            print_minim_object(obj, env, &pp);
            printf("\n");
        }

        free_syntax_loc(loc);
        free_minim_object(obj);
        free_ast(ast);
        free_buffer(bf);
    }

    free_env(env);
    signal(SIGINT, SIG_DFL);
    return 0;
}