#include <string.h>
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "list.h"
#include "syntax.h"

#define CHECK_REC(proc, x, expr)        if (proc == x) return expr(env, ast, perr)
#define UNSYNTAX()

static bool check_syntax_rec(MinimEnv *env, SyntaxNode *ast, MinimObject **perr);

static bool check_syntax_def(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *sym;

    unsyntax_ast(env, ast->children[1], &sym);
    if (!MINIM_OBJ_SYMBOLP(sym))
    {
        *perr = minim_error("identifier should be symbol", "def");
        return false;
    }

    if (ast->childc == 3)
    {
        free_minim_object(sym);
        return check_syntax_rec(env, ast->children[2], perr);
    }
    else
    {
        MinimObject *args, *tmp;
        MinimEnv *env2;
        MinimLambda *lam;

        unsyntax_ast(env, ast->children[2], &args);
        if (MINIM_OBJ_PAIRP(args))
        {
            
        }
        else if (!MINIM_OBJ_SYMBOLP(args))
        {
            *perr = minim_error("expected argument names for %s", "def", MINIM_STRING(sym));
            free_minim_object(sym);
            return false;
        }
        

        init_minim_lambda(&lam);
        init_minim_object(&tmp, MINIM_OBJ_CLOSURE, lam);
        env_intern_sym(env, MINIM_STRING(sym), tmp);
        free_minim_object(args);
        free_minim_object(sym);

        init_env(&env2, env);
        for (size_t i = 3; i < ast->childc; ++i)
        {
            if (!check_syntax_rec(env, ast->children[i], perr))
            {
                pop_env(env2);
                return false;
            }
        }

        pop_env(env2);
    }

    return true;
}

static bool check_syntax_begin(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    for (size_t i = 0; i < ast->childc; ++i)
    {
        if (!check_syntax_rec(env, ast->children[i], perr))
            return false;
    }

    return true;
}

static bool check_syntax_cond(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    for (size_t i = 1; i < ast->childc; ++i)
    {
        MinimObject *branch;
        SyntaxNode *cond;

        unsyntax_ast(env, ast->children[1], &branch);
        if (!minim_listp(branch) || minim_list_length(branch) < 2)
        {
            *perr = minim_error("([<cond> <exprs> ...] ...)", "cond");
            free_minim_object(branch);
            return false;
        }

        // condition
        cond = MINIM_DATA(MINIM_CAR(branch));
        check_syntax_rec(env, ast->children[0], perr);
        if (i + 1 != ast->childc && cond->sym && strcmp(cond->sym, "else") == 0)
        {   
            free_minim_object(branch);
            return minim_error("else clause must be last", "cond");
        }
        
        for (MinimObject *it = MINIM_CDR(branch); it; it = MINIM_CDR(it))
        {
            if (!check_syntax_rec(env, MINIM_DATA(MINIM_CAR(it)), perr))
            {
                free_minim_object(branch);
                return false;
            }
        }

        free_minim_object(branch);
    }

    return true;
}

static bool check_syntax_let(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    for (size_t i = 1; i < ast->childc; ++i)
    {
        MinimObject *branch, *sym;
        SyntaxNode *syn;
        bool body;

        unsyntax_ast(env, ast->children[1], &branch);
        if (!minim_listp(branch) || minim_list_length(branch) != 2)
        {
            *perr = minim_error("([<symbol> <value>] ...)", ast->children[0]->sym);
            free_minim_object(branch);
            return false;
        }

        // identifier
        syn = MINIM_DATA(MINIM_CAR(branch));
        unsyntax_ast(env, syn, &sym);
        if (!MINIM_OBJ_SYMBOLP(sym))
        {
            free_minim_object(sym);
            free_minim_object(branch);
            *perr = minim_error("identifier should be symbol", "let");
            return false;
        }

        syn = MINIM_DATA(MINIM_CADR(branch));
        body = check_syntax_rec(env, syn, perr);
        free_minim_object(sym);
        free_minim_object(branch);
        if (!body)  return false;
    }

    return check_syntax_rec(env, ast->children[2], perr);
}

static bool check_syntax_rec(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *op;

    if (ast->type != SYNTAX_NODE_LIST)
        return true;

    op = env_peek_sym(env, ast->children[0]->sym);
    if (!op)
    {
        *perr = minim_error("unknown operator %s", ast->children[0]->sym);
        return false;
    }

    if (MINIM_OBJ_SYNTAXP(op))
    {
        void *proc = ((void*) op->u.ptrs.p1);

        CHECK_REC(proc, minim_builtin_begin, check_syntax_begin);
        CHECK_REC(proc, minim_builtin_def, check_syntax_def);
        CHECK_REC(proc, minim_builtin_when, check_syntax_begin);
        CHECK_REC(proc, minim_builtin_unless, check_syntax_begin);
        CHECK_REC(proc, minim_builtin_if, check_syntax_begin);
        CHECK_REC(proc, minim_builtin_cond, check_syntax_cond);
        CHECK_REC(proc, minim_builtin_let, check_syntax_let);
    }
    else
    {
        for (size_t i = 0; i < ast->childc; ++i)
        {
            if (!check_syntax_rec(env, ast->children[i], perr))
                return false;
        }
    }

    return true;
}

// Exported

bool check_syntax(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimEnv *env2;
    bool res;

    init_env(&env2, env);
    res = check_syntax_rec(env2, ast, perr);
    pop_env(env2);
    
    return res;
}
