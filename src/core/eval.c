#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/read.h"
#include "../gc/gc.h"
#include "arity.h"
#include "builtin.h"
#include "eval.h"
#include "error.h"
#include "lambda.h"
#include "list.h"
#include "number.h"
#include "syntax.h"

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

static bool is_float(const char *str)
{
    size_t idx = 0;
    bool digit = false;

    if (str[idx] == '+' || str[idx] == '-')
        ++idx;
    
    if (str[idx] >= '0' && str[idx] <= '9')
    {
        ++idx;
        digit = true;
    }
    
    while (str[idx] >= '0' && str[idx] <= '9')
        ++idx;

    if (str[idx] != '.' && str[idx] != 'e')
        return false;

    if (str[idx] == '.')
        ++idx;

    if (str[idx] >= '0' && str[idx] <= '9')
    {
        ++idx;
        digit = true;
    }

    while (str[idx] >= '0' && str[idx] <= '9')
        ++idx;

    if (str[idx] == 'e')
    {
        ++idx;
        if (!str[idx])  return false;

        if ((str[idx] == '+' || str[idx] == '-') &&
            str[idx + 1] >= '0' && str[idx + 1] <= '9')
            idx += 2;

        while (str[idx] >= '0' && str[idx] <= '9')
            ++idx;
    }

    return digit && !str[idx];
}

static bool is_str(char *str)
{
    size_t len = strlen(str);

    if (len < 2 || str[0] != '\"' || str[len - 1] != '\"')
        return false;

    for (size_t i = 1; i < len; ++i)
    {
        if (str[i] == '\"' && str[i - 1] != '\\' && i + 1 != len)
            return false;
    }

    return true;
}

static MinimObject *str_to_node(char *str, MinimEnv *env, bool quote)
{
    MinimObject *res;

    if (is_rational(str))
    {
        mpq_ptr rat = GC_alloc_mpq_ptr();

        mpq_set_str(rat, str, 0);
        mpq_canonicalize(rat);
        init_minim_object(&res, MINIM_OBJ_EXACT, rat);
    }
    else if (is_float(str))
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, strtod(str, NULL));
    }
    else if (is_str(str))
    {
        size_t len = strlen(str) - 1;
        char *tmp = GC_alloc(len * sizeof(char));

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

            if (!res)
                return minim_error("unrecognized symbol", str);
            else if (MINIM_OBJ_SYNTAXP(res))
                return minim_error("bad syntax", str);
        }
    }

    return res;
}

static MinimObject *error_or_exit(MinimObject **args, size_t argc)
{
    for (size_t i = 0; i < argc; ++i)
    {
        if (MINIM_OBJ_THROWNP(args[i]))
            return args[i];
    }

    return NULL;
}

// Unsyntax

static MinimObject *unsyntax_ast_node(MinimEnv *env, SyntaxNode* node, bool rec)
{
    if (node->type == SYNTAX_NODE_LIST)
    {
        MinimObject **args, *res;
        MinimBuiltin proc;

        if (node->childc == 3 && node->children[1]->sym &&
            strcmp(node->children[1]->sym, ".") == 0)
        {
            args = GC_alloc(2 * sizeof(MinimObject*));
            if (rec)
            {
                args[0] = unsyntax_ast_node(env, node->children[0], rec);
                args[1] = unsyntax_ast_node(env, node->children[2], rec);
            }
            else
            {
                init_minim_object(&args[0], MINIM_OBJ_AST, node->children[0]);
                init_minim_object(&args[1], MINIM_OBJ_AST, node->children[2]);
            }

            proc = ((MinimBuiltin) env_peek_sym(env, "cons")->u.ptrs.p1);
            res = proc(env, args, 2);
        }
        else if (node->childc > 2 && node->children[node->childc - 2]->sym &&
                 strcmp(node->children[node->childc - 2]->sym, ".") == 0)
        {
            MinimObject *rest;
            size_t reduc = node->childc - 2;

            args = GC_alloc(reduc * sizeof(MinimObject*));
            for (size_t i = 0; i < reduc; ++i)
            {
                if (rec)    args[i] = unsyntax_ast_node(env, node->children[i], rec);
                else        init_minim_object(&args[i], MINIM_OBJ_AST, node->children[i]);
            }

            proc = ((MinimBuiltin) env_peek_sym(env, "list")->u.ptrs.p1);
            res = proc(env, args, reduc);

            for (rest = res; MINIM_CDR(rest); rest = MINIM_CDR(rest));
            if (rec)    MINIM_CDR(rest) = unsyntax_ast_node(env, node->children[node->childc - 1], rec);
            else        init_minim_object(&MINIM_CDR(rest), MINIM_OBJ_AST, node->children[node->childc - 1]);
        }
        else
        {
            args = GC_alloc(node->childc * sizeof(MinimObject*));
            for (size_t i = 0; i < node->childc; ++i)
            {
                if (rec)    args[i] = unsyntax_ast_node(env, node->children[i], rec);
                else        init_minim_object(&args[i], MINIM_OBJ_AST, node->children[i]);
            }

            proc = ((MinimBuiltin) env_peek_sym(env, "list")->u.ptrs.p1);
            res = proc(env, args, node->childc);
        }

        return res;
    }
    else if (node->type == SYNTAX_NODE_VECTOR)
    {
        MinimObject **args;
        MinimObject *res;

        args = GC_alloc(node->childc * sizeof(MinimObject*));
        for (size_t i = 0; i < node->childc; ++i)
        {
            if (rec)    args[i] = unsyntax_ast_node(env, node->children[i], rec);
            else        init_minim_object(&args[i], MINIM_OBJ_AST, node->children[i]);
        }

        res = minim_builtin_vector(env, args, node->childc);
        return res;
    }
    else if (node->type == SYNTAX_NODE_PAIR)
    {
        MinimObject **args;
        MinimObject *res;

        args = GC_alloc(2 * sizeof(MinimObject*));
        if (rec)
        {
            args[0] = unsyntax_ast_node(env, node->children[0], rec);
            args[1] = unsyntax_ast_node(env, node->children[1], rec);
        }
        else
        {
            init_minim_object(&args[0], MINIM_OBJ_AST, node->children[0]);
            init_minim_object(&args[1], MINIM_OBJ_AST, node->children[1]);
        }

        res = minim_builtin_cons(env, args, 2);
        return res;
    }
    else
    {
        return str_to_node(node->sym, env, true);
    }
}

