#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eval.h"
#include "util.h"

static bool is_int(char *str)
{
    char *it = str;

    if (*it == '+' || *it == '-')
        ++it;

    while (*it >= '0' && *it <= '9')    ++it;
    return (*it == '\0');
}
static int is_err_pred(const void *thing)
{
    MinimObject **pobj = (MinimObject**) thing;
    return ((*pobj)->type == MINIM_OBJ_ERR);
}

// Mainloop

static MinimObject *ast_node_to_obj(MinimEnv *env, MinimAstNode *node)
{
    MinimObject *res;

    if (is_int(node->sym))
        init_minim_object(&res, MINIM_OBJ_NUM, atoi(node->sym));
    else
        init_minim_object(&res, MINIM_OBJ_SYM, node->sym);

    return res;
}

static MinimObject *eval_ast_node(MinimEnv *env, MinimAstNode *node)
{
    if (node == NULL)
        return NULL;

    if (strcmp(node->sym, "quote") == 0)
    {
        MinimObject *quo;
        char* str = malloc((strlen(node->children[0]->sym) + 1) * sizeof(char));

        strcpy(str, "'");
        strcat(str, node->children[0]->sym);
        init_minim_object(&quo, MINIM_OBJ_SYM, str);
        free(str);
        return quo;
    }

    if (node->tag == MINIM_AST_OP)
    {
        MinimObject** args = malloc(node->argc * sizeof(MinimObject*));
        MinimObject** possible_err;
        MinimObject *res, *op;
        MinimBuiltin proc;

        for (int i = 0; i < node->argc; ++i)
            args[i] = eval_ast_node(env, node->children[i]);

        possible_err = for_first(args, node->argc, sizeof(MinimObject*), is_err_pred);
        if (possible_err)
        {
            for (int i = 0; i < node->argc; ++i)    // Clear it so it doesn't get deleted
            {
                if (&args[i] == possible_err)
                    args[i] = NULL;
            }

            free_minim_objects(node->argc, args);
            return *possible_err;
        }

        op = env_get_sym(env, node->sym);
        if (!op)
        {
            free_minim_objects(node->argc, args);
            minim_error(&res, "Unknown operator: %s", node->sym);
            return res;
        }

        proc = ((MinimBuiltin) op->data);
        res = proc(env, node->argc, args);
        free(op);
        return res;
    }
    else
    {
        return ast_node_to_obj(env, node);
    }
}

// Visible functions

int eval_ast(MinimEnv *env, MinimAstNode *ast, MinimObject **pobj)
{
    MinimObject *obj = eval_ast_node(env, ast);
    if (!obj)
    {
        printf("Evaluation failed!\n");
        *pobj = NULL;
        return 0;
    }

    *pobj = obj;
    return 1;
}