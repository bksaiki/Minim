#include <string.h>

#include "../gc/gc.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "list.h"

#define CHECK_REC(proc, x, expr)        if (proc == x) return expr(env, ast, perr)

// ================================ Transform ================================

#define list_len_at_least_two(x) \
    ((x) && MINIM_CAR(x) && MINIM_CDR(x) && MINIM_CADR(x))


static bool
is_match_pattern(MinimObject *match)
{
    if (!list_len_at_least_two(match))
        return false;

    if (MINIM_AST(MINIM_CADR(match))->type != SYNTAX_NODE_DATUM)
        return false;

    return strcmp(MINIM_AST(MINIM_CADR(match))->sym, "...") == 0;
}

static bool
is_replace_pattern(SyntaxNode *replace, size_t idx)
{
    return (replace->type == SYNTAX_NODE_LIST &&
            idx + 1 < replace->childc &&
            replace->children[idx]->sym &&
            replace->children[idx + 1]->sym &&
            strcmp(replace->children[idx + 1]->sym, "...") == 0);
}

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
        // handle null cases
        if (minim_nullp(match_unbox) && minim_nullp(ast_unbox))
            return true;

        if (minim_nullp(match_unbox) || minim_nullp(ast_unbox))
            return false;

        idx = 0;
        len_m = minim_list_length(match_unbox);
        len_a = minim_list_length(ast_unbox);
        it = match_unbox;
        it2 = ast_unbox;

        while (it && it2)
        {
            if (idx != 0 || !ignore_first)
            {
                if (is_match_pattern(it))
                {
                    MinimObject *peek, *bind, *it3;
                    size_t ahead;

                    // printf("pattern in syntax rule\n");
                    // printf(" idx: %zu\n", idx);
                    // printf(" len_m: %zu\n", len_m);
                    // printf(" len_a: %zu\n", len_a);
                    
                    if (idx + 2 > len_m)    // not enough space for pattern variable
                        return false;

                    if (len_a + 2 < len_m)  // not enough space in ast
                        return false;

                    // save (1 before) iter and bind pattern
                    ahead = len_a + 2 - len_m;
                    peek = minim_list_ref(it2, ahead);

                    // env_intern_sym(env, MINIM_STRING(match_unbox), it);
                }
                if (!match_transform(env, MINIM_AST(MINIM_CAR(it)), MINIM_AST(MINIM_CAR(it2)),
                                     reserved_names, reservedc, false))
                {
                    return false;
                }
            }

            ++idx;
            it = MINIM_CDR(it);
            it2 = MINIM_CDR(it2);
        }

        if (!it2 && is_match_pattern(it))
        {
            MinimObject *null;

            init_minim_object(&null, MINIM_OBJ_PAIR, NULL, NULL);  // wrap first
            env_intern_sym(env, MINIM_AST(MINIM_CAR(it))->sym, null);
            it = MINIM_CDDR(it);
        }

        return (!it && !it2);   // both reached end
    }
    else if (MINIM_OBJ_VECTORP(match_unbox) && MINIM_OBJ_VECTORP(ast_unbox))
    {
        len_m = MINIM_VECTOR_LEN(match_unbox);
        len_a = MINIM_VECTOR_LEN(ast_unbox);

        if (len_m != len_a)     return false;
        if (len_m == 0)         return true;
        
        for (size_t i = 0; i < len_m; ++i)
        {
            if (!match_transform(env, MINIM_AST(MINIM_VECTOR_ARR(match_unbox)[i]),
                                 MINIM_AST(MINIM_VECTOR_ARR(ast_unbox)[i]),
                                 reserved_names, reservedc, false))
                return false;
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
    MinimObject *val;

    if (ast->type == SYNTAX_NODE_LIST || ast->type == SYNTAX_NODE_VECTOR)
    {
        for (size_t i = 0; i < ast->childc; ++i)
        {
            if (is_replace_pattern(ast, i))
            {
                val = env_get_sym(env, ast->children[i]->sym);
                if (val && minim_listp(val))
                {
                    size_t len = minim_list_length(val);
                    
                    if (len == 0)   // shrink by two
                    {
                        for (size_t j = i + 2; j < ast->childc; ++j)
                            ast->children[i] = ast->children[j];

                        ast->childc -= 2;
                        ast->children = GC_realloc(ast->children, ast->childc * sizeof(SyntaxNode*));
                    }
                    else if (len == 1) // shrink by one
                    {
                        for (size_t j = i + 2; j < ast->childc; ++j)
                            ast->children[i] = ast->children[j];

                        ast->childc -= 2;
                        ast->children = GC_realloc(ast->children, ast->childc * sizeof(SyntaxNode*));
                        ast->children[i] = MINIM_AST(MINIM_CAR(val));
                    }
                    else if (len == 2) // nothing
                    {

                    }
                    else        // expand
                    {

                    }

                    ++i;
                }
                else
                {
                    *perr = minim_error("not a pattern variable", NULL);
                    return NULL;
                }
            }
            else
            {
                ast->children[i] = apply_transformation(env, ast->children[i], perr);
            }
        }
    }
    else
    {
        val = env_get_sym(env, ast->sym);
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
        copy_syntax_node(&replace, MINIM_AST(MINIM_CADR(rule)));

        if (match_transform(env2, match, ast, reserved_names, reservedc, true))
        {
            ast = apply_transformation(env2, replace, perr);
            return (*perr) ? NULL : ast;
        }
    }

    *perr = minim_error("no matching syntax rule", MINIM_TRANSFORM_NAME(trans));
    return ast;
}

SyntaxNode* transform_syntax_rec(MinimEnv *env, SyntaxNode* ast, MinimObject **perr)
{
    if (ast->type == SYNTAX_NODE_LIST || ast->type == SYNTAX_NODE_VECTOR)
    {
    MinimObject *op;

        if (ast->childc == 0)
        return ast;

        for (size_t i = 0; i < ast->childc; ++i)
        {
            ast->children[i] = transform_syntax_rec(env, ast->children[i], perr);
            if (*perr)   return ast;
        }

        if (ast->children[0]->sym)
        {
            op = env_get_sym(env, ast->children[0]->sym);
            if (op && MINIM_OBJ_TRANSFORMP(op))
            {
                ast = transform_loc(env, op, ast, perr);
                if (*perr)   return ast;
            }
        }
    }

    return ast;
}

SyntaxNode* transform_syntax(MinimEnv *env, SyntaxNode* ast, MinimObject **perr)
{
    MinimEnv *env2;

    *perr = NULL;   // NULL signals success
    init_env(&env2, env, NULL);
    return transform_syntax_rec(env2, ast, perr);
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
