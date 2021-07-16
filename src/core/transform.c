#include <string.h>

#include "../gc/gc.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "list.h"

#define CHECK_REC(proc, x, expr)        if (proc == x) return expr(env, ast, perr)

// ================================ Transform ================================

static bool
match_transform(MinimEnv *env, SyntaxNode *match, SyntaxNode *ast,
                char **reserved_names, size_t reservedc, bool ignore_first)
{
    MinimObject *match_unbox, *ast_unbox, *it, *it2;
    size_t len_m, len_a, idx;

    unsyntax_ast(env, match, &match_unbox);
    unsyntax_ast(env, ast, &ast_unbox);
    if (minim_listp(match_unbox) && minim_listp(ast_unbox))
    {
        len_m = minim_list_length(match_unbox);
        len_a = minim_list_length(ast_unbox);

        if (len_m != len_a)     return false;
        if (len_m == 0)         return true;

        idx = 0;
        it = match_unbox;
        it2 = ast_unbox;
        while (idx < len_m)
        {
            if (idx != 0 || !ignore_first)
            {
                if (!match_transform(env, MINIM_AST(MINIM_CAR(it)), MINIM_AST(MINIM_CAR(it2)),
                                     reserved_names, reservedc, false))
                    return false;
            }

            ++idx;
            it = MINIM_CDR(it);
            it2 = MINIM_CDR(it2);
        }
    }
    else if (MINIM_OBJ_SYMBOLP(match_unbox))    // intern matching syntax
    {
        for (size_t i = 0; i < reservedc; ++i)  // check reserved names
        {
            if (strcmp(MINIM_STRING(match_unbox), reserved_names[i]) == 0)
                return true;
        }

        init_minim_object(&it, MINIM_OBJ_SYNTAX, ast);  // wrap first
        env_intern_sym(env, MINIM_STRING(match_unbox), it);
    }
    else
    {
        return false;
    }

    return true;
}

static SyntaxNode*
apply_transformation(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    if (ast->type == SYNTAX_NODE_LIST)
    {
        for (size_t i = 0; i < ast->childc; ++i)
            ast->children[i] = apply_transformation(env, ast->children[i], perr);
    }
    else
    {
        MinimObject *val = env_get_sym(env, ast->sym);
        if (val)    return MINIM_AST(val);      // replace
    }

    return ast;
}

static SyntaxNode*
transform_loc(MinimEnv *env, MinimObject *trans, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *reserved_lst, *rule, *sym, *it;
    char **reserved_names;
    size_t reservedc;

    unsyntax_ast(env, MINIM_TRANSFORM_AST(trans)->children[1], &reserved_lst);
    reservedc = minim_list_length(reserved_lst);
    reserved_names = GC_alloc(reservedc * sizeof(char*));
    it = reserved_lst;

    for (size_t i = 0; i < reservedc; ++i, it = MINIM_CDR(it))
    {
        unsyntax_ast(env, MINIM_AST(MINIM_CAR(it)), &sym);
        reserved_names[i] = MINIM_STRING(sym);
    }

    for (size_t i = 2; i < MINIM_TRANSFORM_AST(trans)->childc; ++i)
    {
        SyntaxNode *match, *replace;
        MinimEnv *env2;

        init_env(&env2, NULL, NULL);  // isolated environment
        unsyntax_ast(env, MINIM_TRANSFORM_AST(trans)->children[i], &rule);

        match = MINIM_AST(MINIM_CAR(rule));
        replace = MINIM_AST(MINIM_CADR(rule));
        if (match_transform(env2, match, ast, reserved_names, reservedc, true))
        {
            ast = apply_transformation(env, replace, perr);
            return (perr) ? NULL : ast;
        }
    }

    *perr = minim_error("no matching syntax rule", MINIM_TRANSFORM_NAME(trans));
    return ast;
}

SyntaxNode* transform_syntax(MinimEnv *env, SyntaxNode* ast, MinimObject **perr)
{
    MinimObject *op;

    perr = NULL;        // signals success
    if (ast->type != SYNTAX_NODE_LIST)
        return ast;

    op = env_get_sym(env, ast->children[0]->sym);
    if (op && !MINIM_OBJ_SYNTAXP(op))
    {
        for (size_t i = 0; i < ast->childc; ++i)
        {
            transform_syntax(env, ast->children[i], perr);
            if (perr)   return ast;
        }

        if (MINIM_OBJ_TRANSFORMP(op))
        {
            transform_loc(env, op, ast, perr);
            if (perr)   return ast;
        }
    }

    return ast;
}

// ================================ Builtins ================================

MinimObject *minim_builtin_def_syntax(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *trans, *sym;

    unsyntax_ast(env, MINIM_AST(args[0]), &sym);
    init_minim_object(&trans, MINIM_OBJ_TRANSFORM, MINIM_STRING(sym), MINIM_AST(args[1]));
    env_intern_sym(env, MINIM_STRING(sym), trans);

    init_minim_object(&res, MINIM_OBJ_VOID);
    return res;
}

MinimObject *minim_builtin_syntax_rules(MinimEnv *env, MinimObject **args, size_t argc)
{
    return minim_error("should not execute here", "syntax-rules");
}
