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
        {
            init_minim_object(&res, MINIM_OBJ_ERR, "Expected 1 argument for '+'");
            return res;
        }

    }
    else if (strcmp(node->sym, "-") == 0)
    {
        if (argc == 0)  // arity mismatch
        {
            init_minim_object(&res, MINIM_OBJ_ERR, "Expected at least 1 argument for '-'");
            return res;
        }
        else if (argc == 1)
        {
            int val = *((int*)args[0]->data);
            init_minim_object(&res, MINIM_OBJ_NUM, -val);
        }
        else
        {
            init_minim_object(&res, MINIM_OBJ_NUM, *((int*)args[0]->data));
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
            init_minim_object(&res, MINIM_OBJ_ERR, "Expected at least 1 argument for '*'");
            return res;
        }

        init_minim_object(&res, MINIM_OBJ_NUM, *((int*)args[0]->data));
        for (int i = 1; i < argc; ++i)
        {
            int *accum = (int*)res->data;
            int *pval = (int*)args[i]->data;
            *accum *= *pval;
        }
    }
    else // strcmp(node->sym, "/") == 0
    {
        int num, den;

        if (argc != 2)
        {
            init_minim_object(&res, MINIM_OBJ_ERR, "Expected at 2 arguments for '/'");
            return res;
        }

        num = *((int*) args[0]->data);
        den = *((int*) args[1]->data);
        if (den == 0)   init_minim_object(&res, MINIM_OBJ_ERR, "Division by zero");
        else            init_minim_object(&res, MINIM_OBJ_NUM, num / den);
    }

    return res;
}

static MinimObject *eval_pair(MinimAstNode *node, int argc, MinimObject **args)
{
    MinimObject *res;

    if (strcmp(node->sym, "cons") == 0)
    {
        if (node->argc != 2)
            init_minim_object(&res, MINIM_OBJ_ERR, "Expected 2 arguments for 'cons");
        else
            init_minim_object(&res, MINIM_OBJ_PAIR, args[0], args[1]);
    }
    else if (strcmp(node->sym, "car") == 0)
    {
        if (node->argc != 1)
            init_minim_object(&res, MINIM_OBJ_ERR, "Expected 1 argument for 'car");
        else if (args[0]->type != MINIM_OBJ_PAIR)
            init_minim_object(&res, MINIM_OBJ_ERR, "Expected a pair for 'car");
        else
            copy_minim_object(&res, ((MinimObject**) args)[0]->data);
    }
    else if (strcmp(node->sym, "cdr") == 0)
    {
        if (node->argc != 1)
            init_minim_object(&res, MINIM_OBJ_ERR, "Expected 1 argument for 'cdr");
        else if (args[0]->type != MINIM_OBJ_PAIR)
            init_minim_object(&res, MINIM_OBJ_ERR, "Expected a pair for 'cdr");
        else
            copy_minim_object(&res, ((MinimObject**) args)[1]->data);
    }

    return res;
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

    if (node->tag == MINIM_AST_OP)
    {
        MinimObject** args = malloc(node->argc * sizeof(MinimObject*));
        MinimObject** possible_err;
        MinimObject *result;
        
        for (int i = 0; i < node->argc; ++i)
            args[i] = eval_ast_node(env, node->children[i]);

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