#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtin.h"
#include "eval.h"
#include "lambda.h"
#include "list.h"
#include "number.h"

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
        if (it == str)     return false;

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

static bool is_str(char *str)
{
    size_t len = strlen(str);

    if (len < 2 || str[0] != '\"' || str[len - 1] != '\"')
        return false;

    for (size_t i = 1; i < len; ++i)
    {
        if (str[i] == '\"' && str[i - 1] == '\\')
            return false;
    }

    return true;
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
    else if (is_str(str))
    {
        size_t len = strlen(str) - 1;
        char *tmp = malloc(len * sizeof(char));

        strncpy(tmp, &str[1], len - 1);
        tmp[len - 1] = '\0';
        init_minim_object(&res, MINIM_OBJ_STRING, tmp);
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

static MinimObject *first_error(MinimObject **args, size_t argc)
{
    for (size_t i = 0; i < argc; ++i)
    {
        if (args[i]->type == MINIM_OBJ_ERR)
            return args[i];
    }

    return NULL;
}

// Unsyntax

static MinimObject *unsyntax_ast_node(MinimEnv *env, MinimAst* node, bool rec)
{
    if (node->tags & MINIM_AST_OP)
    {
        MinimObject **args, *res;
        MinimBuiltin proc;

        args = malloc(node->argc * sizeof(MinimObject*));
        proc = ((MinimBuiltin) env_peek_sym(env, "list")->data);

        for (size_t i = 0; i < node->argc; ++i)
        {
            if (rec)    args[i] = unsyntax_ast_node(env, node->children[i], rec);
            else        init_minim_object(&args[i], MINIM_OBJ_AST, node->children[i]);
        }

        res = proc(env, args, node->argc);
        free_minim_objects(args, node->argc);

        return res;
    }
    else
    {
        return str_to_node(node->sym, env, true);
    }
}

// Eval mainloop

static MinimObject *eval_ast_node(MinimEnv *env, MinimAst *node)
{
    if (node->tags & MINIM_AST_OP)
    {
        MinimObject *res, *op, *possible_err, **args;
        size_t argc = node->argc - 1;

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

            for (size_t i = 0; i < argc; ++i)
                args[i] = eval_ast_node(env, node->children[i + 1]);          

            possible_err = first_error(args, argc);
            if (possible_err)
            {
                for (size_t i = 0; i < argc; ++i)    // Clear it so it doesn't get deleted
                {
                    if (args[i] == possible_err)
                    {
                        res = possible_err;
                        args[i] = NULL;
                    }
                }
                
                free_minim_objects(args, argc);
                return res;
            }

            res = proc(env, args, argc);
            free_minim_objects(args, argc);
        }
        else if (op->type == MINIM_OBJ_SYNTAX)
        {
            MinimBuiltin proc = ((MinimBuiltin) op->data);

            for (size_t i = 0; i < argc; ++i)
                init_minim_object(&args[i], MINIM_OBJ_AST, node->children[i + 1]);   // initialize ast wrappers
            res = proc(env, args, argc);

            for (size_t i = 0; i < argc; ++i) free(args[i]);
            free(args);
        }
        else if (op->type == MINIM_OBJ_CLOSURE)
        {
            MinimLambda *lam = op->data;

            for (size_t i = 0; i < argc; ++i)
                args[i] = eval_ast_node(env, node->children[i + 1]);          

            possible_err = first_error(args, argc);
            if (possible_err)
            {
                for (size_t i = 0; i < argc; ++i)    // Clear it so it doesn't get deleted
                {
                    if (args[i] == possible_err)
                    {
                        res = possible_err;
                        args[i] = NULL;
                    }
                }
                
                free_minim_objects(args, argc);
                return res;
            }

            res = eval_lambda(lam, env, args, argc);
            free_minim_objects(args, argc);
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

int eval_ast(MinimEnv *env, MinimAst *ast, MinimObject **pobj)
{
    MinimObject *obj = eval_ast_node(env, ast);
    *pobj = obj;
    return !(obj->type & MINIM_OBJ_ERR);
}

int unsyntax_ast(MinimEnv *env, MinimAst *ast, MinimObject **pobj)
{
    MinimObject *obj = unsyntax_ast_node(env, ast, false);
    *pobj = obj;
    return !(obj->type & MINIM_OBJ_ERR);
}

int unsyntax_ast_rec(MinimEnv *env, MinimAst *ast, MinimObject **pobj)
{
    MinimObject *obj = unsyntax_ast_node(env, ast, true);
    *pobj = obj;
    return !(obj->type & MINIM_OBJ_ERR);
}

char *eval_string(char *str, size_t len)
{
    MinimAst *ast;
    MinimObject *obj;
    MinimEnv *env;
    PrintParams pp;
    char *out;

    init_env(&env, NULL);
    env_load_builtins(env);
    set_default_print_params(&pp);

    if (!parse_str(str, &ast))
    {
        char *tmp = "Parsing failed!";
        out = malloc((strlen(tmp) + 1) * sizeof(char));
        strcpy(out, tmp);
        free_env(env);
        return out;
    }

    eval_ast(env, ast, &obj);
    out = print_to_string(obj, env, &pp);

    free_minim_object(obj);
    free_ast(ast);
    free_env(env);
    return out;
}