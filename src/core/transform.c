#include "../gc/gc.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "list.h"

#define CHECK_REC(proc, x, expr)        if (proc == x) return expr(env, ast, perr)

// ================================ Transform ================================

static bool match_transform_rec(MinimEnv *env, SyntaxNode *match, SyntaxNode *ast)
{

}

static bool match_transform(MinimEnv *env, SyntaxNode *match, SyntaxNode *ast)
{
    MinimObject *args;

    unsyntax_ast(env, ast, &args);
    for (MinimObject *it = MINIM_CAR(args), it2 = MINIM_CAR(ast);
         it && MINIM_CAR(it) &&; it = MINIM_CDR(it))
    {
        if (!match_transform_rec(env, MINIM_AST(MINIM_CAR(it)), 
    }

    return false;
}

static bool transform_loc(MinimEnv *env, MinimObject *trans, SyntaxNode *ast, MinimObject **perr)
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
        MinimEnv *env2;

        init_env(&env2, env, NULL);
        unsyntax_ast(env, MINIM_TRANSFORM_AST(trans)->children[i], &rule);
        if (match_transform(env2, MINIM_AST(MINIM_CAR(rule)), ast))
        {
            printf("Matched\n");
        }
    }

    return true;
}

bool transform_syntax(MinimEnv *env, SyntaxNode* ast, MinimObject **perr)
{
    MinimObject *op;

    if (ast->type != SYNTAX_NODE_LIST)
        return true;

    op = env_get_sym(env, ast->children[0]->sym);
    if (op && !MINIM_OBJ_SYNTAXP(op))
    {
        for (size_t i = 0; i < ast->childc; ++i)
        {
            if (!transform_syntax(env, ast->children[i], perr))
                return false;
        }

        if (MINIM_OBJ_TRANSFORMP(op))
        {
            if (!transform_loc(env, op, ast, perr))
                return false;
        }
    }

    return true;
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
