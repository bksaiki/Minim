#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eval.h"
#include "lambda.h"
#include "list.h"
#include "number.h"
#include "util.h"

static bool is_rational(char *str)
{
    char *it = str;

    if ((*it == '+' || *it == '-') &&
        (*(it + 1) >= '0' && *(it + 1) <= '9'))
    {
        it += 2;
    }

    while (*it >= '0' && *it <= '9')    ++it;

    if (*it == '/' && *(it + 1) >= '0' && *(it + 1) <= '9')
    {
        it += 2;
        while (*it >= '0' && *it <= '9')    ++it;
    }

    return (*it == '\0');
}

static bool is_float(char *str)
{
    char *it = str;

    if ((*it == '+' || *it == '-') &&
        (*(it + 1) >= '0' && *(it + 1) <= '9'))
    {
        it += 2;
    }

    while (*it >= '0' && *it <= '9')    ++it;

    if (*it != '.' && *it != 'e')
        return false;

    if (*it == '.')     ++it;

    while (*it >= '0' && *it <= '9')    ++it;

    if (*it == 'e')
    {
        ++it;
        if ((*it == '+' || *it == '-') &&
            (*(it + 1) >= '0' && *(it + 1) <= '9'))
        {
            it += 2;
        }

        while (*it >= '0' && *it <= '9')    ++it;
    }

    return (*it == '\0');
}

static int is_err_pred(const void *thing)
{
    MinimObject **pobj = (MinimObject**) thing;
    return ((*pobj)->type == MINIM_OBJ_ERR);
}

static MinimObject *str_to_node(char *str, MinimEnv *env, bool quote)
{
    MinimObject *res;

    if (is_rational(str))
    {
        MinimNumber *num;
        init_minim_number(&num, MINIM_NUMBER_EXACT);
        str_to_minim_number(num, str);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
    }
    else if (is_float(str))
    {
        MinimNumber *num;
        init_minim_number(&num, MINIM_NUMBER_INEXACT);
        str_to_minim_number(num, str);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
    }
    else
    {
        if (quote)
        {
            init_minim_object(&res, MINIM_OBJ_SYM, str);
        }
        else
        {
            res = env_get_sym(env, str);
            if (!res)   minim_error(&res, "Unrecognized symbol: %s", str);
        }
    }

    return res;
}

// Eval quot mainloop

static MinimObject *eval_sexpr_node(MinimEnv *env, MinimObject *node)
{
    MinimObject *res;

    if (minim_listp(node))
    {
        MinimObject **args, *op, *it, **pair;
        MinimBuiltin proc;
        int argc = minim_list_length(node) - 1;

        pair = node->data;
        args = malloc(argc * sizeof(MinimObject*));
        op = env_peek_sym(env, ((char*) pair[0]->data));
        
        for (int i = 0; i < argc; ++i)
        {
            it = pair[1];
            pair = it->data;
            args[i] = eval_sexpr_node(env, pair[0]);
        }

        proc = op->data;
        res = proc(env, argc, args);
    }
    else
    {
        if (node->type == MINIM_OBJ_SYM)
        {   
            res = env_get_sym(env, ((char*) node->data));
            if (!res)   minim_error(&res, "Unrecognized symbol: %s", ((char*) node->data));
        }
        else
        {
            copy_minim_object(&res, node);
        }
    }

    return res;
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
        return str_to_node(node->sym, env, false);
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
        list = env_peek_sym(env, "list");
        proc = ((MinimBuiltin) list->data);

        for (int i = 0; i < argc; ++i)
            args[i] = ast_node_to_quote(env, node->children[i]);

        res = proc(env, argc, args);
        return res;
    }
    else
    {
        return str_to_node(node->sym, env, true);
    }
}

// Eval mainloop

static MinimObject *eval_ast_node(MinimEnv *env, MinimAstNode *node)
{
    if (node->tag == MINIM_AST_OP)
    {
        MinimObject *res, *op, **possible_err, **args;
        int argc = node->count - 1;

        args = malloc(argc * sizeof(MinimObject*));
        op = env_peek_sym(env, node->children[0]->sym);

        if (!op)
        {
            minim_error(&res, "Unknown operator: %s", node->children[0]->sym);
            free(args);
            return res;
        }

        if (op->type == MINIM_OBJ_FUNC)
        {
            MinimBuiltin proc = ((MinimBuiltin) op->data);

            for (int i = 0; i < argc; ++i)
                args[i] = eval_ast_node(env, node->children[i + 1]);          

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
                return res;
            }

            res = proc(env, argc, args);
        }
        else if (op->type == MINIM_OBJ_SYNTAX)
        {
            MinimBuiltin proc = ((MinimBuiltin) op->data);

            for (int i = 0; i < argc; ++i)
                init_minim_object(&args[i], MINIM_OBJ_AST, node->children[i + 1]);   // initialize ast wrappers
            res = proc(env, argc, args);

            for (int i = 0; i < argc; ++i) free(args[i]);
            free(args);
        }
        else if (op->type == MINIM_OBJ_CLOSURE)
        {
            MinimLambda *lam;

            copy_minim_lambda(&lam, ((MinimLambda*) op->data));
            for (int i = 0; i < argc; ++i)
                args[i] = eval_ast_node(env, node->children[i + 1]);          

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
                return res;
            }

            res = eval_lambda(lam, env, argc, args);
            free_minim_lambda(lam);

            for (int i = 0; i < argc; ++i) free(args[i]);
            free(args);
        }
        else
        {   
            minim_error(&res, "'%s' is not an operator", node->children[0]->sym);
            free(args);
            return res;
        }

        return res;
    }
    else
    {
        return str_to_node(node->sym, env, false);
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

char *eval_string(char *str, size_t len)
{
    MinimAstNode *ast;
    MinimObject *obj;
    MinimEnv *env;
    char *out;

    init_env(&env);
    env_load_builtins(env);

    if (!parse_str(str, &ast))
    {
        char *tmp = "Parsing failed!";
        out = malloc((strlen(tmp) + 1) * sizeof(char));
        strcpy(out, tmp);
        free_env(env);
        return out;
    }

    eval_ast(env, ast, &obj);
    out = print_to_string(obj, env, len);

    free_minim_object(obj);
    free_ast(ast);
    free_env(env);
    return out;
}