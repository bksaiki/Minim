#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "eval.h"

static bool is_int(char *str)
{
    char *it = str;

    if (*it == '+' || *it == '-')
        ++it;

    while (*it >= '0' && *it <= '9')    ++it;
    return (*it == '\0');
}

static int is_num_pred(const void *thing)
{
    MinimObject **pobj = (MinimObject**) thing;
    return ((*pobj)->type == MINIM_OBJ_NUM);
}

static MinimObject *eval_math(MinimAstNode *node, int argc, MinimObject **args)
{
    MinimObject *res;

    if (!all_of(args, argc, sizeof(MinimObject*), is_num_pred))
    {
        printf("Expected numerical arguments\n");
        return NULL;
    }

    if (strcmp(node->sym, "+") == 0)
    {
        if (argc == 0)  // arity mismatch
        {
            printf("Expected at least 1 argument for '+'\n");
            return NULL;
        }

        res = construct_minim_object(MINIM_OBJ_NUM, *((int*)args[0]->data));
        for (int i = 1; i < argc; ++i)
        {
            int *accum = (int*)res->data;
            int *pval = (int*)args[i]->data;
            *accum += *pval;
        }
    }
    else if (strcmp(node->sym, "-") == 0)
    {
        if (argc == 0)  // arity mismatch
        {
            printf("Expected at least 1 argument for '/'\n");
            return NULL;
        }
        else if (argc == 1)
        {
            int val = *((int*)args[0]->data);
            return construct_minim_object(MINIM_OBJ_NUM, -val);
        }
        else
        {
            res = construct_minim_object(MINIM_OBJ_NUM, *((int*)args[0]->data));
            for (int i = 1; i < argc; ++i)
            {
                int *accum = (int*)res->data;
                int *pval = (int*)args[i]->data;
                *accum -= *pval;
            }
        }
    }
    else if (strcmp(node->sym, "*") == 0)
    {
        if (argc == 0)  // arity mismatch
        {
            printf("Expected at least 1 argument for '*'\n");
            return NULL;
        }

        res = construct_minim_object(MINIM_OBJ_NUM, *((int*)args[0]->data));
        for (int i = 1; i < argc; ++i)
        {
            int *accum = (int*)res->data;
            int *pval = (int*)args[i]->data;
            *accum *= *pval;
        }
    }
    else // strcmp(node->sym, "/") == 0
    {
        if (argc != 2)
        {
            printf("Expected 2 arguments for '/'\n");
            return NULL;
        }

        int num = *((int*) args[0]->data);
        int den = *((int*) args[1]->data);
        res = construct_minim_object(MINIM_OBJ_NUM, num / den);
    }

    return res;
}

static MinimObject *ast_node_to_obj(MinimAstNode *node)
{
    if (is_int(node->sym))
    {
        return construct_minim_object(MINIM_OBJ_NUM, atoi(node->sym));
    }
    else
    {
        return construct_minim_object(MINIM_OBJ_SYM, atoi(node->sym));
    }
}

static void free_args(int argc, MinimObject **args)
{
    for (int i = 0; i < argc; ++i)
        free_minim_object(args[i]);
}

static MinimObject *eval_ast_node(MinimAstNode *node)
{
    if (node == NULL)
        return NULL;

    if (node->tag == MINIM_AST_OP)
    {
        MinimObject** args = malloc(node->argc * sizeof(MinimObject*));
        MinimObject *result;
        
        for (int i = 0; i < node->argc; ++i)
            args[i] = eval_ast_node(node->children[i]);

        if (strcmp(node->sym, "+") == 0 || strcmp(node->sym, "-") == 0 ||
            strcmp(node->sym, "*") == 0 || strcmp(node->sym, "/") == 0)
        {
            result = eval_math(node, node->argc, args);
        }
        else
        {  
            result = NULL;
        }

        free_args(node->argc, args);
        free(args);
        return result;
    }
    else
    {
        return ast_node_to_obj(node);
    }
}

// Visible functions

int eval_ast(MinimAstWrapper *ast, MinimObjectWrapper *objw)
{
    MinimObject *result = eval_ast_node(ast->node);
    
    if (!result)
    {
        printf("Evaluation failed!\n");
        return 0;
    }
    else
    {
        objw->obj = result;
        return 1;
    }
}