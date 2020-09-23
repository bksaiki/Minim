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

static int is_err_pred(const void *thing)
{
    MinimObject **pobj = (MinimObject**) thing;
    return ((*pobj)->type == MINIM_OBJ_ERR);
}

static void free_args(int argc, MinimObject **args)
{
    for (int i = 0; i < argc; ++i)
        free_minim_object(args[i]);
}

//
//  Evaluators
//

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
            return construct_minim_object(MINIM_OBJ_ERR, "Expected 1 argument for '+'");

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
            return construct_minim_object(MINIM_OBJ_ERR, "Expected 1 argument for '-'");
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
            return construct_minim_object(MINIM_OBJ_ERR, "Expected 1 argument for '*'");

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
            return construct_minim_object(MINIM_OBJ_ERR, "Expected 2 arguments for '/'");

        int num = *((int*) args[0]->data);
        int den = *((int*) args[1]->data);
        res = ((den == 0) ? construct_minim_object(MINIM_OBJ_ERR, "Division by zero") :
                            construct_minim_object(MINIM_OBJ_NUM, num / den));
    }

    return res;
}

static MinimObject *eval_pair(MinimAstNode *node, int argc, MinimObject **args)
{
    MinimObject *res;

    if (strcmp(node->sym, "cons") == 0)
    {
        if (node->argc != 2)
            return construct_minim_object(MINIM_OBJ_ERR, "Expected 2 arguments for 'cons");
        res = construct_minim_object(MINIM_OBJ_PAIR, args[0], args[1]);
    }
    else if (strcmp(node->sym, "car") == 0)
    {
        MinimObject** pair;

        if (node->argc != 1)
            return construct_minim_object(MINIM_OBJ_ERR, "Expected 1 argument for 'car");

        if (args[0]->type != MINIM_OBJ_PAIR)
            return construct_minim_object(MINIM_OBJ_ERR, "Expected a pair for 'car");

        pair = (MinimObject**)args[0]->data;
        res = copy_minim_object(pair[0]);
    }
    else if (strcmp(node->sym, "cdr") == 0)
    {
        MinimObject** pair;

        if (node->argc != 1)
            return construct_minim_object(MINIM_OBJ_ERR, "Expected 1 argument for 'cdr");

        if (args[0]->type != MINIM_OBJ_PAIR)
            return construct_minim_object(MINIM_OBJ_ERR, "Expected a pair for 'cdr");

        pair = (MinimObject**)args[0]->data;
        res = copy_minim_object(pair[1]);
    }

    return res;
}

// Mainloop

static MinimObject *ast_node_to_obj(MinimAstNode *node)
{
    if (is_int(node->sym))
    {
        return construct_minim_object(MINIM_OBJ_NUM, atoi(node->sym));
    }
    else
    {
        return construct_minim_object(MINIM_OBJ_SYM, node->sym);
    }
}

static MinimObject *eval_ast_node(MinimAstNode *node)
{
    if (node == NULL)
        return NULL;

    if (node->tag == MINIM_AST_OP)
    {
        MinimObject** args = malloc(node->argc * sizeof(MinimObject*));
        MinimObject** possible_err;
        MinimObject *result;
        
        for (int i = 0; i < node->argc; ++i)
            args[i] = eval_ast_node(node->children[i]);

        possible_err = for_first(args, node->argc, sizeof(MinimObject*), is_err_pred);
        if (possible_err)
        {
            result = *possible_err;
            for (int i = 0; i < node->argc; ++i)    // Clear it so it doesn't get deleted
            {
                if (&args[i] == possible_err)
                    args[i] = NULL;
            }
        }
        else if (strcmp(node->sym, "+") == 0 || strcmp(node->sym, "-") == 0 ||
            strcmp(node->sym, "*") == 0 || strcmp(node->sym, "/") == 0)
        {
            result = eval_math(node, node->argc, args);
        }
        else if (strcmp(node->sym, "cons") == 0 || 
                 strcmp(node->sym, "car") == 0 || strcmp(node->sym, "cdr") == 0)
        {
            result = eval_pair(node, node->argc, args);
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