// Eval mainloop

static MinimObject *eval_ast_node(MinimEnv *env, SyntaxNode *node)
{
    if (node->type == SYNTAX_NODE_LIST)
    {
        MinimObject *res, *op, *possible_err, **args;
        size_t argc;

        if (node->childc == 0)
            return minim_error("missing procedure expression", NULL);

        argc = node->childc - 1;
        args = GC_alloc(argc * sizeof(MinimObject*));
        op = env_peek_sym(env, node->children[0]->sym);

        if (!op)
        {
            res = minim_error("unknown operator", node->children[0]->sym);
            return res;
        }

        if (MINIM_OBJ_BUILTINP(op))
        {
            MinimBuiltin proc = ((MinimBuiltin) op->u.ptrs.p1);

            for (size_t i = 0; i < argc; ++i)
                args[i] = eval_ast_node(env, node->children[i + 1]);          

            possible_err = error_or_exit(args, argc);
            if (possible_err)
            {
                res = possible_err;
            }
            else
            {
                if (minim_check_arity(proc, argc, env, &res))
                    res = proc(env, args, argc);
            }
        }
        else if (MINIM_OBJ_SYNTAXP(op))
        {
            MinimBuiltin proc = ((MinimBuiltin) op->u.ptrs.p1);

            for (size_t i = 0; i < argc; ++i)
                init_minim_object(&args[i], MINIM_OBJ_AST, node->children[i + 1]);   // initialize ast wrappers
            res = proc(env, args, argc);

            if (MINIM_OBJ_CLOSUREP(res))
            {
                MinimLambda *lam = res->u.ptrs.p1;
                if (!lam->loc && node->children[0]->loc)
                    copy_syntax_loc(&lam->loc, node->children[0]->loc);
            }
        }
        else if (MINIM_OBJ_CLOSUREP(op))
        {
            MinimLambda *lam = op->u.ptrs.p1;

            for (size_t i = 0; i < argc; ++i)
                args[i] = eval_ast_node(env, node->children[i + 1]);          

            possible_err = error_or_exit(args, argc);
            res = (possible_err) ? possible_err : eval_lambda(lam, env, args, argc);
        }
        else
        {   
            res = minim_error("unknown operator", node->children[0]->sym);
        }

        return res;
    }
    else if (node->type == SYNTAX_NODE_VECTOR)
    {
        MinimObject **args;
        MinimObject *possible_err;

        args = GC_alloc(node->childc * sizeof(MinimObject*));
        for (size_t i = 0; i < node->childc; ++i)
            args[i] = eval_ast_node(env, node->children[i]);

        possible_err = error_or_exit(args, node->childc);
        return (possible_err) ? possible_err : minim_builtin_vector(env, args, node->childc);
    }
    else if (node->type == SYNTAX_NODE_PAIR)
    {
        MinimObject **args;
        MinimObject *possible_err;

        args = GC_alloc(2 * sizeof(MinimObject*));
        args[0] = eval_ast_node(env, node->children[0]);
        args[1] = eval_ast_node(env, node->children[1]);

        possible_err = error_or_exit(args, node->childc);
        return (possible_err) ? possible_err : minim_builtin_cons(env, args, 2);
    }
    else
    {
        return str_to_node(node->sym, env, false);
    }
}

// Visible functions

int eval_ast(MinimEnv *env, SyntaxNode *ast, MinimObject **pobj)
{
    if (!check_syntax(env, ast, pobj))
        return 0;

    *pobj = eval_ast_node(env, ast);
    return !MINIM_OBJ_ERRORP((*pobj));
}

int eval_ast_no_check(MinimEnv* env, SyntaxNode *ast, MinimObject **pobj)
{
    *pobj = eval_ast_node(env, ast);
    return !MINIM_OBJ_ERRORP((*pobj));
}

int unsyntax_ast(MinimEnv *env, SyntaxNode *ast, MinimObject **pobj)
{
    MinimObject *obj = unsyntax_ast_node(env, ast, false);
    *pobj = obj;
    return !MINIM_OBJ_ERRORP(obj);
}

int unsyntax_ast_rec(MinimEnv *env, SyntaxNode *ast, MinimObject **pobj)
{
    MinimObject *obj = unsyntax_ast_node(env, ast, true);
    *pobj = obj;
    return !MINIM_OBJ_ERRORP(obj);
}

char *eval_string(char *str, size_t len)
{
    SyntaxNode *ast;
    MinimObject *obj;
    MinimEnv *env;
    PrintParams pp;
    char *out;

    init_env(&env, NULL);
    minim_load_builtins(env);
    set_default_print_params(&pp);

    if (parse_str(str, &ast))
    {
        char *tmp = "Parsing failed!";
        out = GC_alloc((strlen(tmp) + 1) * sizeof(char));
        strcpy(out, tmp);
        return out;
    }

    eval_ast(env, ast, &obj);
    return print_to_string(obj, env, &pp);
}
