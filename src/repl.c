#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minim.h"
#include "read.h"
#include "repl.h"

int minim_repl()
{
    MinimEnv *env;
    MinimAst *ast;
    MinimObject *obj;
    Buffer *bf;
    PrintParams pp;
    char *input;

    init_env(&env, NULL);
    minim_load_builtins(env);
    minim_load_library(env);
    set_default_print_params(&pp);

    printf("Minim v%s \n", MINIM_VERSION_STR);

    while (1)
    {
        ReadResult rr;
        SyntaxLoc *loc;

        init_buffer(&bf);
        init_syntax_loc(&loc, "REPL");
        set_default_read_result(&rr);
        
        printf("> ");
        while (!(rr.status & READ_RESULT_EOF) || rr.status & READ_RESULT_INCOMPLETE)
        {
            if (rr.status & READ_RESULT_INCOMPLETE)
                writec_buffer(bf, ' ');
                
            rr.status = READ_RESULT_SUCCESS;
            fread_expr(stdin, bf, loc, &rr, '\n');
        }

        free_syntax_loc(loc);

        input = get_buffer(bf);
        if (strlen(input) == 0)
        {
            free_buffer(bf);
            continue;
        }
        else if (strcmp(input, "(exit)") == 0)
        {
            free_buffer(bf);
            break;
        }

        if (!parse_str(input, &ast))
        {
            free_buffer(bf);
            continue;
        }
        
        eval_ast(env, ast, &obj);
        if (obj->type != MINIM_OBJ_VOID)
        {
            print_minim_object(obj, env, &pp);
            printf("\n");
        }

        free_minim_object(obj);
        free_ast(ast);
        free_buffer(bf);
    }

    free_env(env);
    return 0;
}