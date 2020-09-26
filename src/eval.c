#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eval.h"
#include "list.h"
#include "util.h"

static bool is_int(char *str)
{
    char *it = str;

    if ((*it == '+' || *it == '-') &&
        (*(it + 1) >= '0' && *(it + 1) <= '9'))
    {
        it += 2;
    }

    while (*it >= '0' && *it <= '9')    ++it;
    return (*it == '\0');
}
static int is_err_pred(const void *thing)
{
    MinimObject **pobj = (MinimObject**) thing;
    return ((*pobj)->type == MINIM_OBJ_ERR);
}

static MinimObject *ast_node_to_obj(MinimEnv *env, MinimAstNode *node, bool quote)
{
    MinimObject *res;

    if (is_int(node->sym))
    {
        init_minim_object(&res, MINIM_OBJ_NUM, atoi(node->sym));
    }
    else
    {
        if (quote)
        {
            init_minim_object(&res, MINIM_OBJ_SYM, node->sym);
        }
        else
        {
            res = env_get_sym(env, node->sym);
            if (!res)
                minim_error(&res, "Unrecognized symbol: %s", node->sym);   
        } 
    }

    return res;
}

// Eval quot mainloop

static MinimObject *eval_sexpr_node(MinimEnv *env, MinimObject *node)
{
    if (minim_listp(node))
    {
        MinimObject **args, *op, *it, **pair;
        MinimBuiltin proc;
        int argc = minim_list_length(node) - 1;

        args = malloc(argc * sizeof(MinimObject*));
        pair = node->data;
        op = env_peek_sym(env, ((char*) pair[0]->data));
        
        for (int i = 0; i < argc; ++i)
        {
            it = pair[1];
            pair = it->data;
            args[i] = pair[0];
            pair[0] = NULL;
        }

        proc = op->data;
        free_minim_object(node);
        node = proc(env, argc, args);
    }

    return node;
}

// S-expression mainloop

static MinimObject *ast_node_to_sexpr(MinimEnv *env, MinimAstNode* node)
{
    if (node->tag == MINIM_AST_OP)
    {
        MinimObject **args;
        MinimObject *res, *list;
        MinimBuiltin proc;
        int argc = node->count;

        list = env_peek_sym(env, "list");
        proc = ((MinimBuiltin) list->data);

        args = malloc(argc * sizeof(MinimObject*));
        for (int i = 0; i < argc; ++i)
            args[i] = ast_node_to_sexpr(env, node->children[i]);

        res = proc(env, argc + 1, args);
        return res;
    }
    else
    {
        return ast_node_to_obj(env, node, false);
    }
}

// Quote mainloop

static MinimObject *ast_node_to_quote(MinimEnv *env, MinimAstNode* node)
{
    if (node->tag == MINIM_AST_OP)
    {
        MinimObject **args;
        MinimObject *res, *list;
        MinimBuiltin proc;
        int argc = node->count;

        args = malloc(argc * sizeof(MinimObject*));
        list = env_get_sym(env, "list");
        proc = ((MinimBuiltin) list->data);

        for (int i = 0; i < argc; ++i)
            args[i] = ast_node_to_quote(env, node->children[i]);

        res = proc(env, argc, args);
        free_minim_object(list);
        return res;
    }
    else
    {
        return ast_node_to_obj(env, node, true);
    }
}

// Eval mainloop

static MinimObject *eval_ast_node(MinimEnv *env, MinimAstNode *node)
{
    if (node->tag == MINIM_AST_OP)
    {
        MinimObject **args;
        MinimObject *res, *op, **possible_err;
        MinimBuiltin proc;
        int argc = node->count - 1;
        bool consumeNodes = false;

        args = malloc(argc * sizeof(MinimObject*));
        op = env_get_sym(env, node->children[0]->sym);

        if (!op)
        {
            minim_error(&res, "Unknown operator: %s", node->sym);
            return res;
        }

        proc = ((MinimBuiltin) op->data);
        if (op->type == MINIM_OBJ_FUNC)
        {
            for (int i = 0; i < argc; ++i)
                args[i] = eval_ast_node(env, node->children[i + 1]);
        }
        else if (op->type == MINIM_OBJ_SYNTAX)
        {
            consumeNodes = true;
            for (int i = 0; i < argc; ++i)
            {
                init_minim_object(&args[i], MINIM_OBJ_AST, node->children[i + 1]);
                node->children[i + 1] = NULL;
            }
        }
        else
        {   
            minim_error(&res, "'%s' is not an operator", node->sym);
            free(args);
            return res;
        }

        possible_err = for_first(args, argc, sizeof(MinimObject*), is_err_pred);
        if (possible_err)
        {
            for (int i = 0; i < argc; ++i)    // Clear it so it doesn't get deleted
            {
                if (&args[i] == possible_err)
                {
                    res = *possible_err;
                    args[i] = NULL;
                }
            }
            
            free_minim_objects(argc, args);
            free(op);
            return res;
        }

        res = proc(env, argc, args);
        if (consumeNodes)   argc = 0;
        free(op);
        return res;
    }
    else
    {
        return ast_node_to_obj(env, node, false);
    }
}

// Visible functions

int eval_ast(MinimEnv *env, MinimAstNode *ast, MinimObject **pobj)
{
    MinimObject *obj = eval_ast_node(env, ast);
    *pobj = obj;
    return (obj->type != MINIM_OBJ_ERR);
}

int eval_ast_as_quote(MinimEnv *env, MinimAstNode *ast, MinimObject **pobj)
{
    MinimObject *obj = ast_node_to_quote(env, ast);
    *pobj = obj;
    return (obj->type != MINIM_OBJ_ERR);
}

int eval_ast_as_sexpr(MinimEnv *env, MinimAstNode *ast, MinimObject **pobj)
{
    MinimObject *obj = ast_node_to_sexpr(env, ast);
    *pobj = obj;
    return (obj->type != MINIM_OBJ_ERR);
}

int eval_sexpr(MinimEnv *env, MinimObject *expr, MinimObject **pobj)
{
    MinimObject *obj = eval_sexpr_node(env, expr);
    *pobj = obj;
    return (obj->type != MINIM_OBJ_ERR);
}